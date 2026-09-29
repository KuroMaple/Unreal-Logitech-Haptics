#include "MXBridgeCommands.h"
#include "MXBridgeModule.h"

#include "Editor.h"
#include "EditorModeManager.h"
#include "Engine/Blueprint.h"
#include "Engine/Selection.h"
#include "FileHelpers.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "GameFramework/Actor.h"
#include "IAssetViewport.h"
#include "ILiveCodingModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "LevelEditorViewport.h"
#include "Modules/ModuleManager.h"
#include "PlayInEditorDataTypes.h"
#include "SLevelViewport.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "UObject/UObjectIterator.h"

namespace
{
	using FCommandFn = TFunction<void(const TSharedRef<FJsonObject>&)>;

	FLevelEditorModule& LevelEditor()
	{
		return FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	}

	TArray<AActor*> SelectedActors()
	{
		TArray<AActor*> Actors;
		if (USelection* Selection = GEditor->GetSelectedActors())
		{
			Selection->GetSelectedObjects<AActor>(Actors);
		}
		return Actors;
	}

	// ── PIE ────────────────────────────────────────────────────────────────

	bool IsPlaySessionActiveOrQueued()
	{
		return GEditor->PlayWorld != nullptr || GEditor->IsPlaySessionRequestQueued();
	}

	void RequestSession(EPlaySessionWorldType WorldType)
	{
		if (IsPlaySessionActiveOrQueued())
		{
			return;
		}

		// Play in the active level viewport, same as the toolbar's "Selected Viewport" option.
		// RequestPlaySession is queued and starts on the next editor tick.
		FRequestPlaySessionParams Params;
		Params.WorldType = WorldType;
		if (TSharedPtr<IAssetViewport> Viewport = LevelEditor().GetFirstActiveViewport())
		{
			Params.DestinationSlateViewport = Viewport;
		}
		GEditor->RequestPlaySession(Params);
	}

	void PiePlay(const TSharedRef<FJsonObject>&)     { RequestSession(EPlaySessionWorldType::PlayInEditor); }
	void PieSimulate(const TSharedRef<FJsonObject>&) { RequestSession(EPlaySessionWorldType::SimulateInEditor); }

	void PieStop(const TSharedRef<FJsonObject>&)
	{
		if (GEditor->PlayWorld)
		{
			GEditor->RequestEndPlayMap();
		}
	}

	// ── Build / file ───────────────────────────────────────────────────────

	void LiveCoding(const TSharedRef<FJsonObject>&)
	{
		ILiveCodingModule* LiveCodingModule = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
		if (!LiveCodingModule || !LiveCodingModule->IsEnabledForSession())
		{
			UE_LOG(LogMXBridge, Warning, TEXT("MXBridge: Live Coding is not enabled for this session"));
			return;
		}
		if (!LiveCodingModule->IsCompiling())
		{
			// Non-blocking: the result is observed by FMXBridgeHaptics watching IsCompiling().
			LiveCodingModule->Compile(ELiveCodingCompileFlags::None, nullptr);
		}
	}

	void CompileBlueprints(const TSharedRef<FJsonObject>&)
	{
		// Compile every loaded Blueprint with pending changes (what the toolbar Compile button
		// does per-editor). If none are dirty, fall back to the selected actors' Blueprints.
		TArray<UBlueprint*> ToCompile;
		for (TObjectIterator<UBlueprint> It; It; ++It)
		{
			UBlueprint* Blueprint = *It;
			if (!Blueprint->HasAnyFlags(RF_Transient | RF_ClassDefaultObject) && !Blueprint->IsUpToDate())
			{
				ToCompile.Add(Blueprint);
			}
		}

		if (ToCompile.IsEmpty())
		{
			for (AActor* Actor : SelectedActors())
			{
				if (UBlueprint* Blueprint = Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy))
				{
					ToCompile.AddUnique(Blueprint);
				}
			}
		}

		for (UBlueprint* Blueprint : ToCompile)
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		}
		UE_LOG(LogMXBridge, Log, TEXT("MXBridge: compiled %d Blueprint(s)"), ToCompile.Num());
	}

	void SaveAll(const TSharedRef<FJsonObject>&)
	{
		// Same as File > Save All, without the "which packages?" prompt.
		FEditorFileUtils::SaveDirtyPackages(/*bPromptUserToSave=*/ false, /*bSaveMapPackages=*/ true, /*bSaveContentPackages=*/ true);
	}

	void BuildLighting(const TSharedRef<FJsonObject>&)
	{
		FLevelEditorActionCallbacks::BuildLightingOnly_Execute();
	}

	// ── Transform / snapping ───────────────────────────────────────────────

	void SetWidgetMode(UE::Widget::EWidgetMode Mode)
	{
		if (FLevelEditorActionCallbacks::CanSetWidgetMode(Mode))
		{
			FLevelEditorActionCallbacks::SetWidgetMode(Mode);
		}
	}

	void TransformTranslate(const TSharedRef<FJsonObject>&) { SetWidgetMode(UE::Widget::WM_Translate); }
	void TransformRotate(const TSharedRef<FJsonObject>&)    { SetWidgetMode(UE::Widget::WM_Rotate); }
	void TransformScale(const TSharedRef<FJsonObject>&)     { SetWidgetMode(UE::Widget::WM_Scale); }

	void ToggleSpace(const TSharedRef<FJsonObject>&)
	{
		FEditorModeTools& ModeTools = GLevelEditorModeTools();
		const ECoordSystem Current = ModeTools.GetCoordSystem(/*bGetRawValue=*/ true);
		ModeTools.SetCoordSystem(Current == COORD_World ? COORD_Local : COORD_World);
	}

	void ToggleGridSnap(const TSharedRef<FJsonObject>&)
	{
		FLevelEditorActionCallbacks::LocationGridSnap_Clicked();
	}

	// ── Viewport ───────────────────────────────────────────────────────────

	void FocusSelected(const TSharedRef<FJsonObject>&)
	{
		const TArray<AActor*> Actors = SelectedActors();
		if (!Actors.IsEmpty())
		{
			GEditor->MoveViewportCamerasToActor(Actors, /*bActiveViewportOnly=*/ true);
		}
	}

	void ToggleGameView(const TSharedRef<FJsonObject>&)
	{
		if (TSharedPtr<SLevelViewport> Viewport = LevelEditor().GetFirstActiveLevelViewport())
		{
			Viewport->ToggleGameView();
		}
	}

	void SetViewMode(EViewModeIndex Mode)
	{
		if (TSharedPtr<SLevelViewport> Viewport = LevelEditor().GetFirstActiveLevelViewport())
		{
			FLevelEditorViewportClient& Client = Viewport->GetLevelViewportClient();
			Client.SetViewMode(Mode);
			Client.Invalidate();
		}
	}

	void ViewLit(const TSharedRef<FJsonObject>&)       { SetViewMode(VMI_Lit); }
	void ViewUnlit(const TSharedRef<FJsonObject>&)     { SetViewMode(VMI_Unlit); }
	// "Wireframe" in the perspective viewport's View Mode menu is the brush wireframe mode.
	void ViewWireframe(const TSharedRef<FJsonObject>&) { SetViewMode(VMI_BrushWireframe); }

	// ── Navigation ─────────────────────────────────────────────────────────

	void OpenContentBrowser(const TSharedRef<FJsonObject>&)
	{
		FLevelEditorActionCallbacks::OpenContentBrowser();
	}

	void SyncBrowser(const TSharedRef<FJsonObject>&)
	{
		// Ctrl+B: browse to the selected actors' assets.
		FLevelEditorActionCallbacks::FindInContentBrowser_Clicked();
	}

	void OpenAsset(const TSharedRef<FJsonObject>&)
	{
		// Ctrl+P "Open Asset" picker. Its handler is private, so route through a command list
		// with the engine's own global mappings.
		TSharedRef<FUICommandList> Commands = MakeShared<FUICommandList>();
		FGlobalEditorCommonCommands::MapActions(Commands);
		Commands->ExecuteAction(FGlobalEditorCommonCommands::Get().SummonOpenAssetDialog.ToSharedRef());
	}

	void OpenOutputLog(const TSharedRef<FJsonObject>&)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("OutputLog")));
	}

	const TMap<FString, FCommandFn>& Registry()
	{
		static const TMap<FString, FCommandFn> Commands =
		{
			{ TEXT("pie.play"),                 &PiePlay },
			{ TEXT("pie.simulate"),             &PieSimulate },
			{ TEXT("pie.stop"),                 &PieStop },

			{ TEXT("build.livecoding"),         &LiveCoding },
			{ TEXT("build.blueprint_compile"),  &CompileBlueprints },
			{ TEXT("file.save_all"),            &SaveAll },
			{ TEXT("build.lighting"),           &BuildLighting },

			{ TEXT("transform.translate"),      &TransformTranslate },
			{ TEXT("transform.rotate"),         &TransformRotate },
			{ TEXT("transform.scale"),          &TransformScale },
			{ TEXT("transform.toggle_space"),   &ToggleSpace },
			{ TEXT("snap.toggle_grid"),         &ToggleGridSnap },

			{ TEXT("viewport.focus_selected"),  &FocusSelected },
			{ TEXT("viewport.game_view"),       &ToggleGameView },
			{ TEXT("viewport.lit"),             &ViewLit },
			{ TEXT("viewport.unlit"),           &ViewUnlit },
			{ TEXT("viewport.wireframe"),       &ViewWireframe },

			{ TEXT("nav.content_browser"),      &OpenContentBrowser },
			{ TEXT("nav.sync_browser"),         &SyncBrowser },
			{ TEXT("nav.open_asset"),           &OpenAsset },
			{ TEXT("nav.output_log"),           &OpenOutputLog },
		};
		return Commands;
	}
}

bool FMXBridgeCommands::IsKnown(const FString& Id)
{
	return Registry().Contains(Id);
}

void FMXBridgeCommands::Execute(const TSharedRef<FJsonObject>& Event)
{
	check(IsInGameThread());

	const FString Id = Event->GetStringField(TEXT("id"));
	if (const FCommandFn* Fn = Registry().Find(Id))
	{
		UE_LOG(LogMXBridge, Verbose, TEXT("MXBridge: executing %s"), *Id);
		(*Fn)(Event);
	}
}

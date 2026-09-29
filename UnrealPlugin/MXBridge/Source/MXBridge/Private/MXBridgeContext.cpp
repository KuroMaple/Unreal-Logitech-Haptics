#include "MXBridgeContext.h"

#include "Editor.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Modules/ModuleManager.h"
#include "SLevelViewport.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "UnrealWidgetFwd.h"

namespace
{
	int64 GContextSeq = 0;

	FString WidgetModeToString(UE::Widget::EWidgetMode Mode)
	{
		switch (Mode)
		{
		case UE::Widget::WM_Translate:        return TEXT("translate");
		case UE::Widget::WM_Rotate:           return TEXT("rotate");
		case UE::Widget::WM_Scale:            return TEXT("scale");
		case UE::Widget::WM_TranslateRotateZ: return TEXT("translate_rotate_z");
		case UE::Widget::WM_2D:               return TEXT("2d");
		default:                              return TEXT("none");
		}
	}

	FString ViewModeToString(EViewModeIndex Mode)
	{
		switch (Mode)
		{
		case VMI_Lit:            return TEXT("lit");
		case VMI_Unlit:          return TEXT("unlit");
		case VMI_BrushWireframe: // perspective "Wireframe" in the viewport menu
		case VMI_Wireframe:      return TEXT("wireframe");
		case VMI_LightingOnly:   return TEXT("lighting_only");
		default:                 return TEXT("other");
		}
	}

	TSharedRef<FJsonObject> BuildSelection()
	{
		int32 Count = 0;
		UClass* CommonClass = nullptr;
		bool bMixed = false;

		if (USelection* Selected = GEditor->GetSelectedActors())
		{
			for (FSelectionIterator It(*Selected); It; ++It)
			{
				if (const AActor* Actor = Cast<AActor>(*It))
				{
					++Count;
					if (!CommonClass)
					{
						CommonClass = Actor->GetClass();
					}
					else if (CommonClass != Actor->GetClass())
					{
						bMixed = true;
					}
				}
			}
		}

		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("count"), Count);
		Json->SetStringField(TEXT("class"), bMixed ? TEXT("Mixed") : (CommonClass ? CommonClass->GetName() : TEXT("")));
		return Json;
	}

	TSharedRef<FJsonObject> BuildViewport()
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		FString Mode = TEXT("unknown");
		bool bGameView = false;

		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		if (TSharedPtr<SLevelViewport> Viewport = LevelEditor.GetFirstActiveLevelViewport())
		{
			const FLevelEditorViewportClient& Client = Viewport->GetLevelViewportClient();
			Mode = ViewModeToString(Client.GetViewMode());
			bGameView = Client.IsInGameView();
		}

		Json->SetStringField(TEXT("mode"), Mode);
		Json->SetBoolField(TEXT("game_view"), bGameView);
		return Json;
	}
}

TSharedRef<FJsonObject> FMXBridgeContext::Build()
{
	check(IsInGameThread());

	TSharedRef<FJsonObject> Context = MakeShared<FJsonObject>();

	// PlayWorld is non-null for both Play and Simulate sessions.
	Context->SetBoolField(TEXT("is_pie_running"), GEditor->PlayWorld != nullptr);
	Context->SetBoolField(TEXT("is_simulating"), GEditor->bIsSimulatingInEditor);

	Context->SetObjectField(TEXT("selection"), BuildSelection());

	// GLevelEditorModeTools() owns the level editor's gizmo mode and coordinate space.
	FEditorModeTools& ModeTools = GLevelEditorModeTools();
	TSharedRef<FJsonObject> Transform = MakeShared<FJsonObject>();
	Transform->SetStringField(TEXT("mode"), WidgetModeToString(ModeTools.GetWidgetMode()));
	Transform->SetStringField(TEXT("space"), ModeTools.GetCoordSystem() == COORD_Local ? TEXT("local") : TEXT("world"));
	Context->SetObjectField(TEXT("transform"), Transform);

	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	TSharedRef<FJsonObject> Grid = MakeShared<FJsonObject>();
	Grid->SetBoolField(TEXT("enabled"), ViewportSettings->GridEnabled != 0);
	Grid->SetNumberField(TEXT("size"), GEditor->GetGridSize());
	Context->SetObjectField(TEXT("grid"), Grid);

	Context->SetObjectField(TEXT("viewport"), BuildViewport());

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema"), 1);
	Root->SetNumberField(TEXT("seq"), ++GContextSeq);
	Root->SetObjectField(TEXT("context"), Context);
	return Root;
}

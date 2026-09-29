#include "MXBridgeHaptics.h"
#include "MXBridgeModule.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "HttpServerResponse.h"
#include "ILiveCodingModule.h"
#include "Modules/ModuleManager.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

namespace
{
	// Save All writes many packages; coalesce them into one tick.
	constexpr double SaveHapticCooldownSeconds = 1.0;
	constexpr int32 MaxWaitMs = 5000;
}

void FMXBridgeHaptics::Start()
{
	BeginPIEHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FMXBridgeHaptics::OnBeginPIE);
	EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FMXBridgeHaptics::OnEndPIE);
	LightingSucceededHandle = FEditorDelegates::OnLightingBuildSucceeded.AddLambda([this]() { Push(TEXT("happy_alert"), TEXT("build.lighting")); });
	LightingFailedHandle = FEditorDelegates::OnLightingBuildFailed.AddLambda([this]() { Push(TEXT("mad"), TEXT("build.lighting")); });
	PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(this, &FMXBridgeHaptics::OnPackageSaved);
	BlueprintPreCompileHandle = GEditor->OnBlueprintPreCompile().AddRaw(this, &FMXBridgeHaptics::OnBlueprintPreCompile);
	BlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &FMXBridgeHaptics::OnBlueprintCompiled);

	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		LiveCodingPatchHandle = LiveCoding->GetOnPatchCompleteDelegate().AddRaw(this, &FMXBridgeHaptics::OnLiveCodingPatchComplete);
	}

	// Drives long-poll timeouts and Live Coding state tracking. ~20 Hz is plenty.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMXBridgeHaptics::Tick), 0.05f);
}

void FMXBridgeHaptics::Stop()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);

	FEditorDelegates::BeginPIE.Remove(BeginPIEHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);
	FEditorDelegates::OnLightingBuildSucceeded.Remove(LightingSucceededHandle);
	FEditorDelegates::OnLightingBuildFailed.Remove(LightingFailedHandle);
	UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
	if (GEditor)
	{
		GEditor->OnBlueprintPreCompile().Remove(BlueprintPreCompileHandle);
		GEditor->OnBlueprintCompiled().Remove(BlueprintCompiledHandle);
	}
	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		LiveCoding->GetOnPatchCompleteDelegate().Remove(LiveCodingPatchHandle);
	}

	// Release parked long-polls so the client sees a clean response instead of a hang.
	FlushWaiters(/*bForceAll=*/ true);
}

void FMXBridgeHaptics::Push(const TCHAR* Waveform, const TCHAR* Source)
{
	Events.Add({ ++LatestSeq, Waveform, Source, FPlatformTime::Seconds() });
	if (Events.Num() > Capacity)
	{
		Events.RemoveAt(0, Events.Num() - Capacity);
	}
	UE_LOG(LogMXBridge, Verbose, TEXT("MXBridge haptic #%lld: %s (%s)"), LatestSeq, Waveform, Source);
	FlushWaiters(/*bForceAll=*/ false);
}

void FMXBridgeHaptics::HandlePoll(int64 SinceSeq, int32 WaitMs, const FHttpResultCallback& OnComplete)
{
	WaitMs = FMath::Clamp(WaitMs, 0, MaxWaitMs);

	// Anything other than "client is fully caught up" answers immediately:
	//   SinceSeq < LatestSeq  -> there are new events
	//   SinceSeq < 0          -> client is (re)connecting and only wants the current cursor
	//   SinceSeq > LatestSeq  -> editor restarted and the sequence reset; resync the client
	if (SinceSeq != LatestSeq || WaitMs == 0)
	{
		Respond(SinceSeq, OnComplete);
		return;
	}

	Waiters.Add({ SinceSeq, FPlatformTime::Seconds() + WaitMs / 1000.0, OnComplete });
}

void FMXBridgeHaptics::Respond(int64 SinceSeq, const FHttpResultCallback& OnComplete) const
{
	TArray<TSharedPtr<FJsonValue>> Out;
	if (SinceSeq >= 0 && SinceSeq <= LatestSeq)
	{
		for (const FEvent& Event : Events)
		{
			if (Event.Seq > SinceSeq)
			{
				TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
				Json->SetNumberField(TEXT("seq"), Event.Seq);
				Json->SetStringField(TEXT("waveform"), Event.Waveform);
				Json->SetStringField(TEXT("source"), Event.Source);
				Out.Add(MakeShared<FJsonValueObject>(Json));
			}
		}
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("seq"), LatestSeq);
	Root->SetArrayField(TEXT("events"), Out);

	FString Text;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	OnComplete(FHttpServerResponse::Create(Text, TEXT("application/json")));
}

void FMXBridgeHaptics::FlushWaiters(bool bForceAll)
{
	const double Now = FPlatformTime::Seconds();
	for (int32 i = Waiters.Num() - 1; i >= 0; --i)
	{
		const FWaiter& Waiter = Waiters[i];
		if (bForceAll || Waiter.SinceSeq < LatestSeq || Now >= Waiter.Deadline)
		{
			Respond(Waiter.SinceSeq, Waiter.OnComplete);
			Waiters.RemoveAtSwap(i);
		}
	}
}

bool FMXBridgeHaptics::Tick(float)
{
	TickLiveCoding();
	FlushWaiters(/*bForceAll=*/ false);
	return true;
}

// ── Editor hooks ───────────────────────────────────────────────────────────

void FMXBridgeHaptics::OnBeginPIE(bool)
{
	Push(TEXT("sharp_state_change"), TEXT("pie.begin"));
}

void FMXBridgeHaptics::OnEndPIE(bool)
{
	Push(TEXT("damp_state_change"), TEXT("pie.end"));
}

void FMXBridgeHaptics::OnPackageSaved(const FString&, UPackage*, FObjectPostSaveContext Context)
{
	// Ignore cooks / procedural saves; only user-facing editor saves should buzz.
	if (Context.IsProceduralSave() || IsRunningCommandlet())
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - LastSaveHapticTime >= SaveHapticCooldownSeconds)
	{
		LastSaveHapticTime = Now;
		// Saves are frequent, so use the lightest waveform rather than an attention-grabbing one.
		Push(TEXT("subtle_collision"), TEXT("file.save"));
	}
}

void FMXBridgeHaptics::OnBlueprintPreCompile(UBlueprint* Blueprint)
{
	CompilingBlueprints.AddUnique(Blueprint);
}

void FMXBridgeHaptics::OnBlueprintCompiled()
{
	// OnBlueprintCompiled carries no arguments, so check the status of every Blueprint that
	// announced itself in PreCompile since the last batch.
	bool bAnyError = false;
	for (const TWeakObjectPtr<UBlueprint>& Blueprint : CompilingBlueprints)
	{
		if (Blueprint.IsValid() && Blueprint->Status == BS_Error)
		{
			bAnyError = true;
		}
	}
	CompilingBlueprints.Reset();

	if (bAnyError)
	{
		Push(TEXT("mad"), TEXT("build.blueprint_compile"));
	}
}

void FMXBridgeHaptics::OnLiveCodingPatchComplete()
{
	bLiveCodingPatched = true;
}

void FMXBridgeHaptics::TickLiveCoding()
{
	// Live Coding only exposes a success delegate, so infer the outcome from the
	// IsCompiling() edge: a compile that ends without a patch applied counts as a failure.
	// Note: "no changes" compiles also end without a patch and will report as failure.
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	const bool bCompiling = LiveCoding && LiveCoding->IsCompiling();

	if (bCompiling && !bLiveCodingWasCompiling)
	{
		bLiveCodingPatched = false;
	}
	else if (!bCompiling && bLiveCodingWasCompiling)
	{
		Push(bLiveCodingPatched ? TEXT("completed") : TEXT("mad"), TEXT("build.livecoding"));
	}
	bLiveCodingWasCompiling = bCompiling;
}

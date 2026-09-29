#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "HttpResultCallback.h"

class UBlueprint;
class UPackage;
class FObjectPostSaveContext;

/**
 * Collects haptic events from editor delegates and serves them to GET /haptics.
 *
 * Events live in a small ring buffer keyed by an increasing sequence number. The client
 * long-polls with ?since=<last seq seen>; a lost response never loses events because the
 * client simply asks again from its last cursor. Game thread only.
 */
class FMXBridgeHaptics
{
public:
	void Start();
	void Stop();

	void Push(const TCHAR* Waveform, const TCHAR* Source);

	/**
	 * Completes immediately if events newer than SinceSeq exist (or SinceSeq < 0, which just
	 * syncs the cursor), otherwise parks the request until an event arrives or WaitMs elapses.
	 */
	void HandlePoll(int64 SinceSeq, int32 WaitMs, const FHttpResultCallback& OnComplete);

private:
	struct FEvent
	{
		int64 Seq;
		FString Waveform;
		FString Source;
		double Time;
	};

	struct FWaiter
	{
		int64 SinceSeq;
		double Deadline;
		FHttpResultCallback OnComplete;
	};

	void Respond(int64 SinceSeq, const FHttpResultCallback& OnComplete) const;
	void FlushWaiters(bool bForceAll);
	bool Tick(float DeltaTime);

	void OnBeginPIE(bool bIsSimulating);
	void OnEndPIE(bool bIsSimulating);
	void OnPackageSaved(const FString& Filename, UPackage* Package, FObjectPostSaveContext Context);
	void OnBlueprintPreCompile(UBlueprint* Blueprint);
	void OnBlueprintCompiled();
	void OnLiveCodingPatchComplete();
	void TickLiveCoding();

	static constexpr int32 Capacity = 64;
	TArray<FEvent> Events;
	int64 LatestSeq = 0;
	TArray<FWaiter> Waiters;

	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle BeginPIEHandle;
	FDelegateHandle EndPIEHandle;
	FDelegateHandle LightingSucceededHandle;
	FDelegateHandle LightingFailedHandle;
	FDelegateHandle PackageSavedHandle;
	FDelegateHandle BlueprintPreCompileHandle;
	FDelegateHandle BlueprintCompiledHandle;
	FDelegateHandle LiveCodingPatchHandle;

	double LastSaveHapticTime = -1.0;
	TArray<TWeakObjectPtr<UBlueprint>> CompilingBlueprints;
	bool bLiveCodingWasCompiling = false;
	bool bLiveCodingPatched = false;
};

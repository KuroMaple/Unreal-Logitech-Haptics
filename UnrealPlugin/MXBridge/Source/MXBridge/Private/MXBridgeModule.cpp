#include "MXBridgeModule.h"
#include "MXBridgeServer.h"
#include "MXBridgeViewportScroll.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMXBridge);

void FMXBridgeModule::StartupModule()
{
	// Commandlets (cooking, automation) also load editor modules; they have no UI to drive.
	if (IsRunningCommandlet() || !GIsEditor)
	{
		return;
	}

	// Stop (and delete the token file) as soon as exit begins rather than at module unload,
	// which comes much later and never runs if the process is killed mid-shutdown.
	FCoreDelegates::OnPreExit.AddRaw(this, &FMXBridgeModule::ShutdownModule);

	// Horizontal-wheel panning works on its own, even if the HTTP bridge can't start.
	ViewportScroll = MakeUnique<FMXBridgeViewportScroll>();
	ViewportScroll->Start();

	Server = MakeUnique<FMXBridgeServer>();
	if (!Server->Start())
	{
		UE_LOG(LogMXBridge, Error, TEXT("MXBridge failed to start; MX Master actions will be unavailable."));
		Server.Reset();
	}
}

void FMXBridgeModule::ShutdownModule()
{
	FCoreDelegates::OnPreExit.RemoveAll(this);
	if (ViewportScroll)
	{
		ViewportScroll->Stop();
		ViewportScroll.Reset();
	}
	if (Server)
	{
		Server->Stop();
		Server.Reset();
	}
}

IMPLEMENT_MODULE(FMXBridgeModule, MXBridge)

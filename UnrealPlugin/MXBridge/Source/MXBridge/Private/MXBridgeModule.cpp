#include "MXBridgeModule.h"
#include "MXBridgeServer.h"
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

	Server = MakeUnique<FMXBridgeServer>();
	if (!Server->Start())
	{
		UE_LOG(LogMXBridge, Error, TEXT("MXBridge failed to start; MX Master actions will be unavailable."));
		Server.Reset();
		return;
	}

	// Stop (and delete the token file) as soon as exit begins rather than at module unload,
	// which comes much later and never runs if the process is killed mid-shutdown.
	FCoreDelegates::OnPreExit.AddRaw(this, &FMXBridgeModule::ShutdownModule);
}

void FMXBridgeModule::ShutdownModule()
{
	FCoreDelegates::OnPreExit.RemoveAll(this);
	if (Server)
	{
		Server->Stop();
		Server.Reset();
	}
}

IMPLEMENT_MODULE(FMXBridgeModule, MXBridge)

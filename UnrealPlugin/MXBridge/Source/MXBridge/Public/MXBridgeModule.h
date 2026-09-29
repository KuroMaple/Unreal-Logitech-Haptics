#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FMXBridgeServer;

DECLARE_LOG_CATEGORY_EXTERN(LogMXBridge, Log, All);

class FMXBridgeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TUniquePtr<FMXBridgeServer> Server;
};

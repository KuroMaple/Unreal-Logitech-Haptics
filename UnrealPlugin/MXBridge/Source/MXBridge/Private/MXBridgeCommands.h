#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Registry of POST /events command ids -> editor actions.
 * Execute() must be called on the game thread.
 */
class FMXBridgeCommands
{
public:
	static bool IsKnown(const FString& Id);
	static void Execute(const TSharedRef<FJsonObject>& Event);
};

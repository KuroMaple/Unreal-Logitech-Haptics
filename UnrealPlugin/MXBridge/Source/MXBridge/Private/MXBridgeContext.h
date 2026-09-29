#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Builds the GET /context snapshot. Game thread only. */
class FMXBridgeContext
{
public:
	static TSharedRef<FJsonObject> Build();
};

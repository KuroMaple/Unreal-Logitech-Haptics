#pragma once

#include "CoreMinimal.h"
#include "HttpRouteHandle.h"
#include "HttpResultCallback.h"
#include "Dom/JsonObject.h"

class FMXBridgeHaptics;
class IHttpRouter;
struct FHttpServerRequest;

/**
 * Localhost REST server the Logi Actions plugin talks to.
 *
 *   GET  /context  -> editor state snapshot
 *   POST /events   -> ring commands, executed on the game thread
 *   GET  /haptics  -> long-poll for haptic events (?since=<seq>&wait=<ms>)
 *
 * Every request must carry the per-session token in the X-MXBridge-Token header.
 * Port + token are published in <Project>/Saved/MXBridge/bridge.token.
 */
class FMXBridgeServer
{
public:
	FMXBridgeServer();
	~FMXBridgeServer();

	bool Start();
	void Stop();

private:
	bool BindListener();
	void WriteTokenFile() const;
	void DeleteTokenFile() const;

	bool IsAuthorized(const FHttpServerRequest& Request) const;

	bool HandleContext(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleEvents(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleHaptics(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	static void RespondJson(const FHttpResultCallback& OnComplete, const TSharedRef<FJsonObject>& Json, int32 Code = 200);
	static void RespondError(const FHttpResultCallback& OnComplete, int32 Code, const FString& Message);

	FString TokenFilePath() const;

	TSharedPtr<IHttpRouter> Router;
	TArray<FHttpRouteHandle> Routes;
	uint32 Port = 0;
	FString AuthToken;
	TUniquePtr<FMXBridgeHaptics> Haptics;

	/** Queued game-thread tasks check this so they no-op if the server was torn down first. */
	TSharedPtr<bool, ESPMode::ThreadSafe> AliveFlag;
};

#include "MXBridgeServer.h"
#include "MXBridgeModule.h"
#include "MXBridgeCommands.h"
#include "MXBridgeContext.h"
#include "MXBridgeHaptics.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace MXBridge
{
	static constexpr int32 SchemaVersion = 1;
	static constexpr uint32 PortRangeStart = 49500;
	static constexpr uint32 PortRangeSize = 100;
	static const TCHAR* TokenHeader = TEXT("X-MXBridge-Token");
}

FMXBridgeServer::FMXBridgeServer() = default;

FMXBridgeServer::~FMXBridgeServer()
{
	Stop();
}

bool FMXBridgeServer::Start()
{
	// Two random GUIDs = 64 hex chars. Only needs to stop other local processes/browsers from
	// poking the editor, not to survive offline attack.
	AuthToken = FGuid::NewGuid().ToString(EGuidFormats::Digits) + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	AliveFlag = MakeShared<bool, ESPMode::ThreadSafe>(true);

	if (!BindListener())
	{
		return false;
	}

	Routes.Add(Router->BindRoute(FHttpPath(TEXT("/context")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FMXBridgeServer::HandleContext)));
	Routes.Add(Router->BindRoute(FHttpPath(TEXT("/events")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FMXBridgeServer::HandleEvents)));
	Routes.Add(Router->BindRoute(FHttpPath(TEXT("/haptics")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FMXBridgeServer::HandleHaptics)));

	Haptics = MakeUnique<FMXBridgeHaptics>();
	Haptics->Start();

	WriteTokenFile();
	UE_LOG(LogMXBridge, Log, TEXT("MXBridge listening on 127.0.0.1:%u (token file: %s)"), Port, *TokenFilePath());
	return true;
}

void FMXBridgeServer::Stop()
{
	if (AliveFlag.IsValid())
	{
		*AliveFlag = false;
		AliveFlag.Reset();
	}

	if (Haptics)
	{
		Haptics->Stop();
		Haptics.Reset();
	}

	if (Router.IsValid())
	{
		for (const FHttpRouteHandle& Handle : Routes)
		{
			Router->UnbindRoute(Handle);
		}
		Routes.Reset();
		Router.Reset();
		DeleteTokenFile();
	}
}

bool FMXBridgeServer::BindListener()
{
	FHttpServerModule& HttpServer = FHttpServerModule::Get();

	// GetHttpRouter only attempts to bind while listeners are enabled; enabling is idempotent
	// and is what other editor plugins (Remote Control, etc.) do as well.
	HttpServer.StartAllListeners();

	// FHttpServerModule has no "bind to port 0" option, so probe a range starting at a random
	// offset (keeps two open editors from racing for the same port).
	const uint32 Offset = FMath::RandRange(0, MXBridge::PortRangeSize - 1);
	for (uint32 i = 0; i < MXBridge::PortRangeSize; ++i)
	{
		const uint32 Candidate = MXBridge::PortRangeStart + (Offset + i) % MXBridge::PortRangeSize;

		// Force loopback-only and exclusive binding for this port, regardless of the project's
		// DefaultBindAddress. This is an in-memory config override; nothing is written to disk.
		TArray<FString> Overrides;
		GConfig->GetArray(TEXT("HTTPServer.Listeners"), TEXT("ListenerOverrides"), Overrides, GEngineIni);
		Overrides.AddUnique(FString::Printf(TEXT("(Port=%u,BindAddress=127.0.0.1,ReuseAddressAndPort=false)"), Candidate));
		GConfig->SetArray(TEXT("HTTPServer.Listeners"), TEXT("ListenerOverrides"), Overrides, GEngineIni);

		Router = HttpServer.GetHttpRouter(Candidate, /*bFailOnBindFailure=*/ true);
		if (Router.IsValid())
		{
			Port = Candidate;
			return true;
		}
	}

	UE_LOG(LogMXBridge, Error, TEXT("MXBridge could not bind any port in %u-%u"),
		MXBridge::PortRangeStart, MXBridge::PortRangeStart + MXBridge::PortRangeSize - 1);
	return false;
}

FString FMXBridgeServer::TokenFilePath() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("MXBridge") / TEXT("bridge.token"));
}

void FMXBridgeServer::WriteTokenFile() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("schema"), MXBridge::SchemaVersion);
	Json->SetNumberField(TEXT("port"), Port);
	Json->SetStringField(TEXT("token"), AuthToken);
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
	Json->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
	Json->SetStringField(TEXT("started_utc"), FDateTime::UtcNow().ToIso8601());

	FString Text;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Json, Writer);

	if (!FFileHelper::SaveStringToFile(Text, *TokenFilePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogMXBridge, Error, TEXT("MXBridge could not write token file %s"), *TokenFilePath());
	}
}

void FMXBridgeServer::DeleteTokenFile() const
{
	IFileManager::Get().Delete(*TokenFilePath(), /*RequireExists=*/ false, /*EvenReadOnly=*/ true, /*Quiet=*/ true);
}

bool FMXBridgeServer::IsAuthorized(const FHttpServerRequest& Request) const
{
	// Header map keys compare case-insensitively (FString ==).
	const TArray<FString>* Values = Request.Headers.Find(MXBridge::TokenHeader);
	return Values && Values->Num() == 1 && (*Values)[0].Equals(AuthToken, ESearchCase::CaseSensitive);
}

bool FMXBridgeServer::HandleContext(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsAuthorized(Request))
	{
		RespondError(OnComplete, 401, TEXT("missing or invalid token"));
		return true;
	}

	// The HTTP server ticks on the game thread, so reading editor state here is safe.
	// Guard anyway in case a future engine moves listeners to a worker thread.
	if (IsInGameThread())
	{
		RespondJson(OnComplete, FMXBridgeContext::Build());
	}
	else
	{
		TWeakPtr<bool, ESPMode::ThreadSafe> Alive = AliveFlag;
		AsyncTask(ENamedThreads::GameThread, [Alive, OnComplete]()
		{
			if (Alive.IsValid())
			{
				RespondJson(OnComplete, FMXBridgeContext::Build());
			}
		});
	}
	return true;
}

bool FMXBridgeServer::HandleEvents(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsAuthorized(Request))
	{
		RespondError(OnComplete, 401, TEXT("missing or invalid token"));
		return true;
	}

	// Body is raw UTF-8 bytes with no null terminator.
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
	const FString BodyText(Converted.Length(), Converted.Get());

	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BodyText), Root) || !Root.IsValid())
	{
		RespondError(OnComplete, 400, TEXT("body is not a JSON object"));
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* EventValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("events"), EventValues))
	{
		RespondError(OnComplete, 400, TEXT("missing \"events\" array"));
		return true;
	}

	TArray<TSharedPtr<FJsonObject>> Events;
	TArray<FString> Rejected;
	for (const TSharedPtr<FJsonValue>& Value : *EventValues)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		FString Id;
		if (!Value.IsValid() || !Value->TryGetObject(Obj) || !(*Obj)->TryGetStringField(TEXT("id"), Id))
		{
			continue;
		}
		if (FMXBridgeCommands::IsKnown(Id))
		{
			Events.Add(*Obj);
		}
		else
		{
			Rejected.Add(Id);
			UE_LOG(LogMXBridge, Warning, TEXT("MXBridge: unknown command '%s'"), *Id);
		}
	}

	const int32 AcceptedCount = Events.Num();

	// Respond immediately; the commands run on the next game-thread task pump. PIE start, saves,
	// builds etc. can take seconds and must not hold the HTTP connection open.
	if (AcceptedCount > 0)
	{
		TWeakPtr<bool, ESPMode::ThreadSafe> Alive = AliveFlag;
		AsyncTask(ENamedThreads::GameThread, [Alive, Events = MoveTemp(Events)]()
		{
			if (!Alive.IsValid())
			{
				return;
			}
			for (const TSharedPtr<FJsonObject>& Event : Events)
			{
				FMXBridgeCommands::Execute(Event.ToSharedRef());
			}
		});
	}

	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetNumberField(TEXT("accepted"), AcceptedCount);
	TArray<TSharedPtr<FJsonValue>> RejectedJson;
	for (const FString& Id : Rejected)
	{
		RejectedJson.Add(MakeShared<FJsonValueString>(Id));
	}
	Response->SetArrayField(TEXT("rejected"), RejectedJson);
	RespondJson(OnComplete, Response, 202);
	return true;
}

bool FMXBridgeServer::HandleHaptics(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsAuthorized(Request))
	{
		RespondError(OnComplete, 401, TEXT("missing or invalid token"));
		return true;
	}

	// Long-poll: the response may be sent later, when an event arrives or the wait expires.
	// FHttpServerModule allows completing a request after the handler returns.
	const FString* Since = Request.QueryParams.Find(TEXT("since"));
	const FString* Wait = Request.QueryParams.Find(TEXT("wait"));
	const int64 SinceSeq = Since ? FCString::Atoi64(**Since) : -1;
	const int32 WaitMs = Wait ? FCString::Atoi(**Wait) : 0;
	Haptics->HandlePoll(SinceSeq, WaitMs, OnComplete);
	return true;
}

void FMXBridgeServer::RespondJson(const FHttpResultCallback& OnComplete, const TSharedRef<FJsonObject>& Json, int32 Code)
{
	FString Text;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
	FJsonSerializer::Serialize(Json, Writer);

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Text, TEXT("application/json"));
	Response->Code = static_cast<EHttpServerResponseCodes>(Code);
	OnComplete(MoveTemp(Response));
}

void FMXBridgeServer::RespondError(const FHttpResultCallback& OnComplete, int32 Code, const FString& Message)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("error"), Message);
	RespondJson(OnComplete, Json, Code);
}

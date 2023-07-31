// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_WINHTTP

#include "WinHttp/WinHttpHttpManager.h"
#include "WinHttp/Support/WinHttpSession.h"
#include "WinHttp/Support/WinHttpTypes.h"
#include "Http.h"
#include "Misc/CoreDelegates.h"
#include "Stats/Stats.h"

namespace
{
	FWinHttpHttpManager* GWinHttpManager = nullptr;

	DWORD GetPlatformProtocolFlags()
	{
		// Enable "all" protocols (but not SSL2, it is insecure).
		// For legacy reasons, "all" isn't actually all protocols, so we explicitly enable some below based on windows versions
		DWORD ProtocolFlags = WINHTTP_FLAG_SECURE_PROTOCOL_ALL & ~WINHTTP_FLAG_SECURE_PROTOCOL_SSL2;
#if PLATFORM_WINDOWS
		const bool bIsWindows7OrGreater = FPlatformMisc::VerifyWindowsVersion(6, 1);
		if (bIsWindows7OrGreater)
		{
			ProtocolFlags |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1;
		}
		const bool bIsWindows8Point1OrGreater = FPlatformMisc::VerifyWindowsVersion(6, 3);
		if (bIsWindows8Point1OrGreater)
		{
			ProtocolFlags |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
		}
#endif // PLATFORM_WINDOWS
		return ProtocolFlags;
	}
}

FWinHttpHttpManager* FWinHttpHttpManager::GetManager()
{
	return GWinHttpManager;
}

FWinHttpHttpManager::FWinHttpHttpManager()
{
	if (GWinHttpManager == nullptr)
	{
		GWinHttpManager = this;
	}

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddLambda([]()
	{
		if (FWinHttpHttpManager* const Manager = FWinHttpHttpManager::GetManager())
		{
			Manager->HandleApplicationSuspending();
		}
	});
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddLambda([]()
	{
		if (FWinHttpHttpManager* const Manager = FWinHttpHttpManager::GetManager())
		{
			Manager->HandleApplicationResuming();
		}
	});
}

FWinHttpHttpManager::~FWinHttpHttpManager()
{
	if (GWinHttpManager == this)
	{
		GWinHttpManager = nullptr;
	}
}

void FWinHttpHttpManager::OnBeforeFork()
{
	// FHttpManager's OnBeforeFork will flush all active requests, so it will be safe to reset our active sessions
	FHttpManager::OnBeforeFork();
	ActiveSessions.Reset();
}

void FWinHttpHttpManager::HandleApplicationSuspending()
{
	SCOPED_ENTER_BACKGROUND_EVENT(FWinHttpHttpManager_HandleApplicationSuspending);

	Flush(EHttpFlushReason::Background);
	ActiveSessions.Reset();
}

void FWinHttpHttpManager::HandleApplicationResuming()
{
	// No-op
}

void FWinHttpHttpManager::QuerySessionForUrl(const FString& /*UnusedUrl*/, FWinHttpQuerySessionComplete&& Delegate)
{
	// Pretend to be async here so applications properly react on platforms where this is actually async
	AddGameThreadTask([LambdaDelegate = MoveTemp(Delegate)]()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FWinHttpHttpManager_QuerySessionForUrlLambda);

		FWinHttpSession* SessionPtr = nullptr;
		if (FWinHttpHttpManager* HttpManager = GetManager())
		{
			const uint32 DefaultProtocolFlags = GetPlatformProtocolFlags();
			SessionPtr = HttpManager->FindOrCreateSession(DefaultProtocolFlags);
		}

		LambdaDelegate.ExecuteIfBound(SessionPtr);
	});
}

bool FWinHttpHttpManager::ValidateRequestCertificates(IWinHttpConnection& Connection)
{
	// WinHttp already does regular validation of certificates, this is for additional validation
	// NOTE: this is usually not called on the game thread! Everything that happens here must be thread safe!

	// TODO: Add support for client cert pinning here when we want to support that on Windows

	// True means this connection did not fail validation
	return true;
}

void FWinHttpHttpManager::ReleaseRequestResources(IWinHttpConnection& Connection)
{
	// No-op
}

FWinHttpSession* FWinHttpHttpManager::FindOrCreateSession(const uint32 SecurityProtocols)
{
	check(IsInGameThread());

	TUniquePtr<FWinHttpSession>* SessionPtrPtr = ActiveSessions.Find(SecurityProtocols);

	FWinHttpSession* SessionPtr = SessionPtrPtr ? SessionPtrPtr->Get() : nullptr;
	if (!SessionPtr)
	{
		SessionPtr = ActiveSessions.Emplace(SecurityProtocols, MakeUnique<FWinHttpSession>(SecurityProtocols, bPlatformForcesSecureConnections)).Get();
	}

	return SessionPtr;
}

#endif //WITH_WINHTTP

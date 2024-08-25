// Copyright Epic Games, Inc. All Rights Reserved.

#include "Curl/CurlHttpManager.h"

#if WITH_CURL

#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Misc/Paths.h"
#include "Misc/Fork.h"

#include "Curl/CurlHttpThread.h"
#include "Curl/CurlMultiPollEventLoopHttpThread.h"
#include "Curl/CurlMultiWaitEventLoopHttpThread.h"
#include "Curl/CurlSocketEventLoopHttpThread.h"
#include "Curl/CurlHttp.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/IConsoleManager.h"
#include "HttpModule.h"

#if WITH_SSL
#include "Modules/ModuleManager.h"
#include "Ssl.h"
#include <openssl/crypto.h>
#endif

#include "SocketSubsystem.h"
#include "IPAddress.h"

#include "Http.h"

#ifndef DISABLE_UNVERIFIED_CERTIFICATE_LOADING
#define DISABLE_UNVERIFIED_CERTIFICATE_LOADING 0
#endif

extern TAutoConsoleVariable<int32> CVarHttpEventLoopEnableChance;

CURLM* FCurlHttpManager::GMultiHandle = nullptr;
#if !WITH_CURL_XCURL
CURLSH* FCurlHttpManager::GShareHandle = nullptr;
#endif

FCurlHttpManager::FCurlRequestOptions FCurlHttpManager::CurlRequestOptions;

// set functions that will init the memory
namespace LibCryptoMemHooks
{
	void* (*ChainedMalloc)(size_t Size, const char* File, int Line) = nullptr;
	void* (*ChainedRealloc)(void* Ptr, const size_t Size, const char* File, int Line) = nullptr;
	void (*ChainedFree)(void* Ptr, const char* File, int Line) = nullptr;
	bool bMemoryHooksSet = false;

	/** This malloc will init the memory, keeping valgrind happy */
	void* MallocWithInit(size_t Size, const char* File, int Line)
	{
		void* Result = FMemory::Malloc(Size);
		if (LIKELY(Result))
		{
			FMemory::Memzero(Result, Size);
		}

		return Result;
	}

	/** This realloc will init the memory, keeping valgrind happy */
	void* ReallocWithInit(void* Ptr, const size_t Size, const char* File, int Line)
	{
		size_t CurrentUsableSize = FMemory::GetAllocSize(Ptr);
		void* Result = FMemory::Realloc(Ptr, Size);
		if (LIKELY(Result) && CurrentUsableSize < Size)
		{
			FMemory::Memzero(reinterpret_cast<uint8 *>(Result) + CurrentUsableSize, Size - CurrentUsableSize);
		}

		return Result;
	}

	/** This realloc will init the memory, keeping valgrind happy */
	void Free(void* Ptr, const char* File, int Line)
	{
		return FMemory::Free(Ptr);
	}

	void SetMemoryHooks()
	{
		// do not set this in Shipping until we prove that the change in OpenSSL behavior is safe
#if PLATFORM_UNIX && !UE_BUILD_SHIPPING && WITH_SSL
		CRYPTO_get_mem_functions(&ChainedMalloc, &ChainedRealloc, &ChainedFree);
		CRYPTO_set_mem_functions(MallocWithInit, ReallocWithInit, Free);
#endif // PLATFORM_UNIX && !UE_BUILD_SHIPPING && WITH_SSL

		bMemoryHooksSet = true;
	}

	void UnsetMemoryHooks()
	{
		// remove our overrides
		if (LibCryptoMemHooks::bMemoryHooksSet)
		{
			// do not set this in Shipping until we prove that the change in OpenSSL behavior is safe
#if PLATFORM_UNIX && !UE_BUILD_SHIPPING && WITH_SSL
			CRYPTO_set_mem_functions(LibCryptoMemHooks::ChainedMalloc, LibCryptoMemHooks::ChainedRealloc, LibCryptoMemHooks::ChainedFree);
#endif // PLATFORM_UNIX && !UE_BUILD_SHIPPING && WITH_SSL

			bMemoryHooksSet = false;
			ChainedMalloc = nullptr;
			ChainedRealloc = nullptr;
			ChainedFree = nullptr;
		}
	}
}

bool FCurlHttpManager::IsInit()
{
	return GMultiHandle != nullptr;
}

void FCurlHttpManager::InitCurl()
{
	if (IsInit())
	{
		UE_LOG(LogInit, Warning, TEXT("Already initialized multi handle"));
		return;
	}

	int32 CurlInitFlags = CURL_GLOBAL_ALL;
#if WITH_SSL
	// Make sure SSL is loaded so that we can use the shared cert pool, and to globally initialize OpenSSL if possible
	FSslModule& SslModule = FModuleManager::LoadModuleChecked<FSslModule>("SSL");
	if (SslModule.GetSslManager().InitializeSsl())
	{
		// Do not need Curl to initialize its own SSL
		CurlInitFlags = CurlInitFlags & ~(CURL_GLOBAL_SSL);
	}
#endif // #if WITH_SSL

	// Override libcrypt functions to initialize memory since OpenSSL triggers multiple valgrind warnings due to this.
	// Do this before libcurl/libopenssl/libcrypto has been inited.
	LibCryptoMemHooks::SetMemoryHooks();

	CURLcode InitResult = curl_global_init_mem(CurlInitFlags, CurlMalloc, CurlFree, CurlRealloc, CurlStrdup, CurlCalloc);
	if (InitResult == 0)
	{
		curl_version_info_data * VersionInfo = curl_version_info(CURLVERSION_NOW);
		if (VersionInfo)
		{
			UE_LOG(LogInit, Log, TEXT("Using libcurl %s"), ANSI_TO_TCHAR(VersionInfo->version));
			UE_LOG(LogInit, Log, TEXT(" - built for %s"), ANSI_TO_TCHAR(VersionInfo->host));

			if (VersionInfo->features & CURL_VERSION_SSL)
			{
				UE_LOG(LogInit, Log, TEXT(" - supports SSL with %s"), ANSI_TO_TCHAR(VersionInfo->ssl_version));
			}
			else
			{
				// No SSL
				UE_LOG(LogInit, Log, TEXT(" - NO SSL SUPPORT!"));
			}

			if (VersionInfo->features & CURL_VERSION_LIBZ)
			{
				UE_LOG(LogInit, Log, TEXT(" - supports HTTP deflate (compression) using libz %s"), ANSI_TO_TCHAR(VersionInfo->libz_version));
			}

			UE_LOG(LogInit, Log, TEXT(" - other features:"));

#define PrintCurlFeature(Feature)	\
			if (VersionInfo->features & Feature) \
			{ \
			UE_LOG(LogInit, Log, TEXT("     %s"), TEXT(#Feature));	\
			}

			PrintCurlFeature(CURL_VERSION_SSL);
			PrintCurlFeature(CURL_VERSION_LIBZ);

			PrintCurlFeature(CURL_VERSION_DEBUG);
			PrintCurlFeature(CURL_VERSION_IPV6);
			PrintCurlFeature(CURL_VERSION_ASYNCHDNS);
			PrintCurlFeature(CURL_VERSION_LARGEFILE);
			PrintCurlFeature(CURL_VERSION_IDN);
			PrintCurlFeature(CURL_VERSION_CONV);
			PrintCurlFeature(CURL_VERSION_TLSAUTH_SRP);
#undef PrintCurlFeature
		}

		GMultiHandle = curl_multi_init();
		if (NULL == GMultiHandle)
		{
			UE_LOG(LogInit, Fatal, TEXT("Could not initialize create libcurl multi handle! HTTP transfers will not function properly."));
		}

		int32 MaxTotalConnections = 0;
		if (GConfig->GetInt(TEXT("HTTP.Curl"), TEXT("MaxTotalConnections"), MaxTotalConnections, GEngineIni) && MaxTotalConnections > 0)
		{
			const CURLMcode SetOptResult = curl_multi_setopt(GMultiHandle, CURLMOPT_MAX_TOTAL_CONNECTIONS, static_cast<long>(MaxTotalConnections));
			if (SetOptResult != CURLM_OK)
			{
				UE_LOG(LogInit, Warning, TEXT("Failed to set libcurl max total connections options (%d), error %d ('%s')"),
					MaxTotalConnections, static_cast<int32>(SetOptResult), StringCast<TCHAR>(curl_multi_strerror(SetOptResult)).Get());
			}
		}

		int32 MaxConnects = 0;
		if (GConfig->GetInt(TEXT("HTTP.Curl"), TEXT("MaxConnects"), MaxConnects, GEngineIni) && MaxConnects >= 0)
		{
			const CURLMcode SetOptResult = curl_multi_setopt(GMultiHandle, CURLMOPT_MAXCONNECTS, static_cast<long>(MaxConnects));
			if (SetOptResult != CURLM_OK)
			{
				UE_LOG(LogInit, Warning, TEXT("Failed to set libcurl max connects options (%d), error %d ('%s')"),
					MaxConnects, static_cast<int32>(SetOptResult), StringCast<TCHAR>(curl_multi_strerror(SetOptResult)).Get());
			}
		}

#if !WITH_CURL_XCURL
		GShareHandle = curl_share_init();
		if (NULL != GShareHandle)
		{
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
		}
		else
		{
			UE_LOG(LogInit, Fatal, TEXT("Could not initialize libcurl share handle!"));
		}
#endif
	}
	else
	{
		UE_LOG(LogInit, Fatal, TEXT("Could not initialize libcurl (result=%d), HTTP transfers will not function properly."), (int32)InitResult);
	}

	// Init curl request options
	bool bForbidReuse = false;
	if (GConfig->GetBool(TEXT("HTTP.Curl"), TEXT("bForbidReuse"), bForbidReuse, GEngineIni))
	{
		CurlRequestOptions.bDontReuseConnections = bForbidReuse;
	}
	// If set on the command line for debugging this overrides the setting from the .ini file.
	if (FParse::Param(FCommandLine::Get(), TEXT("noreuseconn")))
	{
		CurlRequestOptions.bDontReuseConnections = true;
	}

#if WITH_SSL
	// Set default verify peer value based on availability of certificates
	CurlRequestOptions.bVerifyPeer = SslModule.GetCertificateManager().HasCertificatesAvailable();
#endif

	bool bVerifyPeer = true;
#if DISABLE_UNVERIFIED_CERTIFICATE_LOADING
	CurlRequestOptions.bVerifyPeer = bVerifyPeer;
#else
	if (GConfig->GetBool(TEXT("/Script/Engine.NetworkSettings"), TEXT("n.VerifyPeer"), bVerifyPeer, GEngineIni))
	{
		CurlRequestOptions.bVerifyPeer = bVerifyPeer;
	}
#endif

	bool bAcceptCompressedContent = true;
	if (GConfig->GetBool(TEXT("HTTP"), TEXT("AcceptCompressedContent"), bAcceptCompressedContent, GEngineIni))
	{
		CurlRequestOptions.bAcceptCompressedContent = bAcceptCompressedContent;
	}

	int32 ConfigBufferSize = 0;
	if (GConfig->GetInt(TEXT("HTTP.Curl"), TEXT("BufferSize"), ConfigBufferSize, GEngineIni) && ConfigBufferSize > 0)
	{
		CurlRequestOptions.BufferSize = ConfigBufferSize;
	}

	GConfig->GetBool(TEXT("HTTP.Curl"), TEXT("bAllowSeekFunction"), CurlRequestOptions.bAllowSeekFunction, GEngineIni);

	CurlRequestOptions.MaxHostConnections = FHttpModule::Get().GetHttpMaxConnectionsPerServer();
	if (CurlRequestOptions.MaxHostConnections > 0)
	{
		const CURLMcode SetOptResult = curl_multi_setopt(GMultiHandle, CURLMOPT_MAX_HOST_CONNECTIONS, static_cast<long>(CurlRequestOptions.MaxHostConnections));
		if (SetOptResult != CURLM_OK)
		{
			FUTF8ToTCHAR Converter(curl_multi_strerror(SetOptResult));
			UE_LOG(LogInit, Warning, TEXT("Failed to set max host connections options (%d), error %d ('%s')"),
				CurlRequestOptions.MaxHostConnections, (int32)SetOptResult, Converter.Get());
			CurlRequestOptions.MaxHostConnections = 0;
		}
	}
	else
	{
		CurlRequestOptions.MaxHostConnections = 0;
	}

	TCHAR Home[256] = TEXT("");
	if (FParse::Value(FCommandLine::Get(), TEXT("MULTIHOMEHTTP="), Home, UE_ARRAY_COUNT(Home)))
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSubsystem && SocketSubsystem->GetAddressFromString(Home).IsValid())
		{
			CurlRequestOptions.LocalHostAddr = FString(Home);
		}
	}

	// print for visibility
	CurlRequestOptions.Log();
}

void FCurlHttpManager::FCurlRequestOptions::Log()
{
	UE_LOG(LogInit, Log, TEXT(" CurlRequestOptions (configurable via config and command line):"));
		UE_LOG(LogInit, Log, TEXT(" - bVerifyPeer = %s  - Libcurl will %sverify peer certificate"),
		bVerifyPeer ? TEXT("true") : TEXT("false"),
		bVerifyPeer ? TEXT("") : TEXT("NOT ")
		);

	const FString& ProxyAddress = FHttpModule::Get().GetProxyAddress();
	const bool bUseHttpProxy = !ProxyAddress.IsEmpty();
	UE_LOG(LogInit, Log, TEXT(" - bUseHttpProxy = %s  - Libcurl will %suse HTTP proxy"),
		bUseHttpProxy ? TEXT("true") : TEXT("false"),
		bUseHttpProxy ? TEXT("") : TEXT("NOT ")
		);	
	if (bUseHttpProxy)
	{
		UE_LOG(LogInit, Log, TEXT(" - HttpProxyAddress = '%s'"), *ProxyAddress);
	}

	UE_LOG(LogInit, Log, TEXT(" - bDontReuseConnections = %s  - Libcurl will %sreuse connections"),
		bDontReuseConnections ? TEXT("true") : TEXT("false"),
		bDontReuseConnections ? TEXT("NOT ") : TEXT("")
		);

	UE_LOG(LogInit, Log, TEXT(" - MaxHostConnections = %d  - Libcurl will %slimit the number of connections to a host"),
		MaxHostConnections,
		(MaxHostConnections == 0) ? TEXT("NOT ") : TEXT("")
		);

	UE_LOG(LogInit, Log, TEXT(" - LocalHostAddr = %s"), LocalHostAddr.IsEmpty() ? TEXT("Default") : *LocalHostAddr);

	UE_LOG(LogInit, Log, TEXT(" - BufferSize = %d"), CurlRequestOptions.BufferSize);
}


void FCurlHttpManager::ShutdownCurl()
{
#if !WITH_CURL_XCURL
	if (GShareHandle != nullptr)
	{
		CURLSHcode ShareCleanupCode = curl_share_cleanup(GShareHandle);
		UE_CLOG(ShareCleanupCode != CURLSHE_OK, LogHttp, Warning, TEXT("curl_share_cleanup failed. ReturnValue=[%d]"), static_cast<int32>(ShareCleanupCode));
		GShareHandle = nullptr;
	}
#endif

	if (GMultiHandle != nullptr)
	{
		CURLMcode MutliCleanupCode = curl_multi_cleanup(GMultiHandle);
		UE_CLOG(MutliCleanupCode != CURLM_OK, LogHttp, Warning, TEXT("curl_multi_cleanup failed. ReturnValue=[%d]"), static_cast<int32>(MutliCleanupCode));
		GMultiHandle = nullptr;
	}

	curl_global_cleanup();

	LibCryptoMemHooks::UnsetMemoryHooks();

#if WITH_SSL
	// Shutdown OpenSSL
	FSslModule& SslModule = FModuleManager::LoadModuleChecked<FSslModule>("SSL");
	SslModule.GetSslManager().ShutdownSsl();
#endif // #if WITH_SSL
}

void FCurlHttpManager::OnBeforeFork()
{
	FHttpManager::OnBeforeFork();

	Thread->StopThread();
	ShutdownCurl();
}

void FCurlHttpManager::OnAfterFork()
{
	InitCurl();

	if (FForkProcessHelper::IsForkedChildProcess() == false || FForkProcessHelper::SupportsMultithreadingPostFork() == false)
	{
		// Since this will create a fake thread its safe to create it immediately here
		Thread->StartThread();
	}

	FHttpManager::OnAfterFork();
}

void FCurlHttpManager::OnEndFramePostFork()
{
	if (FForkProcessHelper::SupportsMultithreadingPostFork())
	{
		// We forked and the frame is done, time to start the autonomous thread
		check(FForkProcessHelper::IsForkedMultithreadInstance());
		Thread->StartThread();
	}

	FHttpManager::OnEndFramePostFork();
}

void FCurlHttpManager::UpdateConfigs()
{
	// Update configs - update settings that are safe to update after initialize 
	FHttpManager::UpdateConfigs();

	{
		bool bAcceptCompressedContent = true;
		if (GConfig->GetBool(TEXT("HTTP"), TEXT("AcceptCompressedContent"), bAcceptCompressedContent, GEngineIni))
		{
			if (CurlRequestOptions.bAcceptCompressedContent != bAcceptCompressedContent)
			{
				UE_LOG(LogHttp, Log, TEXT("AcceptCompressedContent changed from %s to %s"), *LexToString(CurlRequestOptions.bAcceptCompressedContent), *LexToString(bAcceptCompressedContent));
				CurlRequestOptions.bAcceptCompressedContent = bAcceptCompressedContent;
			}
		}
	}

	{
		int32 ConfigBufferSize = 0;
		if (GConfig->GetInt(TEXT("HTTP.Curl"), TEXT("BufferSize"), ConfigBufferSize, GEngineIni) && ConfigBufferSize > 0)
		{
			if (CurlRequestOptions.BufferSize != ConfigBufferSize)
			{
				UE_LOG(LogHttp, Log, TEXT("BufferSize changed from %d to %d"), CurlRequestOptions.BufferSize, ConfigBufferSize);
				CurlRequestOptions.BufferSize = ConfigBufferSize;
			}
		}
	}

	{
		bool bConfigAllowSeekFunction = false;
		if (GConfig->GetBool(TEXT("HTTP.Curl"), TEXT("bAllowSeekFunction"), bConfigAllowSeekFunction, GEngineIni))
		{
			if (CurlRequestOptions.bAllowSeekFunction != bConfigAllowSeekFunction)
			{
				UE_LOG(LogHttp, Log, TEXT("bAllowSeekFunction changed from %s to %s"), *LexToString(CurlRequestOptions.bAllowSeekFunction), *LexToString(bConfigAllowSeekFunction));
				CurlRequestOptions.bAllowSeekFunction = bConfigAllowSeekFunction;
			}
		}
	}
}

FHttpThreadBase* FCurlHttpManager::CreateHttpThread()
{
	bool bUseEventLoop = (FMath::RandRange(0, 99) < CVarHttpEventLoopEnableChance.GetValueOnGameThread());

	// Also support to change it through runtime args.
	// Can't set cvar CVarHttpEventLoopEnableChance through runtime args or .ini files because http module initialized too early
	FParse::Bool(FCommandLine::Get(), TEXT("useeventloop="), bUseEventLoop);

	if (bUseEventLoop)
	{
#if WITH_CURL_MULTIPOLL
		UE_LOG(LogInit, Log, TEXT("CreateHttpThread using FCurlMultiPollEventLoopHttpThread"));
		return new FCurlMultiPollEventLoopHttpThread();

#elif WITH_CURL_MULTISOCKET
		UE_LOG(LogInit, Log, TEXT("CreateHttpThread using FCurlSocketEventLoopHttpThread"));
		return new FCurlSocketEventLoopHttpThread();

#elif WITH_CURL_MULTIWAIT
		UE_LOG(LogInit, Log, TEXT("CreateHttpThread using FCurlMultiWaitEventLoopHttpThread"));
		return new FCurlMultiWaitEventLoopHttpThread();
#endif
	}

	UE_LOG(LogInit, Log, TEXT("CreateHttpThread using FCurlHttpThread"));
	return new FCurlHttpThread();
}

bool FCurlHttpManager::SupportsDynamicProxy() const
{
	return true;
}
#endif //WITH_CURL

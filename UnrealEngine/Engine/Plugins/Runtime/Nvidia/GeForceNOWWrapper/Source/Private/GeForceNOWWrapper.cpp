// Copyright Epic Games, Inc. All Rights Reserved.

#if NV_GEFORCENOW

#include "GeForceNOWWrapper.h"
#include "GeForceNOWWrapperPrivate.h"
#include "GeForceNOWActionZoneProcessor.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"

UE_PUSH_MACRO("TEXT")
#undef TEXT
THIRD_PARTY_INCLUDES_START
extern "C"
{

#include "GfnRuntimeSdk_Wrapper.c"
#include "GfnSdk_SecureLoadLibrary.c"

}
THIRD_PARTY_INCLUDES_END
UE_POP_MACRO("TEXT")

bool GeForceNOWWrapper::bIsSdkInitialized = false;

TOptional<bool> GeForceNOWWrapper::bIsRunningInCloud = TOptional<bool>();

GeForceNOWWrapper* GeForceNOWWrapper::Singleton = nullptr;

GeForceNOWWrapper& GeForceNOWWrapper::Get()
{
	check(Singleton); //Crashing here means we're fetching the wrapper too soon.
	return *(Singleton);
}

/*static*/ const FString GeForceNOWWrapper::GetGfnOsTypeString(GfnOsType OsType)
{
	switch (OsType)
	{
	case gfnWindows:
		return "gfnWindows";
	case gfnMacOs:
		return "gfnMacOs";
	case gfnShield:
		return "gfnShield";
	case gfnAndroid:
		return "gfnAndroid";
	case gfnIOs:
		return "gfnIOs";
	case gfnIPadOs:
		return "gfnIPadOs";
	case gfnChromeOs:
		return "gfnChromeOs";
	case gfnLinux:
		return "gfnLinux";
	case gfnTizen:
		return "gfnTizen";
	case gfnWebOs:
		return "gfnWebOs";
	case gfnTvOs:
		return "gfnTvOs";
	case gfnUnknownOs:
	default:
		return "gfnUnknownOs";
	}
	static_assert(GfnOsType::gfnOsTypeMax == 11, "Error: Please update GetGfnOSTypeString with new GfnOSType entries.");
}


/*static*/ bool GeForceNOWWrapper::IsRunningInGFN()
{
	return IsRunningMockGFN() || IsSdkInitialized() && IsRunningInCloud();
}

/*static*/ bool GeForceNOWWrapper::IsRunningMockGFN()
{
#if !UE_BUILD_SHIPPING
	static bool bIsMockGFN = FParse::Param(FCommandLine::Get(), TEXT("MockGFN"));
	static bool IsFileMock = IFileManager::Get().FileExists(TEXT("mockgfn.txt"));

	bIsMockGFN = bIsMockGFN || IsFileMock;
#else
	static bool bIsMockGFN = false;
#endif

	return bIsMockGFN;
}

/*static*/ GfnRuntimeError GeForceNOWWrapper::Initialize()
{
	if (!Singleton)
	{
		Singleton = new GeForceNOWWrapper();
	}

	if (bIsSdkInitialized)
	{
		return gfnSuccess;
	}

	if (IsRunningMockGFN())
	{
		bIsSdkInitialized = true;
		return gfnSuccess;
	}

	FString GFNDllPath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/NVIDIA/GeForceNOW"), FPlatformProcess::GetBinariesSubdirectory(), TEXT("GfnRuntimeSdk.dll"));
	FString GFNDllFullPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*GFNDllPath);
	GFNDllFullPath.ReplaceInline(TEXT("/"), TEXT("\\"), ESearchCase::CaseSensitive);
	const GfnRuntimeError ErrorCode = GfnInitializeSdkFromPathW(gfnDefaultLanguage, *GFNDllFullPath);
	bIsSdkInitialized = ErrorCode == gfnSuccess || ErrorCode == gfnInitSuccessClientOnly;

	return ErrorCode;
}

bool GeForceNOWWrapper::InitializeActionZoneProcessor()
{
	if (bIsSdkInitialized)
	{
		ActionZoneProcessor = MakeShared<GeForceNOWActionZoneProcessor>();
		return ActionZoneProcessor->Initialize();
	}
	return false;
}

/*static*/ GfnRuntimeError GeForceNOWWrapper::Shutdown()
{
	if (Singleton)
	{
		GeForceNOWWrapper& Wrapper = GeForceNOWWrapper::Get();
		if (Wrapper.ActionZoneProcessor.IsValid())
		{
			Wrapper.ActionZoneProcessor->Terminate();
			Wrapper.ActionZoneProcessor.Reset();
		}

		delete Singleton;
		Singleton = nullptr;
	}

	if (bIsSdkInitialized)
	{
		bIsSdkInitialized = false;
		return GfnShutdownSdk();
	}
	else
	{
		return gfnSuccess;
	}
}

/*static*/ bool GeForceNOWWrapper::IsRunningInCloud()
{
	if (IsRunningMockGFN())
	{
		return true;
	}

	if (!bIsSdkInitialized)
	{
		return false;
	}

	if (bIsRunningInCloud.IsSet())
	{
		return bIsRunningInCloud.GetValue();
	}

	bool bLocalIsRunningInCloud;
	GfnIsRunningInCloud(&bLocalIsRunningInCloud);
	bIsRunningInCloud = bLocalIsRunningInCloud;
	return bIsRunningInCloud.GetValue();
}

GfnRuntimeError GeForceNOWWrapper::IsRunningInCloudSecure(GfnIsRunningInCloudAssurance& OutAssurance) const
{
	return GfnIsRunningInCloudSecure(&OutAssurance);
}

GfnRuntimeError GeForceNOWWrapper::SetupTitle(const FString& InPlatformAppId) const
{
	return GfnSetupTitle(TCHAR_TO_ANSI(*InPlatformAppId));
}

GfnRuntimeError GeForceNOWWrapper::NotifyAppReady(bool bSuccess, const FString& InStatus) const
{
	return GfnAppReady(bSuccess, TCHAR_TO_ANSI(*InStatus));
}

GfnRuntimeError GeForceNOWWrapper::NotifyTitleExited(const FString& InPlatformId, const FString& InPlatformAppId) const
{
	return GfnTitleExited(TCHAR_TO_ANSI(*InPlatformId), TCHAR_TO_ANSI(*InPlatformAppId));
}

GfnRuntimeError GeForceNOWWrapper::StartStream(StartStreamInput& InStartStreamInput, StartStreamResponse& OutResponse) const
{
	return GfnStartStream(&InStartStreamInput, &OutResponse);
}

GfnRuntimeError GeForceNOWWrapper::StartStreamAsync(const StartStreamInput& InStartStreamInput, StartStreamCallbackSig StartStreamCallback, void* Context, uint32 TimeoutMs) const
{
	return GfnStartStreamAsync(&InStartStreamInput, StartStreamCallback, Context, TimeoutMs);
}

GfnRuntimeError GeForceNOWWrapper::StopStream() const
{
	return GfnStopStream();
}

GfnRuntimeError GeForceNOWWrapper::StopStreamAsync(StopStreamCallbackSig StopStreamCallback, void* Context, unsigned int TimeoutMs) const
{
	return GfnStopStreamAsync(StopStreamCallback, Context, TimeoutMs);
}

GfnRuntimeError GeForceNOWWrapper::SetActionZone(GfnActionType ActionType, unsigned int Id, GfnRect* Zone)
{
	return GfnSetActionZone(ActionType, Id, Zone);
}

GfnRuntimeError GeForceNOWWrapper::RegisterStreamStatusCallback(StreamStatusCallbackSig StreamStatusCallback, void* Context) const
{
	return GfnRegisterStreamStatusCallback(StreamStatusCallback, Context);
}

GfnRuntimeError GeForceNOWWrapper::RegisterExitCallback(ExitCallbackSig ExitCallback, void* Context) const
{
	return GfnRegisterExitCallback(ExitCallback, Context);
}

GfnRuntimeError GeForceNOWWrapper::RegisterPauseCallback(PauseCallbackSig PauseCallback, void* Context) const
{
	return GfnRegisterPauseCallback(PauseCallback, Context);
}

GfnRuntimeError GeForceNOWWrapper::RegisterInstallCallback(InstallCallbackSig InstallCallback, void* Context) const
{
	return GfnRegisterInstallCallback(InstallCallback, Context);
}

GfnRuntimeError GeForceNOWWrapper::RegisterSaveCallback(SaveCallbackSig SaveCallback, void* Context) const
{
	return GfnRegisterSaveCallback(SaveCallback, Context);
}

GfnRuntimeError GeForceNOWWrapper::RegisterSessionInitCallback(SessionInitCallbackSig SessionInitCallback, void* Context) const
{
	return GfnRegisterSessionInitCallback(SessionInitCallback, Context);
}

GfnRuntimeError GeForceNOWWrapper::RegisterClientInfoCallback(ClientInfoCallbackSig ClientInfoCallback, void* Context) const
{
	return GfnRegisterClientInfoCallback(ClientInfoCallback, Context);
}

GfnRuntimeError GeForceNOWWrapper::GetClientIpV4(FString& OutIpv4) const
{
	const char* Ip = nullptr;
	GfnRuntimeError ErrorCode = GfnGetClientIpV4(&Ip);
	OutIpv4 = FString(Ip);
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::GetClientLanguageCode(FString& OutLanguageCode) const
{
	const char* LanguageCode = nullptr;
	GfnRuntimeError ErrorCode = GfnGetClientLanguageCode(&LanguageCode);

	if (ErrorCode == GfnRuntimeError::gfnSuccess)
	{
		OutLanguageCode = FString(LanguageCode);
		Free(&LanguageCode);
	}
	
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::GetClientCountryCode(FString& OutCountryCode) const
{
	char CountryCode[3] = { 0 };
	GfnRuntimeError ErrorCode = GfnGetClientCountryCode(CountryCode, 3);
	OutCountryCode = FString(CountryCode);
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::GetClientInfo(GfnClientInfo& OutClientInfo) const
{
	return GfnGetClientInfo(&OutClientInfo);
}

GfnRuntimeError GeForceNOWWrapper::GetSessionInfo(GfnSessionInfo& OutSessionInfo) const
{
	return GfnGetSessionInfo(&OutSessionInfo);
}

GfnRuntimeError GeForceNOWWrapper::GetPartnerData(FString& OutPartnerData) const
{
	const char* PartnerData = nullptr;
	GfnRuntimeError ErrorCode = GfnGetPartnerData(&PartnerData);

	if (ErrorCode == GfnRuntimeError::gfnSuccess)
	{
		OutPartnerData = FString(PartnerData);
		Free(&PartnerData);
	}
	
	return ErrorCode;
}	

GfnRuntimeError GeForceNOWWrapper::GetPartnerSecureData(FString& OutPartnerSecureData) const
{
	const char* PartnerSecureData = nullptr;
	GfnRuntimeError ErrorCode = GfnGetPartnerSecureData(&PartnerSecureData);

	if (ErrorCode == GfnRuntimeError::gfnSuccess)
	{
		OutPartnerSecureData = FString(PartnerSecureData);
		Free(&PartnerSecureData);
	}
	
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::GetTitlesAvailable(FString& OutAvailableTitles) const
{
	const char* AvailableTitles = nullptr;
	GfnRuntimeError ErrorCode = GfnGetTitlesAvailable(&AvailableTitles);

	if (ErrorCode == GfnRuntimeError::gfnSuccess)
	{
		OutAvailableTitles = FString(AvailableTitles);
		Free(&AvailableTitles);
	}
	
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::IsTitleAvailable(const FString& InTitleID, bool& OutbIsAvailable) const
{
	GfnRuntimeError ErrorCode = GfnIsTitleAvailable(TCHAR_TO_ANSI(*InTitleID), &OutbIsAvailable);
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::Free(const char** data) const
{
	return GfnFree(data);
}

#endif // NV_GEFORCENOW

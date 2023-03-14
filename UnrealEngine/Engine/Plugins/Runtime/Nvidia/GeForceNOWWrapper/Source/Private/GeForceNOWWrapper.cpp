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
#include "GfnRuntimeSdk_Wrapper.h"
THIRD_PARTY_INCLUDES_END
UE_POP_MACRO("TEXT")

GeForceNOWWrapper::GeForceNOWWrapper()
	: bIsInitialized(false)
{

}

GeForceNOWWrapper::~GeForceNOWWrapper()
{
	Shutdown();
}

GeForceNOWWrapper& GeForceNOWWrapper::Get()
{
	static GeForceNOWWrapper Singleton;
	return Singleton;
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


bool GeForceNOWWrapper::IsRunningInGFN()
{
	return IsRunningMockGFN() || IsInitialized() && IsRunningInCloud();
}

bool GeForceNOWWrapper::IsRunningMockGFN() const
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

GfnRuntimeError GeForceNOWWrapper::Initialize()
{
	if (IsRunningMockGFN())
	{
		bIsInitialized = true;
		return gfnSuccess;
	}

	if (bIsInitialized)
	{
		return gfnSuccess;
	}

	FString GFNDllPath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/NVIDIA/GeForceNOW"), FPlatformProcess::GetBinariesSubdirectory(), TEXT("GfnRuntimeSdk.dll"));
	FString GFNDllFullPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*GFNDllPath);
	GFNDllFullPath.ReplaceInline(TEXT("/"), TEXT("\\"), ESearchCase::CaseSensitive);
	const GfnRuntimeError ErrorCode = GfnInitializeSdkFromPath(gfnDefaultLanguage, *GFNDllFullPath);
	bIsInitialized = ErrorCode == gfnSuccess || ErrorCode == gfnInitSuccessClientOnly;

	if (bIsInitialized)
	{
		FGenericCrashContext::SetEngineData(TEXT("RHI.CloudInstance"), TEXT("GeForceNow"));
	}

	return ErrorCode;
}

bool GeForceNOWWrapper::InitializeActionZoneProcessor()
{
	if (bIsInitialized)
	{
		ActionZoneProcessor = MakeShared<GeForceNOWActionZoneProcessor>();
		return ActionZoneProcessor->Initialize();
	}
	return false;
}

GfnRuntimeError GeForceNOWWrapper::Shutdown()
{
	bIsInitialized = false;
	if (ActionZoneProcessor.IsValid())
	{
		ActionZoneProcessor->Terminate();
		ActionZoneProcessor.Reset();
	}
	return GfnShutdownSdk();
}

bool GeForceNOWWrapper::IsRunningInCloud()
{
	if (IsRunningMockGFN())
	{
		return true;
	}

	if (!bIsInitialized)
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
	char* PlatformAppId = TCHAR_TO_ANSI(*InPlatformAppId);
	return GfnSetupTitle(PlatformAppId);
}

GfnRuntimeError GeForceNOWWrapper::NotifyAppReady(bool bSuccess, const FString& InStatus) const
{
	return GfnAppReady(bSuccess, TCHAR_TO_ANSI(*InStatus));
}

GfnRuntimeError GeForceNOWWrapper::NotifyTitleExited(const FString& InPlatformId, const FString& InPlatformAppId) const
{
	char* PlatformId = TCHAR_TO_ANSI(*InPlatformId);
	char* PlatformAppId = TCHAR_TO_ANSI(*InPlatformAppId);
	return GfnTitleExited(PlatformId, PlatformAppId);
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
	OutLanguageCode = FString(LanguageCode);
	Free(&LanguageCode);
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::GetClientCountryCode(FString& OutCountryCode) const
{
	char CountryCode[3] = { 0 };
	GfnRuntimeError ErrorCode = GfnGetClientCountryCode(CountryCode, 3);
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::GetClientInfo(GfnClientInfo& OutClientInfo) const
{
	return GfnGetClientInfo(&OutClientInfo);
}

GfnRuntimeError GeForceNOWWrapper::GetCustomData(FString& OutCustomData) const
{
	const char* CustomData = nullptr;
	GfnRuntimeError ErrorCode = GfnGetCustomData(&CustomData);
	OutCustomData = FString(CustomData);
	Free(&CustomData);
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::GetAuthData(FString& OutAuthData) const
{
	const char* AuthData = nullptr;
	GfnRuntimeError ErrorCode = GfnGetAuthData(&AuthData);
	OutAuthData = FString(AuthData);
	Free(&AuthData);
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::GetTitlesAvailable(FString& OutAvailableTitles) const
{
	const char* AvailableTitles = nullptr;
	GfnRuntimeError ErrorCode = GfnGetTitlesAvailable(&AvailableTitles);
	OutAvailableTitles = FString(AvailableTitles);
	Free(&AvailableTitles);
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::IsTitleAvailable(const FString& InTitleID, bool& OutbIsAvailable) const
{
	char* TitleID = TCHAR_TO_ANSI(*InTitleID);
	GfnRuntimeError ErrorCode = GfnIsTitleAvailable(TitleID, &OutbIsAvailable);
	return ErrorCode;
}

GfnRuntimeError GeForceNOWWrapper::Free(const char** data) const
{
	return GfnFree(data);
}

#endif // NV_GEFORCENOW
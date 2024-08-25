// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if NV_GEFORCENOW

#include "Containers/UnrealString.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

THIRD_PARTY_INCLUDES_START
#include "GfnRuntimeSdk_CAPI.h"
THIRD_PARTY_INCLUDES_END

class GeForceNOWActionZoneProcessor;

/**
 * Singleton wrapper to manage the GeForceNow SDK
 */
class GEFORCENOWWRAPPER_API GeForceNOWWrapper
{
public:
	/** Load and Initialize the GeforceNOW SDK dll. */
	static GfnRuntimeError Initialize();
	
	/** Unload the GeforceNOW SDK dlls. */
	static GfnRuntimeError Shutdown();
	
	static GeForceNOWWrapper& Get();
	

	static const FString GetGfnOsTypeString(GfnOsType OsType);

	/** Returns true if the GeForceNow SDK is initialized and running in cloud. */
	static bool IsRunningInGFN();

	/** Returns true for mock, but this can be used to differentiate between real and mock. */
	static bool IsRunningMockGFN();

	/** Initializes the Action Zone Processor.Returns true if the initialization was a success. */
	bool InitializeActionZoneProcessor();

	/** Determines if application is running in GeforceNOW environment  and without requiring process elevation. */
	static bool IsRunningInCloud();

	/** Notify GeforceNOW that an application should be readied for launch. */
	GfnRuntimeError SetupTitle(const FString& InPlatformAppId) const;

	/** Notify GeForceNOW that an application is ready to be displayed. */
	GfnRuntimeError NotifyAppReady(bool bSuccess, const FString& InStatus) const;
	/** Notify GeforceNOW that an application has exited. */
	GfnRuntimeError NotifyTitleExited(const FString& InPlatformId, const FString& InPlatformAppId) const;

	/** Request GeforceNOW client to start a streaming session of an application in a synchronous (blocking) fashion. */
	GfnRuntimeError StartStream(StartStreamInput& InStartStreamInput, StartStreamResponse& OutResponse) const;
	/** Request GeforceNOW client to start a streaming session of an application in an asynchronous fashion. */
	GfnRuntimeError StartStreamAsync(const StartStreamInput& InStartStreamInput, StartStreamCallbackSig StartStreamCallback, void* Context, uint32 TimeoutMs) const;

	/** Request GeforceNOW client to stop a streaming session of an application in a synchronous (blocking) fashion. */
	GfnRuntimeError StopStream() const;
	/** Request GeforceNOW client to stop a streaming session of an application in an asynchronous fashion. */
	GfnRuntimeError StopStreamAsync(StopStreamCallbackSig StopStreamCallback, void* Context, unsigned int TimeoutMs) const;

	/** Use to invoke special events on the client from the GFN cloud environment */
	GfnRuntimeError SetActionZone(GfnActionType ActionType, unsigned int Id, GfnRect* Zone);

	/** Registers a callback that gets called on the user's PC when the streaming session state changes. */
	GfnRuntimeError RegisterStreamStatusCallback(StreamStatusCallbackSig StreamStatusCallback, void* Context) const;
	/** Registers an application function to call when GeforceNOW needs to exit the game. */
	GfnRuntimeError RegisterExitCallback(ExitCallbackSig ExitCallback, void* Context) const;
	/** Registers an application callback with GeforceNOW to be called when GeforceNOW needs to pause the game on the user's behalf. */
	GfnRuntimeError RegisterPauseCallback(PauseCallbackSig PauseCallback, void* Context) const;
	/** Registers an application callback with GeforceNOW to be called after a successful call to SetupTitle. */
	GfnRuntimeError RegisterInstallCallback(InstallCallbackSig InstallCallback, void* Context) const;
	/** Registers an application callback with GeforceNOW to be called when GeforceNOW needs the application to save user progress. */
	GfnRuntimeError RegisterSaveCallback(SaveCallbackSig SaveCallback, void* Context) const;
	/** Registers an application callback to be called when a GeforceNOW user has connected to the game seat. */
	GfnRuntimeError RegisterSessionInitCallback(SessionInitCallbackSig SessionInitCallback, void* Context) const;
	/** Registers an application callback with GFN to be called when client info changes. */
	GfnRuntimeError RegisterClientInfoCallback(ClientInfoCallbackSig ClientInfoCallback, void* Context) const;

	/** Gets user client's IP address. */
	GfnRuntimeError GetClientIpV4(FString& OutIpv4) const;
	/** Gets user's client language code in the form "<lang>-<country>" using a standard ISO 639-1 language code and ISO 3166-1 Alpha-2 country code. */
	GfnRuntimeError GetClientLanguageCode(FString& OutLanguageCode) const;
	/** Gets user’s client country code using ISO 3166-1 Alpha-2 country code. */
	GfnRuntimeError GetClientCountryCode(FString& OutCountryCode) const;
	/** Gets user's client data. */
	GfnRuntimeError GetClientInfo(GfnClientInfo& OutClientInfo) const;
	/** Gets user's session data. */
	GfnRuntimeError GetSessionInfo(GfnSessionInfo& OutSessionInfo) const;
	/** Retrieves secure partner data that is either a) passed by the client in the gfnStartStream call or b) sent in response to Deep Link nonce validation. */
	GfnRuntimeError GetPartnerData(FString& OutPartnerData) const;
	/** Use during cloud session to retrieve secure partner data. */
	GfnRuntimeError GetPartnerSecureData(FString& OutPartnerSecureData) const;
	/** Retrieves all titles that can be launched in the current game streaming session. */
	GfnRuntimeError GetTitlesAvailable(FString& OutAvailableTitles) const;

	/** Determines if calling application is running in GeforceNOW environment, and what level of security assurance that the result is valid. */
	GfnRuntimeError IsRunningInCloudSecure(GfnIsRunningInCloudAssurance& OutAssurance) const;
	/** Determines if a specific title is available to launch in current streaming session. */
	GfnRuntimeError IsTitleAvailable(const FString& InTitleID, bool& OutbIsAvailable) const;
	
	/** Returns true is the GeforceNOW SDK dll was loaded and initialized. */
	static bool IsSdkInitialized() { return bIsSdkInitialized; }

private:

	/** Singleton access only. */
	GeForceNOWWrapper(){}
	~GeForceNOWWrapper() {};

	/** Free memory allocated by gfnGetTitlesAvailable and the likes. */
	GfnRuntimeError Free(const char** data) const;

	/** Is the DLL loaded and GfnInitializeSdk was called and succeeded. */
	static bool bIsSdkInitialized;

	/** Is the DLL running in the GeForce Now environment. */
	static TOptional<bool> bIsRunningInCloud;

	/** Keeps track of actions zones for GeForce NOW. Action Zones are used for things like keyboard invocation within the GeForce NOW app.*/
	TSharedPtr<GeForceNOWActionZoneProcessor> ActionZoneProcessor;

	static GeForceNOWWrapper* Singleton;
};

#endif // NV_GEFORCENOW

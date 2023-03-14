// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if NV_GEFORCENOW

#include "CoreMinimal.h"

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
	virtual ~GeForceNOWWrapper();
	static GeForceNOWWrapper& Get();

	static const FString GetGfnOsTypeString(GfnOsType OsType);

	/** Returns true if the GeForceNow SDK is initialized and running in cloud. */
	bool IsRunningInGFN();

	/** Returns true for mock, but this can be used to differentiate between real and mock. */
	bool IsRunningMockGFN() const;

	/** Load and Initialize the GeforceNOW SDK dll. */
	GfnRuntimeError Initialize();

	/** Initializes the Action Zone Processor.Returns true if the initialization was a success. */
	bool InitializeActionZoneProcessor();

	/** Unload the GeforceNOW SDK dlls. */
	GfnRuntimeError Shutdown();

	/** Determines if application is running in GeforceNOW environment  and without requiring process elevation. */
	bool IsRunningInCloud();

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
	/** Retrieves custom data passed in by the client in the StartStream call. */
	GfnRuntimeError GetCustomData(FString& OutCustomData) const;
	/** Retrieves custom authorization passed in by the client in the StartStream call. */
	GfnRuntimeError GetAuthData(FString& OutAuthData) const;
	/** Retrieves all titles that can be launched in the current game streaming session. */
	GfnRuntimeError GetTitlesAvailable(FString& OutAvailableTitles) const;

	/** Determines if calling application is running in GeforceNOW environment, and what level of security assurance that the result is valid. */
	GfnRuntimeError IsRunningInCloudSecure(GfnIsRunningInCloudAssurance& OutAssurance) const;
	/** Determines if a specific title is available to launch in current streaming session. */
	GfnRuntimeError IsTitleAvailable(const FString& InTitleID, bool& OutbIsAvailable) const;
	
	/** Returns true is the GeforceNOW SDK dll was loaded and initialized. */
	bool IsInitialized() const { return bIsInitialized; }

private:

	/** Singleton access only. */
	GeForceNOWWrapper();

	/** Free memory allocated by gfnGetTitlesAvailable and the likes. */
	GfnRuntimeError Free(const char** data) const;

	/** Is the DLL loaded and GfnInitializeSdk was called and succeeded. */
	bool bIsInitialized = false;

	/** Is the DLL running in the GeForce Now environment. */
	TOptional<bool> bIsRunningInCloud;

	/** Keeps track of actions zones for GeForce NOW. Action Zones are used for things like keyboard invocation within the GeForce NOW app.*/
	TSharedPtr<GeForceNOWActionZoneProcessor> ActionZoneProcessor;
};

#endif // NV_GEFORCENOW
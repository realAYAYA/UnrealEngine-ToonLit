// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2020-2021 NVIDIA Corporation. All rights reserved.

//
// ===============================================================================================
//
// C-based wrapper API declarations for GFN Runtime SDK APIs
//
// ===============================================================================================
/**
* @file GfnRuntimeSdk_Wrapper.h
*
* Wrapper API definitions
*/
///
/// @page wrapper_apis Native Runtime API Export Wrapper Reference
///
/// @section wrapper_introduction Introduction
/// The NVIDIA GFN Runtime SDK provides a set of APIs as export functions from a dynamic
/// library. To ease integration and use of these APIs, the GFN Runtime SDK provides
/// this optional set of pure-C functions that manage the lifetime of the library as well as
/// wrap the export function calls. This allows integrators to avoid having to work with
/// export functions directly if they so choose.
///
/// Most of the functions defined in this header file have the same definitions and calling
/// characteristics as the export function they wrap. As such, they will return the same
/// data types as well as error codes. As such, the documentation for each is purposefully
/// pared down to avoid duplication. There are a few exceptions, and those functions will have
/// more extensive documentation that calls out the differences.
///
/// @section wrapper_api_reference API Wrapper Reference
/// @subsection wrapper_section Wrapper Methods
/// @ref wrapper
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnInitializeSdk
///
/// @copydoc GfnInitializeSdk
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnShutdownSdk
///
/// @copydoc GfnShutdownSdk
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnIsRunningInCloud
///
/// @copydoc GfnIsRunningInCloud
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnIsRunningInCloudSecure
///
/// @copydoc GfnIsRunningInCloudSecure
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnFree
///
/// @copydoc GfnFree
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnGetClientIpV4
///
/// @copydoc GfnGetClientIpV4
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnGetClientIpV4
///
/// @copydoc GfnGetClientIpV4
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnGetClientLanguageCode
///
/// @copydoc GfnGetClientLanguageCode
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnGetCustomData
///
/// @copydoc GfnGetCustomData
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnIsTitleAvailable
///
/// @copydoc GfnIsTitleAvailable
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnGetTitlesAvailable
///
/// @copydoc GfnGetTitlesAvailable
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnGetClientInfo
///
/// @copydoc GfnGetClientInfo
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnRegisterStreamStatusCallback
///
/// @copydoc GfnRegisterStreamStatusCallback
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnStartStream
///
/// @copydoc GfnStartStream
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnStartStreamAsync
///
/// @copydoc GfnStartStreamAsync
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnSetupTitle
///
/// @copydoc GfnSetupTitle
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnTitleExited
///
/// @copydoc GfnTitleExited
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnRegisterExitCallback
///
/// @copydoc GfnRegisterExitCallback
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnRegisterPauseCallback
///
/// @copydoc GfnRegisterPauseCallback
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnRegisterInstallCallback
///
/// @copydoc GfnRegisterInstallCallback
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnRegisterSaveCallback
///
/// @copydoc GfnRegisterSaveCallback
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnRegisterSessionInitCallback
///
/// @copydoc GfnRegisterSessionInitCallback
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnAppReady
///
/// @copydoc GfnAppReady
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnSetActionZone
///
/// @copydoc GfnSetActionZone
///
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnRegisterClientInfoCallback
///
/// @copydoc GfnRegisterClientInfoCallback
/// 
/// Language | API
/// -------- | -------------------------------------
/// C        | @ref GfnRegisterNetworkStatusCallback
///
/// @copydoc GfnRegisterNetworkStatusCallback
/// 

#include "GfnRuntimeSdk_CAPI.h"

#ifdef _WIN32
#include <windows.h>
/// @brief Global handle to loaded GFN SDK Library module
extern HMODULE g_gfnSdkModule;
#endif
#ifdef __cplusplus
extern "C"
{
#endif
    /// @defgroup wrapper API Wrapper Methods
    /// @{

    /// @par Description
    /// Loads the GFN SDK dynamic library and calls @ref gfnInitializeRuntimeSdk. By default, the function
    /// expects the library to be in the same folder as the loading process's executable. For
    /// security reasons, the dynamic library is loaded by fully-quanitified path. If the GFN SDK
    /// library is packaged in another folder, you will need to locally modify the function to
    /// reference that location.
    ///
    /// @par Environment
    /// Cloud and Client
    ///
    /// @par Usage
    /// Call as soon as possible during application startup.
    ///
    /// @param language                   - Language to use for any UI, such as GFN download and install progress dialogs.
    ///                                     Defaults to system language if not defined.
    /// @retval gfnSuccess                - If the SDK was initialized and all SDK features are available.
    /// @retval gfnInitSuccessClientOnly  - If the SDK was initialized, but only client-side functionality is available, such as
    ///                                     calls to gfnStartStream. By definition, gfnIsRunningInCloud is expected to return false
    ///                                     in this scenario.
    /// @retval gfnDllNotPresent          - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound            - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnInitializeSdk(GfnDisplayLanguage language);

    /// @par Description
    /// Loads the GFN SDK dynamic library from the given path and calls @ref gfnInitializeRuntimeSdk.
    ///
    /// @par Environment
    /// Cloud and Client
    ///
    /// @par Usage
    /// Call as soon as possible during application startup.
    ///
    /// @param language                   - Language to use for any UI, such as GFN download and install progress dialogs.
    ///                                     Defaults to system language if not defined.
    /// @param sdkLibraryPath             - Fully-quantified path to GFN SDK Library including the name GfnRuntimeSdk.dll,
    ///                                     note that dll cannot be renamed to any other string.
    /// @retval gfnSuccess                - If the SDK was initialized and all SDK features are available.
    /// @retval gfnInitSuccessClientOnly  - If the SDK was initialized, but only client-side functionality is available, such as
    ///                                     calls to gfnStartStream. By definition, gfnIsRunningInCloud is expected to return false
    ///                                     in this scenario.
    /// @retval gfnDllNotPresent          - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound            - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnInitializeSdkFromPath(GfnDisplayLanguage language, const wchar_t* sdkLibraryPath);

    ///
    /// @par Description
    /// Calls @ref gfnShutdownRuntimeSdk to releases the SDK and resources and disconnects from GFN
    /// backend and then unloads unloads the GFN SDK Library.
    ///
    /// @par Environment
    /// Cloud and Client
    ///
    /// @par Usage
    /// Call during application shutdown or when GFN Runtime API methods are no longer needed.
    /// @retval gfnSuccess                - If the SDK was initialized and all SDK features are available.
    /// @retval gfnDllNotPresent          - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound            - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnShutdownSdk(void);

    ///
    /// @par Description
    /// Calls @ref gfnIsRunningInCloud to determine if calling application is running in GFN environment,
    /// and without requiring process elevation.
    ///
    /// @par Environment
    /// Cloud and Client
    ///
    /// @par Usage
    /// Use to quickly determine whether to enable / disable any low-value GFN cloud environment
    /// specific application logic, for example, to block any calls to @ref GfnStartStream() to
    /// avoid an error, or to know if @ref GetTitlesAvailable can be called without an error.
    ///
    /// @warning
    /// This API is meant to fill the need to quickly determine if the call looks to be in the
    /// GFN cloud environment. It purposefully trades off resource-intensive checks for fast response.
    /// Do not tie any logic or features of value to this API as the call stack could be tampered with.
    /// For that purpose, use @ref GfnIsRunningInCloudSecure.
    ///
    /// @param runningInCloud             - Pointer to a boolean that receives true if running in GeForce NOW
    ///                                     cloud or false if not in the GeForce NOW cloud.
    /// @retval gfnSuccess                - If the query was successful.
    /// @retval gfnDllNotPresent          - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound            - The API was not found in the GFN SDK Library
    ///
    /// @note
    /// This function's definition differs from the export that it wraps, as it returns a
    /// GfnRuntimeError instead of bool.
    GfnRuntimeError GfnIsRunningInCloud(bool* runningInCloud);

    ///
    /// @par Description
    /// Calls @ref gfnIsRunningInCloudSecure to determine if calling application is running in GFN environment,
    /// and what level of security assurance that the result is valid.
    ///
    /// @par Environment
    /// Cloud and Client
    /// Elevated Process
    ///
    /// @par Usage
    /// Call from an elevated process to securely determine whether running in GFN cloud, and use the
    /// GfnIsRunningInCloudAssurance value to decide the risk to enable any application specific logic
    /// for that environment.
    ///
    /// @warning
    /// This API must be called from a high-integrity elevated process, or it will fail by design. To prevent
    /// man-in-the-middle (MiM) attacks, you must also securely load the SDK library, checking the integrity
    /// of the digital signature on the binary. Make sure to use the value returned from GfnIsRunningInCloudassurance
    /// to decide if the check was certain enough without tampering to enable the logic or feature associated
    /// with the API call.
    ///
    /// @param assurance                  - Likelihood and level of security assurance defined via @ref GfnIsRunningInCloudAssurance that API is running in GFN cloud environment
    ///
    /// @retval gfnSuccess                - If the query was successful.
    /// @retval gfnRequiredElevation      - The API was called from a non-elevated process
    /// @retval gfnDllNotPresent          - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound            - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnIsRunningInCloudSecure(GfnIsRunningInCloudAssurance* assurance);

    ///
    /// @par Description
    /// Calls @ref gfnGetClientIp to get user client's IP address.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Call this during application start or from the platform client in
    /// order to get the user's external client IP address.
    ///
    /// @param clientIp - Output IPv4 in string format. Example: "192.168.0.1". Call @ref GfnFree to free the memory.
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnInvalidParameter      - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment  - If called in a client environment
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    /// @note
    /// To avoid leaking memory, call @ref gfnFree once done with the data.
    GfnRuntimeError GfnGetClientIpV4(const char** clientIp);

    ///
    /// @par Description
    /// Calls @ref gfnGetClientLanguageCode to gets user's client language code in the form
    /// "<lang>-<country>" using a standard ISO 639-1 language code and ISO 3166-1 Alpha-2 country code.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Call this during application start or from the platform client in
    /// order to get the user's language and country settings.
    ///
    /// @param languageCode - Language code as a string. Example: "en-US". Call @ref GfnFree to free the memory.
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnInvalidParameter      - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment  - If called in a client environment
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    /// @note
    /// To avoid leaking memory, call @ref gfnFree once done with the data.
    GfnRuntimeError GfnGetClientLanguageCode(const char** languageCode);

    ///
    /// @par Description
    /// Gets user's client country code using ISO 3166-1 Alpha-2 country code.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Call this during application start or from the platform client in order to get
    /// the user's country code.
    ///
    /// @param[out] countryCode          - Country code as a 2 character string. Example: “US”
    /// @param length                    - Length of pchCountryCode character array
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnInvalidParameter      - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment  - If called in a client environment
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnGetClientCountryCode(char* countryCode, unsigned int length);

    ///
    /// @par Description
    /// Gets user's client data.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Call this during application start or from the platform client in order to get
    /// the data on the user's client.
    ///
    /// @param[out] clientInfo           - A structure to hold clientInfo data
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnInvalidParameter      - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment  - If called in a client environment
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
        GfnRuntimeError GfnGetClientInfo(GfnClientInfo* clientInfo);
    ///
    /// @par Description
    /// Calls @ref GfnGetCustomData to retrieves custom data passed in by the client in the
    /// gfnStartStream call.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Use during cloud session to retrieve custom data
    ///
    /// @param customData               - Populated with the custom data. Call @ref GfnFree to free the memory.
    ///
    /// @retval gfnSuccess              - On success
    /// @retval gfnInvalidParameter     - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment - If called in a client environment
    /// @retval gfnDllNotPresent        - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound          - The API was not found in the GFN SDK Library
    /// @note
    /// To avoid leaking memory, call @ref gfnFree once done with the data.
    GfnRuntimeError GfnGetCustomData(const char** customData);

    ///
    /// @par Description
    /// Calls @ref GfnGetAuthData to retrieves custom authorization passed in by the client in the
    /// gfnStartStream call.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Use during cloud session to retrieve custom data
    ///
    /// @param authData               - Populated with the authorization data. Call @ref GfnFree to free the memory.
    ///
    /// @retval gfnSuccess              - On success
    /// @retval gfnInvalidParameter     - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment - If called in a client environment
    /// @retval gfnDllNotPresent        - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound          - The API was not found in the GFN SDK Library
    /// @note
    /// To avoid leaking memory, call @ref gfnFree once done with the data.
    GfnRuntimeError GfnGetAuthData(const char** authData);

    ///
    /// @par Description
    /// Calls @ref gfnIsTitleAvailable to determines if a specific title is available to launch
    /// in current streaming session.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Use to determine if a title is available to be launched from the active GFN cloud instance,
    /// for example, to show a "Play" button in a platform launcher's UI.
    ///
    /// @param platformAppId             - Identifier of the requested title to check
    /// @param isAvailable               - Pointer to a boolean that receives true if the title is
    ///                                    available, or or false if not available.
    ///
    /// @retval gfnSuccess               - If the query was successful.
    /// @retval gfnCallWrongEnvironment  - If called in a client environment
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    ///
    /// @note
    /// This function's definition differs from the export that it wraps, as it returns a
    /// GfnRuntimeError instead of bool.
    GfnRuntimeError GfnIsTitleAvailable(const char* platformAppId, bool* isAvailable);

    ///
    /// @par Description
    /// Calls @ref gfnGetTitlesAvailable to retrieves all titles that can be launched in the
    /// current game streaming session.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Use to retrieve a list of all titles available to launch in the current streaming session,
    /// for example, to add "Play" buttons to all titles instead of calling gfnIsTitleAvailable on
    /// each title.
    ///
    /// @param platformAppIds            - Comma-delimited list of platform identifiers. Memory is
    ///                                    allocated for the list. Call @ref GfnFree to free the memory.
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnInvalidParameter      - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment  - If called in a client environment
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    ///
    /// @note
    /// To avoid leaking memory, call @ref gfnFree once done with the title list.
    GfnRuntimeError GfnGetTitlesAvailable(const char** platformAppIds);

    ///
    /// @par Description
    /// Calls @ref gfnFree to free memory allocated by @ref gfnGetTitlesAvailable
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Use to free memory after a call to a memory-allocating function and you are finished with the data
    ///
    /// @param data                      - Pointer to data to free
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnInvalidParameter      - NULL pointer passed in
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnFree(const char** data);


    ///
    /// @par Description
    /// Calls @ref gfnRegisterStreamStatusCallback to register a callback that gets called on the
    /// user's PC when the streaming session state changes.
    ///
    /// @par Environment
    /// Client
    ///
    /// @par Usage
    /// Register a function to call when stream status changes on the user's client PC
    ///
    /// @param streamStatusCallback      - Function pointer to application code to call after
    ///                                    the Geforce NOW streamer changes state
    /// @param userContext               - Pointer to user context, which will be passed unmodified to the
    ///                                    registered callback. Can be NULL.
    ///
    /// @retval gfnSuccess               - On success, when running on the client PC
    /// @retval gfnCallWrongEnvironment  - If callback could not be registered since this function
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnRegisterStreamStatusCallback(StreamStatusCallbackSig streamStatusCallback, void* userContext);

    ///
    /// @par Description
    /// Calls @ref gfnStartStream to request GFN client to start a streaming session of an application
    /// in a synchronous (blocking) fashion.
    ///
    /// @par Environment
    /// Client
    ///
    /// @par Usage
    /// Use to start a streaming session.
    ///
    /// @param startStreamInput         - Pointer to a StartStreamInput structure.
    /// @param response                 - Start streaming response.
    ///
    /// @retval gfnSuccess              - On success
    /// @retval gfnInvalidParameter     - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment - If called in a cloud environment
    /// @retval gfnStreamFailure        - Network failure or other error prevented the stream from starting
    /// @retval gfnDllNotPresent        - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound          - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnStartStream(StartStreamInput* startStreamInput, StartStreamResponse* response);

    ///
    /// @par Description
    /// Calls @ref gfnStartStreamAsync to request GFN client to start a streaming session of an
    /// application in an asynchronous fashion.
    ///
    /// @par Environment
    /// Client
    ///
    /// @par Usage
    /// Use to start a streaming session.
    ///
    /// @param startStreamInput          - Pointer to a StartStreamInput structure.
    /// @param cb                        - A StartStreamCallbackSig callback to be called with streaming events.
    /// @param context                   - User context
    /// @param timeoutMs                 - Time after which attempt to start streaming will be aborted.
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnInvalidParameter      - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment  - If called in a cloud environment
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    ///
    /// @note
    /// This function's definition differs from the export that it wraps, as it returns a
    /// GfnRuntimeError instead of being void for consistent error reporting.
    GfnRuntimeError GfnStartStreamAsync(const StartStreamInput* startStreamInput, StartStreamCallbackSig cb, void* context, unsigned int timeoutMs);

    ///
    /// @par Description
    /// Calls @ref gfnStopStream to request GFN client to stop a streaming session of an application
    /// in a synchronous (blocking) fashion.
    ///
    /// @par Environment
    /// Client
    ///
    /// @par Usage
    /// Use to stop a streaming session started by the same process
    ///
    /// @retval gfnSuccess              - On success
    /// @retval gfnCallWrongEnvironment - If called in a cloud environment
    /// @retval gfnStreamStopFailure    - Failure to stop the streaming session
    /// @retval gfnDllNotPresent        - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound          - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnStopStream(void);

    ///
    /// @par Description
    /// Calls @ref gfnStopStreamAsync to request GFN client to stop a streaming session of an
    /// application in an asynchronous fashion.
    ///
    /// @par Environment
    /// Client
    ///
    /// @par Usage
    /// Use to start a streaming session.
    ///
    /// @param cb                        - A StartStreamCallbackSig callback to be called with streaming events.
    /// @param context                   - User context
    /// @param timeoutMs                 - Time after which attempt to start streaming will be aborted.
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnStreamStopFailure     - Failure to stop the streaming session
    /// @retval gfnInvalidParameter      - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment  - If called in a cloud environment
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    ///
    /// @note
    /// This function's definition differs from the export that it wraps, as it returns a
    /// GfnRuntimeError instead of being void for consistent error reporting.
    GfnRuntimeError GfnStopStreamAsync(StopStreamCallbackSig cb, void* context, unsigned int timeoutMs);

    ///
    /// @par Description
    /// Calls @ref gfnSetupTitle to notify Geforce NOW that an application should be readied for launch.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Use to prepare an application for launch on Geforce NOW, and block on the result.
    ///
    /// @param platformAppId             - Identifier of the requested application to setup
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnInvalidParameter      - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment  - If called in a client environment
    /// @retval gfnSetupTitleFailure     - The Geforce NOW SDK failed to set up the title
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnSetupTitle(const char* platformAppId);

    ///
    /// @par Description
    /// Calls @ref GfnTitleExited to notify GFN that an application has exited.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Use to notify GFN that your application has exited.
    ///
    /// @param platformId                - Identifier of the launcher service, e.g. "Steam"
    /// @param platformAppId             - Identifier of the application that has exited.
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnInvalidParameter      - NULL pointer passed in
    /// @retval gfnCallWrongEnvironment  - If called in a client environment
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnTitleExited(const char* platformId, const char* platformAppId);

    ///
    /// @par Description
    /// Calls @ref gfnRegisterExitCallback to register an application function to call when Geforce NOW
    /// needs to exit the game.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Register an application function to call when Geforce NOW needs to exit the game.
    ///
    /// @param exitCallback             - Function pointer to application code to call when Geforce NOW
    ///                                   needs to exit the game.
    /// @param userContext              - Pointer to user context, which will be passed unmodified to the
    ///                                   registered callback. Can be NULL.
    ///
    /// @retval gfnSuccess              - On success, when running in a Geforce NOW environment
    /// @retval gfnInvalidParameter     - If exitCallback function pointer is NULL.
    /// @retval gfnCallWrongEnvironment - If callback could not be registered since this function
    ///                                   was called outside of a cloud execution environment
    /// @retval gfnDllNotPresent        - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound          - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnRegisterExitCallback(ExitCallbackSig exitCallback, void* userContext);

    ///
    /// @par Description
    /// Calls @ref gfnRegisterPauseCallback to register an application callback with Geforce NOW
    /// to be called when Geforce NOW needs to pause the game on the user's behalf.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Register an application function to call when Geforce NOW needs to pause the game.
    ///
    /// @param pauseCallback            - Function pointer to application code to call when
    ///                                   Geforce NOW needs to pause the game
    /// @param userContext              - Pointer to user context, which will be passed unmodified to the
    ///                                   registered callback. Can be NULL.
    ///
    /// @retval gfnSuccess              - On success, when running in a Geforce NOW environment
    /// @retval gfnInvalidParameter     - If pauseCallback function pointer is NULL.
    /// @retval gfnCallWrongEnvironment - If callback could not be registered since this function
    ///                                   was called outside of a cloud execution environment
    /// @retval gfnDllNotPresent        - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound          - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnRegisterPauseCallback(PauseCallbackSig pauseCallback, void* userContext);

    ///
    /// @par Description
    /// Calls @ref gfnRegisterInstallCallback to register an application callback with Geforce NOW
    /// to be called after a successful call to @ref gfnSetupTitle.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Register a function to call after a successful call to gfnSetupTitle.
    ///
    /// @param installCallback          - Function pointer to application code to call after
    ///                                   Geforce NOW successfully performs its own title setup.
    /// @param userContext              - Pointer to user context, which will be passed unmodified to the
    ///                                   registered callback. Can be NULL.
    ///
    /// @retval gfnSuccess              - On success, when running in a Geforce NOW environment
    /// @retval gfnInvalidParameter     - If installCallback function pointer is NULL.
    /// @retval gfnCallWrongEnvironment - If callback could not be registered since this function
    ///                                   was called outside of a cloud execution environment
    /// @retval gfnDllNotPresent        - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound          - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnRegisterInstallCallback(InstallCallbackSig installCallback, void* userContext);

    ///
    /// @par Description
    /// Calls @ref gfnRegisterSaveCallback to register an application callback with GFN to be
    /// called when GFN needs the application to save user progress.
    ///
    /// @par Usage
    /// Register an application function to call when GFN needs the application to save
    ///
    /// @param saveCallback             - Function pointer to application code to call when GFN needs the application to save
    /// @param userContext              - Pointer to user context, which will be passed unmodified to the
    ///                                   callback specified. Can be NULL.
    ///
    /// @retval gfnSuccess              - On success when running in a GFN environment
    /// @retval gfnDllNotPresent        - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound          - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnRegisterSaveCallback(SaveCallbackSig saveCallback, void* userContext);

    ///
    /// @par Description
    /// Calls @ref gfnRegisterSessionInitCallback to register an application callback to be called when a GFN user has
    /// connected to the game seat.
    ///
    /// @par Usage
    /// Register an application function to call when a GFN user has connected to the game seat
    ///
    /// @param sessionInitCallback      - Function pointer to application code to call when the user has connected
    /// @param userContext              - Pointer to user context, which will be passed unmodified to the
    ///                                   callback specified. Can be NULL.
    ///
    /// @retval gfnSuccess              - On success when running in a GFN environment
    /// @retval gfnInvalidParameter     - Callback was NULL
    /// @retval gfnDllNotPresent        - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound          - The API was not found in the GFN SDK Library
    /// @retval gfnCallWrongEnvironment - The on-seat dll detected that it was not on a game seat
    GfnRuntimeError GfnRegisterSessionInitCallback(SessionInitCallbackSig sessionInitCallback, void* userContext);


    ///
    /// @par Description
    /// Registers an application callback with GFN to be called when client info changes
    ///
    /// @param clientInfoCallback       - Function pointer to application code to call when GFN client data changes
    ///
    /// @retval gfnSuccess              - On success when running in a GFN environment
    /// @retval gfnDllNotPresent        - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound          - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnRegisterClientInfoCallback(ClientInfoCallbackSig clientInfoCallback, void* userContext);

    ///
    /// @par Description
    /// Calls @ref GfnAppReady to notify GFN that an application is ready to be displayed.
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Use to notify GFN that your application is ready to be streamed.
    ///
    /// @param success                - True if startup was successful
    /// @param status                 - Optional startup status
    ///
    /// @retval gfnSuccess               - On success
    /// @retval gfnCallWrongEnvironment  - If called in a client environment
    /// @retval gfnDllNotPresent         - GFN SDK Library could not be found.
    /// @retval gfnAPINotFound           - The API was not found in the GFN SDK Library
    GfnRuntimeError GfnAppReady(bool success, const char * status);

    ///
    /// @par Description
    /// Sends Active zone coordinates to GFN Client
    ///
    /// @par Environment
    /// Cloud
    ///
    /// @par Usage
    /// Use to invoke special events on the client from the GFN cloud environment
    ///
    /// @param type                 - Populated with relevant GfnActionType
    /// @param id                   - unique unsigned int type identifier for this event
    /// @param zone                 - Optional zone coordinates relevant to type specified
    ///
    /// @retval gfnSuccess              - Call was successful
    /// @retval gfnInputExpected        - Expected zone to have a value
    /// @retval gfnComError             - There was SDK internal communication error
    /// @retval gfnInitFailure          - SDK was not initialized
    /// @retval gfnInvalidParameter     - Invalid parameters provided
    /// @retval gfnThrottled            - API call was throttled for exceeding limit
    /// @retval gfnUnhandledException   - API ran into an unhandled error and caught an exception before it returned to client code
    /// @return Otherwise, appropriate error code
    GfnRuntimeError GfnSetActionZone(GfnActionType type, unsigned int id, GfnRect* zone);
    /// @}
#ifdef __cplusplus
    } // extern "C"
#endif

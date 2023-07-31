// Copyright 2020 Google LLC
//
// The Play Core Native SDK is licensed to you under the Play Core Software
// Development Kit Terms of Service -
// https://developer.android.com/guide/playcore/license.
// By using the Play Core Native SDK, you agree to the Play Core Software
// Development Kit Terms of Service.

#ifndef PLAY_APP_UPDATE_H_
#define PLAY_APP_UPDATE_H_

#include <jni.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @defgroup appupdate Play In-App Update
/// Native API for Play In-App Update
/// @{

/// Errors that can be encountered while using the in-app update API.
enum AppUpdateErrorCode {
  /// No error has occurred.
  APP_UPDATE_NO_ERROR = 0,

  /// An unknown error occurred.
  APP_UPDATE_UNKNOWN_ERROR = -2,

  /// The in-app update API isn't available on this device.
  APP_UPDATE_API_NOT_AVAILABLE = -3,

  /// The function call was invalid, for example due to specifying a null
  /// parameter.
  APP_UPDATE_INVALID_REQUEST = -4,

  /// The update is unavailable to this user or device.
  APP_UPDATE_UNAVAILABLE = -5,

  /// The update isn't allowed due to the current device state, for example low
  /// battery or low disk space.
  APP_UPDATE_NOT_ALLOWED = -6,

  /// The update has not been (fully) downloaded yet.
  APP_UPDATE_DOWNLOAD_NOT_PRESENT = -7,

  /// The update is already in progress and there is no UI flow to resume.
  APP_UPDATE_IN_PROGRESS = -8,

  /// The Play Store app is either not installed or not the official version.
  APP_UPDATE_PLAY_STORE_NOT_FOUND = -9,

  /// The app isn't owned by any user on this device. An app is "owned" if it
  /// has been acquired from the Play Store.
  APP_UPDATE_APP_NOT_OWNED = -10,

  /// An internal error occurred.
  APP_UPDATE_INTERNAL_ERROR = -100,

  /// The requested operation failed: call AppUpdateManager_init() first.
  APP_UPDATE_INITIALIZATION_NEEDED = -110,

  /// Error initializing dependencies.
  APP_UPDATE_INITIALIZATION_FAILED = -111,
};

/// Status returned when requesting an in-app update.
enum AppUpdateStatus {
  /// The update status is unknown.
  APP_UPDATE_STATUS_UNKNOWN = 0,

  /// Download of an update is pending and will be processed soon.
  APP_UPDATE_PENDING = 1,

  /// Download of an update is in progress.
  APP_UPDATE_DOWNLOADING = 2,

  /// An update is being installed.
  APP_UPDATE_INSTALLING = 3,

  /// An update has been successfully installed.
  APP_UPDATE_INSTALLED = 4,

  /// An update has failed.
  APP_UPDATE_FAILED = 5,

  /// An update has been canceled.
  APP_UPDATE_CANCELED = 6,

  /// An update has been fully downloaded.
  APP_UPDATE_DOWNLOADED = 11,

  /// Request for update info is pending and will be processed soon.
  APP_UPDATE_REQUEST_INFO_PENDING = 100,

  /// Request for update info has failed.
  APP_UPDATE_REQUEST_INFO_FAILED = 101,

  /// Request for update info has completed.
  APP_UPDATE_REQUEST_INFO_COMPLETED = 102,

  /// Request for starting an update is pending and will be processed soon.
  APP_UPDATE_REQUEST_START_UPDATE_PENDING = 110,

  /// Request for completing an update is pending and will be processed soon.
  APP_UPDATE_REQUEST_COMPLETE_UPDATE_PENDING = 120,
};

/// Availability info for an in-app update.
enum AppUpdateAvailability {
  /// Update availability is unknown.
  APP_UPDATE_AVAILABLILITY_UNKNOWN = 0,

  /// No updates are available.
  APP_UPDATE_NOT_AVAILABLE = 1,

  /// An update is available.
  APP_UPDATE_AVAILABLE = 2,

  /// An update has been triggered by the developer and is in progress.
  APP_UPDATE_TRIGGERED_IN_PROGRESS = 3
};

/// Methods for performing the in-app update flow. Note: regardless of the
/// method selected, the app needs to be restarted to install an update.
enum AppUpdateType {
  /// The app update type is unknown or unspecified.
  APP_UPDATE_TYPE_UNKNOWN = -1,

  /// Flexible update flow, where the user can still use the app while the
  /// update is downloaded.
  APP_UPDATE_TYPE_FLEXIBLE = 0,

  /// Immediate update flow, where the user is unable to interact with the app
  /// while the update is downloaded and installed.
  APP_UPDATE_TYPE_IMMEDIATE = 1
};

///  An opaque struct used to access information about an update.
typedef struct AppUpdateInfo_ AppUpdateInfo;

/// An opaque struct used to provide options for an update.
typedef struct AppUpdateOptions_ AppUpdateOptions;

/// Initialize the in-app update API, making the other functions available to
/// call.
/// @param jvm The app's single JavaVM, which can be obtained from
/// ANativeActivity's "vm" field.
/// @param android_context An Android Context, which can be obtained from
/// ANativeActivity's "clazz" field.
/// @return APP_UPDATE_NO_ERROR if initialization succeeded, or an error if not.
/// @see AppUpdateManager_destroy
AppUpdateErrorCode AppUpdateManager_init(JavaVM* jvm, jobject android_context);

/// Frees up memory allocated for the in-app update API. Does nothing if
/// AppUpdateManager_init() hasn't been called.
void AppUpdateManager_destroy();

/// Registers an internal update status listener. Must be called in
/// ANativeActivity ANativeActivityCallbacks's onResume, or equivalent.
/// @return APP_UPDATE_NO_ERROR if the call succeeded, or an error if not.
AppUpdateErrorCode AppUpdateManager_onResume();

/// Deregisters an internal update status listener. Must be called in
/// ANativeActivity ANativeActivityCallbacks's onPause, or equivalent.
/// @return APP_UPDATE_NO_ERROR if the call succeeded, or an error if not.
AppUpdateErrorCode AppUpdateManager_onPause();

/// Asynchronously requests information about an update. Needs to be called once
/// before AppUpdateManager_requestStartUpdate() to obtain an AppUpdateInfo,
/// which is required to launch the in-app update flow.
///
/// Use AppUpdateManager_getInfo() to poll for the result.
///
/// Note: this function isn't idempotent and restarts the request on every call.
/// @return APP_UPDATE_NO_ERROR if the request started successfully, or an error
/// if not.
AppUpdateErrorCode AppUpdateManager_requestInfo();

/// Asynchronously requests to start the in-app update flow. Use
/// AppUpdateManager_getInfo() to monitor the current status and
/// download progress of the ongoing asynchronous update operation.
/// @param info The AppUpdateInfo containing information needed for an update.
/// @param options The AppUpdateOptions specifying the update flow type.
/// @param android_activity An Android Activity, which can be obtained from
/// ANativeActivity's "clazz" field.
/// @return APP_UPDATE_NO_ERROR if the request started successfully, or an error
/// if not.
AppUpdateErrorCode AppUpdateManager_requestStartUpdate(
    AppUpdateInfo* info, AppUpdateOptions* options, jobject android_activity);

/// Asynchronously requests to complete a flexible in-app update flow that was
/// started via AppUpdateManager_requestStartUpdate().
/// This function can be called after the update reaches the
/// APP_UPDATE_DOWNLOADED status.
/// After calling this function the system installer will close the app,
/// perform the update, and then restart the app at the updated version.
/// @return APP_UPDATE_NO_ERROR if the update completion request started
/// successfully, or an error if not.
AppUpdateErrorCode AppUpdateManager_requestCompleteUpdate();

/// Gets the result of an ongoing or completed call to
/// AppUpdateManager_requestInfo(), AppUpdateManager_requestStartUpdate(), or
/// AppUpdateManager_requestCompleteUpdate(). This function doesn't make JNI
/// calls and can be called every frame to monitor progress.
/// @param out_info An out parameter for receiving the result.
/// @return APP_UPDATE_NO_ERROR if the request is ongoing or successful, or an
/// error indicating why in-app update may not currently be possible.
/// @see AppUpdateInfo_destroy
AppUpdateErrorCode AppUpdateManager_getInfo(AppUpdateInfo** out_info);

/// Releases the specified AppUpdateInfo and any references it holds. Except for
/// rare cases, this function doesn't make JNI calls and can be called every
/// frame.
/// @param info The AppUpdateInfo to free.
void AppUpdateInfo_destroy(AppUpdateInfo* info);

/// Returns the latest available version code for the specified AppUpdateInfo.
/// @param info The AppUpdateInfo for which to get available version code.
/// @return The available version code for the update, or 0 if unknown.
uint32_t AppUpdateInfo_getAvailableVersionCode(AppUpdateInfo* info);

/// Returns the update availability for the specified AppUpdateInfo.
/// @param info The AppUpdateInfo for which to get update availability.
/// @return The update availability, for example APP_UPDATE_AVAILABLE.
AppUpdateAvailability AppUpdateInfo_getAvailability(AppUpdateInfo* info);

/// Returns the update status for the specified AppUpdateInfo.
/// @param info The AppUpdateInfo for which to get update status.
/// @return The update status, for example APP_UPDATE_REQUEST_INFO_PENDING.
AppUpdateStatus AppUpdateInfo_getStatus(AppUpdateInfo* info);

/// Returns the number of days that the Google Play Store app on the user's
/// device has known about an available update.
/// @param info The AppUpdateInfo for which to get client version staleness.
/// @return The number of days an app update has been available, or -1 if
/// unknown or unavailable.
int32_t AppUpdateInfo_getClientVersionStalenessDays(AppUpdateInfo* info);

/// Returns the priority for this update, as defined by the developer in the
/// Google Play Developer API.
/// @param info The AppUpdateInfo for which to get update priority.
/// @return The update priority, or 0 if unknown or unavailable.
int32_t AppUpdateInfo_getPriority(AppUpdateInfo* info);

/// Returns the total number in bytes already downloaded for the specified
/// AppUpdateInfo.
/// @param info The AppUpdateInfo for which to get bytes downloaded.
/// @return The total number of bytes already downloaded.
uint64_t AppUpdateInfo_getBytesDownloaded(AppUpdateInfo* info);

/// Returns the total bytes to download for the specified AppUpdateInfo.
/// @param info The AppUpdateInfo for which to get the total bytes to download.
/// @return The total size in bytes for the update, or 0 if unknown.
uint64_t AppUpdateInfo_getTotalBytesToDownload(AppUpdateInfo* info);

/// Returns whether an update is allowed for the specified AppUpdateInfo and
/// AppUpdateOptions. This function's result can be checked prior to calling
/// AppUpdateManager_requestStartUpdate().
/// @param info An AppUpdateInfo to check for whether an update is allowed.
/// @param options The AppUpdateOptions specifying the update flow type.
/// @return True if the specified update options are allowed, or false if either
/// the update isn't allowed or if there is an error, for example due to invalid
/// parameters.
bool AppUpdateInfo_isUpdateTypeAllowed(AppUpdateInfo* info,
                                       AppUpdateOptions* options);

/// Creates a new AppUpdateOptions with the specified AppUpdateType.
/// @param type The AppUpdateType specifying the update flow type.
/// @param out_options An out parameter to store the created AppUpdateOptions.
/// @return APP_UPDATE_NO_ERROR if an AppUpdateOptions is created successfully,
/// or an error if not.
/// @see AppUpdateOptions_destroy
AppUpdateErrorCode AppUpdateOptions_createOptions(
    AppUpdateType type, AppUpdateOptions** out_options);

/// Releases the specified AppUpdateOptions and any references it holds.
/// @param options The AppUpdateOptions to free.
void AppUpdateOptions_destroy(AppUpdateOptions* options);

/// Returns the update type for the specified AppUpdateOptions.
/// @param options The AppUpdateOptions for which to get the update type.
/// @return The type of app update flow.
AppUpdateType AppUpdateOptions_getAppUpdateType(AppUpdateOptions* options);

/// Returns whether the app installer should be allowed to delete some asset
/// packs from the app's storage before attempting to update the app, for
/// example if disk space is limited.
/// @param options The AppUpdateOptions specifying the update flow type.
/// @return Bool indicating whether the app installer should be allowed to
/// delete asset packs (if necessary) during an app update.
bool AppUpdateOptions_isAssetPackDeletionAllowed(AppUpdateOptions* options);

/// Indicates whether the app installer is allowed to delete existing asset
/// packs while updating the app, for example if disk space is limited.
/// @param options The AppUpdateOptions on which to configure this setting.
/// @param allow Boolean indicating whether the app installer is allowed to
/// delete asset packs (if necessary) during an app update.
/// @return APP_UPDATE_NO_ERROR if the value is set successfully, or an error if
/// not.
AppUpdateErrorCode AppUpdateOptions_setAssetPackDeletionAllowed(
    AppUpdateOptions* options, bool allow);

/// @}

#ifdef __cplusplus
};  // extern "C"
#endif

#endif  // PLAY_APP_UPDATE_H_

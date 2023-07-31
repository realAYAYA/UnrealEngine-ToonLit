// Copyright 2019 Google LLC
//
// The Play Core Native SDK is licensed to you under the Play Core Software
// Development Kit Terms of Service -
// https://developer.android.com/guide/playcore/license.
// By using the Play Core Native SDK, you agree to the Play Core Software
// Development Kit Terms of Service.

#ifndef PLAY_REVIEW_H_
#define PLAY_REVIEW_H_

#include <jni.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @defgroup review Play In-App Review
/// Native API for Play In-App Review
/// @{

/// Errors that can be encountered while using the in-app review API.
enum ReviewErrorCode {
  /// No error has occurred.
  REVIEW_NO_ERROR = 0,

  /// The requested operation failed: need to call ReviewManager_init() first.
  REVIEW_INITIALIZATION_NEEDED = -1,

  /// Error initializing dependencies.
  REVIEW_INITIALIZATION_FAILED = -2,

  /// The requested operation failed: need to call
  /// ReviewManager_requestReviewFlow() first.
  REVIEW_REQUEST_FLOW_NEEDED = -3,

  /// ReviewManager_requestReviewFlow() failed.
  REVIEW_REQUEST_FLOW_FAILED = -4,

  /// An invalid parameter was passed to the function.
  REVIEW_INVALID_REQUEST = -5,

  /// The Play Store app is either not installed or not the official version.
  REVIEW_PLAY_STORE_NOT_FOUND = -6,
};

/// Status returned when requesting or launching the in-app review flow.
enum ReviewStatus {
  /// Unknown Review status.
  REVIEW_STATUS_UNKNOWN = 0,

  /// Waiting for ReviewManager_requestReviewFlow() asynchronous operation to
  /// finish.
  REVIEW_REQUEST_FLOW_PENDING = 1,

  /// ReviewManager_requestReviewFlow() asynchronous operation has finished.
  REVIEW_REQUEST_FLOW_COMPLETED = 2,

  /// Waiting for ReviewManager_launchReviewFlow() asynchronous operation to
  /// finish.
  REVIEW_LAUNCH_FLOW_PENDING = 3,

  /// ReviewManager_launchReviewFlow() asynchronous operation has finished,
  /// and it will receive this ReviewStatus regardless of whether the user
  /// completed the review or the user dismissed the dialog.
  REVIEW_LAUNCH_FLOW_COMPLETED = 4,
};

/// Initialize the in-app review API, making the other functions available to
/// call.
/// @param jvm The app's single JavaVM, for example from ANativeActivity's "vm"
/// field.
/// @param android_context An Android Context, for example from
/// ANativeActivity's "clazz" field.
/// @return REVIEW_NO_ERROR if initialization succeeded, or an error if it
/// failed. In case of failure the in-app review API is unavailable, and there
/// will be an error in logcat. The most common reason for failure is that the
/// PlayCore AAR is missing or some of its classes/methods weren't retained by
/// ProGuard.
/// @see ReviewManager_destroy()
ReviewErrorCode ReviewManager_init(JavaVM* jvm, jobject android_context);

/// Frees up memory allocated for the in-app review API. Does nothing if
/// ReviewManager_init() hasn't been called.
void ReviewManager_destroy();

/// Asynchronously requests the information needed to launch the in-app review
/// flow. Use ReviewManager_getReviewStatus() to monitor progress and get the
/// result.
///
/// Note: ReviewManager_requestReviewFlow() and ReviewManager_launchReviewFlow()
/// should not be called simultaneously from multiple threads.
///
/// @return REVIEW_NO_ERROR if the request started successfully, or an error
/// if it failed.
ReviewErrorCode ReviewManager_requestReviewFlow();

/// Asynchronously requests to display the in-app review flow to the user. Use
/// ReviewManager_getReviewStatus() to monitor progress and get the result.
///
/// Note: ReviewManager_requestReviewFlow() and ReviewManager_launchReviewFlow()
/// should not be called simultaneously from multiple threads.
///
/// Note: in some circumstances the review flow will not be shown to the user,
/// for example if they have already seen it recently. Don't assume that calling
/// this function will always display the review dialog.
///
/// @param android_activity An Android Activity object parameter, it can be
/// obtained from ANativeActivity's "clazz" field, and it will be used for
/// launching the review dialog as part of the Android Activity stack.
/// @return REVIEW_NO_ERROR if the request started successfully, or an error
/// if it failed.
ReviewErrorCode ReviewManager_launchReviewFlow(jobject android_activity);

/// Gets the state of an ongoing asynchronous operation: requesting or launching
/// the flow.
///
/// ReviewManager_requestReviewFlow() and ReviewManager_launchReviewFlow()
/// execute an asynchronous operation, and this function helps to keep track of
/// the current status of their asynchronous operation.
///
/// When using ReviewManager_requestReviewFlow(), the possible
/// ReviewStatus values are:
/// * REVIEW_REQUEST_FLOW_PENDING
/// * REVIEW_REQUEST_FLOW_COMPLETED
/// * REVIEW_STATUS_UNKNOWN - in case of failure
///
/// When using ReviewManager_launchReviewFlow(), the possible
/// ReviewStatus values are:
/// * REVIEW_LAUNCH_FLOW_PENDING
/// * REVIEW_LAUNCH_FLOW_COMPLETED
///
/// @param out_status An out parameter for receiving the review status result.
/// @return A ReviewErrorCode indicating an error associated with the most
/// recent asynchronous operation request.
ReviewErrorCode ReviewManager_getReviewStatus(ReviewStatus* out_status);

/// @}

#ifdef __cplusplus
};  // extern "C"
#endif

#endif  // PLAY_REVIEW_H_

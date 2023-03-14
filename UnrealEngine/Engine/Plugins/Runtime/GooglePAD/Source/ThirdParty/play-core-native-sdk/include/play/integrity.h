// Copyright 2021 Google LLC
//
// The Play Core Native SDK is licensed to you under the Play Core Software
// Development Kit Terms of Service -
// https://developer.android.com/guide/playcore/license.
// By using the Play Core Native SDK, you agree to the Play Core Software
// Development Kit Terms of Service.

#ifndef PLAY_INTEGRITY_H_
#define PLAY_INTEGRITY_H_

#include <jni.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @defgroup integrity Play Integrity API
/// Native Play Integrity API
/// @{

/// Errors that can be encountered while using the integrity API.
enum IntegrityErrorCode {
  /// No error has occurred.
  INTEGRITY_NO_ERROR = 0,

  /// Integrity API is not available.
  ///
  /// The Play Store version might be old, or the application is not allowlisted
  /// to use this API.
  /// Recommended actions:
  /// * Make sure that an app is allowlisted to use the API.
  /// * Ask the user to update Play Store.
  INTEGRITY_API_NOT_AVAILABLE = -1,

  /// No Play Store app is found on device or not official version is installed.
  ///
  /// Ask the user to install an official and recent version of Play Store.
  INTEGRITY_PLAY_STORE_NOT_FOUND = -2,

  /// No available network is found.
  ///
  /// Ask the user to check for a connection.
  INTEGRITY_NETWORK_ERROR = -3,

  /// No Play Store account is found on device.
  ///
  /// Ask the user to authenticate in Play Store.
  INTEGRITY_PLAY_STORE_ACCOUNT_NOT_FOUND = -4,

  /// The calling app is not installed.
  ///
  /// Something is wrong (possibly an attack). Non-actionable.
  INTEGRITY_APP_NOT_INSTALLED = -5,

  /// Play Services is not available or version is too old.
  ///
  /// Ask the user to Install or Update Play Services.
  INTEGRITY_PLAY_SERVICES_NOT_FOUND = -6,

  /// The calling app UID (user id) does not match the one from Package Manager.
  ///
  /// Something is wrong (possibly an attack). Non-actionable.
  INTEGRITY_APP_UID_MISMATCH = -7,

  /// The calling app is making too many requests to the API and hence is
  /// throttled.
  ///
  /// Retry with an exponential backoff.
  INTEGRITY_TOO_MANY_REQUESTS = -8,

  /// Binding to the service in the Play Store has failed. This can be due to
  /// having an old Play Store version installed on the device.
  ///
  /// Ask the user to update Play Store.
  INTEGRITY_CANNOT_BIND_TO_SERVICE = -9,

  /// Nonce length is too short. The nonce must be a minimum of 16 bytes (before
  /// Base64 encoding) to allow for a better security.
  ///
  /// Retry with a longer nonce.
  INTEGRITY_NONCE_TOO_SHORT = -10,

  /// Nonce length is too long. The nonce must be less than 500 bytes before
  /// Base64 encoding.
  ///
  /// Retry with a shorter nonce.
  INTEGRITY_NONCE_TOO_LONG = -11,

  /// Unknown internal Google server error.
  ///
  /// Retry with an exponential backoff. Consider filing a bug if fails
  /// consistently.
  INTEGRITY_GOOGLE_SERVER_UNAVAILABLE = -12,

  /// Nonce is not encoded as a Base64 web-safe no-wrap string.
  ///
  /// Retry with correct nonce format.
  INTEGRITY_NONCE_IS_NOT_BASE64 = -13,

  /// The Play Store needs to be updated.
  ///
  /// Ask the user to update the Google Play Store.
  INTEGRITY_PLAY_STORE_VERSION_OUTDATED = -14,

  /// Play Services needs to be updated.
  ///
  /// Ask the user to update Google Play Services.
  INTEGRITY_PLAY_SERVICES_VERSION_OUTDATED = -15,

  /// Unknown internal error.
  ///
  /// Retry with an exponential backoff. Consider filing a bug if fails
  /// consistently.
  INTEGRITY_INTERNAL_ERROR = -100,

  /// IntegrityManager is not initialized.
  ///
  /// Call IntegrityManager_init() first.
  INTEGRITY_INITIALIZATION_NEEDED = -101,

  /// There was an error initializing the Integrity API.
  ///
  /// Retry with an exponential backoff. Consider filing a bug if fails
  /// consistently.
  INTEGRITY_INITIALIZATION_FAILED = -102,

  /// Invalid argument passed to the Integrity API.
  ///
  /// Retry with correct argument.
  INTEGRITY_INVALID_ARGUMENT = -103,
};

/// Status returned when requesting integrity tokens.
enum IntegrityResponseStatus {
  /// The response status is unknown.
  INTEGRITY_RESPONSE_UNKNOWN = 0,

  /// Waiting for IntegrityManager_requestIntegrityToken() asynchronous
  /// operation to finish.
  INTEGRITY_RESPONSE_PENDING = 1,

  /// IntegrityManager_requestIntegrityToken() asynchronous operation has
  /// finished.
  INTEGRITY_RESPONSE_COMPLETED = 2,
};

/// An opaque struct used to provide information about an integrity token
/// request.
typedef struct IntegrityTokenRequest_ IntegrityTokenRequest;

/// An opaque struct used to access information about an integrity token
/// response.
typedef struct IntegrityTokenResponse_ IntegrityTokenResponse;

/// Initialize the Play Integrity API, making the other functions available to
/// call.
///
/// In case of failure the Play Integrity API is unavailable, and there will be
/// an error in logcat. The most common reason for failure is that the PlayCore
/// AAR is missing or some of its classes/methods weren't retained by ProGuard.
/// @param jvm The app's single JavaVM, for example  from ANativeActivity's "vm"
/// field.
/// @param android_context An Android Context, for example  from
/// ANativeActivity's "clazz" field.
/// @return INTEGRITY_NO_ERROR if initialization succeeded, or an error if
/// failed.
/// @see IntegrityManager_destroy
IntegrityErrorCode IntegrityManager_init(JavaVM* jvm, jobject android_context);

/// Frees up memory allocated for the Integrity API. Does nothing if
/// IntegrityManager_init() hasn't been called.
void IntegrityManager_destroy();

/// Starts an asynchronous operation to obtain a Play Integrity API token.
///
/// For an ongoing asynchronous operation, use
/// IntegrityTokenResponse_getStatus() to poll for the status of the
/// IntegrityTokenResponse. When the status reaches
/// INTEGRITY_RESPONSE_COMPLETED with INTEGRITY_NO_ERROR, use
/// IntegrityTokenResponse_getToken() to acquire the resulting token.
///
/// @param request The request containing the nonce.
/// @param out_response An out parameter for receiving the result.
/// @return INTEGRITY_NO_ERROR if the request started successfully,
/// corresponding error if failed.
/// @see IntegrityTokenResponse_destroy
IntegrityErrorCode IntegrityManager_requestIntegrityToken(
    IntegrityTokenRequest* request, IntegrityTokenResponse** out_response);

/// Gets the status of an ongoing IntegrityManager_requestIntegrityToken()
/// asynchronous operation.
///
/// This function can be used to poll for the completion of a call to
/// IntegrityManager_requestIntegrityToken(). This function does not make any
/// JNI calls and can be called every frame.
///
/// @param response The IntegrityTokenResponse for which to get status.
/// @param out_status An out parameter for receiving the
/// IntegrityResponseStatus.
/// @return An IntegrityErrorCode indicating the error associated with the given
/// IntegrityTokenResponse.
IntegrityErrorCode IntegrityTokenResponse_getStatus(
    IntegrityTokenResponse* response, IntegrityResponseStatus* out_status);

/// Gets the token retrieved by a successful
/// IntegrityManager_requestIntegrityToken() asynchronous operation, returns
/// null pointer otherwise.
///
/// <p>The JSON payload is signed and encrypted as a nested JWT, that is <a
/// href="https://tools.ietf.org/html/rfc7516">JWE</a> of <a
/// href="https://tools.ietf.org/html/rfc7515">JWS</a>.
///
/// <p>JWE uses <a
/// href="https://tools.ietf.org/html/rfc7518#section-4.4">A256KW</a> as a key
/// wrapping algorithm and <a
/// href="https://tools.ietf.org/html/rfc7518#section-5.3">A256GCM</a> as a
/// content encryption algorithm. JWS uses <a
/// href="https://tools.ietf.org/html/rfc7518#section-3.4">ES256</a> as a
/// signing algorithm.
///
/// <p>All decryption and verification should be done within a secure server
/// environment. Do not decrypt or verify the received token from within the
/// client app. In particular, never expose any decryption keys to the client
/// app.
///
/// The string returned here is owned by the API, and the pointer will be valid
/// until the corresponding IntegrityTokenResponse is freed by calling
/// IntegrityTokenResponse_destroy().
/// @param response The IntegrityTokenResponse specifying the asynchronous
/// operation.
/// @return A token which contains the response for the integrity related
/// enquiries.
const char* IntegrityTokenResponse_getToken(IntegrityTokenResponse* response);

/// Creates a new IntegrityTokenRequest opaque struct.
/// @param out_request An out parameter for receiving the result.
/// @return An IntegrityErrorCode, which if not INTEGRITY_NO_ERROR indicates
/// that the out parameter shouldn't be used.
/// @see IntegrityTokenRequest_destroy
IntegrityErrorCode IntegrityTokenRequest_create(
    IntegrityTokenRequest** out_request);

/// Sets the nonce in an IntegrityTokenRequest with a given string.
///
/// <p>A nonce is a unique token which is ideally bound to the context (e.g.
/// hash of the user id and timestamp). The provided nonce will be a part of the
/// signed response token, which will allow you to compare it to the original
/// one and hence prevent replay attacks.
///
/// <p>Nonces should always be generated in a secure server environment. Do not
/// generate a nonce from within the client app.
///
/// <p>It must be Base64 encoded in web-safe no-wrap form.
///
/// @param request The IntegrityTokenRequest for which to set token.
/// @param nonce The nonce for the request.
/// @return An IntegrityErrorCode, which if not INTEGRITY_NO_ERROR indicates
/// that this operation has failed.
IntegrityErrorCode IntegrityTokenRequest_setNonce(
    IntegrityTokenRequest* request, const char* nonce);

/// Sets the cloud project number to link to the integrity token.
///
/// <p>This is an optional field and is meant to be used only for apps not
/// available on Google Play or by SDKs that include the Play Integrity API.
///
/// <p>Cloud project number is an automatically generated unique identifier for
/// your Google Cloud project. It can be found in Project info in your Google
/// Cloud Console for the cloud project where Play Integrity API is enabled.
///
/// @param request The IntegrityTokenRequest to set cloudProjectNumber.
/// @param cloudProjectNumber The cloudProjectNumber for the request.
/// @return An IntegrityErrorCode, which if not INTEGRITY_NO_ERROR indicates
/// that this operation has failed.
IntegrityErrorCode IntegrityTokenRequest_setCloudProjectNumber(
    IntegrityTokenRequest* request, int64_t cloudProjectNumber);

/// Releases the specified IntegrityTokenRequest and any references it holds.
/// @param request The request to free.
void IntegrityTokenRequest_destroy(IntegrityTokenRequest* request);

/// Releases the specified IntegrityTokenResponse and any references it holds.
/// @param response The response to free.
void IntegrityTokenResponse_destroy(IntegrityTokenResponse* response);

/// @}

#ifdef __cplusplus
};  // extern "C"
#endif

#endif  // PLAY_INTEGRITY_H_

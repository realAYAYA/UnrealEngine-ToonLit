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
  /// Integrity API is not enabled, or the Play Store version might be old.
  /// Recommended actions:
  /// * Make sure that Integrity API is enabled in Google Play Console.
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

  /// No Play Store account is found on device. Note that the Play Integrity API
  /// now supports unauthenticated requests. This error code is used only for
  /// older Play Store versions that lack support.
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

  /// The provided cloud project number is invalid.
  ///
  /// Use the cloud project number which can be found in Project info in your
  /// Google Cloud Console for the cloud project where Play Integrity API is
  /// enabled.
  INTEGRITY_CLOUD_PROJECT_NUMBER_IS_INVALID = -16,

  /// There was a transient error in the client device.
  ///
  /// Introduced in Play Core Java library version 1.1.0 (prior versions
  /// returned a token with empty Device Integrity Verdict). If the error
  /// persists after a few retries, you should assume that the device has failed
  /// integrity checks and act accordingly.
  INTEGRITY_CLIENT_TRANSIENT_ERROR = -17,

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

/// Errors that can be encountered while using the standard integrity API.
enum StandardIntegrityErrorCode {
  STANDARD_INTEGRITY_NO_ERROR = 0,

  /// Standard Integrity API is not available.
  ///
  /// Standard Integrity API is not enabled, or the Play Store version might be
  /// old. Recommended actions:
  /// * Make sure to be allowlisted to use Standard Integrity API.
  /// * Make sure that Integrity API is enabled in Google Play Console.
  /// * Ask the user to update Play Store.
  STANDARD_INTEGRITY_API_NOT_AVAILABLE = -1,

  /// No Play Store app is found on device or not official version is installed.
  ///
  /// Ask the user to install an official and recent version of Play Store.
  STANDARD_INTEGRITY_PLAY_STORE_NOT_FOUND = -2,

  /// No available network is found.
  ///
  /// Ask the user to check for a connection.
  STANDARD_INTEGRITY_NETWORK_ERROR = -3,

  /// The calling app is not installed.
  ///
  /// Something is wrong (possibly an attack). Non-actionable.
  STANDARD_INTEGRITY_APP_NOT_INSTALLED = -5,

  /// Play Services is not available or version is too old.
  ///
  /// Ask the user to Install or Update Play Services.
  STANDARD_INTEGRITY_PLAY_SERVICES_NOT_FOUND = -6,

  /// The calling app UID (user id) does not match the one from Package Manager.
  ///
  /// Something is wrong (possibly an attack). Non-actionable.
  STANDARD_INTEGRITY_APP_UID_MISMATCH = -7,

  /// The calling app is making too many requests to the API and hence is
  /// throttled.
  ///
  /// Retry with an exponential backoff.
  STANDARD_INTEGRITY_TOO_MANY_REQUESTS = -8,

  /// Binding to the service in the Play Store has failed. This can be due to
  /// having an old Play Store version installed on the device or device memory
  /// is overloaded.
  ///
  /// Ask the user to update Play Store.
  /// Retry with an exponential backoff.
  STANDARD_INTEGRITY_CANNOT_BIND_TO_SERVICE = -9,

  /// Unknown internal Google server error.
  ///
  /// Retry with an exponential backoff. Consider filing a bug if fails
  /// consistently.
  STANDARD_INTEGRITY_GOOGLE_SERVER_UNAVAILABLE = -12,

  /// The Play Store needs to be updated.
  ///
  /// Ask the user to update the Google Play Store.
  STANDARD_INTEGRITY_PLAY_STORE_VERSION_OUTDATED = -14,

  /// Play Services needs to be updated.
  ///
  /// Ask the user to update Google Play Services.
  STANDARD_INTEGRITY_PLAY_SERVICES_VERSION_OUTDATED = -15,

  /// The provided cloud project number is invalid.
  ///
  /// Use the cloud project number which can be found in Project info in your
  /// Google Cloud Console for the cloud project where Play Integrity API is
  /// enabled.
  CLOUD_PROJECT_NUMBER_IS_INVALID = -16,

  /// The provided request hash is too long. The request hash length must be
  /// less than 500 bytes.
  ///
  /// Retry with a shorter request hash.
  STANDARD_REQUEST_HASH_TOO_LONG = -17,

  /// There was a transient error in the client device.
  ///
  /// Retry with an exponential backoff.
  ///
  /// If the error persists after a few retries, you should assume that the
  /// device has failed integrity checks and act accordingly.
  STANDARD_CLIENT_TRANSIENT_ERROR = -18,

  /// The StandardIntegrityTokenProvider is invalid (e.g. it is outdated). This
  /// error can only be returned for StandardIntegrityTokenProvider_request().
  ///
  /// Request a new integrity token provider by calling
  /// StandardIntegrityManager_prepareIntegrityToken().
  STANDARD_INTEGRITY_TOKEN_PROVIDER_INVALID = -19,

  /// Unknown internal error.
  ///
  /// Retry with an exponential backoff. Consider filing a bug if fails
  /// consistently.
  STANDARD_INTEGRITY_INTERNAL_ERROR = -100,

  /// StandardIntegrityManager is not initialized.
  ///
  /// Call StandardIntegrityManager_init() first.
  STANDARD_INTEGRITY_INITIALIZATION_NEEDED = -101,

  /// There was an error initializing the Standard Integrity API.
  ///
  /// Retry with an exponential backoff. Consider filing a bug if fails
  /// consistently.
  STANDARD_INTEGRITY_INITIALIZATION_FAILED = -102,

  /// Invalid argument passed to the Standard Integrity API.
  ///
  /// Retry with correct argument.
  STANDARD_INTEGRITY_INVALID_ARGUMENT = -103,
};

/// Status returned when requesting integrity tokens/providers.
enum IntegrityResponseStatus {
  /// The response status is unknown.
  INTEGRITY_RESPONSE_UNKNOWN = 0,

  /// Waiting for asynchronous operation to finish.
  INTEGRITY_RESPONSE_PENDING = 1,

  /// The asynchronous operation has finished.
  INTEGRITY_RESPONSE_COMPLETED = 2,
};

/// An opaque struct used to provide information about an integrity token
/// request.
typedef struct IntegrityTokenRequest_ IntegrityTokenRequest;

/// An opaque struct used to access information about an integrity token
/// response.
typedef struct IntegrityTokenResponse_ IntegrityTokenResponse;

/// An opaque struct used to represent a prepare integrity token request.
typedef struct PrepareIntegrityTokenRequest_ PrepareIntegrityTokenRequest;

/// An opaque struct used to represent a standard integrity token provider.
typedef struct StandardIntegrityTokenProvider_ StandardIntegrityTokenProvider;

/// An opaque struct used to represent a standard integrity token request.
typedef struct StandardIntegrityTokenRequest_ StandardIntegrityTokenRequest;

/// An opaque struct used to represent a standard integrity token response.
typedef struct StandardIntegrityToken_ StandardIntegrityToken;

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
/// <p>See
/// https://developer.android.com/google/play/integrity/verdict#token-format.
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
/// <p>It must be base64 encoded in web-safe no-wrap form.
///
/// <p>See https://developer.android.com/google/play/integrity/verdict#nonce
/// for details about the nonce requirements and recommendations.
///
/// @param request The IntegrityTokenRequest for which to set token.
/// @param nonce The nonce for the request.
/// @return An IntegrityErrorCode, which if not INTEGRITY_NO_ERROR indicates
/// that this operation has failed.
IntegrityErrorCode IntegrityTokenRequest_setNonce(
    IntegrityTokenRequest* request, const char* nonce);

/// Sets the cloud project number to link to the integrity token.
///
/// <p>This field is required for <a
/// href="https://developer.android.com/google/play/integrity/setup#apps-exclusively-distributed-outside-google-play">
/// apps exclusively distributed outside of Google Play</a> and <a
/// href="https://developer.android.com/google/play/integrity/setup#sdks">SDKs</a>.
/// For apps distributed on Google Play, the cloud project number is configured
/// in the Play Console and need not be set on the request.
///
/// <p>Cloud project number can be found in Project info in your Google Cloud
/// Console for the cloud project where Play Integrity API is enabled.
///
/// <p>Calls to <a
/// href="https://developer.android.com/google/play/integrity/verdict#decrypt-verify-google-servers">
/// decrypt the token on Google's server</a> must be authenticated using the
/// cloud account that was linked to the token in this request.
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

/// Creates a new PrepareIntegrityTokenRequest opaque struct.
/// @param out_request An out parameter for receiving the result.
/// @return STANDARD_INTEGRITY_NO_ERROR if initialized successfully; any other
/// value indicates that an error has occurred and the out parameter shouldn't
/// be used.
/// @see PrepareIntegrityTokenRequest_destroy
StandardIntegrityErrorCode PrepareIntegrityTokenRequest_create(
    PrepareIntegrityTokenRequest** out_request);

/// Sets the given cloud project number in PrepareIntegrityTokenRequest.
/// This method should be called after initialising the request via
/// PrepareIntegrityTokenRequest_create method.
/// @param request A PrepareIntegrityTokenRequest for which to set the cloud
/// project number.
/// @param cloud_project_number The cloud project number.
/// @return STANDARD_INTEGRITY_NO_ERROR on success; any other
/// value indicates that an error has occurred due to invalid arguments.
StandardIntegrityErrorCode PrepareIntegrityTokenRequest_setCloudProjectNumber(
    PrepareIntegrityTokenRequest* request, int64_t cloud_project_number);

/// Releases the specified PrepareIntegrityTokenRequest and any references it
/// holds.
/// @param request A PrepareIntegrityTokenRequest to free.
void PrepareIntegrityTokenRequest_destroy(
    PrepareIntegrityTokenRequest* request);

/// Initialize the Standard Integrity Manager, making the other methods
/// available to call.
///
/// In case of failure, the Standard Integrity API is unavailable, and there
/// will be an error in logcat. The most common reason for failure is that the
/// PlayCore AAR is missing or some of its classes/methods weren't retained by
/// ProGuard.
/// @param jvm The app's single JavaVM. For example, from ANativeActivity's "vm"
/// field.
/// @param android_context An Android Context. For example, from
/// ANativeActivity's "clazz" field.
/// @return STANDARD_INTEGRITY_NO_ERROR if initialization succeeded, or an error
/// on failure.
/// @see StandardIntegrityManager_destroy
StandardIntegrityErrorCode StandardIntegrityManager_init(
    JavaVM* jvm, jobject android_context);

/// Asynchronously prepares the integrity token and makes it available for
/// requesting via StandardIntegrityTokenProvider. This method can be called
/// from time to time to refresh the resulting StandardIntegrityTokenProvider.
/// Note that this API makes a call to Google servers and hence
/// requires a network connection.
///
/// Note that the API is in beta mode.
///
/// @param request The request containing the cloud project number.
/// @param out_provider An out parameter for receiving the token provider.
/// @return STANDARD_INTEGRITY_NO_ERROR if request started successfully; any
/// other value indicates that an error has occurred and the out parameter
/// shouldn't be used.
/// @see StandardIntegrityTokenProvider_destroy
StandardIntegrityErrorCode StandardIntegrityManager_prepareIntegrityToken(
    PrepareIntegrityTokenRequest* request,
    StandardIntegrityTokenProvider** out_provider);

/// Frees up memory allocated for the Standard Integrity Manager. Does nothing
/// if StandardIntegrityManager_init() hasn't been called.
void StandardIntegrityManager_destroy();

/// Creates a new StandardIntegrityTokenRequest opaque struct.
/// @param out_request An out parameter for receiving the result.
/// @return STANDARD_INTEGRITY_NO_ERROR if initialized successfully; any other
/// value indicates that an error has occurred and the out parameter shouldn't
/// be used.
/// @see StandardIntegrityTokenRequest_destroy
StandardIntegrityErrorCode StandardIntegrityTokenRequest_create(
    StandardIntegrityTokenRequest** out_request);

/// Sets a request hash in a StandardIntegrityTokenRequest; the integrity token
/// will be bound to this request hash. It is recommended but not required.
/// @param request The StandardIntegrityTokenRequest for which to set the
/// request hash.
/// @param request_hash A request hash to bind the integrity token to.
/// string.
/// @return STANDARD_INTEGRITY_NO_ERROR on success; any other
/// value indicates that an error has occurred due to invalid arguments.
StandardIntegrityErrorCode StandardIntegrityTokenRequest_setRequestHash(
    StandardIntegrityTokenRequest* request, const char* request_hash);

/// Releases the specified StandardIntegrityTokenRequest and any references it
/// holds.
/// @param request A StandardIntegrityTokenRequest to free.
void StandardIntegrityTokenRequest_destroy(
    StandardIntegrityTokenRequest* request);

/// Gets the status of an ongoing
/// StandardIntegrityManager_prepareIntegrityToken() asynchronous operation.
/// @param provider The StandardIntegrityTokenProvider for which to get status.
/// @param out_status An out parameter for receiving the
/// IntegrityResponseStatus.
/// @return STANDARD_INTEGRITY_NO_ERROR if the request is successful or in
/// progress; any other value indicates an error that has occurred while
/// requesting token provider.
StandardIntegrityErrorCode StandardIntegrityTokenProvider_getStatus(
    StandardIntegrityTokenProvider* provider,
    IntegrityResponseStatus* out_status);

/// Asynchronously generates and returns a token for integrity-related
/// enquiries. This must be called after
/// StandardIntegrityManager_prepareIntegrityToken completes.
///
/// Note that the API is in beta mode.
///
/// @param provider A StandardIntegrityTokenProvider used to request token.
/// @param request A StandardIntegrityTokenRequest containing an optional
/// request hash.
/// @param out_token An out parameter for receiving the result.
/// @return STANDARD_INTEGRITY_NO_ERROR if the token request was initiated
/// successfully; any other value indicates that an error has occurred and the
/// out parameter shouldn't be used.
/// @see StandardIntegrityToken_destroy
StandardIntegrityErrorCode StandardIntegrityTokenProvider_request(
    StandardIntegrityTokenProvider* provider,
    StandardIntegrityTokenRequest* request, StandardIntegrityToken** out_token);

/// Releases the specified StandardIntegrityTokenProvider and any references it
/// holds.
/// @param provider The provider object to free.
void StandardIntegrityTokenProvider_destroy(
    StandardIntegrityTokenProvider* provider);

/// Gets the status of an ongoing StandardIntegrityTokenProvider_request
/// asynchronous operation.
/// @param token The StandardIntegrityToken for which to get status.
/// @param out_status An out parameter for receiving the
/// IntegrityResponseStatus.
/// @return STANDARD_INTEGRITY_NO_ERROR if the request is successful or in
/// progress; any other value indicates an error that has occurred while
/// requesting token.
StandardIntegrityErrorCode StandardIntegrityToken_getStatus(
    StandardIntegrityToken* token, IntegrityResponseStatus* out_status);

/// Gets the token retrieved by a completed
/// StandardIntegrityTokenProvider_request asynchronous operation, returns null
/// pointer otherwise. The string returned here is owned by the API, and the
/// pointer will be valid until the corresponding StandardIntegrityToken is
/// freed by calling StandardIntegrityToken_destroy.
/// @param token A StandardIntegrityToken response to retrieve token from.
/// @return A token which contains the response for the integrity related
/// enquiries.
const char* StandardIntegrityToken_getToken(StandardIntegrityToken* token);

/// Releases the specified StandardIntegrityToken and any references it holds.
/// @param token A StandardIntegrityToken response to free.
void StandardIntegrityToken_destroy(StandardIntegrityToken* token);

/// @}

#ifdef __cplusplus
};  // extern "C"
#endif

#endif  // PLAY_INTEGRITY_H_

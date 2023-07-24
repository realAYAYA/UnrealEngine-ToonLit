//
// AzureSpatialAnchors
// This file was auto-generated from SscApiModelDirect.cs.
//

#include <stdint.h>

#ifndef __SSC_HEADER__
#define __SSC_HEADER__

#ifdef __cplusplus
extern "C" {
#endif

// Handle types for per-platform objects.
#if __APPLE__
struct objc_object;
typedef struct objc_object *ssc_pal_object;
#elif _WIN32
struct IUnknown;
typedef struct IUnknown *ssc_pal_object;
#elif __ANDROID__
typedef void *ssc_pal_object;
#else
typedef void *ssc_pal_object;
#endif
typedef ssc_pal_object ssc_platform_anchor_handle;
typedef ssc_pal_object ssc_platform_frame_handle;
typedef ssc_pal_object ssc_platform_session_handle;

// Handle types.
struct ssc_object;
struct ssc_anchor_locate_criteria;
struct ssc_anchor_located_event_args;
struct ssc_cloud_spatial_anchor;
struct ssc_cloud_spatial_anchor_map;
struct ssc_cloud_spatial_anchor_session_deferral;
struct ssc_cloud_spatial_anchor_session_diagnostics;
struct ssc_cloud_spatial_anchor_session;
struct ssc_cloud_spatial_anchor_watcher;
struct ssc_geo_location;
struct ssc_idictionary_string_string;
struct ssc_ilist_string;
struct ssc_locate_anchors_completed_event_args;
struct ssc_near_anchor_criteria;
struct ssc_near_device_criteria;
struct ssc_on_log_debug_event_args;
struct ssc_platform_location_provider;
struct ssc_sensor_capabilities;
struct ssc_sensor_fingerprint_event_args;
struct ssc_session_configuration;
struct ssc_session_error_event_args;
struct ssc_session_status;
struct ssc_session_updated_event_args;
struct ssc_token_required_event_args;
typedef struct ssc_object *ssc_object_handle;
typedef struct ssc_anchor_locate_criteria *ssc_anchor_locate_criteria_handle;
typedef struct ssc_anchor_located_event_args *ssc_anchor_located_event_args_handle;
typedef struct ssc_cloud_spatial_anchor *ssc_cloud_spatial_anchor_handle;
typedef struct ssc_cloud_spatial_anchor_map *ssc_cloud_spatial_anchor_map_handle;
typedef struct ssc_cloud_spatial_anchor_session_deferral *ssc_cloud_spatial_anchor_session_deferral_handle;
typedef struct ssc_cloud_spatial_anchor_session_diagnostics *ssc_cloud_spatial_anchor_session_diagnostics_handle;
typedef struct ssc_cloud_spatial_anchor_session *ssc_cloud_spatial_anchor_session_handle;
typedef struct ssc_cloud_spatial_anchor_watcher *ssc_cloud_spatial_anchor_watcher_handle;
typedef struct ssc_geo_location *ssc_geo_location_handle;
typedef struct ssc_idictionary_string_string *ssc_idictionary_string_string_handle;
typedef struct ssc_ilist_string *ssc_ilist_string_handle;
typedef struct ssc_locate_anchors_completed_event_args *ssc_locate_anchors_completed_event_args_handle;
typedef struct ssc_near_anchor_criteria *ssc_near_anchor_criteria_handle;
typedef struct ssc_near_device_criteria *ssc_near_device_criteria_handle;
typedef struct ssc_on_log_debug_event_args *ssc_on_log_debug_event_args_handle;
typedef struct ssc_platform_location_provider *ssc_platform_location_provider_handle;
typedef struct ssc_sensor_capabilities *ssc_sensor_capabilities_handle;
typedef struct ssc_sensor_fingerprint_event_args *ssc_sensor_fingerprint_event_args_handle;
typedef struct ssc_session_configuration *ssc_session_configuration_handle;
typedef struct ssc_session_error_event_args *ssc_session_error_event_args_handle;
typedef struct ssc_session_status *ssc_session_status_handle;
typedef struct ssc_session_updated_event_args *ssc_session_updated_event_args_handle;
typedef struct ssc_token_required_event_args *ssc_token_required_event_args_handle;

// Typedefs.
typedef char ssc_bool;
typedef long long ssc_callback_cookie;
typedef unsigned char ssc_uint8;

// Enumerations.
/**
 * Use the data category values to determine what data is returned in an AnchorLocateCriteria object.
 */
typedef enum ssc_anchor_data_category_
{
    /**
     * No data is returned.
     */
    ssc_anchor_data_category_none = 0,
    /**
     * Returns Anchor properties including AppProperties.
     */
    ssc_anchor_data_category_properties = 1,
    /**
     * Returns spatial information about an Anchor.
     */
    ssc_anchor_data_category_spatial = 2
} ssc_anchor_data_category;

/**
 * Possible values returned when querying PlatformLocationProvider for Bluetooth capabilities
 */
typedef enum ssc_bluetooth_status_result_
{
    /**
     * Bluetooth beacons data is available.
     */
    ssc_bluetooth_status_result_available = 0,
    /**
     * Bluetooth was disabled in the SensorCapabilities.
     */
    ssc_bluetooth_status_result_disabled_capability = 1,
    /**
     * No sensor fingerprint provider has been created.
     */
    ssc_bluetooth_status_result_missing_sensor_fingerprint_provider = 2,
    /**
     * No bluetooth beacons have been found.
     */
    ssc_bluetooth_status_result_no_beacons_found = 3
} ssc_bluetooth_status_result;

/**
 * Identifies the source of an error in a cloud spatial session.
 */
typedef enum ssc_cloud_spatial_error_code_
{
    /**
     * Amount of Metadata exceeded the allowed limit (currently 4k).
     */
    ssc_cloud_spatial_error_code_metadata_too_large = 0,
    /**
     * Application did not provide valid credentials and therefore could not authenticate with the Cloud Service.
     */
    ssc_cloud_spatial_error_code_application_not_authenticated = 1,
    /**
     * Application did not provide any credentials for authorization with the Cloud Service.
     */
    ssc_cloud_spatial_error_code_application_not_authorized = 2,
    /**
     * Multiple stores (on the same device or different devices) made concurrent changes to the same Spatial Entity and so this particular change was rejected.
     */
    ssc_cloud_spatial_error_code_concurrency_violation = 3,
    /**
     * Not enough Neighborhood Spatial Data was available to complete the desired Create operation.
     */
    ssc_cloud_spatial_error_code_not_enough_spatial_data = 4,
    /**
     * No Spatial Location Hint was available (or it was not specific enough) to support rediscovery from the Cloud at a later time.
     */
    ssc_cloud_spatial_error_code_no_spatial_location_hint = 5,
    /**
     * Application cannot connect to the Cloud Service.
     */
    ssc_cloud_spatial_error_code_cannot_connect_to_server = 6,
    /**
     * Cloud Service returned an unspecified error.
     */
    ssc_cloud_spatial_error_code_server_error = 7,
    /**
     * The Spatial Entity has already been associated with a different Store object, so cannot be used with this current Store object.
     */
    ssc_cloud_spatial_error_code_already_associated_with_adifferent_store = 8,
    /**
     * SpatialEntity already exists in a Store but TryCreateAsync was called.
     */
    ssc_cloud_spatial_error_code_already_exists = 9,
    /**
     * A locate operation was requested, but the criteria does not specify anything to look for.
     */
    ssc_cloud_spatial_error_code_no_locate_criteria_specified = 10,
    /**
     * An access token was required but not specified; handle the TokenRequired event on the session to provide one.
     */
    ssc_cloud_spatial_error_code_no_access_token_specified = 11,
    /**
     * The session was unable to obtain an access token and so the creation could not proceed.
     */
    ssc_cloud_spatial_error_code_unable_to_obtain_access_token = 12,
    /**
     * There were too many requests made from this Account ID, so it is being throttled.
     */
    ssc_cloud_spatial_error_code_too_many_requests = 13,
    /**
     * The LocateCriteria options that were specified are not valid because they're missing a required value.
     */
    ssc_cloud_spatial_error_code_locate_criteria_missing_required_values = 14,
    /**
     * The LocateCriteria options that were specified are not valid because they're in conflict with settings for another mode.
     */
    ssc_cloud_spatial_error_code_locate_criteria_in_conflict = 15,
    /**
     * The LocateCriteria options that were specified are not valid values.
     */
    ssc_cloud_spatial_error_code_locate_criteria_invalid = 16,
    /**
     * The LocateCriteria options that were specified are not valid because they're not currently supported.
     */
    ssc_cloud_spatial_error_code_locate_criteria_not_supported = 17,
    /**
     * Encountered an unknown error on the session.
     */
    ssc_cloud_spatial_error_code_unknown = 19,
    /**
     * The Http request timed out.
     */
    ssc_cloud_spatial_error_code_http_timeout = 20
} ssc_cloud_spatial_error_code;

/**
 * Possible values returned when querying PlatformLocationProvider for GeoLocation capabilities
 */
typedef enum ssc_geo_location_status_result_
{
    /**
     * GeoLocation data is available.
     */
    ssc_geo_location_status_result_available = 0,
    /**
     * GeoLocation was disabled in the SensorCapabilities.
     */
    ssc_geo_location_status_result_disabled_capability = 1,
    /**
     * No sensor fingerprint provider has been created.
     */
    ssc_geo_location_status_result_missing_sensor_fingerprint_provider = 2,
    /**
     * No GPS data has been received.
     */
    ssc_geo_location_status_result_no_gpsdata = 3
} ssc_geo_location_status_result;

/**
 * Use this enumeration to determine whether an anchor was located, and the reason why it may have failed.
 */
typedef enum ssc_locate_anchor_status_
{
    /**
     * The anchor was already being tracked.
     */
    ssc_locate_anchor_status_already_tracked = 0,
    /**
     * The anchor was found.
     */
    ssc_locate_anchor_status_located = 1,
    /**
     * The anchor was not found.
     */
    ssc_locate_anchor_status_not_located = 2,
    /**
     * The anchor cannot be found - it was deleted or the identifier queried for was incorrect.
     */
    ssc_locate_anchor_status_not_located_anchor_does_not_exist = 3
} ssc_locate_anchor_status;

static const char* ssc_locate_anchor_status_Array[] =
{
    "ssc_locate_anchor_status_already_tracked",
    "ssc_locate_anchor_status_located",
    "ssc_locate_anchor_status_not_located",
    "ssc_locate_anchor_status_not_located_anchor_does_not_exist"
};

/**
 * Use this enumeration to indicate the method by which anchors can be located.
 */
typedef enum ssc_locate_strategy_
{
    /**
     * Indicates that any method is acceptable.
     */
    ssc_locate_strategy_any_strategy = 0,
    /**
     * Indicates that anchors will be located primarily by visual information.
     */
    ssc_locate_strategy_visual_information = 1,
    /**
     * Indicates that anchors will be located primarily by relationship to other anchors.
     */
    ssc_locate_strategy_relationship = 2
} ssc_locate_strategy;

/**
 * Defines logging severity levels.
 */
typedef enum ssc_session_log_level_
{
    /**
     * Specifies that logging should not write any messages.
     */
    ssc_session_log_level_none = 0,
    /**
     * Specifies logs that indicate when the current flow of execution stops due to a failure.
     */
    ssc_session_log_level_error = 1,
    /**
     * Specifies logs that highlight an abnormal or unexpected event, but do not otherwise cause execution to stop.
     */
    ssc_session_log_level_warning = 2,
    /**
     * Specifies logs that track the general flow.
     */
    ssc_session_log_level_information = 3,
    /**
     * Specifies logs used for interactive investigation during development.
     */
    ssc_session_log_level_debug = 4,
    /**
     * Specifies all messages should be logged.
     */
    ssc_session_log_level_all = 5
} ssc_session_log_level;

/**
 * Use this enumeration to describe the kind of feedback that can be provided to the user about data
 */
typedef enum ssc_session_user_feedback_
{
    /**
     * No specific feedback is available.
     */
    ssc_session_user_feedback_none = 0,
    /**
     * Device is not moving enough to create a neighborhood of key-frames.
     */
    ssc_session_user_feedback_not_enough_motion = 1,
    /**
     * Device is moving too quickly for stable tracking.
     */
    ssc_session_user_feedback_motion_too_quick = 2,
    /**
     * The environment doesn't have enough feature points for stable tracking.
     */
    ssc_session_user_feedback_not_enough_features = 4
} ssc_session_user_feedback;

typedef enum ssc_status_
{
    /**
     * Success
     */
    ssc_status_ok = 0,
    /**
     * Failed
     */
    ssc_status_failed = 1,
    /**
     * Cannot access a disposed object.
     */
    ssc_status_object_disposed = 2,
    /**
     * Out of memory.
     */
    ssc_status_out_of_memory = 12,
    /**
     * Invalid argument.
     */
    ssc_status_invalid_argument = 22,
    /**
     * The value is out of range.
     */
    ssc_status_out_of_range = 34,
    /**
     * Not implemented.
     */
    ssc_status_not_implemented = 38,
    /**
     * The key does not exist in the collection.
     */
    ssc_status_key_not_found = 77,
    /**
     * Amount of Metadata exceeded the allowed limit (currently 4k).
     */
    ssc_status_metadata_too_large = 78,
    /**
     * Application did not provide valid credentials and therefore could not authenticate with the Cloud Service.
     */
    ssc_status_application_not_authenticated = 79,
    /**
     * Application did not provide any credentials for authorization with the Cloud Service.
     */
    ssc_status_application_not_authorized = 80,
    /**
     * Multiple stores (on the same device or different devices) made concurrent changes to the same Spatial Entity and so this particular change was rejected.
     */
    ssc_status_concurrency_violation = 81,
    /**
     * Not enough Neighborhood Spatial Data was available to complete the desired Create operation.
     */
    ssc_status_not_enough_spatial_data = 82,
    /**
     * No Spatial Location Hint was available (or it was not specific enough) to support rediscovery from the Cloud at a later time.
     */
    ssc_status_no_spatial_location_hint = 83,
    /**
     * Application cannot connect to the Cloud Service.
     */
    ssc_status_cannot_connect_to_server = 84,
    /**
     * Cloud Service returned an unspecified error.
     */
    ssc_status_server_error = 85,
    /**
     * The Spatial Entity has already been associated with a different Store object, so cannot be used with this current Store object.
     */
    ssc_status_already_associated_with_adifferent_store = 86,
    /**
     * SpatialEntity already exists in a Store but TryCreateAsync was called.
     */
    ssc_status_already_exists = 87,
    /**
     * A locate operation was requested, but the criteria does not specify anything to look for.
     */
    ssc_status_no_locate_criteria_specified = 88,
    /**
     * An access token was required but not specified; handle the TokenRequired event on the session to provide one.
     */
    ssc_status_no_access_token_specified = 89,
    /**
     * The session was unable to obtain an access token and so the creation could not proceed.
     */
    ssc_status_unable_to_obtain_access_token = 90,
    /**
     * There were too many requests made from this Account ID, so it is being throttled.
     */
    ssc_status_too_many_requests = 91,
    /**
     * The LocateCriteria options that were specified are not valid because they're missing a required value.
     */
    ssc_status_locate_criteria_missing_required_values = 92,
    /**
     * The LocateCriteria options that were specified are not valid because they're in conflict with settings for another mode.
     */
    ssc_status_locate_criteria_in_conflict = 93,
    /**
     * The LocateCriteria options that were specified are not valid values.
     */
    ssc_status_locate_criteria_invalid = 94,
    /**
     * The LocateCriteria options that were specified are not valid because they're not currently supported.
     */
    ssc_status_locate_criteria_not_supported = 95,
    /**
     * Encountered an unknown error on the session.
     */
    ssc_status_unknown = 96,
    /**
     * The Http request timed out.
     */
    ssc_status_http_timeout = 97
} ssc_status;

/**
 * Possible values returned when querying PlatformLocationProvider for Wifi capabilities
 */
typedef enum ssc_wifi_status_result_
{
    /**
     * Wifi data is available.
     */
    ssc_wifi_status_result_available = 0,
    /**
     * Wifi was disabled in the SensorCapabilities.
     */
    ssc_wifi_status_result_disabled_capability = 1,
    /**
     * No sensor fingerprint provider has been created.
     */
    ssc_wifi_status_result_missing_sensor_fingerprint_provider = 2,
    /**
     * No Wifi access points have been found.
     */
    ssc_wifi_status_result_no_access_points_found = 3
} ssc_wifi_status_result;

// Callbacks.
typedef void (*ssc_anchor_located_delegate)(ssc_callback_cookie cookie, ssc_anchor_located_event_args_handle args);
typedef void (*ssc_locate_anchors_completed_delegate)(ssc_callback_cookie cookie, ssc_locate_anchors_completed_event_args_handle args);
typedef void (*ssc_on_log_debug_delegate)(ssc_callback_cookie cookie, ssc_on_log_debug_event_args_handle args);
typedef void (*ssc_session_error_delegate)(ssc_callback_cookie cookie, ssc_session_error_event_args_handle args);
typedef void (*ssc_session_updated_delegate)(ssc_callback_cookie cookie, ssc_session_updated_event_args_handle args);
typedef void (*ssc_token_required_delegate)(ssc_callback_cookie cookie, ssc_token_required_event_args_handle args);
typedef void (*ssc_updated_sensor_fingerprint_required_delegate)(ssc_callback_cookie cookie, ssc_sensor_fingerprint_event_args_handle args);

// Exported functions.
extern ssc_status ssc_anchor_locate_criteria_addref(ssc_anchor_locate_criteria_handle handle);
extern ssc_status ssc_anchor_locate_criteria_create(ssc_anchor_locate_criteria_handle* instance);
extern ssc_status ssc_anchor_locate_criteria_get_bypass_cache(ssc_anchor_locate_criteria_handle handle, ssc_bool* result);
extern ssc_status ssc_anchor_locate_criteria_get_identifiers(ssc_anchor_locate_criteria_handle handle, const char * ** result, int* result_count);
extern ssc_status ssc_anchor_locate_criteria_get_near_anchor(ssc_anchor_locate_criteria_handle handle, ssc_near_anchor_criteria_handle* result);
extern ssc_status ssc_anchor_locate_criteria_get_near_device(ssc_anchor_locate_criteria_handle handle, ssc_near_device_criteria_handle* result);
extern ssc_status ssc_anchor_locate_criteria_get_requested_categories(ssc_anchor_locate_criteria_handle handle, ssc_anchor_data_category* result);
extern ssc_status ssc_anchor_locate_criteria_get_strategy(ssc_anchor_locate_criteria_handle handle, ssc_locate_strategy* result);
extern ssc_status ssc_anchor_locate_criteria_release(ssc_anchor_locate_criteria_handle handle);
extern ssc_status ssc_anchor_locate_criteria_set_bypass_cache(ssc_anchor_locate_criteria_handle handle, ssc_bool value);
extern ssc_status ssc_anchor_locate_criteria_set_identifiers(ssc_anchor_locate_criteria_handle handle, const char * * value, int value_count);
extern ssc_status ssc_anchor_locate_criteria_set_near_anchor(ssc_anchor_locate_criteria_handle handle, ssc_near_anchor_criteria_handle value);
extern ssc_status ssc_anchor_locate_criteria_set_near_device(ssc_anchor_locate_criteria_handle handle, ssc_near_device_criteria_handle value);
extern ssc_status ssc_anchor_locate_criteria_set_requested_categories(ssc_anchor_locate_criteria_handle handle, ssc_anchor_data_category value);
extern ssc_status ssc_anchor_locate_criteria_set_strategy(ssc_anchor_locate_criteria_handle handle, ssc_locate_strategy value);
extern ssc_status ssc_anchor_located_event_args_addref(ssc_anchor_located_event_args_handle handle);
extern ssc_status ssc_anchor_located_event_args_get_anchor(ssc_anchor_located_event_args_handle handle, ssc_cloud_spatial_anchor_handle* result);
extern ssc_status ssc_anchor_located_event_args_get_identifier(ssc_anchor_located_event_args_handle handle, const char ** result);
extern ssc_status ssc_anchor_located_event_args_get_status(ssc_anchor_located_event_args_handle handle, ssc_locate_anchor_status* result);
extern ssc_status ssc_anchor_located_event_args_get_strategy(ssc_anchor_located_event_args_handle handle, ssc_locate_strategy* result);
extern ssc_status ssc_anchor_located_event_args_get_watcher(ssc_anchor_located_event_args_handle handle, ssc_cloud_spatial_anchor_watcher_handle* result);
extern ssc_status ssc_anchor_located_event_args_release(ssc_anchor_located_event_args_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_addref(ssc_cloud_spatial_anchor_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_create(ssc_cloud_spatial_anchor_handle* instance);
extern ssc_status ssc_cloud_spatial_anchor_get_app_properties(ssc_cloud_spatial_anchor_handle handle, ssc_idictionary_string_string_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_get_expiration(ssc_cloud_spatial_anchor_handle handle, int64_t* result);
extern ssc_status ssc_cloud_spatial_anchor_get_identifier(ssc_cloud_spatial_anchor_handle handle, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_get_local_anchor(ssc_cloud_spatial_anchor_handle handle, ssc_platform_anchor_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_get_version_tag(ssc_cloud_spatial_anchor_handle handle, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_map_addref(ssc_cloud_spatial_anchor_map_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_map_get_identifier(ssc_cloud_spatial_anchor_map_handle handle, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_map_get_name(ssc_cloud_spatial_anchor_map_handle handle, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_map_release(ssc_cloud_spatial_anchor_map_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_release(ssc_cloud_spatial_anchor_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_addref(ssc_cloud_spatial_anchor_session_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_create(ssc_cloud_spatial_anchor_session_handle* instance);
extern ssc_status ssc_cloud_spatial_anchor_session_create_anchor_async(ssc_cloud_spatial_anchor_session_handle handle, ssc_cloud_spatial_anchor_handle anchor);
extern ssc_status ssc_cloud_spatial_anchor_session_create_watcher(ssc_cloud_spatial_anchor_session_handle handle, ssc_anchor_locate_criteria_handle criteria, ssc_cloud_spatial_anchor_watcher_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_session_deferral_addref(ssc_cloud_spatial_anchor_session_deferral_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_deferral_complete(ssc_cloud_spatial_anchor_session_deferral_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_deferral_release(ssc_cloud_spatial_anchor_session_deferral_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_delete_anchor_async(ssc_cloud_spatial_anchor_session_handle handle, ssc_cloud_spatial_anchor_handle anchor);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_addref(ssc_cloud_spatial_anchor_session_diagnostics_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_create_manifest_async(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, const char * description, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_get_images_enabled(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, ssc_bool* result);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_get_log_directory(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_get_log_level(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, ssc_session_log_level* result);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_get_max_disk_size_in_mb(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, int* result);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_release(ssc_cloud_spatial_anchor_session_diagnostics_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_set_images_enabled(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, ssc_bool value);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_set_log_directory(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, const char * value);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_set_log_level(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, ssc_session_log_level value);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_set_max_disk_size_in_mb(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, int value);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_submit_manifest_async(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, const char * manifest_path);
extern ssc_status ssc_cloud_spatial_anchor_session_dispose(ssc_cloud_spatial_anchor_session_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_get_access_token_with_account_key_async(ssc_cloud_spatial_anchor_session_handle handle, const char * account_key, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_access_token_with_authentication_token_async(ssc_cloud_spatial_anchor_session_handle handle, const char * authentication_token, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_active_watchers(ssc_cloud_spatial_anchor_session_handle handle, ssc_cloud_spatial_anchor_watcher_handle ** result, int* result_count);
extern ssc_status ssc_cloud_spatial_anchor_session_get_active_watchers_count(ssc_cloud_spatial_anchor_session_handle handle, int* result_count);
extern ssc_status ssc_cloud_spatial_anchor_session_get_active_watchers_items(ssc_cloud_spatial_anchor_session_handle handle, ssc_cloud_spatial_anchor_watcher_handle* result_array, int* result_count);
extern ssc_status ssc_cloud_spatial_anchor_session_get_anchor_properties_async(ssc_cloud_spatial_anchor_session_handle handle, const char * identifier, ssc_cloud_spatial_anchor_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_configuration(ssc_cloud_spatial_anchor_session_handle handle, ssc_session_configuration_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_diagnostics(ssc_cloud_spatial_anchor_session_handle handle, ssc_cloud_spatial_anchor_session_diagnostics_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_location_provider(ssc_cloud_spatial_anchor_session_handle handle, ssc_platform_location_provider_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_log_level(ssc_cloud_spatial_anchor_session_handle handle, ssc_session_log_level* result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_middleware_versions(ssc_cloud_spatial_anchor_session_handle handle, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_nearby_anchor_ids_async(ssc_cloud_spatial_anchor_session_handle handle, ssc_near_device_criteria_handle criteria, ssc_ilist_string_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_sdk_package_type(ssc_cloud_spatial_anchor_session_handle handle, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_session(ssc_cloud_spatial_anchor_session_handle handle, ssc_platform_session_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_session_id(ssc_cloud_spatial_anchor_session_handle handle, const char ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_session_status_async(ssc_cloud_spatial_anchor_session_handle handle, ssc_session_status_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_session_process_frame(ssc_cloud_spatial_anchor_session_handle handle, ssc_platform_frame_handle frame);
extern ssc_status ssc_cloud_spatial_anchor_session_refresh_anchor_properties_async(ssc_cloud_spatial_anchor_session_handle handle, ssc_cloud_spatial_anchor_handle anchor);
extern ssc_status ssc_cloud_spatial_anchor_session_release(ssc_cloud_spatial_anchor_session_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_reset(ssc_cloud_spatial_anchor_session_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_set_anchor_located(ssc_cloud_spatial_anchor_session_handle handle, ssc_callback_cookie value, ssc_anchor_located_delegate value_fn);
extern ssc_status ssc_cloud_spatial_anchor_session_set_error(ssc_cloud_spatial_anchor_session_handle handle, ssc_callback_cookie value, ssc_session_error_delegate value_fn);
extern ssc_status ssc_cloud_spatial_anchor_session_set_locate_anchors_completed(ssc_cloud_spatial_anchor_session_handle handle, ssc_callback_cookie value, ssc_locate_anchors_completed_delegate value_fn);
extern ssc_status ssc_cloud_spatial_anchor_session_set_location_provider(ssc_cloud_spatial_anchor_session_handle handle, ssc_platform_location_provider_handle value);
extern ssc_status ssc_cloud_spatial_anchor_session_set_log_level(ssc_cloud_spatial_anchor_session_handle handle, ssc_session_log_level value);
extern ssc_status ssc_cloud_spatial_anchor_session_set_middleware_versions(ssc_cloud_spatial_anchor_session_handle handle, const char * value);
extern ssc_status ssc_cloud_spatial_anchor_session_set_on_log_debug(ssc_cloud_spatial_anchor_session_handle handle, ssc_callback_cookie value, ssc_on_log_debug_delegate value_fn);
extern ssc_status ssc_cloud_spatial_anchor_session_set_sdk_package_type(ssc_cloud_spatial_anchor_session_handle handle, const char * value);
extern ssc_status ssc_cloud_spatial_anchor_session_set_session(ssc_cloud_spatial_anchor_session_handle handle, ssc_platform_session_handle value);
extern ssc_status ssc_cloud_spatial_anchor_session_set_session_updated(ssc_cloud_spatial_anchor_session_handle handle, ssc_callback_cookie value, ssc_session_updated_delegate value_fn);
extern ssc_status ssc_cloud_spatial_anchor_session_set_token_required(ssc_cloud_spatial_anchor_session_handle handle, ssc_callback_cookie value, ssc_token_required_delegate value_fn);
extern ssc_status ssc_cloud_spatial_anchor_session_set_updated_sensor_fingerprint_required(ssc_cloud_spatial_anchor_session_handle handle, ssc_callback_cookie value, ssc_updated_sensor_fingerprint_required_delegate value_fn);
extern ssc_status ssc_cloud_spatial_anchor_session_start(ssc_cloud_spatial_anchor_session_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_stop(ssc_cloud_spatial_anchor_session_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_session_update_anchor_properties_async(ssc_cloud_spatial_anchor_session_handle handle, ssc_cloud_spatial_anchor_handle anchor);
extern ssc_status ssc_cloud_spatial_anchor_set_expiration(ssc_cloud_spatial_anchor_handle handle, int64_t value);
extern ssc_status ssc_cloud_spatial_anchor_set_local_anchor(ssc_cloud_spatial_anchor_handle handle, ssc_platform_anchor_handle value);
extern ssc_status ssc_cloud_spatial_anchor_watcher_addref(ssc_cloud_spatial_anchor_watcher_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_watcher_get_identifier(ssc_cloud_spatial_anchor_watcher_handle handle, int* result);
extern ssc_status ssc_cloud_spatial_anchor_watcher_release(ssc_cloud_spatial_anchor_watcher_handle handle);
extern ssc_status ssc_cloud_spatial_anchor_watcher_stop(ssc_cloud_spatial_anchor_watcher_handle handle);
extern ssc_status ssc_geo_location_addref(ssc_geo_location_handle handle);
extern ssc_status ssc_geo_location_create(ssc_geo_location_handle* instance);
extern ssc_status ssc_geo_location_get_altitude(ssc_geo_location_handle handle, float* result);
extern ssc_status ssc_geo_location_get_horizontal_error(ssc_geo_location_handle handle, float* result);
extern ssc_status ssc_geo_location_get_latitude(ssc_geo_location_handle handle, double* result);
extern ssc_status ssc_geo_location_get_longitude(ssc_geo_location_handle handle, double* result);
extern ssc_status ssc_geo_location_get_vertical_error(ssc_geo_location_handle handle, float* result);
extern ssc_status ssc_geo_location_release(ssc_geo_location_handle handle);
extern ssc_status ssc_geo_location_set_altitude(ssc_geo_location_handle handle, float value);
extern ssc_status ssc_geo_location_set_horizontal_error(ssc_geo_location_handle handle, float value);
extern ssc_status ssc_geo_location_set_latitude(ssc_geo_location_handle handle, double value);
extern ssc_status ssc_geo_location_set_longitude(ssc_geo_location_handle handle, double value);
extern ssc_status ssc_geo_location_set_vertical_error(ssc_geo_location_handle handle, float value);
extern ssc_status ssc_get_error_details(void * handle, const char ** result_message, const char ** result_requestCorrelationVector, const char ** result_responseCorrelationVector);
extern ssc_status ssc_idictionary_string_string_addref(ssc_idictionary_string_string_handle handle);
extern ssc_status ssc_idictionary_string_string_clear(ssc_idictionary_string_string_handle handle);
extern ssc_status ssc_idictionary_string_string_get_count(ssc_idictionary_string_string_handle handle, int* result);
extern ssc_status ssc_idictionary_string_string_get_item(ssc_idictionary_string_string_handle handle, const char * key, const char ** result);
extern ssc_status ssc_idictionary_string_string_get_key(ssc_idictionary_string_string_handle handle, int index, const char ** result);
extern ssc_status ssc_idictionary_string_string_release(ssc_idictionary_string_string_handle handle);
extern ssc_status ssc_idictionary_string_string_remove_key(ssc_idictionary_string_string_handle handle, const char * key);
extern ssc_status ssc_idictionary_string_string_set_item(ssc_idictionary_string_string_handle handle, const char * key, const char * value);
extern ssc_status ssc_ilist_string_addref(ssc_ilist_string_handle handle);
extern ssc_status ssc_ilist_string_get_count(ssc_ilist_string_handle handle, int* result);
extern ssc_status ssc_ilist_string_get_item(ssc_ilist_string_handle handle, int index, const char ** result);
extern ssc_status ssc_ilist_string_release(ssc_ilist_string_handle handle);
extern ssc_status ssc_ilist_string_remove_item(ssc_ilist_string_handle handle, int index);
extern ssc_status ssc_ilist_string_set_item(ssc_ilist_string_handle handle, int index, const char * value);
extern ssc_status ssc_locate_anchors_completed_event_args_addref(ssc_locate_anchors_completed_event_args_handle handle);
extern ssc_status ssc_locate_anchors_completed_event_args_get_cancelled(ssc_locate_anchors_completed_event_args_handle handle, ssc_bool* result);
extern ssc_status ssc_locate_anchors_completed_event_args_get_watcher(ssc_locate_anchors_completed_event_args_handle handle, ssc_cloud_spatial_anchor_watcher_handle* result);
extern ssc_status ssc_locate_anchors_completed_event_args_release(ssc_locate_anchors_completed_event_args_handle handle);
extern ssc_status ssc_near_anchor_criteria_addref(ssc_near_anchor_criteria_handle handle);
extern ssc_status ssc_near_anchor_criteria_create(ssc_near_anchor_criteria_handle* instance);
extern ssc_status ssc_near_anchor_criteria_get_distance_in_meters(ssc_near_anchor_criteria_handle handle, float* result);
extern ssc_status ssc_near_anchor_criteria_get_max_result_count(ssc_near_anchor_criteria_handle handle, int* result);
extern ssc_status ssc_near_anchor_criteria_get_source_anchor(ssc_near_anchor_criteria_handle handle, ssc_cloud_spatial_anchor_handle* result);
extern ssc_status ssc_near_anchor_criteria_release(ssc_near_anchor_criteria_handle handle);
extern ssc_status ssc_near_anchor_criteria_set_distance_in_meters(ssc_near_anchor_criteria_handle handle, float value);
extern ssc_status ssc_near_anchor_criteria_set_max_result_count(ssc_near_anchor_criteria_handle handle, int value);
extern ssc_status ssc_near_anchor_criteria_set_source_anchor(ssc_near_anchor_criteria_handle handle, ssc_cloud_spatial_anchor_handle value);
extern ssc_status ssc_near_device_criteria_addref(ssc_near_device_criteria_handle handle);
extern ssc_status ssc_near_device_criteria_create(ssc_near_device_criteria_handle* instance);
extern ssc_status ssc_near_device_criteria_get_distance_in_meters(ssc_near_device_criteria_handle handle, float* result);
extern ssc_status ssc_near_device_criteria_get_max_result_count(ssc_near_device_criteria_handle handle, int* result);
extern ssc_status ssc_near_device_criteria_release(ssc_near_device_criteria_handle handle);
extern ssc_status ssc_near_device_criteria_set_distance_in_meters(ssc_near_device_criteria_handle handle, float value);
extern ssc_status ssc_near_device_criteria_set_max_result_count(ssc_near_device_criteria_handle handle, int value);
extern ssc_status ssc_on_log_debug_event_args_addref(ssc_on_log_debug_event_args_handle handle);
extern ssc_status ssc_on_log_debug_event_args_get_message(ssc_on_log_debug_event_args_handle handle, const char ** result);
extern ssc_status ssc_on_log_debug_event_args_release(ssc_on_log_debug_event_args_handle handle);
extern ssc_status ssc_platform_location_provider_addref(ssc_platform_location_provider_handle handle);
extern ssc_status ssc_platform_location_provider_create(ssc_platform_location_provider_handle* instance);
extern ssc_status ssc_platform_location_provider_get_bluetooth_status(ssc_platform_location_provider_handle handle, ssc_bluetooth_status_result* result);
extern ssc_status ssc_platform_location_provider_get_geo_location_status(ssc_platform_location_provider_handle handle, ssc_geo_location_status_result* result);
extern ssc_status ssc_platform_location_provider_get_location_estimate(ssc_platform_location_provider_handle handle, ssc_geo_location_handle* result);
extern ssc_status ssc_platform_location_provider_get_sensors(ssc_platform_location_provider_handle handle, ssc_sensor_capabilities_handle* result);
extern ssc_status ssc_platform_location_provider_get_wifi_status(ssc_platform_location_provider_handle handle, ssc_wifi_status_result* result);
extern ssc_status ssc_platform_location_provider_release(ssc_platform_location_provider_handle handle);
extern ssc_status ssc_platform_location_provider_start(ssc_platform_location_provider_handle handle);
extern ssc_status ssc_platform_location_provider_stop(ssc_platform_location_provider_handle handle);
extern ssc_status ssc_sensor_capabilities_addref(ssc_sensor_capabilities_handle handle);
extern ssc_status ssc_sensor_capabilities_get_bluetooth_enabled(ssc_sensor_capabilities_handle handle, ssc_bool* result);
extern ssc_status ssc_sensor_capabilities_get_geo_location_enabled(ssc_sensor_capabilities_handle handle, ssc_bool* result);
extern ssc_status ssc_sensor_capabilities_get_known_beacon_proximity_uuids(ssc_sensor_capabilities_handle handle, const char * ** result, int* result_count);
extern ssc_status ssc_sensor_capabilities_get_wifi_enabled(ssc_sensor_capabilities_handle handle, ssc_bool* result);
extern ssc_status ssc_sensor_capabilities_release(ssc_sensor_capabilities_handle handle);
extern ssc_status ssc_sensor_capabilities_set_bluetooth_enabled(ssc_sensor_capabilities_handle handle, ssc_bool value);
extern ssc_status ssc_sensor_capabilities_set_geo_location_enabled(ssc_sensor_capabilities_handle handle, ssc_bool value);
extern ssc_status ssc_sensor_capabilities_set_known_beacon_proximity_uuids(ssc_sensor_capabilities_handle handle, const char * * value, int value_count);
extern ssc_status ssc_sensor_capabilities_set_wifi_enabled(ssc_sensor_capabilities_handle handle, ssc_bool value);
extern ssc_status ssc_sensor_fingerprint_event_args_addref(ssc_sensor_fingerprint_event_args_handle handle);
extern ssc_status ssc_sensor_fingerprint_event_args_get_geo_position(ssc_sensor_fingerprint_event_args_handle handle, ssc_geo_location_handle* result);
extern ssc_status ssc_sensor_fingerprint_event_args_release(ssc_sensor_fingerprint_event_args_handle handle);
extern ssc_status ssc_sensor_fingerprint_event_args_set_geo_position(ssc_sensor_fingerprint_event_args_handle handle, ssc_geo_location_handle value);
extern ssc_status ssc_session_configuration_addref(ssc_session_configuration_handle handle);
extern ssc_status ssc_session_configuration_get_access_token(ssc_session_configuration_handle handle, const char ** result);
extern ssc_status ssc_session_configuration_get_account_domain(ssc_session_configuration_handle handle, const char ** result);
extern ssc_status ssc_session_configuration_get_account_id(ssc_session_configuration_handle handle, const char ** result);
extern ssc_status ssc_session_configuration_get_account_key(ssc_session_configuration_handle handle, const char ** result);
extern ssc_status ssc_session_configuration_get_authentication_token(ssc_session_configuration_handle handle, const char ** result);
extern ssc_status ssc_session_configuration_release(ssc_session_configuration_handle handle);
extern ssc_status ssc_session_configuration_set_access_token(ssc_session_configuration_handle handle, const char * value);
extern ssc_status ssc_session_configuration_set_account_domain(ssc_session_configuration_handle handle, const char * value);
extern ssc_status ssc_session_configuration_set_account_id(ssc_session_configuration_handle handle, const char * value);
extern ssc_status ssc_session_configuration_set_account_key(ssc_session_configuration_handle handle, const char * value);
extern ssc_status ssc_session_configuration_set_authentication_token(ssc_session_configuration_handle handle, const char * value);
extern ssc_status ssc_session_error_event_args_addref(ssc_session_error_event_args_handle handle);
extern ssc_status ssc_session_error_event_args_get_error_code(ssc_session_error_event_args_handle handle, ssc_cloud_spatial_error_code* result);
extern ssc_status ssc_session_error_event_args_get_error_message(ssc_session_error_event_args_handle handle, const char ** result);
extern ssc_status ssc_session_error_event_args_get_watcher(ssc_session_error_event_args_handle handle, ssc_cloud_spatial_anchor_watcher_handle* result);
extern ssc_status ssc_session_error_event_args_release(ssc_session_error_event_args_handle handle);
extern ssc_status ssc_session_status_addref(ssc_session_status_handle handle);
extern ssc_status ssc_session_status_get_ready_for_create_progress(ssc_session_status_handle handle, float* result);
extern ssc_status ssc_session_status_get_recommended_for_create_progress(ssc_session_status_handle handle, float* result);
extern ssc_status ssc_session_status_get_session_create_hash(ssc_session_status_handle handle, int* result);
extern ssc_status ssc_session_status_get_session_locate_hash(ssc_session_status_handle handle, int* result);
extern ssc_status ssc_session_status_get_user_feedback(ssc_session_status_handle handle, ssc_session_user_feedback* result);
extern ssc_status ssc_session_status_release(ssc_session_status_handle handle);
extern ssc_status ssc_session_updated_event_args_addref(ssc_session_updated_event_args_handle handle);
extern ssc_status ssc_session_updated_event_args_get_status(ssc_session_updated_event_args_handle handle, ssc_session_status_handle* result);
extern ssc_status ssc_session_updated_event_args_release(ssc_session_updated_event_args_handle handle);
extern ssc_status ssc_token_required_event_args_addref(ssc_token_required_event_args_handle handle);
extern ssc_status ssc_token_required_event_args_get_access_token(ssc_token_required_event_args_handle handle, const char ** result);
extern ssc_status ssc_token_required_event_args_get_authentication_token(ssc_token_required_event_args_handle handle, const char ** result);
extern ssc_status ssc_token_required_event_args_get_deferral(ssc_token_required_event_args_handle handle, ssc_cloud_spatial_anchor_session_deferral_handle* result);
extern ssc_status ssc_token_required_event_args_release(ssc_token_required_event_args_handle handle);
extern ssc_status ssc_token_required_event_args_set_access_token(ssc_token_required_event_args_handle handle, const char * value);
extern ssc_status ssc_token_required_event_args_set_authentication_token(ssc_token_required_event_args_handle handle, const char * value);

// Flat string version of APIs.
extern ssc_status ssc_anchor_locate_criteria_get_identifiers_flat(ssc_anchor_locate_criteria_handle handle, const ssc_uint8 ** result, int* result_count);
extern ssc_status ssc_anchor_locate_criteria_set_identifiers_flat(ssc_anchor_locate_criteria_handle handle, const ssc_uint8 * value, int value_count);
extern ssc_status ssc_sensor_capabilities_get_known_beacon_proximity_uuids_flat(ssc_sensor_capabilities_handle handle, const ssc_uint8 ** result, int* result_count);
extern ssc_status ssc_sensor_capabilities_set_known_beacon_proximity_uuids_flat(ssc_sensor_capabilities_handle handle, const ssc_uint8 * value, int value_count);

#ifdef _UNICODE
extern ssc_status ssc_anchor_locate_criteria_get_identifiers_wide(ssc_anchor_locate_criteria_handle handle, const wchar_t * ** result, int* result_count);
extern ssc_status ssc_anchor_locate_criteria_get_identifiers_wide_flat(ssc_anchor_locate_criteria_handle handle, const ssc_uint8 ** result, int* result_count);
extern ssc_status ssc_anchor_locate_criteria_set_identifiers_wide(ssc_anchor_locate_criteria_handle handle, const wchar_t * * value, int value_count);
extern ssc_status ssc_anchor_locate_criteria_set_identifiers_wide_flat(ssc_anchor_locate_criteria_handle handle, const ssc_uint8 * value, int value_count);
extern ssc_status ssc_anchor_located_event_args_get_identifier_wide(ssc_anchor_located_event_args_handle handle, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_get_identifier_wide(ssc_cloud_spatial_anchor_handle handle, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_get_version_tag_wide(ssc_cloud_spatial_anchor_handle handle, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_map_get_identifier_wide(ssc_cloud_spatial_anchor_map_handle handle, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_map_get_name_wide(ssc_cloud_spatial_anchor_map_handle handle, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_create_manifest_async_wide(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, const wchar_t * description, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_get_log_directory_wide(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_set_log_directory_wide(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, const wchar_t * value);
extern ssc_status ssc_cloud_spatial_anchor_session_diagnostics_submit_manifest_async_wide(ssc_cloud_spatial_anchor_session_diagnostics_handle handle, const wchar_t * manifest_path);
extern ssc_status ssc_cloud_spatial_anchor_session_get_access_token_with_account_key_async_wide(ssc_cloud_spatial_anchor_session_handle handle, const wchar_t * account_key, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_access_token_with_authentication_token_async_wide(ssc_cloud_spatial_anchor_session_handle handle, const wchar_t * authentication_token, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_anchor_properties_async_wide(ssc_cloud_spatial_anchor_session_handle handle, const wchar_t * identifier, ssc_cloud_spatial_anchor_handle* result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_middleware_versions_wide(ssc_cloud_spatial_anchor_session_handle handle, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_sdk_package_type_wide(ssc_cloud_spatial_anchor_session_handle handle, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_get_session_id_wide(ssc_cloud_spatial_anchor_session_handle handle, const wchar_t ** result);
extern ssc_status ssc_cloud_spatial_anchor_session_set_middleware_versions_wide(ssc_cloud_spatial_anchor_session_handle handle, const wchar_t * value);
extern ssc_status ssc_cloud_spatial_anchor_session_set_sdk_package_type_wide(ssc_cloud_spatial_anchor_session_handle handle, const wchar_t * value);
extern ssc_status ssc_get_error_details_wide(void * handle, const wchar_t ** result_message, const wchar_t ** result_requestCorrelationVector, const wchar_t ** result_responseCorrelationVector);
extern ssc_status ssc_idictionary_string_string_get_item_wide(ssc_idictionary_string_string_handle handle, const wchar_t * key, const wchar_t ** result);
extern ssc_status ssc_idictionary_string_string_get_key_wide(ssc_idictionary_string_string_handle handle, int index, const wchar_t ** result);
extern ssc_status ssc_idictionary_string_string_remove_key_wide(ssc_idictionary_string_string_handle handle, const wchar_t * key);
extern ssc_status ssc_idictionary_string_string_set_item_wide(ssc_idictionary_string_string_handle handle, const wchar_t * key, const wchar_t * value);
extern ssc_status ssc_ilist_string_get_item_wide(ssc_ilist_string_handle handle, int index, const wchar_t ** result);
extern ssc_status ssc_ilist_string_set_item_wide(ssc_ilist_string_handle handle, int index, const wchar_t * value);
extern ssc_status ssc_on_log_debug_event_args_get_message_wide(ssc_on_log_debug_event_args_handle handle, const wchar_t ** result);
extern ssc_status ssc_sensor_capabilities_get_known_beacon_proximity_uuids_wide(ssc_sensor_capabilities_handle handle, const wchar_t * ** result, int* result_count);
extern ssc_status ssc_sensor_capabilities_get_known_beacon_proximity_uuids_wide_flat(ssc_sensor_capabilities_handle handle, const ssc_uint8 ** result, int* result_count);
extern ssc_status ssc_sensor_capabilities_set_known_beacon_proximity_uuids_wide(ssc_sensor_capabilities_handle handle, const wchar_t * * value, int value_count);
extern ssc_status ssc_sensor_capabilities_set_known_beacon_proximity_uuids_wide_flat(ssc_sensor_capabilities_handle handle, const ssc_uint8 * value, int value_count);
extern ssc_status ssc_session_configuration_get_access_token_wide(ssc_session_configuration_handle handle, const wchar_t ** result);
extern ssc_status ssc_session_configuration_get_account_domain_wide(ssc_session_configuration_handle handle, const wchar_t ** result);
extern ssc_status ssc_session_configuration_get_account_id_wide(ssc_session_configuration_handle handle, const wchar_t ** result);
extern ssc_status ssc_session_configuration_get_account_key_wide(ssc_session_configuration_handle handle, const wchar_t ** result);
extern ssc_status ssc_session_configuration_get_authentication_token_wide(ssc_session_configuration_handle handle, const wchar_t ** result);
extern ssc_status ssc_session_configuration_set_access_token_wide(ssc_session_configuration_handle handle, const wchar_t * value);
extern ssc_status ssc_session_configuration_set_account_domain_wide(ssc_session_configuration_handle handle, const wchar_t * value);
extern ssc_status ssc_session_configuration_set_account_id_wide(ssc_session_configuration_handle handle, const wchar_t * value);
extern ssc_status ssc_session_configuration_set_account_key_wide(ssc_session_configuration_handle handle, const wchar_t * value);
extern ssc_status ssc_session_configuration_set_authentication_token_wide(ssc_session_configuration_handle handle, const wchar_t * value);
extern ssc_status ssc_session_error_event_args_get_error_message_wide(ssc_session_error_event_args_handle handle, const wchar_t ** result);
extern ssc_status ssc_token_required_event_args_get_access_token_wide(ssc_token_required_event_args_handle handle, const wchar_t ** result);
extern ssc_status ssc_token_required_event_args_get_authentication_token_wide(ssc_token_required_event_args_handle handle, const wchar_t ** result);
extern ssc_status ssc_token_required_event_args_set_access_token_wide(ssc_token_required_event_args_handle handle, const wchar_t * value);
extern ssc_status ssc_token_required_event_args_set_authentication_token_wide(ssc_token_required_event_args_handle handle, const wchar_t * value);
#endif // _UNICODE

#ifdef __cplusplus
}
#endif

#endif // __SSC_HEADER__

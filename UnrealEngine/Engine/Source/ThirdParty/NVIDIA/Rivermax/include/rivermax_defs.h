/*
 * Copyright © 2023-2023 NVIDIA CORPORATION & AFFILIATES. ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of Nvidia Corporation and its affiliates
 * (the "Company") and all right, title, and interest in and to the software
 * product, including all associated intellectual property rights, are and
 * shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 */
#ifndef SRC_RIVERMAX_DEFS_H_
#define SRC_RIVERMAX_DEFS_H_

#ifdef __linux__
#include <netinet/in.h>
#ifndef __export
#define __export
#endif
#define OVERLAPPED void
#else
#include <winsock2.h>
#ifndef __export
#ifdef _USRDLL
#define __export __declspec(dllexport)
#else
#define __export __declspec(dllimport)
#endif
#endif
#endif

#if __cplusplus < 201103L
#include <stdint.h>
#else
#include <cinttypes>
#endif
#include <stddef.h>
#include <stdint.h>

/** Deprecation decorator */
#if defined(__DOXYGEN_ONLY__)
#define __RMX_DEPRECATED
#elif defined(_MSC_VER)
#define __RMX_DEPRECATED __declspec(deprecated)
#elif defined(__GNUC__)
#define __RMX_DEPRECATED __attribute__((deprecated))
#else
#define __RMX_DEPRECATED
#endif

/** Inline-enforcing decorator */
#if defined(_MSC_VER)
#define __RMX_INLINE __forceinline
#elif defined(__GNUC__) && __has_attribute(always_inline)
#define __RMX_INLINE inline __attribute__((always_inline))
#else
#define __RMX_INLINE static inline
#endif

/** When maximal amount is set */
#define RMX_MAX_SUB_BLOCKS_PER_MEM_BLOCK 2

/** Supported maximal amount of duplicate streams (SMPTE 2022-7) */
#define RMX_MAX_DUP_STREAMS 2

#define RMX_CEILING(_value_,_significance_) \
    (((_value_) + (_significance_) - 1)/(_significance_))

#define RMX_PLACEHOLDER_ALIGNED(_size_)    \
    void *placeholder[RMX_CEILING(_size_, sizeof(void*))]

/** Current library version */
#define RMX_VERSION_MAJOR  1
#define RMX_VERSION_MINOR  31
#define RMX_VERSION_PATCH  10

/** @brief Status codes */
typedef enum {

    /** Operation completed successfully */
    RMX_OK                             =   0,

    /* Functional error codes */

    /**@{*/
    /** Functional error */
    RMX_UNKNOWN_ISSUE                  =  20,
    RMX_NO_HW_RESOURCES                =  21,
    RMX_NO_FREE_CHUNK                  =  22,
    RMX_NO_CHUNK_TO_SEND               =  23,
    RMX_HW_SEND_QUEUE_IS_FULL          =  24,
    RMX_NO_MEMORY                      =  25,
    RMX_NOT_INITIALIZED                =  26,
    RMX_NO_DEVICE                      =  28,
    RMX_BUSY                           =  29,
    RMX_CANCELLED                      =  30,
    RMX_HW_COMPLETION_ISSUE            =  31,
    RMX_LICENSE_ISSUE                  =  32,
    RMX_NO_ATTACH                      =  34,
    RMX_STEERING_ISSUE                 =  35,
    RMX_CHECKSUM_ISSUE                 =  37,
    RMX_DESTINATION_UNREACHABLE        =  38,
    RMX_MEMORY_REGISTRATION            =  39,
    /**@}*/

    /* Environment/system error codes */

    /** Missing driver or underlying application */
    RMX_NO_DEPENDENCY                  = 100,

    /* Performance related error codes */

    /** For example, if exceeds link rate limit */
    RMX_EXCEEDS_LIMIT                  = 200,

    /* Capability error codes */

    /** Not supported by Rivermax */
    RMX_UNSUPPORTED                     = 300,
    /** Clock type not supported by the device in use */
    RMX_CLOCK_TYPE_NOT_SUPPORTED        = 301,
    /** Device doesn't support PTP real time clock */
    RMX_UNSUPPORTED_PTP_RT_CLOCK_DEVICE = 302,
    /** Underlying functionality is not implemented */
    RMX_NOT_IMPLEMENTED                 = 303,
    /** Not supported method was called */
    RMX_METHOD_NOT_SUPPORTED            = 304,

    /* Bad input codes */

    /**@{*/
    /** Invalid function parameter */
    RMX_INVALID_PARAM_MIX              = 350,
    RMX_INVALID_PARAM_1                = 351,
    RMX_INVALID_PARAM_2                = 352,
    RMX_INVALID_PARAM_3                = 353,
    RMX_INVALID_PARAM_4                = 354,
    RMX_INVALID_PARAM_5                = 355,
    RMX_INVALID_PARAM_6                = 356,
    RMX_INVALID_PARAM_7                = 357,
    RMX_INVALID_PARAM_8                = 358,
    RMX_INVALID_PARAM_9                = 359,
    RMX_INVALID_PARAM_10               = 360,
    /**@}*/

    /* System-related status codes */

    /** Interruption signal was captured by Rivermax */
    RMX_SIGNAL                         = 500,

} rmx_status;

/**
 * @brief Metadata for API attributes
 * @warning Metadata structure should not be used directly for
 *          backward-compatibility reasons
 */
typedef struct rmx_attribs_metadata_v1 {
    uint64_t bitmap;
} rmx_attribs_metadata;

/**
 * @defgroup RivermaxVer Version
 * @{
 */

/**
 * @brief Core version of the library */
typedef struct rmx_version_v1 {
    uint32_t major; /**< MAJOR changes in library, attention required */
    uint32_t minor; /**< MINOR fully compatible changes in library */
    uint32_t patch; /**< PATCH-level, non-functional changes: bugfixes, etc. */
} rmx_version;

/**@}*/

/**
 * @defgroup RivermaxInit Library Initialization
 * @{
 * @}
 */

/**
 * @defgroup RivermaxDevice Device
 * @{ */

/**
 * @struct rmx_device_list
 * @brief A list of physical devices */
typedef struct rmx_device_list_v1 rmx_device_list;

/**
 * @struct rmx_device
 * @brief A physical device descriptor (including virtual devices) */
typedef struct rmx_device_v1 rmx_device;

/**@}*/

/**
 * @defgroup RivermaxDeviceInterface Device Interface
 * @{ */

/**
 * @brief A device-interface descriptor
 */
typedef struct rmx_ip_addr_v1 {
    uint16_t family;
    union {
        struct in_addr ipv4;
        uint8_t reserved[16];
    } addr;
} rmx_ip_addr;

/**
 * @brief A device-interface descriptor */
typedef struct rmx_device_iface_v1 {
    RMX_PLACEHOLDER_ALIGNED(24);
} rmx_device_iface;

/**
 * @brief Device capability types
 * @memberof rmx_device_capabilities
 */
typedef enum {
    /** Real Time Clock (RTC) */
    RMX_DEVICE_CAP_PTP_CLOCK                           = 0,
    /** Ordered placement of incoming packets according to RTP sequence number */
    RMX_DEVICE_CAP_RTP_SEQN_PLACEMENT_ORDER            = 1,
    /** Ordered placement of incoming packets according to RTP extended sequence number */
    RMX_DEVICE_CAP_RTP_EXTEND_SEQN_PLACEMENT_ORDER     = 2,
    /** RTP dynamic header data split */
    RMX_DEVICE_CAP_RTP_DYNAMIC_HDS                     = 3,

    RMX_DEVICE_CAP_TOTAL_COUNT
} rmx_device_capability;

/**
 * @brief User device capabilities
 * @implements rmx_attribs_metadata
 */
typedef struct rmx_device_capabilities_v1 {
    RMX_PLACEHOLDER_ALIGNED(8);
} rmx_device_capabilities;

/**
 * @brief Rivermax device configuration attributes.
 * @memberof rmx_device_config
 */
typedef enum {

    /** RTP dynamic header data split for SMPTE-2110-20 protocol.
     *
     * When set, Rivermax will be able to receive packets with RTP application
     * header for SMPTE-2110-20 protocol with varied number of SRDs when doing
     * header data split.
     *
     * @note RTP dynamic HDS for SMPTE-2110-20 protocol supported
     *       when #RMX_DEVICE_CAP_RTP_DYNAMIC_HDS is supported.
     */
    RMX_DEVICE_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS    = 0,

    /** RTP dynamic header data split for dynamic number of RTP CSRC fields.
     *
     * When set, Rivermax will be able to receive packets with RTP application
     * header with varied number of RTP CSRC fields when doing header data
     * split.
     *
     * @note RTP dynamic HDS for dynamic number of CSRC fields in RTP header
     *       supported, when #RMX_DEVICE_CAP_RTP_DYNAMIC_HDS is supported.
     * @note This option doesn't stand alone but is supported only in
     *       conjunction with #RMX_DEVICE_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS
     *       and doesn't stand alone.
     */
    RMX_DEVICE_CONFIG_RTP_CSRC_FIELDS_DYNAMIC_HDS      = 1,
} rmx_device_config_attribute;

/**
 * @brief User device configuration
 * @implements rmx_attribs_metadata
 */
typedef struct rmx_device_config_v1 {
    RMX_PLACEHOLDER_ALIGNED(8);
} rmx_device_config;

/**@}*/

/**
 * @defgroup RivermaxClock Clock and Time
 * @{
 */

/**
 * @brief Supported type-types defined by the source and the units
 */
typedef enum {
    RMX_TIME_PTP           = 0,
    RMX_TIME_RAW_NANO      = 1,
    RMX_TIME_RAW_CYCLES    = 2,
} rmx_time_type;

/**
 * @brief User clock configuration */
typedef struct rmx_user_clock_params_v1 {
    RMX_PLACEHOLDER_ALIGNED(24);
} rmx_user_clock_params;

/**
 * @brief PTP clock configuration */
typedef struct rmx_ptp_clock_params_v1 {
    RMX_PLACEHOLDER_ALIGNED(24);
} rmx_ptp_clock_params;

/**
 * @typedef rmx_user_clock_handler
 * @brief User clock-handler */
typedef uint64_t (*rmx_user_clock_handler)(void*);

/**@}*/

/**
 * @defgroup RivermaxNotifications Notifications
 * @{
 */

/**
 * @brief Notification descriptor */
typedef struct rmx_notification_v1 {
    RMX_PLACEHOLDER_ALIGNED(12);
} rmx_notification_params;

/**
 * @brief Event Channel descriptor */
typedef struct rmx_event_channel_params_v1 {
    RMX_PLACEHOLDER_ALIGNED(12);
} rmx_event_channel_params;

/**@}*/

/**
 * @defgroup RegisterMemory Memory Registration
 * @{
 */

/** @brief An MKey preserved value to indicate non-registered memory */
#define RMX_MKEY_INVALID ((rmx_mkey_id)(-1L))

/**
 * @typedef rmx_mkey_id
 * @brief A memory registration key Id */
typedef uint32_t rmx_mkey_id;

/**
 * @typedef rmx_stream_id
 * @brief A stream Id */
typedef uint32_t rmx_stream_id;

/**
 * @brief Memory registration options
 * @memberof rmx_mem_reg_params
 */
typedef enum {
    RMX_MEM_REGISTRY_ZERO_BASED    = 0,
} rmx_mem_reg_params_option;

/**
 * @brief A memory region descriptor */
typedef struct rmx_mem_region_v1 {
    void *addr;
    size_t length;
    rmx_mkey_id mkey;
} rmx_mem_region;

/**
 * @brief A memory region descriptor with multiple keys */
typedef struct rmx_mem_multi_key_region_v1 {
    void *addr;
    size_t length;
    rmx_mkey_id mkey[RMX_MAX_DUP_STREAMS];
} rmx_mem_multi_key_region;

/**
 * @brief A memory registration descriptor */
typedef struct rmx_mem_reg_params_v1 {
    RMX_PLACEHOLDER_ALIGNED(40);
} rmx_mem_reg_params;

/**@}*/

/**
 * @defgroup InputStream Input Stream
 * @{
 */

/**
 * @brief Rivermax Input Stream types
 * @memberof rmx_input_stream_params
 */
typedef enum {
    /** Access to full raw packets, including the network headers */
    RMX_INPUT_RAW_PACKET            = 0,
    /** Access to the contents of the L4 network layer */
    RMX_INPUT_APP_PROTOCOL_PACKET   = 1,
    /** Access to the pure data payload, without application headers
     * @note This option is not supported yet. */
    RMX_INPUT_APP_PROTOCOL_PAYLOAD  = 2,

    RMX_INPUT_STREAM_TYPES_TOTAL
} rmx_input_stream_params_type;

/**
 * @brief Rivermax receive stream options
 * @memberof rmx_input_stream_params
 */
typedef enum {
    /**
     * When set, Input Stream will locate the incoming
     * packets according to RTP sequence number */
    RMX_INPUT_STREAM_RTP_SEQN_PLACEMENT_ORDER       = 0,

    /**
     * When set, Input Stream will locate the  incoming
     * packets according to RTP extended sequence number */
    RMX_INPUT_STREAM_RTP_EXT_SEQN_PLACEMENT_ORDER   = 1,

    /**
     * When set, Rivermax will be able to receive packets with RTP application
     * header for SMPTE-2110-20 protocol with varied number of SRDs when doing
     * header data split.
     *
     * @note The associated device shall be configured
     *       with #RMX_DEVICE_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS */
    RMX_INPUT_STREAM_RTP_SMPTE_2110_20_DYNAMIC_HDS  = 4,

    /**
     * When set, Rivermax will be able to receive packets with RTP application
     * header with varied number of CSRC fields when doing header data split.
     *
     * @note The associated device shall be configured
     *       with #RMX_DEVICE_CONFIG_RTP_CSRC_FIELDS_DYNAMIC_HDS */
    RMX_INPUT_STREAM_RTP_CSRC_FIELDS_DYNAMIC_HDS    = 5,

    /**
     * When set, Rivermax returns within @ref rmx_input_completion an array
     * with a @ref rmx_input_packet_info "packet information" entry per
     * each received packet.
     */
    RMX_INPUT_STREAM_CREATE_INFO_PER_PACKET         = 7,

    RMX_INPUT_STREAM_CREATE_OPTIONS_TOTAL
} rmx_input_option;

/**
 * @brief Timestamp format supported by an Input Stream
 * @memberof rmx_input_stream_params
 */
typedef enum {
    /** Raw number of the HW clock written upon packet's arrival */
    RMX_INPUT_TIMESTAMP_RAW_COUNTER = 0,
    /** The same as #RMX_INPUT_TIMESTAMP_RAW_COUNTER, but converted to nanoseconds */
    RMX_INPUT_TIMESTAMP_RAW_NANO    = 1,
    /** A timestamp in nsec synched to PTP, registered upon packet's arrival */
    RMX_INPUT_TIMESTAMP_SYNCED      = 2,
} rmx_input_timestamp_format;

/**
 * @brief A descriptor for an Input-Stream builder
 */
typedef struct rmx_input_stream_params_v1 {
    RMX_PLACEHOLDER_ALIGNED(184);
} rmx_input_stream_params;

/**
 * @brief An Input-Stream flow descriptor
 * @memberof rmx_input_stream_params
 */
typedef struct rmx_input_flow_v1 {
    RMX_PLACEHOLDER_ALIGNED(64);
} rmx_input_flow;

/**
 * @brief A chunk handle for an Input-Stream
 * @memberof rmx_input_stream_params
 */
typedef struct rmx_input_chunk_handle_v1 {
    RMX_PLACEHOLDER_ALIGNED(16);
} rmx_input_chunk_handle;

/**
 * @brief An Input-Stream completion flag
 * @memberof rmx_input_completion_metadata
 */
typedef enum {
    /** Indicates there are more ready pending packets */
    RMX_INPUT_COMPLETION_FLAG_MORE  = 0,
} rmx_input_completion_flag;

/**
 * @brief Metadata for an Input-Stream chunk-completion
 * @warning Metadata structure should not be used directly for
 *          backward-compatibility reasons
 */
typedef struct rmx_input_completion_metadata_v1 {
    uint32_t chunk_size;
    uint32_t seqn_first;
    uint32_t flags;
    uint32_t _reserved;
    uint64_t timestamp_first;
    uint64_t timestamp_last;
    const void *ptr[RMX_MAX_SUB_BLOCKS_PER_MEM_BLOCK];
} rmx_input_completion_metadata;

/**
 * @brief An Input-Stream chunk completion details
 * @implements rmx_input_completion_metadata
 */
typedef struct rmx_input_completion_v1 {
    RMX_PLACEHOLDER_ALIGNED(sizeof(struct rmx_input_completion_metadata_v1));
} rmx_input_completion;

/**
 * @brief Metadata for an Input-Stream packet-info
 * @warning Metadata structure should not be used directly for
 *          backward-compatibility reasons
 */
typedef struct rmx_input_packet_info_metadata_v1 {
    uint16_t size[RMX_MAX_SUB_BLOCKS_PER_MEM_BLOCK];
    uint32_t flow_tag;
    uint64_t timestamp;
} rmx_input_packet_info_metadata;

/**
 * @brief A packet info of an Input-Stream chunk
 * @implements rmx_input_packet_info_metadata
 */
typedef struct rmx_input_packet_info_v1 {
    RMX_PLACEHOLDER_ALIGNED(sizeof(struct rmx_input_packet_info_metadata_v1));
} rmx_input_packet_info;

/**@}*/

/**
 * @defgroup MediaOutputStream Media Output-Stream
 * @{
 */

/**
 * @brief A descriptor for a Media Output-Stream builder */
typedef struct rmx_output_media_stream_params_v1 {
    RMX_PLACEHOLDER_ALIGNED(96);
} rmx_output_media_stream_params;

/**
 * @brief A descriptor for a memory block of a Media Output-Stream
 * @memberof rmx_output_media_stream_params
 */
typedef struct rmx_output_media_mem_block_v1 {
    RMX_PLACEHOLDER_ALIGNED(96);
} rmx_output_media_mem_block;

/**
 * @brief A descriptor for a specific context in a Media Output-Stream
 * @memberof rmx_output_media_stream_params
 */
typedef struct rmx_output_media_context_v1 {
    RMX_PLACEHOLDER_ALIGNED(24);
} rmx_output_media_context;

/**
 * @brief Metadata of a Media Output-Stream chunk handle
 * @warning Metadata structure should not be used directly for
 *          backward-compatibility reasons
 */
typedef struct rmx_output_media_chunk_handle_metadata_v1 {
    void *strides[RMX_MAX_SUB_BLOCKS_PER_MEM_BLOCK];
    uint16_t *packet_sizes[RMX_MAX_SUB_BLOCKS_PER_MEM_BLOCK];
    uint64_t flags;
} rmx_output_media_chunk_handle_metadata;

/**
 * @brief A descriptor of a chunk handle for a Media Output-Stream
 *
 * This handle is designed both to acquire a chunk
 * via @ref rmx_output_media_get_next_chunk and to commit it to the wire
 * via @ref rmx_output_media_commit_chunk. It maintains a cursor for each
 * of this APIs - one cursor for the last acquired chunk, and another
 * for the next chunk to commit.
 * @note This object can be reconstructed each time. However, it's highly
 *       recommended to reuse it for the same stream to reduce CPU cycles.
 * @implements rmx_output_media_chunk_handle_metadata
 */
typedef struct rmx_output_media_chunk_handle_v1 {
    RMX_PLACEHOLDER_ALIGNED(72);
} rmx_output_media_chunk_handle;

/**@}*/

/**
 * @defgroup GenericOutputStream Generic Output-Stream
 * @{
 */

/**
 * @brief A rate descriptor for a Generic Output-Stream
 * @memberof rmx_output_gen_stream_params
 */
typedef struct rmx_output_gen_rate_v1 {
    RMX_PLACEHOLDER_ALIGNED(32);
} rmx_output_gen_rate;

/**
 * @brief A descriptor for a Generic Output-Stream builder */
typedef struct rmx_output_gen_stream_params_v1 {
    RMX_PLACEHOLDER_ALIGNED(96);
} rmx_output_gen_stream_params;

/**
 * @brief Metadata of a chunk handle for a Generic Output-Stream
 * @warning Metadata structure should not be used directly for
 *          backward-compatibility reasons
 */
typedef struct rmx_output_gen_chunk_handle_metadata_v1 {
    uint64_t flags;
} rmx_output_gen_chunk_handle_metadata;

/**
 * @brief A chunk descriptor for a Generic Output-Stream
 * @implements rmx_output_gen_chunk_handle_metadata
 */
typedef struct rmx_output_gen_chunk_handle_v1 {
    RMX_PLACEHOLDER_ALIGNED(64);
} rmx_output_gen_chunk_handle;

/**@}*/

/**
 * @defgroup AllOutputStreams_Common Output Streams Common API
 * @{
 */

/** @brief Commit options supported by Output Streams */
typedef enum {
    /**
     * If not set, the time provided in the function call represents the PTP
     * time-stamp (TAI time) in nanoseconds of the time point to send the
     * first packet of the next frame or field (depending if the scan is
     * progressive or not).
     * If set, the time provided in the function call represents for how
     * much time, in nanoseconds, the transmission of the chunk will be delayed.
     */
    RMX_OUTPUT_DELTA_TIME            = 0,

    /**
     * If set, the hardware stops the stream transmission after the current
     * chunk gets sent.
     * Transmission can be resumed at any time by committing an new chunk.
     */
    RMX_OUTPUT_PAUSE_AFTER_COMMIT    = 1,
} rmx_output_commit_option;

/**@}*/

/** @cond VERSION_MAPPING */
#define rmx_cleanup                                    rmx_cleanup_v1
#define rmx_get_version_numbers                        rmx_get_version_numbers_v1
#define rmx_get_version_string                         rmx_get_version_string_v1
#define rmx_set_cpu_affinity                           rmx_set_cpu_affinity_v1
#define rmx_enable_system_signal_handling              rmx_enable_system_signal_handling_v1
#define rmx_get_device_list                            rmx_get_device_list_v1
#define rmx_free_device_list                           rmx_free_device_list_v1
#define rmx_get_device_count                           rmx_get_device_count_v1
#define rmx_get_device                                 rmx_get_device_v1
#define rmx_get_device_interface_name                  rmx_get_device_interface_name_v1
#define rmx_get_device_ip_count                        rmx_get_device_ip_count_v1
#define rmx_get_device_ip_address                      rmx_get_device_ip_address_v1
#define rmx_get_device_mac_address                     rmx_get_device_mac_address_v1
#define rmx_get_device_id                              rmx_get_device_id_v1
#define rmx_get_device_serial_number                   rmx_get_device_serial_number_v1
#define rmx_retrieve_device_iface                      rmx_retrieve_device_iface_v1
#define rmx_clear_device_capabilities_enquiry          rmx_clear_device_capabilities_enquiry_v1
#define rmx_enquire_device_capabilities                rmx_enquire_device_capabilities_v1
#define rmx_apply_device_config                        rmx_apply_device_config_v1
#define rmx_revert_device_config                       rmx_revert_device_config_v1
#define rmx_init_user_clock                            rmx_init_user_clock_v1
#define rmx_set_user_clock_handler                     rmx_set_user_clock_handler_v1
#define rmx_set_user_clock_context                     rmx_set_user_clock_context_v1
#define rmx_use_user_clock                             rmx_use_user_clock_v1
#define rmx_init_ptp_clock                             rmx_init_ptp_clock_v1
#define rmx_set_ptp_clock_device                       rmx_set_ptp_clock_device_v1
#define rmx_use_ptp_clock                              rmx_use_ptp_clock_v1
#define rmx_get_time                                   rmx_get_time_v1
#define rmx_init_notification                          rmx_init_notification_v1
#define rmx_set_notification_overlapped                rmx_set_notification_overlapped_v1
#define rmx_request_notification                       rmx_request_notification_v1
#define rmx_init_event_channel                         rmx_init_event_channel_v1
#define rmx_set_event_channel_handle                   rmx_set_event_channel_handle_v1
#define rmx_establish_event_channel                    rmx_establish_event_channel_v1
#define rmx_set_memory_address                         rmx_set_memory_address_v1
#define rmx_set_memory_as_zero_based                   rmx_set_memory_as_zero_based_v1
#define rmx_set_memory_mkey_id                         rmx_set_memory_mkey_id_v1
#define rmx_init_mem_registry                          rmx_init_mem_registry_v1
#define rmx_set_mem_registry_option                    rmx_set_mem_registry_option_v1
#define rmx_register_memory                            rmx_register_memory_v1
#define rmx_deregister_memory                          rmx_deregister_memory_v1
#define rmx_get_memory_mkey_id                         rmx_get_memory_mkey_id_v1
#define rmx_input_init_stream                          rmx_input_init_stream_v1
#define rmx_input_set_mem_capacity_in_packets          rmx_input_set_mem_capacity_in_packets_v1
#define rmx_input_get_mem_capacity_in_packets          rmx_input_get_mem_capacity_in_packets_v1
#define rmx_input_set_stream_nic_address               rmx_input_set_stream_nic_address_v1
#define rmx_input_enable_stream_option                 rmx_input_enable_stream_option_v1
#define rmx_input_set_mem_sub_block_count              rmx_input_set_mem_sub_block_count_v1
#define rmx_input_set_entry_size_range                 rmx_input_set_entry_size_range_v1
#define rmx_input_get_mem_block_buffer                 rmx_input_get_mem_block_buffer_v1
#define rmx_input_get_stride_size                      rmx_input_get_stride_size_v1
#define rmx_input_determine_mem_layout                 rmx_input_determine_mem_layout_v1
#define rmx_input_set_timestamp_format                 rmx_input_set_timestamp_format_v1
#define rmx_input_enable_info_per_packet               rmx_input_enable_info_per_packet_v1
#define rmx_input_create_stream                        rmx_input_create_stream_v1
#define rmx_input_destroy_stream                       rmx_input_destroy_stream_v1
#define rmx_input_init_flow                            rmx_input_init_flow_v1
#define rmx_input_set_flow_local_addr                  rmx_input_set_flow_local_addr_v1
#define rmx_input_set_flow_remote_addr                 rmx_input_set_flow_remote_addr_v1
#define rmx_input_set_flow_tag                         rmx_input_set_flow_tag_v1
#define rmx_input_attach_flow                          rmx_input_attach_flow_v1
#define rmx_input_detach_flow                          rmx_input_detach_flow_v1
#define rmx_input_set_completion_moderation            rmx_input_set_completion_moderation_v1
#define rmx_input_init_chunk_handle                    rmx_input_init_chunk_handle_v1
#define rmx_input_get_next_chunk                       rmx_input_get_next_chunk_v1
#define rmx_input_get_chunk_completion                 rmx_input_get_chunk_completion_v1
#define rmx_input_get_packet_info                      rmx_input_get_packet_info_v1
#define rmx_output_media_init                          rmx_output_media_init_v1
#define rmx_output_media_set_sdp                       rmx_output_media_set_sdp_v1
#define rmx_output_media_set_idx_in_sdp                rmx_output_media_set_idx_in_sdp_v1
#define rmx_output_media_set_packets_per_frame         rmx_output_media_set_packets_per_frame_v1
#define rmx_output_media_set_source_ports              rmx_output_media_set_source_ports_v1
#define rmx_output_media_set_pcp                       rmx_output_media_set_pcp_v1
#define rmx_output_media_set_dscp                      rmx_output_media_set_dscp_v1
#define rmx_output_media_set_ecn                       rmx_output_media_set_ecn_v1
#define rmx_output_media_assign_mem_blocks             rmx_output_media_assign_mem_blocks_v1
#define rmx_output_media_init_mem_blocks               rmx_output_media_init_mem_blocks_v1
#define rmx_output_media_get_sub_block                 rmx_output_media_get_sub_block_v1
#define rmx_output_media_get_dup_sub_block             rmx_output_media_get_dup_sub_block_v1
#define rmx_output_media_set_chunk_count               rmx_output_media_set_chunk_count_v1
#define rmx_output_media_set_sub_block_count           rmx_output_media_set_sub_block_count_v1
#define rmx_output_media_set_packets_per_chunk         rmx_output_media_set_packets_per_chunk_v1
#define rmx_output_media_set_stride_size               rmx_output_media_set_stride_size_v1
#define rmx_output_media_set_packet_layout             rmx_output_media_set_packet_layout_v1
#define rmx_output_media_create_stream                 rmx_output_media_create_stream_v1
#define rmx_output_media_destroy_stream                rmx_output_media_destroy_stream_v1
#define rmx_output_media_init_context                  rmx_output_media_init_context_v1
#define rmx_output_media_set_context_block             rmx_output_media_set_context_block_v1
#define rmx_output_media_get_local_address             rmx_output_media_get_local_address_v1
#define rmx_output_media_get_remote_address            rmx_output_media_get_remote_address_v1
#define rmx_output_media_init_chunk_handle             rmx_output_media_init_chunk_handle_v1
#define rmx_output_media_set_chunk_packet_count        rmx_output_media_set_chunk_packet_count_v1
#define rmx_output_media_get_next_chunk                rmx_output_media_get_next_chunk_v1
#define rmx_output_media_commit_chunk                  rmx_output_media_commit_chunk_v1
#define rmx_output_media_cancel_unsent_chunks          rmx_output_media_cancel_unsent_chunks_v1
#define rmx_output_media_skip_chunks                   rmx_output_media_skip_chunks_v1
#define rmx_output_gen_init_stream                     rmx_output_gen_init_stream_v1
#define rmx_output_gen_set_local_addr                  rmx_output_gen_set_local_addr_v1
#define rmx_output_gen_set_packets_per_chunk           rmx_output_gen_set_packets_per_chunk_v1
#define rmx_output_gen_set_remote_addr                 rmx_output_gen_set_remote_addr_v1
#define rmx_output_gen_set_max_sub_blocks              rmx_output_gen_set_max_sub_blocks_v1
#define rmx_output_gen_set_pcp                         rmx_output_gen_set_pcp_v1
#define rmx_output_gen_set_dscp                        rmx_output_gen_set_dscp_v1
#define rmx_output_gen_set_ecn                         rmx_output_gen_set_ecn_v1
#define rmx_output_gen_set_rate                        rmx_output_gen_set_rate_v1
#define rmx_output_gen_create_stream                   rmx_output_gen_create_stream_v1
#define rmx_output_gen_destroy_stream                  rmx_output_gen_destroy_stream_v1
#define rmx_output_gen_init_rate                       rmx_output_gen_init_rate_v1
#define rmx_output_gen_set_rate_max_burst              rmx_output_gen_set_rate_max_burst_v1
#define rmx_output_gen_set_rate_typical_packet_size    rmx_output_gen_set_rate_typical_packet_size_v1
#define rmx_output_gen_update_rate                     rmx_output_gen_update_rate_v1
#define rmx_output_gen_init_chunk_handle               rmx_output_gen_init_chunk_handle_v1
#define rmx_output_gen_get_next_chunk                  rmx_output_gen_get_next_chunk_v1
#define rmx_output_gen_set_chunk_remote_addr           rmx_output_gen_set_chunk_remote_addr_v1
#define rmx_output_gen_append_packet_to_chunk          rmx_output_gen_append_packet_to_chunk_v1
#define rmx_output_gen_commit_chunk                    rmx_output_gen_commit_chunk_v1
#define rmx_output_get_chunk_count                     rmx_output_get_chunk_count_v1
#define rmx_output_update_dscp                         rmx_output_update_dscp_v1
#define rmx_output_update_ecn                          rmx_output_update_ecn_v1
/** @endcond */

#endif /* SRC_RIVERMAX_DEFS_H_ */


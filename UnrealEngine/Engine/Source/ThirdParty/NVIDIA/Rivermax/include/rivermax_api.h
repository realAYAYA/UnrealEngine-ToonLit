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
#ifndef SRC_RIVERMAX_API_H_
#define SRC_RIVERMAX_API_H_

#include "rivermax_defs.h"
#include "rivermax_deprecated.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup RivermaxVer */
/** @{ */
//=============================================================================

/**
 * @brief Get numeric representation of the library version.
 * @return A structure with core version of the library
 */
__export
const rmx_version *rmx_get_version_numbers(void);

/**
 * @brief Get the library version in a string format.
 *
 * @return A string with the semantic representation of the library version,
 *         see https://semver.org/spec/v2.0.0.html.
 */
__export
const char *rmx_get_version_string(void);

//=============================================================================
/** @} RivermaxVer */


/** @addtogroup RivermaxInit */
/** @{ */
//=============================================================================

/**
 * @brief Helper to set CPUs in an affinity bitmap
 *        for @ref rmx_set_cpu_affinity
 * @param[in] bitmask      A bitmap with a bit per core.
 * @param[in] core_number  The core number to mark in the bitmap
 */
__RMX_INLINE
void rmx_mark_cpu_for_affinity(uint64_t *bitmask, size_t core_number)
{
    if (bitmask) {
        const size_t bits_in_dword = sizeof(*bitmask) << 3;
        size_t dword_index = core_number / bits_in_dword;
        size_t in_dword_offset = core_number % bits_in_dword;
        bitmask[dword_index] |= (1ULL << in_dword_offset);
    }
}

/**
 * @brief Set affinity to a specific set of CPU cores.
 * @param[in] bitmask       A bitmap with enable-bit per core.
 * @param[in] core_count   The total amount of cores in the system.
 * @return Status code as defined by @ref rmx_status
 */
__export
rmx_status rmx_set_cpu_affinity(const uint64_t *bitmask, size_t core_count);

/**
 * @brief Enable handling of system signals
 * @return Status code as defined by @ref rmx_status
 * @remark Signal handling is disabled by default
 */
__export
rmx_status rmx_enable_system_signal_handling(void);

/**
 * @brief Initialize Rivermax library
 * @return Status code as defined by @ref rmx_status
 * @remark It initializes library to follow policy of the latest version upon
 *         compilation.
 * @warning It SHALL be called before any other Rivermax API,
 *          except for @ref RivermaxVer "version APIs"
 */
__export rmx_status _rmx_init(const rmx_version *policy);
__RMX_INLINE
rmx_status rmx_init(void)
{
    const rmx_version policy = {
        RMX_VERSION_MAJOR,
        RMX_VERSION_MINOR,
        RMX_VERSION_PATCH
    };
    return _rmx_init(&policy);
}

/**
 * @brief Performs Rivermax library cleanups.
 *
 * This routine finalizes and releases the resources allocated
 * by the Rivermax library.
 *
 * @return Status code as defined by @ref rmx_status
 * @warning An application cannot call any Rivermax routine
 *          after @ref rmx_cleanup call.
 */
__export
rmx_status rmx_cleanup(void);

//=============================================================================
/** @} RivermaxInit */



/** @addtogroup RivermaxDevice */
/** @{ */
//=============================================================================

/**
 * @brief Obtain a list of devices supported by Rivermax
 * @param[out] list  A pointer to a pointer that refers
 *                   to @ref rmx_device_list
 * @return The amount of devices in the list; upon error 0 is returned
 * @see rmx_device_list
 */
__export
size_t rmx_get_device_list(rmx_device_list **list);

/**
 * @brief Free a list of devices supported by Rivermax
 * @param[in] list  A pointer to a list obtained via @ref rmx_get_device_list
 * @remark It shall be a list obtained
 *         with @ref rmx_get_device_list
 * @see rmx_device_list
 */
__export
void rmx_free_device_list(rmx_device_list *list);

/**
 * @brief Get the device count of the given device list
 * @param[in] list A pointer to a list obtained via @ref rmx_get_device_list
 * @return The device count
 * @see rmx_device_list
 */
__export
size_t rmx_get_device_count(const rmx_device_list *list);

/**
 * @brief Get the device referred by the index out of the device list
 * @param[in] list  A pointer to a list obtained via @ref rmx_get_device_list
 * @param[in] index An index of the device in the list
 * @return A pointer to the device descriptor
 * @remark On error (e.g., out-of-range index) NULL is returned
 * @see rmx_device_list
*/
__export
const rmx_device *rmx_get_device(const rmx_device_list *list,
    size_t index);

/**
 * @brief Get an interface name of the given device
 * @param[in] device    A device obtained via @ref rmx_get_device
 * @return The name of the device's interface
 * @remark On error (e.g., out-of-range index) NULL is returned
 * @see rmx_device
 */
__export
const char *rmx_get_device_interface_name(const rmx_device *device);

/**
 * @brief Get the amount of IPs associated with the device
 * @param[in] device    A device obtained via @ref rmx_get_device
 * @return The amount of IPs
 * @see rmx_device
 */
__export
size_t rmx_get_device_ip_count(const rmx_device *device);

/**
 * @brief Get the IP address of a device using types of Socket API
 * @param[in] device    A device obtained via @ref rmx_get_device
 * @param[in] index     An index of the address in the list
 * @return The device's @ref rmx_ip_addr "IP address"
 * @remark On error (e.g., out-of-range index) NULL is returned
 * @remark See Socket API for more details
 * @see rmx_device
 */
__export
const rmx_ip_addr *rmx_get_device_ip_address(const rmx_device *device,
    size_t index);

/**
 * @brief Get the MAX address of the specified device as a byte array
 * @param[in] device    A device obtained via @ref rmx_get_device
 * @return A pointer to a byte array representing MAC
 * @remark On error (e.g., out-of-range index) NULL is returned
 * @see rmx_device
 */
__export
const uint8_t *rmx_get_device_mac_address(const rmx_device *device);

/**
 * @brief Get the ID of the specified device
 * @param[in] device    A device obtained via @ref rmx_get_device
 * @return The device's ID
 * @see rmx_device
 */
__export
uint32_t rmx_get_device_id(const rmx_device *device);

/**
 * @brief Get the serial number of the specified device
 * @param[in] device    A device obtained via @ref rmx_get_device
 * @return A string with the serial number
 * @remark On error (e.g., out-of-range index) NULL is returned
 * @see rmx_device
 */
__export
const char *rmx_get_device_serial_number(const rmx_device *device);

//=============================================================================
/**@} RivermaxDevice */


/** @addtogroup RivermaxDeviceInterface */
/** @{ */
//=============================================================================

/**
 * @brief Draw a device interface associated with the specified IP address
 * @param[out] device_iface  A device-interface descriptor
 * @param[in]  ip            An IP address of the desired device-inteface
 * @return Status code as defined by @ref rmx_status
 * @remark On error (e.g., out-of-range index) NULL is returned
 * @see rmx_device_iface
 */
__export
rmx_status rmx_retrieve_device_iface(rmx_device_iface *device_iface,
    const rmx_ip_addr *ip);

/**
 * @brief Convenience variant of API @ref rmx_retrieve_device_iface for IPv4
 *        based addresses.
 *
 * @param[out] device_iface  A device-interface descriptor
 * @param[in]  ip            An IP address of the desired device-inteface
 * @return Status code as defined by @ref rmx_status
 * @remark See @ref rmx_retrieve_device_iface
 * @see rmx_device_iface
 */
__RMX_INLINE
rmx_status rmx_retrieve_device_iface_ipv4(rmx_device_iface *device_iface,
    const struct in_addr *ip){
    rmx_ip_addr ip_addr;
    ip_addr.family = AF_INET;
    ip_addr.addr.ipv4 = *ip;
    return rmx_retrieve_device_iface(device_iface, &ip_addr);
}

/**
 * @brief Clear device capabilities enquiry before configuring it
 *        with @ref rmx_mark_device_capability_for_enquiry
 * @param[out] caps         Configuration of capabilities enquiry
 * @see rmx_device_capabilities
 */
__RMX_INLINE
void rmx_clear_device_capabilities_enquiry(rmx_device_capabilities *caps)
{
    rmx_attribs_metadata *attributes = (rmx_attribs_metadata*)(void*)caps;
    if (attributes) {
        attributes->bitmap = 0ULL;
    }
}

/**
 * @brief Mask the specified device capability to enquire
 *        with @ref rmx_enquire_device_capabilities
 * @param[out] caps         Configuration of capabilities enquiry
 * @param[in]  capability   An enquired capability
 * @see rmx_device_capabilities
 */
__RMX_INLINE
void rmx_mark_device_capability_for_enquiry(rmx_device_capabilities *caps,
    rmx_device_capability capability)
{
    rmx_attribs_metadata *attributes = (rmx_attribs_metadata*)(void*)caps;
    if (attributes) {
        attributes->bitmap |= (1ULL << capability);
    }
}

/**
 * @brief Query device the supported capabilities specified
 *        with @ref rmx_mark_device_capability_for_enquiry
 *
 * After this call one can use @ref rmx_is_device_capability_supported to
 * obtain the results of the enquiry.
 *
 * @param[in]     device_iface A device interface obtained
 *                             via @ref rmx_retrieve_device_iface
 * @param[in,out] caps         The capabilities raised for enquiry
 *                             with @ref rmx_mark_device_capability_for_enquiry
 * @return Status code as defined by @ref rmx_status
 * @see rmx_device_iface
 */
__export
rmx_status rmx_enquire_device_capabilities(const rmx_device_iface *device_iface,
    rmx_device_capabilities *caps);

/**
 * @brief Does the specified device support the requested capability
 * @param[in] caps          Device capabilities obtained
 *                          via @ref rmx_enquire_device_capabilities
 * @param[in] capability    An enquired capability
 * @return TRUE - if supported; FALSE - otherwise
 */
__RMX_INLINE
bool rmx_is_device_capability_supported(const rmx_device_capabilities *caps,
    rmx_device_capability capability)
{
    rmx_attribs_metadata *attributes = (rmx_attribs_metadata*)(void*)caps;
    return (attributes) && (attributes->bitmap & (1ULL << capability));
}

/**
 * @brief Clear device configuration attributes before configuring it
 *        with @ref rmx_set_device_config_attribute
 * @param[out] config       A device configuration
 * @see rmx_device_config
 */
__RMX_INLINE
void rmx_clear_device_config_attributes(rmx_device_config *config)
{
    rmx_attribs_metadata *attributes = (rmx_attribs_metadata*)(void*)config;
    if (attributes) {
        attributes->bitmap = 0ULL;
    }
}

/**
 * @brief Set attribute in a device configuration
 * @param[out] config       A device configuration
 * @param[in]  attribute    A required configuration flag
 * @see rmx_device_config
 */
__RMX_INLINE
void rmx_set_device_config_attribute(rmx_device_config *config,
    rmx_device_config_attribute attribute)
{
    rmx_attribs_metadata *attributes = (rmx_attribs_metadata*)(void*)config;
    if (attributes) {
        attributes->bitmap |= (1ULL << attribute);
    }
}

/**
 * @brief Apply to a device a configuration
 * @param[in] device_iface  A device interface obtained
 *                          via @ref rmx_retrieve_device_iface
 * @param[in] config        A device configuration that was set
 *                          via @ref rmx_set_device_config_attribute
 * @return Status code as defined by @ref rmx_status
 * @see rmx_device_iface
 */
__export
rmx_status rmx_apply_device_config(const rmx_device_iface *device_iface,
    rmx_device_config *config);

/**
 * @brief Revert the specified configuration in the device.
 * @param[in] device_iface  A device interface obtained
 *                          via @ref rmx_retrieve_device_iface
 * @param[in] config        A device configuration
 * @return Status code as defined by @ref rmx_status
 * @see rmx_device_iface
 */
__export
rmx_status rmx_revert_device_config(const rmx_device_iface *device_iface,
    rmx_device_config *config);

//=============================================================================
/**@} RivermaxDeviceInterface */


/** @addtogroup RivermaxClock
 *
 * This set of APIs allows to configure Rivermax to operate either based on
 * a clock services provided by the user, or based on PTP.
 *
 * @note If none of this options is configured, Rivermax uses System clock.
 */
/** @{ */
//=============================================================================

/**
 * @brief Initialize user-clock configuration before setting it
 *        for @ref rmx_use_user_clock
 * @param[out] params   User-clock configuration parameters
 * @see rmx_user_clock_params
 */
__export
void rmx_init_user_clock(rmx_user_clock_params *params);

/**
 * @brief Set handler for @ref rmx_use_user_clock
 * @param[out] params   User-clock configuration parameters
 * @param[in]  handler  A function pointer to the user-clock handler.
 * @see rmx_user_clock_params
 */
__export
void rmx_set_user_clock_handler(rmx_user_clock_params *params,
    rmx_user_clock_handler handler);

/**
 * @brief Set context for @ref rmx_use_user_clock
 * @param[out] params   User-clock configuration parameters
 * @param[in]  ctx      A Optional user context for the handler;
 *                      if not specified, NULL will be assumed.
 * @see rmx_user_clock_params
 */
__export
void rmx_set_user_clock_context(rmx_user_clock_params *params, void *ctx);

/**
 * @brief Tell Rivermax to use a clock driven by user-clock handler
 * @param[in,out] params    user-clock configuration parameters
 * @return Status code as defined by @ref rmx_status
 * @see rmx_user_clock_params
 */
__export
rmx_status rmx_use_user_clock(const rmx_user_clock_params *params);

/**
 * @brief Initialize PTP configuration before setting it
 *        for @ref rmx_use_ptp_clock
 * @param[out] params    PTP-clock configuration parameters
 * @see rmx_ptp_clock_params
 */
__export
void rmx_init_ptp_clock(rmx_ptp_clock_params *params);

/**
 * @brief Set device for @ref rmx_use_ptp_clock
 * @param[out] params       PTP-clock configuration parameters
 * @param[in]  device_iface A device interface obtained
 *                          with @ref rmx_retrieve_device_iface
 * @see rmx_ptp_clock_params
 */
__export
void rmx_set_ptp_clock_device(rmx_ptp_clock_params *params,
    const rmx_device_iface *device_iface);

/**
 * @brief Tell Rivermax to use a clock driven by PTP
 * @param[in,out] params    PTP-clock configuration parameters
 * @return Status code as defined by @ref rmx_status
 * @see rmx_ptp_clock_params
 */
__export
rmx_status rmx_use_ptp_clock(const rmx_ptp_clock_params *params);

/**
 * @brief Get the current time
 * @param[in] type  Time type defining the source and measurement units
 * @param[in] time  The time value
 * @return Status code as defined by @ref rmx_status
 */
__export
rmx_status rmx_get_time(rmx_time_type type, uint64_t *time);

//=============================================================================
/**@} RivermaxClock */


/** @addtogroup RivermaxNotifications */
/** @{ */
//=============================================================================

#ifdef __linux__
typedef int rmx_event_channel_handle;
#else
typedef HANDLE rmx_event_channel_handle;
#endif

/**
 * @brief Initialize a event-channel for the specified stream before
 *        issuing @ref rmx_establish_event_channel
 * @param[in,out] params   A descriptor of the event-channel parameters
 * @param[in]     id       A stream ID
 * @see rmx_event_channel_params
 */
__export
void rmx_init_event_channel(rmx_event_channel_params *params,
    rmx_stream_id id);

/**
 * @brief Set a target event-channel handle
 *        for @ref rmx_establish_event_channel
 * @param[in,out] params   A descriptor of the event-channel parameters
 * @param[in]     handle   A pointer to the handle to initialize for the
 *                         channel
 * @see rmx_event_channel_params
 */
__export
void rmx_set_event_channel_handle(rmx_event_channel_params *params,
    rmx_event_channel_handle *handle);

/**
 * @brief Establish an event-channel for a stream read/write notifications
 * @param[out] params  A handle to a stream event-channel parameters
 * @return Status code as defined by @ref rmx_status
 * @remark To receive notifications one shall
 *         use @ref rmx_request_notification
 * @note In Windows environment it's strongly recommended to use Input/Output
 *       completion port (IOCP) GetQueuedCompletionStatus() WINAPI to handle
 *       the notification
 * @see rmx_event_channel_params
 */
__export
rmx_status rmx_establish_event_channel(rmx_event_channel_params *params);

/**
 * @brief Initialize a notification for the specified stream before
 *        issuing @ref rmx_request_notification
 * @param[in,out] params  A descriptor of the notification parameters
 * @param[in]     id      A stream ID
 * @see rmx_notification_params
 */
__export
void rmx_init_notification(rmx_notification_params *params,
    rmx_stream_id id);

#ifndef __linux__
/**
 * @brief Set stream for @ref rmx_request_notification
 * @param[in,out] params        A descriptor of the notification parameters
 * @param[in]     overlapped    A pointer to an OVERLAPPED structure
 * @remark If the stream has available chunks, @ref rmx_request_notification
 *         returns #RMX_BUSY. The user should call for next-chunk,
 *         which will handle them (without additional notifications).
 * @remark This setting needed for Windows only
 * @see rmx_notification_params
 */
__export
void rmx_set_notification_overlapped(rmx_notification_params *params,
    OVERLAPPED *overlapped);
#endif

/**
 * @brief Request to generate an event upon read/write.
 * @param[in,out] params    A descriptor of the notification parameters
 * @return Status code as defined by @ref rmx_status
 * @attention It requires an event channel being properly established
 *            using @ref rmx_establish_event_channel
 */
__export
rmx_status rmx_request_notification(rmx_notification_params *params);

//=============================================================================
/**@} RivermaxNotifications */


/** @addtogroup RegisterMemory */
/** @{ */
//=============================================================================

/**
 * @brief Initialize the memory registration parameters to register within
 *        the specified device
 * @brief Set the device associated with the memory registration
 * @param[in,out] params        A memory-registration parameters
 * @param[in]     device_iface  A device interface obtained
 *                              with @ref rmx_retrieve_device_iface
 * @see rmx_mem_reg_params
 */
__export
void rmx_init_mem_registry(rmx_mem_reg_params *params,
    const rmx_device_iface *device_iface);

/**
 * @brief Set a memory-registration option
 * @param[in,out] params   A memory-registration parameters
 * @param[in]     option   A registration option
 * @see rmx_mem_reg_params
 */
__export
void rmx_set_mem_registry_option(rmx_mem_reg_params *params,
    rmx_mem_reg_params_option option);

/**
 * @brief Register the specified memory block
 * @param[in,out] mem       Memory entry descriptor; the size and address of
 *                          the buffer our input, mkey is the output
 * @param[in]     params    A memory-registration parameters
 * @return Status code as defined by @ref rmx_status
 * @remark The field mkey is set to an assigned memory key. All other fields
 *         are an input.
 * @see rmx_mem_region
 */
__export
rmx_status rmx_register_memory(rmx_mem_region *mem,
    const rmx_mem_reg_params *params);

/**
 * @brief Deregister a memory registered with @ref rmx_register_memory
 * @param[in] mem          A memory entry descriptor with the key obtained
 *                         upon registration via @ref rmx_register_memory
 * @param[in] device_iface A device interface obtained
 *                         with @ref rmx_retrieve_device_iface
 * @return Status code as defined by @ref rmx_status
 * @remark The field mkey is set to an assigned memory key. Other fields are
 *         an input.
 * @see rmx_mem_region
 */
__export
rmx_status rmx_deregister_memory(const rmx_mem_region *mem,
    const rmx_device_iface *device_iface);

//=============================================================================
/**@} RegisterMemory */


/** @addtogroup InputStream */
/** @{ */
//=============================================================================

/** @defgroup InputBuild Building a Stream */
/** @{ */
//-----------------------------------------------------------------------------
/**
 * @brief Initialize the builder for an Input Stream of the specified type
 * @param[out] params   A stream creation parameters
 * @param[in]  type     The desired type for the stream
 * @see rmx_input_stream_params
 */
__export
void rmx_input_init_stream(rmx_input_stream_params *params,
    rmx_input_stream_params_type type);

/**
 * @brief Set the capacity of the Input Buffer in packet count
 *        for @ref rmx_input_create_stream
 *        and for @ref rmx_input_determine_mem_layout
 * @param[out] params   A stream creation parameters
 * @param[in]  count    The amount of packets
 * @remark If @ref rmx_input_determine_mem_layout is called, this field might
 *         be adjusted, depending on the calculations made by this API
 * @see rmx_input_stream_params
 */
__export
void rmx_input_set_mem_capacity_in_packets(rmx_input_stream_params *params, size_t count);

/**
 * @brief Set the Input NIC address for the stream
 *        for @ref rmx_input_create_stream
 *        and for @ref rmx_input_determine_mem_layout
 * @param[out] params       A stream creation parameters
 * @param[in]  nic_address  A NIC socket address
 * @see rmx_input_stream_params
 */
__export
void rmx_input_set_stream_nic_address(rmx_input_stream_params *params,
    const struct sockaddr *nic_address);

/**
 * @brief Enables the specified option for the Input Stream
 *        for @ref rmx_input_create_stream
 *        and for @ref rmx_input_determine_mem_layout
 * @param[out] params   A stream creation parameters
 * @param[in]  option   An option to configure the stream with
 * @see rmx_input_stream_params
 */
__export
void rmx_input_enable_stream_option(rmx_input_stream_params *params,
    rmx_input_option option);

/**
 * @brief Set the amount of sub-blocks composing the memory block of the
 *        Input stream for @ref rmx_input_create_stream and
 *        for @ref rmx_input_determine_mem_layout
 * @param[out] params     A stream creation parameters
 * @param[in]  count      An amount of sub-blocks (2 - when HDS, 1 - otherwise);
 *                        when HDS is configured, the first entry is for headers
 * @see rmx_input_stream_params
 */
__export
void rmx_input_set_mem_sub_block_count(rmx_input_stream_params *params, size_t count);

/**
 * @brief Set range of expected entry sizes in the specified memory sub-block,
 *        for @ref rmx_input_create_stream and
 *        for @ref rmx_input_determine_mem_layout
 * @param[out] params       A stream creation parameters
 * @param[in]  sub_block_id The index of the relevant memory sub-block
 * @param[in]  min_size     A minimal expected size to be received
 * @param[in]  max_size     A maximal expected size to be received
 * @see rmx_input_stream_params
 */
__export
void rmx_input_set_entry_size_range(rmx_input_stream_params *params,
    size_t sub_block_id, size_t min_size, size_t max_size);

/**
 * @brief Set a uniform size for the entries within the specified memory
 *        sub-block, for @ref rmx_input_create_stream and
 *        for @ref rmx_input_determine_mem_layout
 * @param[out] params        A stream creation parameters
 * @param[in]  sub_block_id  The index of the relevant memory sub-block
 * @param[in]  size          An expected size to be received
 * @note This is a convenience function. It's functionality is equivalent
 *       to calling @ref rmx_input_set_entry_size_range with
 *       @p min_size == @p max_size
 * @see rmx_input_stream_params
 */
__RMX_INLINE
void rmx_input_set_entry_uniform_size(rmx_input_stream_params *params,
    size_t sub_block_id, size_t size)
{
    rmx_input_set_entry_size_range(params, sub_block_id, size, size);
}

/**
 * @brief Determine the memory layout for the Input Stream and adjust
 *        the sizes required for the memory allocations accordingly
 * @param[in,out] params  A stream creation parameters properly set using the
 *                        preceding APIs in @ref InputStream "this API section"
 * @remark After calling this API the caller is expected to allocate
 *         the determined amount of memory and register it
 *         using @ref rmx_register_memory, whereas the relevant memory buffer
 *         should be acquired with @ref rmx_input_get_mem_block_buffer
 * @return Status code as defined by @ref rmx_status
 * @see rmx_input_stream_params
 */
__export
rmx_status rmx_input_determine_mem_layout(rmx_input_stream_params *params);

/**
 * @brief Get the capacity of the Input Memory-Block in packet count
 *        for @ref rmx_input_create_stream and
 *        for @ref rmx_input_determine_mem_layout
 * @param[out] params   A stream creation parameters
 * @remark If @ref rmx_input_determine_mem_layout is called, this field might
 *         be adjusted, depending on the calculations made by this API
 * @see rmx_input_stream_params
 */
__export
size_t rmx_input_get_mem_capacity_in_packets(const rmx_input_stream_params *params);

/**
 * @brief Get the resident memory details of the specified memory sub-block
 *        for @ref rmx_input_create_stream and
 *        for @ref rmx_input_determine_mem_layout
 * This API should be used to gain access to a memory block descriptor
 * that will be used to create an Input Stream for incoming data.
 *
 * @param[out] params        A stream creation parameters
 * @param[in]  sub_block_id  The index of the relevant memory sub-block
 * @return The memory details of the specified memory-block
 * @remark The return pointer can be used to register memory
 *         using @ref rmx_register_memory.
 * @warning Upon wrong @p sub_block_id index value, NULL is returned.
 * @see rmx_input_stream_params
 */
__export
rmx_mem_region *rmx_input_get_mem_block_buffer(rmx_input_stream_params *params,
    size_t sub_block_id);

/**
 * @brief Get a stride size for the specified memory sub-block
 *        after @ref rmx_input_determine_mem_layout was called
 * @param[out] params        A stream creation parameters
 * @param[in]  sub_block_id  The index of the relevant memory sub-block
 * @return A stride size
 * @warning Upon wrong @p sub_block_id index value, 0 is returned.
 * @see rmx_input_stream_params
 */
__export
size_t rmx_input_get_stride_size(const rmx_input_stream_params *params,
    size_t sub_block_id);

/**
 * @brief Set the timestamp format of the Input Stream
 *        for @ref rmx_input_create_stream
 * @param[out] params   A stream creation parameters
 * @param[in]  format   A timestamp format used to mark packets upon arrival
 * @see rmx_input_stream_params
 */
__export
void rmx_input_set_timestamp_format(rmx_input_stream_params *params,
    rmx_input_timestamp_format format);

/**
 * @brief Create an Input Stream
 * @param[in,out] params   A completely configured stream descriptor
 * @param[out]    id       A stream ID of the created stream
 * @return Status code as defined by @ref rmx_status
 * @see rmx_input_stream_params
 */
__export
rmx_status rmx_input_create_stream(rmx_input_stream_params *params, rmx_stream_id *id);

//-----------------------------------------------------------------------------
/**@} InputBuild */

/**
 * @brief Destroys an Input Stream
 * @param[in] id    A stream ID of obtained with @ref rmx_input_create_stream
 * @return Status code as defined by @ref rmx_status
 */
__export
rmx_status rmx_input_destroy_stream(rmx_stream_id id);

/** @defgroup InputFlow Flows */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Initialize a network flow for @ref rmx_input_attach_flow
 * @param[out] flow     A network flow descriptor
 * @note There are special values for IP address and IP port that serve as a
 *       wild cards:
 *        - port 0 is a wildcard for any source port
 *        - address 0.0.0.0:0 is a wildcard for any source address
 * @note Not calling to @ref rmx_input_set_flow_local_addr
 *       or @ref rmx_input_set_flow_remote_addr will assume zero value for
 *       a respectful parameter.
 * @see rmx_input_flow
 */
__export
void rmx_input_init_flow(rmx_input_flow *flow);

/**
 * @brief Set a local address of a network flow for @ref rmx_input_attach_flow
 * @param[out] flow     A network flow descriptor
 * @param[in]  local    local IP/Port,
 *                      appears as a destination IP/Port in the incoming packet
 * @see rmx_input_flow
 */
__export
void rmx_input_set_flow_local_addr(rmx_input_flow *flow,
    const struct sockaddr *local);

/**
 * @brief Set a remote address of a network flow for @ref rmx_input_attach_flow
 * @param[out] flow     A network flow descriptor
 * @param[in]  remote   remote peer IP/Port,
 *                      appears as a source IP/Port in the incoming packet
 * @see rmx_input_flow
 */
__export
void rmx_input_set_flow_remote_addr(rmx_input_flow *flow,
    const struct sockaddr *remote);

/**
 * @brief Set the identifier of a network flow for @ref rmx_input_attach_flow
 * @param[out] flow     A network flow descriptor
 * @param[in]  tag      A tag assigned by the application to the flow
 * @see rmx_input_flow
 */
__export
void rmx_input_set_flow_tag(rmx_input_flow *flow, uint32_t tag);

/**
 * @brief Attach a network flow to a specified Input Stream
 * @param[in] id   A stream id, obtained by @ref rmx_input_create_stream
 * @param[in] flow A network flow descriptor
 * @return Status code as defined by @ref rmx_status
 * @see rmx_input_flow
 */
__export
rmx_status rmx_input_attach_flow(rmx_stream_id id,
    const rmx_input_flow *flow);

/**
 * @brief Detach a network flow from a specified Input Stream
 * @param[in] id   A stream id, obtained by @ref rmx_input_create_stream
 * @param[in] flow A network flow descriptor
 * @return Status code as defined by @ref rmx_status
 * @see rmx_input_flow
 */
__export
rmx_status rmx_input_detach_flow(rmx_stream_id id,
    const rmx_input_flow *flow);

//-----------------------------------------------------------------------------
/**@} InputFlow */

/**
 * @brief Set completion moderation parameters of a chunk reception for
 *        the specified Input Stream
 * @param[in] id           A stream id obtained by @ref rmx_input_create_stream
 * @param[in] min_count    A minimal number of packets to return
 * @param[in] max_count    A maximal number of packets to return
 * @param[in] timeout_usec A timeout in usec that @ref rmx_input_get_next_chunk
 *                         will busy-wait for at least @p min_count of packets
 * @return Status code as defined by @ref rmx_status
 */
__export
rmx_status rmx_input_set_completion_moderation(rmx_stream_id id,
    size_t min_count, size_t max_count, int timeout_usec);

/** @defgroup InputChunk Handling Chunks */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Initialize a chunk handle to acquire chunks for an Input Stream
 *        with @ref rmx_input_get_next_chunk and to access chunk's contents
 * @param[out] handle A chunk handle for an Input Stream
 * @param[in]  id     A stream id, obtained by @ref rmx_input_create_stream
 * @remark  The same handle can be used to acquire several chunks and
 *          then process the contents of the acquired chunk.
 * @warning If several threads are working to maximize the throughput of the
 *          stream, each thread shall construct their own chunk handle using
 *          this API.
 * @see rmx_input_chunk_handle
 */
__export
void rmx_input_init_chunk_handle(rmx_input_chunk_handle *handle,
    rmx_stream_id id);

/**
 * @brief Awaits for Rivermax to receive the amount of data that is configured
 *        by @ref rmx_input_set_completion_moderation
 * @param[in,out] handle A chunk handle for an Input Stream obtained
 *                       with @ref rmx_input_init_chunk_handle
 * @return Status code as defined by @ref rmx_status
 * @remarks The chunk details can be obtained
 *          via @ref rmx_input_get_chunk_completion and, if configured,
 *          via @ref rmx_input_get_packet_info (the latter is optional)
 * @warning Calling this API without attach will result in getting
 *          status #RMX_NO_ATTACH
 * @warning The chunk details remains valid only until the next call
 *          of @ref rmx_input_get_next_chunk
 * @note    The function busy-waits until either the minimal amount of packets
 *          set by @ref rmx_input_set_completion_moderation arrives or
 *          the processes is interrupted by a signal
 * @note    The call can return an error code, and still have data arrived,
 *          e.g. if Rivermax detects a checksum issue, it
 *          returns #RMX_CHECKSUM_ISSUE, but all the packets that
 *          arrived prior to failure are placed in their respectful buffers
 * @see rmx_input_chunk_handle
 */
__export
rmx_status rmx_input_get_next_chunk(rmx_input_chunk_handle *handle);

/**
 * @brief Get the completion details of the chunk acquired for an Input Stream
 *        via @ref rmx_input_get_next_chunk
 * @param[in,out] handle A chunk handle for an Input Stream obtained
 *                       with @ref rmx_input_init_chunk_handle
 * @return The @ref rmx_input_completion "completion" details of the chunk
 * @remark NULL is returned upon incorrect or obsolete stream handle
 * @see rmx_input_chunk_handle
 */
__export
const rmx_input_completion *rmx_input_get_chunk_completion(rmx_input_chunk_handle *handle);

/** @return The pointer to a buffer of the specified packet and sub-block id
 *  @see rmx_input_completion
 */
__RMX_INLINE
const void *rmx_input_get_completion_ptr(const rmx_input_completion *completion, size_t sub_block_id)
{
    return ((const rmx_input_completion_metadata*)(const void*)completion)->ptr[sub_block_id];
}

/** @return The size of the acquired chunk
 *  @see rmx_input_completion
 */
__RMX_INLINE
size_t rmx_input_get_completion_chunk_size(const rmx_input_completion *completion)
{
    return (size_t)((const rmx_input_completion_metadata*)(const void*)completion)->chunk_size;
}

/** @return The first SEQN within the acquired chunk
 *  @see rmx_input_completion
 */
__RMX_INLINE
uint32_t rmx_input_get_completion_seqn_first(const rmx_input_completion *completion)
{
    return ((const rmx_input_completion_metadata*)(const void*)completion)->seqn_first;
}

/** @return The indication whether the flag is set within the acquired chunk
 *  @see rmx_input_completion
 */
__RMX_INLINE
bool rmx_input_get_completion_flag(const rmx_input_completion *completion,
    rmx_input_completion_flag flag)
{
    return 0 !=
        (((const rmx_input_completion_metadata*)(const void*)completion)->flags & (1 << flag));
}

/** @return The first timestamp within the acquired chunk
 *  @see rmx_input_completion
 */
__RMX_INLINE
uint64_t rmx_input_get_completion_timestamp_first(const rmx_input_completion *completion)
{
    return ((const rmx_input_completion_metadata*)(const void*)completion)->timestamp_first;
}

/** @return The last timestamp within the acquired chunk
 *  @see rmx_input_completion
 */
__RMX_INLINE
uint64_t rmx_input_get_completion_timestamp_last(const rmx_input_completion *completion)
{
    return ((const rmx_input_completion_metadata*)(const void*)completion)->timestamp_last;
}

/**
 * @brief Get the packet-info of the specified packet within the chunk acquired
 *        via @ref rmx_input_get_next_chunk
 * @param[in,out] handle A chunk handle for an Input Stream obtained
 *                       with @ref rmx_input_init_chunk_handle
 * @param[in] packet_id   A packet id to get the packet info for
 * @return The associated @ref rmx_input_packet_info "packet info"
 * @remark NULL is returned upon incorrect id or an obsolete handle
 *         if #RMX_INPUT_STREAM_CREATE_INFO_PER_PACKET is not set
 *         via @ref rmx_input_enable_stream_option
 * @see rmx_input_chunk_handle
 */
__export
const rmx_input_packet_info *rmx_input_get_packet_info(rmx_input_chunk_handle *handle,
    size_t packet_id);

/** @return The size of the contents of the specified packet and block id
 *  @see rmx_input_packet_info
 */
__RMX_INLINE
size_t rmx_input_get_packet_size(const rmx_input_packet_info *info, size_t sub_block_id)
{
    return (size_t)((rmx_input_packet_info_metadata*)(void*)info)->size[sub_block_id];
}

/** @return The user-assigned flow-tag associated with the specified packet
 *  @see rmx_input_packet_info
 */
__RMX_INLINE
uint32_t rmx_input_get_packet_flow_tag(const rmx_input_packet_info *info)
{
    return ((rmx_input_packet_info_metadata*)(void*)info)->flow_tag;
}

/** @return The arrival timestamp of the specified packet
 *  @see rmx_input_packet_info
 */
__RMX_INLINE
uint64_t rmx_input_get_packet_timestamp(const rmx_input_packet_info *info)
{
    return ((rmx_input_packet_info_metadata*)(void*)info)->timestamp;
}

//-----------------------------------------------------------------------------
/**@} InputChunk */

//=============================================================================
/**@} InputStream */


/** @addtogroup MediaOutputStream */
/** @{ */
//=============================================================================

/** @defgroup MediaOutputStream_Media Media Parameters */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Initialize Media Output-Stream descriptor before configuring it
 *        for @ref rmx_output_media_create_stream
 * @param[out] params    A Media Output-Stream creation parameters
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_init(rmx_output_media_stream_params *params);

/**
 * @brief Set Media Output-Stream attributes in SDP format
 *        for @ref rmx_output_media_create_stream
 * @param[in,out] params   A Media Output-Stream creation parameters
 * @param[in]     sdp      An index of the relevant memory block
 * @note  List of required SDP attributes:
 *        - protocol version, v=, must be set to zero
 *        - Connection Data c= field
 *        - a=fmtp: format, sampling, width, height, exactframerate,
 *          depth, colorimetry, PM, TP
 *        - a=source-filter
 *        - Media name and transport address, m=, "video" must be provided
 *        List of Optional supported SDP parameters and their defaults:
 *        - a=fmtp:
 *          interlace: default 0, segmented: default 0, MAXUDP default: 1460
 *        - a=group:DUP.
 * @note  If DUP exists then the number of identification tags (tokens
 *        following the "DUP" semantics) has to correspond to the number of
 *        `m=video` blocks.
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_set_sdp(rmx_output_media_stream_params *params,
    const char *sdp);

/**
 * @brief Set an index of the media block within the SDP file
 *        for @ref rmx_output_media_create_stream
 *
 * The enumeration of the indexes starts from zero and corresponds to the
 * order of appearance of the media blocks within the SDP file.
 * Media streams may be grouped by the session-level "a=group" attribute.
 * @param[out] params           A Media Output-Stream creation parameters
 * @param[in]  media_block_idx  A zero-based index of the media block
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_set_idx_in_sdp(rmx_output_media_stream_params *params,
    size_t media_block_idx);

/**
 * @brief Set the amount of packets in a single media frame
 *        for @ref rmx_output_media_create_stream
 * @param[out] params   A Media Output-Stream creation parameters
 * @param[in]  count    An amount of packets in a single media frame
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_set_packets_per_frame(rmx_output_media_stream_params *params,
    size_t count);

/**
 * @brief Set a connection source port per each redundant stream
 *        for @ref rmx_output_media_create_stream
 * @param[out] params   A Media Output-Stream creation parameters
 * @param[in]  count    An amount of redundant streams/source-ports
 *                      (the maximal supported value is #RMX_MAX_DUP_STREAMS)
 * @param[in]  ports    An array of the source-ports
 * @note This API shall be called only if the user source port allocation
 *       is needed.
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_set_source_ports(rmx_output_media_stream_params *params,
    const uint16_t *ports, size_t count);

//-----------------------------------------------------------------------------
/**@} MediaOutputStream_Media */

/** @defgroup MediaOutputStream_Net Network Parameters */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Set a PCP attribute for QoS on Layer 2 (IEEE 802.1Qbb)
 *        for @ref rmx_output_media_create_stream
 * @param[out] params   A Media Output-Stream creation parameters
 * @param[in]  pcp      A value of a corresponding Class of Quality
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_set_pcp(rmx_output_media_stream_params *params, uint8_t pcp);

/**
 * @brief Set a DSCP attribute for QoS on Layer 3 (RFC 4594)
 *        for @ref rmx_output_media_create_stream
 * @param[out] params   A Media Output-Stream creation parameters
 * @param[in]  dscp     A DSCP value stating the priority
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_set_dscp(rmx_output_media_stream_params *params, uint8_t dscp);

/**
 * @brief Set an Explicit Congestion Notification field value
 *        for @ref rmx_output_media_create_stream
 * @param[out] params   A Media Output-Stream creation parameters
 * @param[in]  ecn      An ECN value setting the required method
 * @remark   Default value is "ECN is non-capable", i.e. = 0.
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_set_ecn(rmx_output_media_stream_params *params, uint8_t ecn);

//-----------------------------------------------------------------------------
/**@} MediaOutputStream_Net */

/** @defgroup MediaOutputStream_Mem Memory Assignment */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Initialize the memory-blocks of a Media Output-Stream
 *        for @ref rmx_output_media_create_stream
 * @param[in]  mem_blocks An array of memory blocks
 * @param[in]  count      An amount of memory blocks
 * @see rmx_output_media_mem_block
 */
__export
void rmx_output_media_init_mem_blocks(rmx_output_media_mem_block *mem_blocks,
    size_t count);

/**
 * @brief Set a number of resident chunks to accommodate in a memory of the
 *        specified memory-block for @ref rmx_output_media_create_stream
 * @param[out] mem_block  A memory block descriptor
 * @param[in]  count      A number of chunks
 * @see rmx_output_media_mem_block
 */
__export
void rmx_output_media_set_chunk_count(rmx_output_media_mem_block *mem_block,
    size_t count);

/**
 * @brief Set the number of memory sub-blocks that should reside in a resident
 *        memory for @ref rmx_output_media_assign_mem_blocks
 * @param[out] mem_block A memory block descriptor
 * @param[in]  count     An amount of sub-blocks (2 - when HDS, 1 - otherwise);
 *                       when HDS is configured, the first entry is for headers
 * @see rmx_output_media_mem_block
 */
__export
void rmx_output_media_set_sub_block_count(rmx_output_media_mem_block *mem_block,
    size_t count);

/**
 * @brief Get the memory details of the specified memory sub-block
 *        for @ref rmx_output_media_assign_mem_blocks
 *
 * This API should be used to gain access to a memory sub-block descriptor
 * that will be used to create a Media Output-Stream.
 *
 * @param[out] mem_block     A memory block descriptor
 * @param[in]  sub_block_id  An index of a relevant memory sub-block
 * @return The memory details of the specified memory-block
 * @warning Upon wrong @p sub_block_id index value, NULL is returned.
 * @warning Shall not be called prior
 *          to @ref rmx_output_media_set_sub_block_count
 * @note There is another version of this API designed for use-cases like
 *       SMPTE 2022-7, when the same memory is registered with several keys,
 *       see @ref rmx_output_media_get_dup_sub_block
 * @see rmx_output_media_mem_block
 */
__export
rmx_mem_region *rmx_output_media_get_sub_block(rmx_output_media_mem_block *mem_block,
    size_t sub_block_id);

/**
 * @brief Get the RAM details of the specified memory sub-block,
 *        when a sub-block has to be registered with several keys
 *        for @ref rmx_output_media_assign_mem_blocks
 *
 * This API should be used to gain access to a memory sub-block descriptor
 * that will be used to create stream for sending Media data. This API
 * is an alternative to @ref rmx_output_media_get_sub_block, and it extends
 * the latter for the use-cases like SMPTE 2022-7, when the same data is
 * transmitted via several devices and thus requires multiple registrations.
 *
 * @param[out] mem_block     A memory block descriptor
 * @param[in]  sub_block_id  An index of a relevant memory sub-block
 * @return The memory details of the specified memory-block
 * @warning Upon wrong @p sub_block_id index value, NULL is returned.
 * @warning Shall not be called prior
 *          to @ref rmx_output_media_set_sub_block_count
 * @see rmx_output_media_mem_block
 */
__export
rmx_mem_multi_key_region *rmx_output_media_get_dup_sub_block(rmx_output_media_mem_block *mem_block,
    size_t sub_block_id);

/**
 * @brief Set layout of packets in the specified sub-blocks across all the
 *        chunks of the specified memory block
 * @param[out] mem_block    A memory block descriptor
 * @param[in]  sub_block_id An index of the relevant memory sub-block
 * @param[in]  packet_sizes An array of effective packet sizes per stride across
 *                          the entire @ref MediaOutputStream_Mem "memory buffer"
 * @warning Shall be called for a Static Media Output-Stream only
 * @note    The same information for Dynamic streams is passed upon each
 *          commited chunk, using @ref rmx_output_media_get_chunk_packet_sizes
 * @warning When specified, the total amount of entries in @p packet_sizes
 *          shall cover all entries in the memory block, i.e.:
 *          len(packet_sizes) = @ref rmx_output_media_set_chunk_count "chunk_count"
 *                * @ref rmx_output_media_set_packets_per_chunk "packets_per_chunk"
 * @warning Shall not be called prior
 *          to @ref rmx_output_media_set_sub_block_count
 * @see rmx_output_media_mem_block
 */
__export
void rmx_output_media_set_packet_layout(rmx_output_media_mem_block *mem_block,
    size_t sub_block_id, const uint16_t *packet_sizes);

/**
 * @brief Assign to the specified Media Output-Stream the memory blocks that
 *        will be used by it to transmit data on the wire.
 * @param[out] params     A Media Output-Stream creation parameters
 * @param[in]  mem_blocks An array of memory blocks
 * @param[in]  count      An amount of memory blocks
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_assign_mem_blocks(rmx_output_media_stream_params *params,
    rmx_output_media_mem_block *mem_blocks, size_t count);

//-----------------------------------------------------------------------------
/**@} MediaOutputStream_Mem */

/** @defgroup MediaOutputStream_Layout Memory-layout */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Set a number of packets composing a single resident chunk
 *        for @ref rmx_output_media_create_stream
 * @param[out] params             A Media Output-Stream creation parameters
 * @param[in]  packets_per_chunk  A number of packets in a single chunk
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_set_packets_per_chunk(rmx_output_media_stream_params *params,
    size_t packets_per_chunk);

/**
 * @brief Set the stride size for the packet-data in the specified
 *        memory sub-blocks of the specified Media Output-Stream
 *        for @ref rmx_output_media_create_stream
 * @param[out] params        A Media Output-Stream creation parameters
 * @param[in]  sub_block_id  An index of the relevant memory sub-block
 * @param[in]  stride_size   A size of a stride
 * @see rmx_output_media_stream_params
 */
__export
void rmx_output_media_set_stride_size(rmx_output_media_stream_params *params,
    size_t sub_block_id, const size_t stride_size);

//-----------------------------------------------------------------------------
/**@} MediaOutputStream_Layout */

/**
 * @brief Creates a media-Output Stream
 * @param[in,out] params   A configured Media Output-Stream descriptor
 * @param[out]    id       An ID of the created stream
 * @return Status code as defined by @ref rmx_status
 * @see rmx_output_media_stream_params
 */
__export
rmx_status rmx_output_media_create_stream(rmx_output_media_stream_params *params,
    rmx_stream_id *id);

/**
 * @brief Destroys a media-Output Stream
 * @param[in] id    An ID of the created Media Output-Stream
 *                  with @ref rmx_output_media_create_stream
 * @return Status code as defined by @ref rmx_status
 * @see rmx_output_media_stream_params
 */
__export
rmx_status rmx_output_media_destroy_stream(rmx_stream_id id);

/**
 * @brief Set a stream of the Media Output-Stream context
 * @param[out] context  A Media Output-Stream context
 * @param[in]  id       An ID of a stream created
 *                      with @ref rmx_output_media_create_stream
 * @warning If several threads are manipulating the stream using
 *          the methods of the Media Output-Stream context, each thread
 *          should construct their own context using this API.
 * @see rmx_output_media_context
 */
__export
void rmx_output_media_init_context(rmx_output_media_context *context,
    rmx_stream_id id);

/**
 * @brief Set an SDP media block id of the Media Output-Stream context
 * @param[in,out] context   A Media Output-Stream context
 * @param[in]     id        A zero-based index of the media block in SDP
 * @see rmx_output_media_context
 */
__export
void rmx_output_media_set_context_block(rmx_output_media_context *context,
    size_t id);

/**
 * @brief Returns the local address of a Media Output-Stream, i.e.
 *        a source point
 * @param[in]  context  A Media Output-Stream context
 * @param[out] address  A local IP/Port, appears as a source IP/Port
 *                      in an outgoing packet
 * @return Status code as defined by @ref rmx_status
 * @see rmx_output_media_context
 */
__export
rmx_status rmx_output_media_get_local_address(const rmx_output_media_context *context,
    struct sockaddr *address);

/**
 * @brief Returns the target address of a Media Output-Stream, i.e.
 *        a destination point
 * @param[in]  context  A Media Output-Stream context
 * @param[out] address  A remote peer IP/Port, appears as destination IP/Port
 *                      in an outgoing packet
 * @return Status code as defined by @ref rmx_status
 * @see rmx_output_media_context
 */
__export
rmx_status rmx_output_media_get_remote_address(const rmx_output_media_context *context,
    struct sockaddr *address);

/** @defgroup MediaOutputStream_Chunk Handling Chunks */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Initialize a chunk handle to acquire chunks
 *        with @ref rmx_output_media_get_next_chunk and to commit them with
 *        with @ref rmx_output_media_commit_chunk
 * @param[out] handle   A chunk handle, which can be either of a Static or
 *                      a Dynamic Media Output-Stream
 * @param[in]  id       An ID of the created Media Output-Stream
 *                      with @ref rmx_output_media_create_stream
 * @remark  The same handle can be used to acquire and fill several chunks and
 *          then to commit them, which is performed in the order of the
 *          acquisition.
 * @note A chunk handle for the same Media Output-Stream can be copied and
 *       reconstructed many times, and the order will still be preserved.
 * @note The reconstructed handle shall be reconfigured (e.g. options,
 *       pointers), whereas the copied handle copies the configurations of
 *       the copy source.
 * @warning If several threads are working to maximize the throughput of the
 *          stream, each thread shall construct their own chunk handle using
 *          this API.
 * @see rmx_output_media_chunk_handle
 */
__export
void rmx_output_media_init_chunk_handle(rmx_output_media_chunk_handle *handle,
    rmx_stream_id id);

/**
 * @brief Set a number of packets that is needed to compose the next chunk to
 *        be acquired with @ref rmx_output_media_get_next_chunk
 * @param[in,out] handle A chunk handle, which can be either of a Static or
 *                       a Dynamic Media Output-Stream
 * @param[in]  packets_in_chunk A number of packets in this chunk
 * @warning It shall be called only for chunks of a stream that was created
 *          with @ref rmx_output_media_create_stream as
 *          a Dynamic Media Output-Stream
 * @see rmx_output_media_chunk_handle
 */
__export
void rmx_output_media_set_chunk_packet_count(rmx_output_media_chunk_handle *handle,
    size_t packets_in_chunk);

/**
 * @brief Acquire the next free chunk for the Media Output-Stream
 * @param[in,out] handle A chunk handle, which can be either of a Static or
 *                       a Dynamic Media Output-Stream
 * @return Status code as defined by @ref rmx_status
 * @note If there are no available free chunks, status #RMX_NO_FREE_CHUNK is
 *       returned to the caller.
 *       The application shall continue retrying until the status code changes.
 * @see rmx_output_media_chunk_handle
 */
__export
rmx_status rmx_output_media_get_next_chunk(rmx_output_media_chunk_handle *handle);

/**
 * @return Get a pointer to the strides of the last acquired chunk for
 *         the specified sub-block
 * @param[in,out] handle    A chunk handle, which can be either of a Static or
 *                          a Dynamic Media Output-Stream
 * @param[in] sub_block_id  An index of the relevant memory sub-block
 * @return A pointer to the strides
 * @warning It shall be called only after a successful call
 *          to @ref rmx_output_media_get_next_chunk
 * @warning It shall be called with @p sub_block_id of memory blocks, for
 *          which @ref rmx_output_media_set_packet_layout was called with
 *          a non-zero @p rmx_output_media_set_packet_layout::stride_size
 * @see rmx_output_media_chunk_handle
 */
__RMX_INLINE
void *rmx_output_media_get_chunk_strides(rmx_output_media_chunk_handle *handle,
    size_t sub_block_id)
{
    return ((rmx_output_media_chunk_handle_metadata*)(void*)(handle))->strides[sub_block_id];
}

/**
 * @return Get a pointer to an array for packet sizes of the last acquired
 *         chunk and the specified sub-block, which is to be filled before
 *         calling to @ref rmx_output_media_commit_chunk
 * @param[in,out] handle    A chunk handle, which can be either of a Static or
 *                          a Dynamic Media Output-Stream
 * @param[in] sub_block_id  An index of the relevant memory sub-block
 * @warning It shall be called only for chunks of a stream that was created
 *          as a Dynamic Stream with @ref rmx_output_media_create_stream
 * @warning It shall be called only after a successful call
 *          to @ref rmx_output_media_get_next_chunk
 * @warning It shall be called with @p sub_block_id of memory blocks, for
 *          which @ref rmx_output_media_set_packet_layout was called with
 *          a non-zero @p rmx_output_media_set_packet_layout::stride_size
 * @see rmx_output_media_chunk_handle
 */
__RMX_INLINE
uint16_t *rmx_output_media_get_chunk_packet_sizes(rmx_output_media_chunk_handle *handle,
    size_t sub_block_id)
{
    return ((rmx_output_media_chunk_handle_metadata*)(void*)(handle))->packet_sizes[sub_block_id];
}

/**
 * @brief Set a chunk option to control a commit of the next chunk
 *        with @ref rmx_output_media_commit_chunk
 * @param[in] handle     A chunk handle, which can be either of a Static or
 *                       a Dynamic Media Output-Stream
 * @param[in] option     An option controlling the chunk commit
 * @see rmx_output_media_chunk_handle
 */
__RMX_INLINE
void rmx_output_media_set_chunk_option(const rmx_output_media_chunk_handle *handle,
    rmx_output_commit_option option)
{
    rmx_output_media_chunk_handle_metadata *metadata = ((rmx_output_media_chunk_handle_metadata*)(void*)(handle));
    metadata->flags |= (1ULL << option);
}

/**
 * @brief Clear a chunk option from controls that will be used by the
 *        next call to @ref rmx_output_media_commit_chunk
 * @param[in] handle     A chunk handle, which can be either of a Static or
 *                       a Dynamic Media Output-Stream
 * @param[in] option     An option to be cleared out
 * @see rmx_output_media_chunk_handle
 */
__RMX_INLINE
void rmx_output_media_clear_chunk_option(const rmx_output_media_chunk_handle *handle,
    rmx_output_commit_option option)
{
    rmx_output_media_chunk_handle_metadata *metadata = (rmx_output_media_chunk_handle_metadata*)(void*)(handle);
    metadata->flags &= ~(1ULL << option);
}

/**
 * @brief Clear out all chunk options from controls that will be used by the
 *        next call to @ref rmx_output_media_commit_chunk
 * @param[in] handle     A chunk handle, which can be either of a Static or
 *                       a Dynamic Media Output-Stream
 * @see rmx_output_media_chunk_handle
 */
__RMX_INLINE
void rmx_output_media_clear_chunk_all_options(const rmx_output_media_chunk_handle *handle)
{
    rmx_output_media_chunk_handle_metadata *metadata = (rmx_output_media_chunk_handle_metadata*)(void*)(handle);
    metadata->flags = 0;
}

/**
 * @brief Send to the wire a Media Output-Stream chunk that is the oldest one
 *        acquired with @ref rmx_output_media_get_next_chunk
 * @param[in] handle  A chunk handle, which can be either of a Static or
 *                    a Dynamic Media Output-Stream
 * @param[in] time    The time value in a format defined
 *                    by @ref rmx_output_commit_option
 * @return Status code as defined by @ref rmx_status
 * @remark #RMX_OK status means that send request was posted to wire.
 * @remark #RMX_NO_CHUNK_TO_SEND status means no chunks left to send; one
 *         shall call @ref rmx_output_media_get_next_chunk and only then shall
 *         retry this call.
 * @remark #RMX_HW_SEND_QUEUE_IS_FULL means that the send queue is full,
 *         one shall repeat the call until the status changes.
 *         another retry shall be performed after
 *         calling @ref rmx_output_media_get_next_chunk
 * @remark #RMX_NO_MEMORY means that for a Dynamic Stream, if a
 *         packet size exceeds the size of a stride.
 * @see rmx_output_media_chunk_handle
 */
__export
rmx_status rmx_output_media_commit_chunk(const rmx_output_media_chunk_handle *handle,
    uint64_t time);

/**
 * @brief Cancel all unsent chunks, which were obtained
          via @ref rmx_output_media_get_next_chunk, but were not yet committed
 * @return Status code as defined by @ref rmx_status
 * @param[in] handle    A chunk handle, which can be either of a Static or
 *                      a Dynamic Media Output-Stream
 * @return Status code as defined by @ref rmx_status
 */
__export
rmx_status rmx_output_media_cancel_unsent_chunks(const rmx_output_media_chunk_handle *handle);

/**
 * @brief Skip ahead to a memory chunk, which is due to be sent in the future
 *
 * This function skips ahead to a first chunk with a future timestamp.
 * The already commited chunks will be sent by the hardware. However, the
 * chunks that were obtained via @ref rmx_output_media_get_next_chunk but
 * not yet commited will be returned back to Rivermax ownership and may not
 * be used by the user application. On next first call to
 * @ref rmx_output_media_commit_chunk the application shall provide the time
 * at which to schedule the sending of the chunk. After that the application
 * may continue the regular flow.
 * @param[in] handle        A chunk handle for a media stream both, which
 *                          can be either Static or Dynamic
 * @param[in] chunks_count  An amount of chunks to skip ahead
 * @return Status code as defined by @ref rmx_status
 * @note Skip ahead must be done to the beginning of a frame. The application
 *       can skip N frames ahead, but the amount to skip is provided in chunks.
 */
__export
rmx_status rmx_output_media_skip_chunks(const rmx_output_media_chunk_handle *handle,
    size_t chunks_count);

//-----------------------------------------------------------------------------
/**@} MediaOutputStream_Chunk */

//=============================================================================
/**@} MediaOutputStream */

/** @addtogroup GenericOutputStream */
/** @{ */
//=============================================================================

/**
 * @brief Initialize a stream descriptor for Generic Output-Stream to be
 *        created with @ref rmx_output_gen_create_stream
 * @param[out] params   A pointer to a Generic Output-Stream descriptor
 * @see rmx_output_gen_stream_params
 */
__export
void rmx_output_gen_init_stream(rmx_output_gen_stream_params *params);

/**
 * @brief Set a maximal amount of packets per chunk to configure a
 *        Generic Output-Stream with @ref rmx_output_gen_create_stream
 * @param[out] params           A Generic Output-Stream creation parameters
 * @param[in]  max_packet_count A maximal amount of packets per chunk
 * @see rmx_output_gen_stream_params
 */
__export
void rmx_output_gen_set_packets_per_chunk(rmx_output_gen_stream_params *params,
    size_t max_packet_count);

/**
 * @brief Set a local address for a Generic Output-Stream,
 *        for @ref rmx_output_gen_create_stream
 * @param[out] params   A Generic Output-Stream creation parameters
 * @param[in]  addr     A valid address value
 * @see rmx_output_gen_stream_params
 */
__export
void rmx_output_gen_set_local_addr(rmx_output_gen_stream_params *params,
    const struct sockaddr *addr);

/**
 * @defgroup GenericOutputStreamOptional Optional Parameters
 */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Set a remote address for a Generic Output-Stream to put it into
 *        a connected mode
 * @param[out] params   A Generic Output-Stream creation parameters
 * @param[in]  addr     A valid address value
 * @see rmx_output_gen_stream_params
 */
__export
void rmx_output_gen_set_remote_addr(rmx_output_gen_stream_params *params,
    const struct sockaddr *addr);

/**
 * @brief Set a maximal amount of sub-blocks per packet, i.e. max size of
 *        IOV vector per packet, to declare a memory boundary for a Generic
 *        Output-Stream, when calling to @ref rmx_output_gen_create_stream
 * @param[out] params                A Generic Output-Stream creation parameters
 * @param[in]  sub_blocks_per_packet A maximal amount of sub-block per packet
 * @note The value of @p max_packet_count is limited by
 *       RMX_MAX_SUB_BLOCKS_PER_MEM_BLOCK
 * @see rmx_output_gen_stream_params
 */
__export
void rmx_output_gen_set_max_sub_blocks(rmx_output_gen_stream_params *params,
    size_t sub_blocks_per_packet);

/**
 * @brief Set a PCP attribute for QoS on Layer 2 (IEEE 802.1Qbb)
 *        for @ref rmx_output_gen_create_stream
 * @param[out] params   A Generic Output-Stream creation parameters
 * @param[in]  pcp      A value of a corresponding Class of Quality
 * @see rmx_output_gen_stream_params
 */
__export
void rmx_output_gen_set_pcp(rmx_output_gen_stream_params *params, uint8_t pcp);

/**
 * @brief Set a DSCP attribute for QoS on Layer 3 (RFC 4594)
 *        for @ref rmx_output_gen_create_stream
 * @param[out] params   A Generic Output-Stream creation parameters
 * @param[in]  dscp     A DSCP value stating the priority
 * @see rmx_output_gen_stream_params
 */
__export
void rmx_output_gen_set_dscp(rmx_output_gen_stream_params *params, uint8_t dscp);

/**
 * @brief Set an Explicit Congestion Notification field value
 *        for @ref rmx_output_gen_create_stream
 * @param[out] params   A Generic Output-Stream creation parameters
 * @param[in]  ecn      An ECN value setting the required method
 * @remark   Default value is "ECN is non-capable", i.e. = 0.
 * @see rmx_output_gen_stream_params
 */
__export
void rmx_output_gen_set_ecn(rmx_output_gen_stream_params *params, uint8_t ecn);

/**
 * @brief Set rate parameters for @ref rmx_output_gen_create_stream, which were
 *        configured with @ref GenericOutputStreamRate "Rate-configuration APIs"
 * @param[out] params   A Generic Output-Stream creation parameters
 * @param[in]  rate     A rate-parameters descriptor
 * @remark If NULL pointer is provided for @p rate than default rate values
 *         (i.e. unrestricted) are restored.
 * @see rmx_output_gen_stream_params
 */
__export
void rmx_output_gen_set_rate(rmx_output_gen_stream_params *params,
    const rmx_output_gen_rate *rate);

//-----------------------------------------------------------------------------
/**@} GenericOutputStreamOptional */

/**
 * @brief Creates a Generic Output-Stream
 * @param[in,out] params   A configured Generic Output-Stream descriptor
 * @param[out]    id       An ID of the created stream
 * @return Status code as defined by @ref rmx_status
 * @see rmx_output_gen_stream_params
 */
__export
rmx_status rmx_output_gen_create_stream(rmx_output_gen_stream_params *params,
    rmx_stream_id *id);

/**
 * @brief Destroys a Generic Output-Stream
 * @param[in] id    An ID of a Generic Output-Stream
 *                  with @ref rmx_output_gen_create_stream
 * @return Status code as defined by @ref rmx_status
 * @see rmx_output_gen_stream_params
 */
__export
rmx_status rmx_output_gen_destroy_stream(rmx_stream_id id);

/** @defgroup GenericOutputStreamRate Rate configuration */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Initialize a descriptor to adjust rate configuration for
 *        a Generic Output-Stream.
 * @param[out] rate A rate-configuration descriptor
 * @param[in]  bps  A requested bit-rate
 * @see rmx_output_gen_rate
 */
__export
void rmx_output_gen_init_rate(rmx_output_gen_rate *rate, uint64_t bps);

/**
 * @brief Set maximal allowed burst as part of rate configuration for
 *        a Generic Output-Stream.
 * @param[out] rate         A rate-configuration descriptor
 * @param[in]  packet_count A maximal amount of packets in a burst
 * @see rmx_output_gen_rate
 */
__export
void rmx_output_gen_set_rate_max_burst(rmx_output_gen_rate *rate,
    size_t packet_count);

/**
 * @brief Set typical size of a packet as part of rate configuration for
 *        a Generic Output-Stream
 * @param[out] rate             A rate-configuration descriptor
 * @param[in]  size_in_bytes    A bytesize of a typical packet
 * @see rmx_output_gen_rate
 */
__export
void rmx_output_gen_set_rate_typical_packet_size(rmx_output_gen_rate *rate,
    size_t size_in_bytes);

/**
 * @brief Update rate configuration for a Generic Output-Stream
 * @param[in] id    An Id of a Generic Output-Stream
 *                  with @ref rmx_output_gen_create_stream
 * @param[in] rate  A rate-configuration descriptor
 * @return Status code as defined by @ref rmx_status
 * @attention One can restore to a full bandwidth by providing NULL pointer
 *            for the rate input argument.
 * @see rmx_output_gen_rate
 */
__export
rmx_status rmx_output_gen_update_rate(rmx_stream_id id,
    const rmx_output_gen_rate *rate);

//-----------------------------------------------------------------------------
/**@} GenericOutputStreamRate */

/** @defgroup GenericOutputStreamChunk Handling Chunks */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Initialize a chunk handle to acquire
 *        (with @ref rmx_output_gen_get_next_chunk) and to commit (with
 *        with @ref rmx_output_gen_commit_chunk) Generic Output-Stream chunks
 * @param[out] handle   A chunk handle of a Generic Output-Stream
 * @param[in]  id       An ID of a Generic Output-Stream
 *                      with @ref rmx_output_gen_create_stream
 * @remark  The same handle can be re-used to acquire and send chunks. Upon
 *          each call to @ref rmx_output_gen_get_next_chunk the handle is set
 *          bound to the newly acquired chunk until it is sent
 *          via @ref rmx_output_gen_commit_chunk
 * @see rmx_output_gen_chunk_handle
 */
__export
void rmx_output_gen_init_chunk_handle(rmx_output_gen_chunk_handle *handle,
    rmx_stream_id id);

/**
 * @brief Acquire the next free chunk for a Generic Output-Stream
 * @param[in,out] handle An initialized chunk handle for a Generic Output-Stream
 * @return Status code as defined by @ref rmx_status
 * @note If there are no available free chunks, status #RMX_NO_FREE_CHUNK is
 *       returned to the caller.
 *       The application shall continue retrying until the status code changes.
 * @see rmx_output_gen_chunk_handle
 */
__export
rmx_status rmx_output_gen_get_next_chunk(rmx_output_gen_chunk_handle *handle);

/**
 * @brief Binds the specified chunk of a Generic Output-Stream to a remote
 *        address, via @ref rmx_output_gen_get_next_chunk
 * @param[out] handle  A chunk descriptor
 * @param[in]  addr    A valid address value
 * @warning It shall be called for every chunk of a stream,
 *          which upon creation wasn't set into a connected mode
 *          via @ref rmx_output_gen_set_remote_addr
 * @warning This API shall be called for a chunk before adding packets, i.e.
 *          prior to @ref rmx_output_gen_append_packet_to_chunk
 * @see rmx_output_gen_stream_params
 */
__export
void rmx_output_gen_set_chunk_remote_addr(rmx_output_gen_chunk_handle *handle,
    const struct sockaddr *addr);

/**
 * @brief Set a chunk option to control a commit of the next chunk
 *        with @ref rmx_output_gen_commit_chunk
 * @param[in] handle  A chunk handle for a Generic Output-Stream
 * @param[in] option  An option controlling the chunk commit
 * @see rmx_output_gen_chunk_handle
 */
__RMX_INLINE
void rmx_output_gen_set_chunk_option(rmx_output_gen_chunk_handle *handle,
    rmx_output_commit_option option)
{
    rmx_output_gen_chunk_handle_metadata *metadata =
        ((rmx_output_gen_chunk_handle_metadata*)(void*)(handle));
    metadata->flags |= (1ULL << option);
}

/**
 * @brief Clear a chunk option from controls that will be used by the
 *        next call to @ref rmx_output_gen_commit_chunk
 * @param[in] handle  A chunk handle for a Generic Output-Stream
 * @param[in] option  An option to be cleared out
 * @see rmx_output_gen_chunk_handle
 */
__RMX_INLINE
void rmx_output_gen_clear_chunk_option(rmx_output_gen_chunk_handle *handle,
    rmx_output_commit_option option)
{
    rmx_output_gen_chunk_handle_metadata *metadata =
      (rmx_output_gen_chunk_handle_metadata*)(void*)(handle);
    metadata->flags &= ~(1ULL << option);
}

/**
 * @brief Clear out all chunk options from controls that will be used by the
 *        next call to @ref rmx_output_gen_commit_chunk
 * @param[in] handle  A chunk handle for a Generic Output-Stream
 * @see rmx_output_gen_chunk_handle
 */
__RMX_INLINE
void rmx_output_gen_clear_chunk_all_options(rmx_output_gen_chunk_handle *handle)
{
    rmx_output_gen_chunk_handle_metadata *metadata =
      (rmx_output_gen_chunk_handle_metadata*)(void*)(handle);
    metadata->flags = 0;
}

/**
 * @brief Append a packet to a chunk of a Generic Output-Stream
 * @param[in,out] handle      A chunk handle of a Generic Output-Stream
 * @param[in]     sub_blocks  An IOV vector composing the packet
 * @param[in]     count       An amount of memory sub-blocks in the IOV vector
 * @return Status code as defined by @ref rmx_status
 * @see rmx_output_gen_chunk_handle
 */
__export
rmx_status rmx_output_gen_append_packet_to_chunk(rmx_output_gen_chunk_handle *handle,
    const rmx_mem_region *sub_blocks, size_t count);

/**
 * @brief Send to the wire a Generic Output-Stream chunk that is the oldest
 *        one acquired with @ref rmx_output_gen_get_next_chunk
 * @param[in] handle  A chunk handle for a Generic Output-Stream
 * @param[in] time    The time value in a format defined
 *                    by @ref rmx_output_commit_option
 * @return Status code as defined by @ref rmx_status
 * @remark #RMX_OK status means that send request was posted to wire.
 * @remark #RMX_NO_CHUNK_TO_SEND status means no chunks left to send; one
 *         shall call @ref rmx_output_gen_get_next_chunk and only then shall
 *         retry this call.
 * @remark #RMX_HW_SEND_QUEUE_IS_FULL means that the send queue is full,
 *         one shall repeat the call until the status changes.
 *         another retry shall be performed after
 *         calling @ref rmx_output_gen_get_next_chunk
 * @see rmx_output_gen_chunk_handle
 */
__export
rmx_status rmx_output_gen_commit_chunk(rmx_output_gen_chunk_handle *handle,
    uint64_t time);

//-----------------------------------------------------------------------------
/**@} GenericOutputStreamChunk */

//=============================================================================
/**@} GenericOutputStream */

/** @addtogroup AllOutputStreams_Common */
/** @{ */
//=============================================================================

/**
 * @brief Get the amount of resident chunks assigned to an Output Stream
 * @param[in]  id      An Id of a stream created with
 *                     either @ref rmx_output_media_create_stream
 *                     or @ref rmx_output_gen_create_stream
 * @param[out] count   A number of chunks
 * @return Status code as defined by @ref rmx_status
 */
__export
rmx_status rmx_output_get_chunk_count(rmx_stream_id id, size_t *count);

/**
 * @brief Update a DSCP attribute of an Output-Stream's traffic
 * @param[in] id   An Id of a stream created with
 *                 either @ref rmx_output_media_create_stream
 *                 or @ref rmx_output_gen_create_stream
 * @param[in] dscp A new DSCP value to adjust priority of the stream's traffic
 */
__export
rmx_status rmx_output_update_dscp(rmx_stream_id id, uint8_t dscp);

/**
 * @brief Update a ECN attribute of an Output-Stream's traffic
 * @param[in] id   An Id of a stream created with
 *                 either @ref rmx_output_media_create_stream
 *                 or @ref rmx_output_gen_create_stream
 * @param[in] ecn  A new ECN value for the stream's traffic
 */
__export
rmx_status rmx_output_update_ecn(rmx_stream_id id, uint8_t ecn);

//=============================================================================
/**@} AllOutputStreams_Common */


#ifdef __cplusplus
}
#endif

#endif /* SRC_RIVERMAX_API_H_ */

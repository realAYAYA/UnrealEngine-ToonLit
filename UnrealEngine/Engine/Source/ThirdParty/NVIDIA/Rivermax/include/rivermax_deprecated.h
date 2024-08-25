/*
 * Copyright © 2017-2023 NVIDIA CORPORATION & AFFILIATES. ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of Nvidia Corporation and its affiliates
 * (the "Company") and all right, title, and interest in and to the software
 * product, including all associated intellectual property rights, are and
 * shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 */

#ifndef SRC_RIVERMAX_DEPRECATED_H_
#define SRC_RIVERMAX_DEPRECATED_H_

#ifndef SRC_RIVERMAX_API_H_
#include "rivermax_defs.h"
#endif

/**
 * Define a Rivermax SDK version
 */
#define RMAX_MAJOR_VERSION  RMX_VERSION_MAJOR
#define RMAX_MINOR_VERSION  RMX_VERSION_MINOR
#define RMAX_PATCH_VERSION  RMX_VERSION_PATCH

typedef uint64_t rmax_cpu_mask_t;

#define RMAX_CPU_SETSIZE 1024
#define RMAX_NCPUBITS    (8 * sizeof(rmax_cpu_mask_t))

/**
 * Maximum number of redundant streams supported by Rivermax.
 */
#define RMAX_MAX_DUP_STREAMS 2

typedef enum {
    RIVERMAX_INIT_CONFIG_NONE            =   0,
    /**
    * @brief Causes Rivermax to handle signal sent by OS, and return RMAX_SIGNAL in API functions.
    *
    * When calling @ref rmax_in_get_next_chunk Rivermax might block depending on the
    * min_chunk_size_in_strides given. When this flag is given Rivermax will catch SIGNINT, SIGTERM
    * in Linux and CTRL_C_EVENT, CTRL_BREAK_EVENT, CTRL_CLOSE_EVENT in Windows and stop blocking
    * Immediately. If user also registered to handle events his handlers will also be called.
    * After handling all API functions will return RMAX_SIGNAL.
    * @note default is off
    */
    RIVERMAX_HANDLE_SIGNAL               =   (1ul << 0), /**< features */

    RIVERMAX_CPU_MASK                    =   (1ul << 1), /**< use @ref rmax_init_config.cpu_mask to set CPU affinity */
} rivermax_init_config_flags;

/**
* @brief Data structure to describe Rivermax clock types
*
* rmax_user_clock_handler:
*     User clock handler allows the user to provide Rivermax an alternative method of querying
*     clock time.
*
*     For example: User may utilize rmax_user_clock_handler to return PTP (TAI) time. By default
*                  Rivermax is using system time (UTC).
*
*     Note:
*     - The handler should return the time in nanosecond units.
*     - Handler Implementation should be time sensitive to avoid performance degradation
*     - User application should use the same time source provided by the handler.
*
* rmax_ptp_clock:
*     - Allows Rivermax using PTP time provided by a DPU Real Time Clock (RTC)
*
*     Note:
*     - Supported for Windows OS VM only.
*/

typedef enum {
    RIVERMAX_SYSTEM_CLOCK       = (1ul << 0),
    RIVERMAX_USER_CLOCK_HANDLER = (1ul << 1),
    RIVERMAX_PTP_CLOCK          = (1ul << 2),
} rmax_clock_types;

struct rmax_clock_t {
    rmax_clock_types clock_type;
    union types {
        struct user_clock { /* RIVERMAX_USER_CLOCK_HANDLER */

            /* User source clock handler */
            uint64_t (*clock_handler)(void *ctx);

            /* Optional user context for the handler.
             * If context is not required, this must be set to NULL */
            void *ctx;
        } rmax_user_clock_handler;

        struct ptp_clock { /* RIVERMAX_PTP_CLOCK */

            /* Device IP address */
            struct in_addr device_ip_addr;

            /* Currently not in use, MUST be set to zero */
            uint8_t domain;
        } rmax_ptp_clock;
    } clock_u;
};

/**
* @brief Data structure to describe CPU mask for the Rivermax internal thread
*
*/
struct rmax_cpu_set_t {
    rmax_cpu_mask_t rmax_bits[RMAX_CPU_SETSIZE / RMAX_NCPUBITS];
};

/**
 * @brief Tuning parameters for Rivermax library.
 *
 * The structure defines the parameters that are used for
 * Rivermax library tuning during Rivermax library @ref rmax_init "initialization".
 *
 */
struct rmax_init_config {
    /* Mask of valid fields in this structure, using bits from
     * @ref rivermax_init_config_flags */
    uint64_t flags;

    /* A bit mask representing the CPUs in the system, used for setting the
     * internal thread affinity */
    struct rmax_cpu_set_t cpu_mask;
};

typedef int rmax_stream_id;
typedef int rmax_out_comp_id;
typedef uint32_t rmax_mkey_id;

/**
 * @brief Status codes
 *
 */
typedef enum {
    /* Operation completed successfully */
    RMAX_OK                                  = RMX_OK,

    /* Failure codes */
    RMAX_ERR_UNKNOWN_ISSUE                   = RMX_UNKNOWN_ISSUE,
    RMAX_ERR_NO_HW_RESOURCES                 = RMX_NO_HW_RESOURCES,
    RMAX_ERR_NO_FREE_CHUNK                   = RMX_NO_FREE_CHUNK,
    RMAX_ERR_NO_CHUNK_TO_SEND                = RMX_NO_CHUNK_TO_SEND,
    RMAX_ERR_HW_SEND_QUEUE_FULL              = RMX_HW_SEND_QUEUE_IS_FULL,
    RMAX_ERR_NO_MEMORY                       = RMX_NO_MEMORY,
    RMAX_ERR_NOT_INITIALAZED                 = RMX_NOT_INITIALIZED,
    RMAX_ERR_NO_DEVICE                       = RMX_NO_DEVICE,
    RMAX_ERR_BUSY                            = RMX_BUSY,
    RMAX_ERR_CANCELLED                       = RMX_CANCELLED,
    RMAX_ERR_HW_COMPLETION_ISSUE             = RMX_HW_COMPLETION_ISSUE,
    RMAX_ERR_LICENSE_ISSUE                   = RMX_LICENSE_ISSUE,
    RMAX_ERR_NO_ATTACH                       = RMX_NO_ATTACH,
    RMAX_ERR_STEERING_ISSUE                  = RMX_STEERING_ISSUE,
    RMAX_ERR_CHECKSUM_ISSUE                  = RMX_CHECKSUM_ISSUE,
    RMAX_ERR_DESTINATION_UNREACHABLE         = RMX_DESTINATION_UNREACHABLE,
    RMAX_ERR_MEMORY_REGISTRATION             = RMX_MEMORY_REGISTRATION,
    /* missing driver or underlying application */
    RMAX_ERR_NO_DEPENDENCY                   = RMX_NO_DEPENDENCY,
    /* for example exceeds link rate limit */
    RMAX_ERR_EXCEEDS_LIMIT                   = RMX_EXCEEDS_LIMIT,
    /* not supported by Rivermax */
    RMAX_ERR_UNSUPPORTED                     = RMX_UNSUPPORTED,
    /* clock type not supported by the device in use */
    RMAX_ERR_CLOCK_TYPE_NOT_SUPPORTED        = RMX_CLOCK_TYPE_NOT_SUPPORTED,
    /* Device doesn't support PTP real time clock */
    RMAX_ERR_UNSUPPORTED_PTP_RT_CLOCK_DEVICE = RMX_UNSUPPORTED_PTP_RT_CLOCK_DEVICE,

    RMAX_ERR_NOT_IMPLEMENTED                 = RMX_NOT_IMPLEMENTED,
    RMAX_ERR_METHOD_NOT_SUPPORTED_BY_STREAM  = RMX_METHOD_NOT_SUPPORTED,

    /* Failure due to invalid function parameter */
    RMAX_INVALID_PARAMETER_MIX               = RMX_INVALID_PARAM_MIX,
    RMAX_ERR_INVALID_PARAM_1                 = RMX_INVALID_PARAM_1,
    RMAX_ERR_INVALID_PARAM_2                 = RMX_INVALID_PARAM_2,
    RMAX_ERR_INVALID_PARAM_3                 = RMX_INVALID_PARAM_3,
    RMAX_ERR_INVALID_PARAM_4                 = RMX_INVALID_PARAM_4,
    RMAX_ERR_INVALID_PARAM_5                 = RMX_INVALID_PARAM_5,
    RMAX_ERR_INVALID_PARAM_6                 = RMX_INVALID_PARAM_6,
    RMAX_ERR_INVALID_PARAM_7                 = RMX_INVALID_PARAM_7,
    RMAX_ERR_INVALID_PARAM_8                 = RMX_INVALID_PARAM_8,
    RMAX_ERR_INVALID_PARAM_9                 = RMX_INVALID_PARAM_9,
    RMAX_ERR_INVALID_PARAM_10                = RMX_INVALID_PARAM_10,
    /* Ctrl-C was captured by Rivermax */
    RMAX_SIGNAL                              = RMX_SIGNAL,

    RMAX_ERR_LAST                            = 600
} rmax_status_t;

/**
 * @brief: Rivermax device configuration flags.
 */
typedef enum rmax_dev_config_flags {
    /** Configure nothing */
    RMAX_DEV_CONFIG_NONE = 0ULL,
    /** Enables RTP dynamic header data split usage for SMPTE-2110-20 protocol.
     *
     * When set, Rivermax will be able to receive packets with RTP application
     * header for SMPTE-2110-20 protocol with varied number of SRDs when doing header data split.
     *
     * Use @ref rmax_device_config_t.ip_address to set the IP address of the device
     * dynamic header data split support will be enabled on.
     *
     * @note: RTP dynamic HDS for SMPTE-2110-20 protocol supported
     * when @ref rmax_device_caps_mask_t.RMAX_DEV_CAP_RTP_DYNAMIC_HDS is supported.
     */
     RMAX_DEV_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS_CONFIG = (1ULL << RMX_DEVICE_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS),
    /** Enables RTP dynamic header data split usage for dynamic number of RTP CSRC fields.
     *
     * When set, Rivermax will be able to receive packets with RTP application
     * header with varied number of RTP CSRC fields when doing header data split.
     *
     * Use @ref rmax_device_config_t.ip_address to set the IP address of the device
     * dynamic header data split support will be enabled on.
     *
     * @note:
     *     - RTP dynamic HDS for dynamic number of CSRC fields in RTP header supported
     *       when @ref rmax_device_caps_mask_t.RMAX_DEV_CAP_RTP_DYNAMIC_HDS is supported.
     *     - This option is supported when enabled with @ref RMAX_DEV_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS_CONFIG
     *       and doesn't stand alone.
     */
     RMAX_DEV_CONFIG_RTP_CSRC_FIELDS_DYNAMIC_HDS_CONFIG = (1ULL << RMX_DEVICE_CONFIG_RTP_CSRC_FIELDS_DYNAMIC_HDS),
     RMAX_DEV_CONFIG_LAST,
} rmax_dev_config_flags_t;

/**
 * @brief: Bit mask used to query needed device capabilities.
 */
typedef enum rmax_device_caps_mask {
    /** Query nothing */
    RMAX_DEV_CAP_NONE = 0x00ULL,
    /** Real Time Clock (RTC) is supported */
    RMAX_DEV_CAP_PTP_CLOCK = (1ULL << RMX_DEVICE_CAP_PTP_CLOCK),
    /** Stream which will place the incoming packets according to RTP sequence number is supported */
    RMAX_DEV_CAP_STREAM_RTP_SEQN_PLACEMENT_ORDER = (1ULL << RMX_DEVICE_CAP_RTP_SEQN_PLACEMENT_ORDER),
    /** Stream which will place the incoming packets according to RTP extended sequence number is supported */
    RMAX_DEV_CAP_STREAM_RTP_EXT_SEQN_PLACEMENT_ORDER = (1ULL << RMX_DEVICE_CAP_RTP_EXTEND_SEQN_PLACEMENT_ORDER),
    /** RTP dynamic header data split is supported */
    RMAX_DEV_CAP_RTP_DYNAMIC_HDS = (1ULL << RMX_DEVICE_CAP_RTP_DYNAMIC_HDS),
    RMAX_DEV_CAP_LAST,
} rmax_device_caps_mask_t;

/**
 * @brief: Data structure to describe device capabilities.
 *
 * @param[in] supported_caps - Each bit represent capability using bits from @ref rmax_device_caps_mask_t.
 * Non zero value means that this capabilities is supported.
 */
typedef struct rmax_device_caps {
    uint64_t supported_caps;
} rmax_device_caps_t;

/**
 * @brief: Data structure to describe Rivermax device configuration.
 *
 * @param[in] config_flags - Configuration flags bit mask of @ref rmax_dev_config_flags_t
 * @param[in] ip_address - IP address of the device.
 */
typedef struct rmax_device_config {
    uint64_t config_flags;
    struct in_addr ip_address;
} rmax_device_config_t;

/**
 * Commit call flags
 */
typedef enum {
    RMAX_DELTA_TIME                           = 1 << 0,

 /*
 * @brief RMAX_PAUSE_AFTER_COMMIT
 *
 * This flag informs Rivermax that there is no more data to send on a stream beyond
 * the current commit. All chunks that have been committed up to and including the call
 * to @ref rmax_out_commit with the @ref RMAX_PAUSE_AFTER_COMMIT flag set are sent as
 * scheduled, after which the stream enters a pause state.
 * Sending may be resumed at any time by issuing a new commit. When resuming sending,
 * the commit timeout must be at a time later than that used when putting the stream in
 * pause state.
 * Example: an audio stream application may pause the sending of new data by calling
 * commit with this flag set.
 */
    RMAX_PAUSE_AFTER_COMMIT                   = 1 << 1,
    RMAX_OUT_COMMIT_SIGNAL                    = 1 << 2,
} rmax_commit_flags_t;

typedef enum rmax_out_gen_stream_init_attr {
    RMAX_OUT_STREAM_REM_ADDR    =   (1ul << 0),
    RMAX_OUT_STREAM_RATE        =   (1ul << 1),
    RMAX_OUT_STREAM_QOS         =   (1ul << 2),
    RMAX_OUT_STREAM_MAX_IOV     =   (1ul << 3),
    RMAX_OUT_STREAM_SIZE        =   (1ul << 4),
    RMAX_OUT_STREAM_ECN         =   (1ul << 5),
} rmax_out_gen_stream_init_attr_t;

/**
 * @param data_ptr address of data memory block
 *    If @data_ptr is NULL Rivermax implicitly allocates the memory blocks.
 * @param app_hdr_ptr address of application header memory block
 *    If app_hdr_ptr is NULL and @ref app_hdr_stride_size is non zero
 *    Rivermax implicitly allocates the memory block for the application header.
 * @param data_size_arr - array describing the actual data size used in each stride
 *    The array values define how many bytes of each stride must be sent to
 *    the wire, the remaining bytes in each stride are ignored.
 *    @ref data_size_arr must contain exactly one entry for each stride of the block.
 *    Example:
 *    Entry #0 holds data size of stride #0 of chunk #0
 *    Entry #1 holds data size of stride #1 of chunk #0
 *    Entry #2 holds data size of stride #0 of chunk #1
 *    Entry #3 holds data size of stride #1 of chunk #1
 *    Entry #N holds data size of stride #0 of chunk #M
 *    Entry #N+1 holds data size of stride #1 of chunk #M
 *    Size of the array must be: @ref chunks_num * @ref chunk_size_in_strides.
 *    Note: data size must be <= stride size of the memory block.
 *    Note: last entry/ies of the last chunk in frame/field can be zero.
 *            In this case the remaining strides are ignored (allows to send a sub
 *            chunk at the end of the frame/field).
 *            A chunk cannot start with zeros in its @ref data_size_arr.
 * @param app_hdr_size_arr same as @ref data_size_arr for application headers
 * @param chunks_num - how many chunks fits in this memory block.
 * @param data_mkey - Memory keys representing the packet payload registered memory regions
 *    for each redundant stream in the same order as streams listed in the SDP file.
 *    Ignored if RMAX_OUT_BUFFER_ATTR_DATA_MKEY_IS_SET flag is not set in
 *    buffer attributes.
 * @param app_hdr_mkey - Memory keys representing the application headers
 *    registered memory regions for each redundant stream in the same order as
 *    streams listed in the SDP file. Ignored if
 *    RMAX_OUT_BUFFER_ATTR_APP_HDR_MKEY_IS_SET flag is not set in buffer
 *    attributes.
 *
 * Note:
 *    Rivermax supports two modes of operation:
 *    1. Static setting of data sizes
 *       In this case the data_size_arr is provided at stream creation time and
 *       not changed thereafter. In this case @ref rmax_out_get_next_chunk is used
 *       to obtain chunks.
 *    2. Dynamic setting of data sizes
 *       This is indicated by NOT providing the data_size_arr at stream creation
 *       (i.e. providing a NULL pointer). In this case @ref rmax_out_get_next_chunk_dynamic
 *       is used to obtain both a new chunk and a pointer to a size array - both of which
 *       must be filled before calling @ref rmax_out_commit.
 *
 *    Static or dynamic mode are determined according to whether the
 *    data_size_arr was or wasn't provided at stream creation time.
 *
 *    With respect to the header sizes array:
 *    1. Static setting of data sizes
 *       At stream creation time, if in the containing @ref rmax_buffer_attr
 *       @ref app_hdr_stride_size != 0, @ref app_hdr_size_arr must point to an array of
 *       header sizes. Otherwise it must be NULL.
 *    2. Dynamic setting of data sizes
 *       At stream creation time,
 *       - If in the containing @ref rmax_buffer_attr @ref app_hdr_stride_size != 0:
 *         - @app_hdr_size_arr may point to an array of header sizes, in which
 *           case the header sizes are set for the duration of the stream and
 *           cannot be amended thereafter.
 *         - @app_hdr_size_arr may be NULL, and is obtained together with the
 *           chunk location and @data_size_arr by the call to
 *           @ref rmax_out_get_next_chunk_dynamic. It must be filled before
 *           calling @ref rmax_out_commit.
 *       - If in the containing @ref rmax_buffer_attr @ref app_hdr_stride_size == 0:
 *         A header size array must not be provided at stream creation time
 *         and it is not provided by @ref rmax_out_get_next_chunk_dynamic.
*/
struct rmax_mem_block {
    void          *data_ptr;
    void          *app_hdr_ptr;
    uint16_t      *data_size_arr;
    uint16_t      *app_hdr_size_arr;
    size_t        chunks_num;
    rmax_mkey_id  data_mkey[RMAX_MAX_DUP_STREAMS];
    rmax_mkey_id  app_hdr_mkey[RMAX_MAX_DUP_STREAMS];
};

/**
 * Rivermax output buffer attributes flags.
 * RMAX_OUT_BUFFER_ATTR_DATA_MKEY_IS_SET - when set Rivermax will skip memory
 *      registration for data pointer and use @ref data_mkey field values from
 *      @ref rmax_mem_block as memory key for data memory block
 * RMAX_OUT_BUFFER_ATTR_APP_HDR_MKEY_IS_SET - when set Rivermax will skip
 *      memory registration for application headers pointer and use @ref
 *      app_hdr_mkey field values from @ref rmax_mem_block as memory key for
 *      header memory block
 */
typedef enum rmax_out_buffer_attr_flags_t {
    RMAX_OUT_BUFFER_ATTR_FLAG_NONE = 0x00,
    RMAX_OUT_BUFFER_ATTR_DATA_MKEY_IS_SET = 0x01,
    RMAX_OUT_BUFFER_ATTR_APP_HDR_MKEY_IS_SET = 0x02,
} rmax_out_buffer_attr_flags;

/**
 * IP packet header Explicit Congestion Notification (ECN) field values.
 */
typedef enum rmax_out_ip_header_ecn_value {
    RMAX_OUT_IP_HDR_ECN_NON_CAPABLE = 0,
    RMAX_OUT_IP_HDR_ECN_CAPABLE_ECT0 = 1,
    RMAX_OUT_IP_HDR_ECN_CAPABLE_ECT1 = 2,
    RMAX_OUT_IP_HDR_ECN_CONGESTION = 3,
} rmax_out_ip_header_ecn_value_t;

/**
 * @param chunk_size_in_strides - size of data\app_header chunks in units of
 *     data\app_header strides
 * @param mem_block_array describes memory block:
 *     memory block pointer, number of chunks in block and block data sizes.
 * @param mem_block_array_len length of mem_block_array
 * @param data_stride_size gap between payloads in memory (to keep alignment)
 * @param app_hdr_stride_size gap between headers in memory
 *     (to keep alignment)
 *     If zero application headers are ignored and only data chunks are handled
 *     by rivermax: rmax_out_get_next_chunk returns pointer only to data chunk and
 *     rmax_out_commit sends only data chunks.
 * @attr_flags - as described in buffer attributes
 */
struct rmax_buffer_attr {
    size_t                chunk_size_in_strides;
    struct rmax_mem_block *mem_block_array;
    size_t                mem_block_array_len;
    uint16_t              data_stride_size;
    uint16_t              app_hdr_stride_size;
    rmax_out_buffer_attr_flags attr_flags;
};

/**
 * @param dscp - The ipv4 DSCP value.
 * @param pcp - The vlan priority.
 * @note if zero ignore
 */
struct rmax_qos_attr {
    uint8_t dscp;
    uint8_t pcp;
};

/*
 *@brief Output stream Completion Queue create attributes
 *@ref comp_q_id - Completion Queue to report commits completions status
 *@ref out_stream_ctx - Output stream context that shall appear
 *in stream completions
 *
 */
struct rmax_out_comp_prop {
    rmax_out_comp_id comp_q_id;
    void *out_stream_ctx;
};

/* @brief Output stream rate limit attributes
 * @ref rate_bps - rate in bps
 * @ref max_burst_in_pkt_num - max allowed burst size in number of packets
 * @ref typical_packet_sz - stream typical packet size in Bytes (optional)
 * @ref min_packet_sz - stream minimal packet size in Bytes (optional)
 * @ref max_packet_sz - stream maximal packet size in Bytes (optional)
 * @note For Generic API typical_packet_sz is mandatory field and
 * min_packet_sz/max_packet_sz are reserved for future use.
 */
struct rmax_out_rate_attr {
    uint64_t rate_bps;
    uint32_t max_burst_in_pkt_num;
    uint16_t typical_packet_sz;
    uint16_t min_packet_sz;
    uint16_t max_packet_sz;
};

/**
 * @struct rmax_out_stream_params
 * @brief Output streams create parameters
 *
 * @param [in] sdp_chr - media attributes in SDP format
 *   List of required SDP attributes:
 *     - protocol version, v=, must be set to zero
 *     - Connection Data c= field
 *     - a=fmtp: format, sampling, width, height, exactframerate,
 *        depth, colorimetry, PM, TP
 *     - a=source-filter
 *     -Media name and transport address, m=, "video" must be provided
 *   List of Optional supported SDP parameters and their defaults:
 *     a=fmtp:
 *     interlace: default 0, segmented- default 0, MAXUDP default: 1460
 *     a=group:DUP.
 *     Notice: if DUP exists then the number of identification tags
 *     (tokens following the "DUP" semantic) has to correspond to the number
 *     of m=video blocks.
 *  Example of SDP string:
 *    char *sdp =
 *    "v=0\n"
 *    "m=video 50000 RTP/AVP 112\n"
 *    "c=IN IP4 239.100.9.10/32\n"
 *    "a=source-filter:incl IN IP4 239.100.9.10 192.168.100.7
 *    "a=fmtp:112 sampling=YCbCr-4:2:2; width=1280; height=720;
 *    "exactframerate=60000/1001; depth=10; TCS=SDR; "
 *    "colorimetry=BT709; PM=2110GPM; TP=2110TPN; "
 *    "SSN=ST2110-20:2017; \n";
 * @param [in] buffer_attr - Buffer attributes.
 * @param [in] qos - QoS attributes.
 * @param [in] ecn - Explicit Congestion Notification field value (default - RMAX_OUT_IP_HDR_ECN_NON_CAPABLE = 0).
 * @param [in] num_packets_per_frame - Number of packets in a single frame.
 * @param [in] media_block_index - Index of the media block in SDP file (starting at zero)
 *   in order of appearance for which to open an output stream.
 *   Media streams may be grouped by the session-level "a=group" attribute.
 *   @note: currently only grouping with the DUP semantic is supported.
 *          In case of DUP video streams, Rivermax creates the duplicate video
 *          stream implicitly and manages the data duplication internally.
 *          From the user's perspective there is always a single output stream.
 *          The index of either of the DUP session's media blocks may be used in
 *          this case (note that each media block, starting at the "m="
 *          directive, has its own index - according to its position within the
 *          SDP file).
 * @param [in] source_port_arr - The source port of the connection for each redundant stream.
 *             Array should be allocated only if user source port allocation is needed.
 * @param [in] source_port_arr_sz - Size of source port array.
 *   @note: Maximum of RMAX_MAX_DUP_STREAMS redundant streams are supported.
 * @param [in] flags - TBD flags.
 */
struct rmax_out_stream_params {
    const char *sdp_chr;
    struct rmax_buffer_attr *buffer_attr;
    struct rmax_qos_attr *qos;
    uint8_t ecn;
    uint32_t num_packets_per_frame;
    uint32_t media_block_index;
    uint16_t *source_port_arr;
    size_t source_port_arr_sz;
    uint64_t flags;
};

/*
 * @brief Generic output streams create parameters
 * Required attributes:
 * @param [in] local_addr - local IP and L4 port to use (today only UDPv4 is supported).
 * If L4 port = 0, Rivermax allocates the port.
 * @param [in] max_chunk_size - max number of packets per chunk
 * @param [in] opt_field_mask - bitmask for optional fields. Use @ref rmax_out_gen_stream_init_attr values to indicate
 *             which of the optional fields Rivermax should relate to.
 * Optional attributes:
 * @param [in] remote_addr - remote IP and L4 port to use (today only UDPv4 is supported)
 *             Note: if @remote_addr is provided only @rmax_out_commit_chunk must be used to commit chunks.
 *             When @remote_addr is not provided only @rmax_out_commit_chunk_to must be used to commit chunks.
 *             Violation of the above rules can lead to undefined behavior.
 * @param [in] rate - rate attributes see rmax_out_rate_attr_t,
 * by default rate limit is not enforced for the stream
 * @param [in] qos - QoS attributes see rmax_qos_attr, by default DSCP or pcp fields are zero
 * @param [in] ecn - Explicit Congestion Notification field value.
 * @param [in] max_iovec_num - max io vector entries number for a single packet, default is 2 entries.
 * @param [in] Stream size in chunks. Default is: (32k / @max_chunk_size).
 * @param [in] comp_q_prop - Completion Queue  properties see rmax_out_comp_prop, by default CQ == NULL and completions are not generated, otherwise completion is generated per selected by application chunks
 * @param [in] flags: TBD flags
 */
struct rmax_out_gen_stream_params {
    struct sockaddr *local_addr;
    size_t max_chunk_size;
    uint64_t opt_field_mask;
    struct sockaddr *remote_addr;
    struct rmax_out_rate_attr rate;
    struct rmax_qos_attr qos;
    uint8_t ecn;
    size_t max_iovec_num;
    size_t size_in_chunks;
    struct rmax_out_rate_attr comp_q_prop;
    uint64_t flags;
};

/*
 * @brief Represents IO vector entry
 * @ref addr - IO vector entry virtual address
 * @ref length - IO vector entry address length
 * @ref mid - registration ID of memory region to which IO vector entry belongs, see @ref rmax_register_memory
*/
struct rmax_iov {
    uint64_t addr;
    uint32_t length;
    rmax_mkey_id mid;
};

/*
 * @brief Represents a packet
 * @ref iovec - packet's IO vector
 * @ref count - number of @ref rmax_iov entries in the packet, must be <= max_iovec_num provided on stream creation
 */
struct rmax_packet {
    struct rmax_iov *iovec;
    size_t count;
};

/*
 * @brief Represents chunks of packets
 * @ref packets - chunk's array of packets
 * @ref size - number of packet in the chunk, must be <= max_chunk_size provided on stream creation
 * @ref chunk_ctx - committed chunk context to return in completion, reserved if Completion Queue is not used
 */
struct rmax_chunk {
    struct rmax_packet *packets;
    size_t size;
    void *chunk_ctx;
};

/**
 * Rivermax input stream types
 * RMAX_RAW_PACKET - application gets access to full raw packet including the network headers
 * RMAX_APP_PROTOCOL_PACKET - application gets access only to L4 payload
 * RMAX_APP_PROTOCOL_PAYLOAD - application gets access to application payload without application header
 *                             This option is not supported currently.
 */
typedef enum rmax_in_stream_type_t {
    RMAX_RAW_PACKET             = RMX_INPUT_RAW_PACKET,
    RMAX_APP_PROTOCOL_PACKET    = RMX_INPUT_APP_PROTOCOL_PACKET,
    RMAX_APP_PROTOCOL_PAYLOAD   = RMX_INPUT_APP_PROTOCOL_PAYLOAD,
} rmax_in_stream_type;

/**
 * RMAX_PACKET_TIMESTAMP_RAW_COUNTER - raw number written by HW representing the HW clock when packet
 * was received
 *  RMAX_PACKET_TIMESTAMP_RAW_NANO - time in nanoseconds when packet was received
 *  RMAX_PACKET_TIMESTAMP_SYNCED - time in nanoseconds synced with PTP GM when packet was received
 */
typedef enum rmax_in_timestamp_format_t {
    RMAX_PACKET_TIMESTAMP_RAW_COUNTER   = RMX_INPUT_TIMESTAMP_RAW_COUNTER,
    RMAX_PACKET_TIMESTAMP_RAW_NANO      = RMX_INPUT_TIMESTAMP_RAW_NANO,
    RMAX_PACKET_TIMESTAMP_SYNCED        = RMX_INPUT_TIMESTAMP_SYNCED,
} rmax_in_timestamp_format;

/**
 * Rivermax input stream creation flags.
 * RMAX_IN_CREATE_STREAM_INFO_PER_PACKET - when set application gets packet information
 * represented by @packet_info for each received packet.
 * Rivermax fills the packet info into @packet_info_arr field of @ref rmax_in_completion.
 */
typedef enum rmax_in_flags_t {
    RMAX_IN_FLAG_NONE = 0x00UL,
    RMAX_IN_CREATE_STREAM_INFO_PER_PACKET = (1UL << (RMX_INPUT_STREAM_CREATE_INFO_PER_PACKET - 1)),
} rmax_in_flags;

/**
 * Rivermax input stream completion flags.
 * RMAX_IN_COMP_FLAG_NONE - indicates there are no more ready pending packets
 * RMAX_IN_COMP_FLAG_MORE - indicates there are more ready pending packets
 */
typedef enum rmax_in_comp_flags_t {
    RMAX_IN_COMP_FLAG_NONE,
    RMAX_IN_COMP_FLAG_MORE = (1UL << RMX_INPUT_COMPLETION_FLAG_MORE),
} rmax_in_comp_flags;

/**
 * @param ptr [in] - memory pointer to use for the buffer, if NULL Rivermax will allocate,
 *     please use @ref rmax_in_query_buffer_size to get the size. The memory must be aligned to
 *     cache line unless one of the following @ref rmax_in_buffer_attr_flags used:
 *         - RMAX_IN_BUFFER_ATTER_STREAM_RTP_SEQN_PLACEMENT_ORDER
 *         - RMAX_IN_BUFFER_ATTER_STREAM_RTP_EXT_SEQN_PLACEMENT_ORDER
 *     flags are being used in which case the memory must be aligned to page size.
 * @param min_size [in] - minimal size that will be received by input stream
 * @param max_size [in] - maximal size that will be received by input stream
 * @param stride_size [out]- offset between the beginning of one data stride to the next
 * @param mkey [in] - Memory key representing the registered memory region if
 *      registered by application.
 */

struct rmax_in_memblock {
   void     *ptr;
   uint16_t min_size;
   uint16_t max_size;
   uint16_t stride_size;
   rmax_mkey_id mkey;
};

/**
 * @brief: Rivermax in buffer attributes flags.
 */
typedef enum rmax_in_buffer_attr_flags_t {
    /** No buffer attribute flags */
    RMAX_IN_BUFFER_ATTER_FLAG_NONE = 0x00ULL,
    /** When set, input stream will locate the incoming packets according to RTP sequence number */
    RMAX_IN_BUFFER_ATTER_STREAM_RTP_SEQN_PLACEMENT_ORDER    = (1UL << RMX_INPUT_STREAM_RTP_SEQN_PLACEMENT_ORDER),
    /** When set, input stream will locate the  incoming packets according to RTP extended sequence number */
    RMAX_IN_BUFFER_ATTER_STREAM_RTP_EXT_SEQN_PLACEMENT_ORDER= (1UL << RMX_INPUT_STREAM_RTP_EXT_SEQN_PLACEMENT_ORDER),
    /**
     * When set, Rivermax skips memory registration and use @ref mkey field from @ref rmax_in_buffer_attr.data
     * as memory key for data memory */
    RMAX_IN_BUFFER_ATTR_BUFFER_DATA_MKEY_IS_SET = 0x04ULL,
    /**
     * When set, Rivermax skips memory registration and use @ref mkey field from @refr max_in_buffer_attr.hdr
     * as memory key for application headers memory block */
    RMAX_IN_BUFFER_ATTR_BUFFER_APP_HDR_MKEY_IS_SET = 0x08ULL,
    /**
     * When set, Rivermax will be able to receive packets with RTP application
     * header for SMPTE-2110-20 protocol with varied number of SRDs when doing header data split.
     *
     * @note: - Device assigned to this buffer must enable RTP dynamic HDS for SMPTE-2110-20 protocol
     *          by setting @ref rmax_dev_config_flags_t.RMAX_DEV_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS_CONFIG. */
    RMAX_IN_BUFFER_ATTR_BUFFER_RTP_SMPTE_2110_20_DYNAMIC_HDS= (1UL << RMX_INPUT_STREAM_RTP_SMPTE_2110_20_DYNAMIC_HDS),
    /**
     * When set, Rivermax will be able to receive packets with RTP application
     * header with varied number of CSRC fields when doing header data split.
     *
     * @note: - Device assigned to this buffer must enable RTP dynamic HDS for dynamic number of CSRC fields
     *          by setting @ref rmax_dev_config_flags_t.RMAX_DEV_CONFIG_RTP_CSRC_FIELDS_DYNAMIC_HDS_CONFIG. */
    RMAX_IN_BUFFER_ATTR_BUFFER_RTP_CSRC_FIELDS_DYNAMIC_HDS = (1UL << RMX_INPUT_STREAM_RTP_CSRC_FIELDS_DYNAMIC_HDS),

    RMAX_IN_BUFFER_ATTR_LAST,
} rmax_in_buffer_attr_flags;

/**
 * @param num_of_elements [in\out] - set by application as hint the requested number of
 *     packets that should be receivable by the input stream. The actual number is returned
 *     by Rivermax.
 * @param data - describes payload part of an incoming packet
 * @param hdr - describes user header part of an incoming packet
 * @note
 *     - When using header splitting with dynamic header size, i.e. @ref hdr.stride_size > 0 and
 *       @ref rmax_in_buffer_attr_flags::RMAX_IN_BUFFER_ATTR_BUFFER_RTP_<*>_DYNAMIC_HDS is set,
 *       @ref hdr.stride_size should be big enough to contain the header and the zero padding that will be added by Rivermax.
 *       @ref hdr.stride_size should be set to the maximum header size possible
 *       by the chosen dynamic header data split configuration.
 *           - @ref rmax_dev_config_flags_t.RMAX_DEV_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS_CONFIG, @ref hdr.stride_size is
 *             12(RTP constant part) + 20(SRDs part) = 32 [bytes].
 *           - @ref rmax_dev_config_flags_t.RMAX_DEV_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS_CONFIG and
 *             @ref rmax_dev_config_flags_t.RMAX_DEV_CONFIG_RTP_CSRC_FIELDS_DYNAMIC_HDS_CONFIG, @ref hdr.stride_size is
 *             12(RTP constant part) + 60(Max of CSRC part) + 20(SRDs part) = 92 [bytes].
 * @attr_flags - describes in buffer attributes, see @ref rmax_in_buffer_attr_flags.
 * @note In case user provides a data pointer and no hdr pointer but does provide
 * header sizes, it is assumed that the length of the data buffer is big enough to hold both the
 * user header and payload. Both sizes can be calculated by @ref rmax_in_query_buffer_size
 *
 */
struct rmax_in_buffer_attr {
    uint32_t                  num_of_elements;
    struct rmax_in_memblock   *data;
    struct rmax_in_memblock   *hdr;
    uint64_t attr_flags;
};

/**
 * Rivermax flow represents a network flow defined by <Src IP, Dst IP, Src Port, Dst Port>
 * Single or multiple network flows can be read from a single input stream.
 * After input stream creation application defines which incoming network flows the NIC
 * will deliver to input stream via rmax_in_attach_flow(flow_attr)- aka application attaches flows
 * to input stream.
 * Flows can be dynamically added to input stream or removed from input stream.
 * In case no networks flows are attached to input stream no traffic will be delivered to it.
 *
 * @ref local_addr - local IP/Port of the local host, i.e. appears as destination IP/Port address
 *     in the incoming packet
 * @ref remote_addr - IP/Port of the peer, i.e. appears as a source IP/Port address in the incoming
 *     packet
 * @ref flow_id - Flow ID is a non-zero value, set by the application and is
 *     associated with a unique flow of a given stream. It is returned upon
 *     packet reception via @rmax_in_packet_info field of @rmax_in_completion.
 *     It allows dispatching the received packet to the right flow (without
 *     parsing network headers), it is essential in case multiple flows are
 *     associated with the same input stream.
 *     To disable the use of Flow ID, the application must set it with a value of zero.
 */
struct rmax_in_flow_attr {
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    uint32_t flow_id;
};

/**
 * @brief: Represents incoming packet info.
 *
 * @param[in] data_size - the size of the packet received.
 * @note:
 *     if header splitting is disabled (hdr_stride_size == 0) data size represents the whole
 *     incoming packet.
 *     Examples:
 *     - for RMAX_RAW_PACKET type data_size represents the entire packet starting from L2 headers.
 *     - for RMAX_APP_PROTOCOL_PACKET data_size represents only L4 payload
 *     If header splitting is enabled (hdr_stride_size > 0) data_size represents only data without
 *     the split headers.
 * @param[in] hdr_size - the size of the header, reserved when hdr_stride_size == 0.
 * @note:
 *        - For RMAX_RAW_PACKET hdr_size represents both network header and application header (e.g. RTP) sizes.
 *        - For RMAX_APP_PROTOCOL_PACKET hdr_size represents only application header (e.g. RTP) sizes,
 *          without network headers that are removed.
 *        - When using header splitting with dynamic header size, i.e. @ref rmax_in_buffer_attr.hdr.stride_size > 0 and
 *          @ref rmax_in_buffer_attr_flags::RMAX_IN_BUFFER_ATTR_BUFFER_RTP_<*>_DYNAMIC_HDS is set,
 *          the header size will be always @ref rmax_in_buffer_attr.hdr.stride_size with
 *          zero padding at the end of the header.
 *          The padding size will be @ref hdr_size - <real header size received>.
 *          When hdr_size is the maximum header size possible by the chosen dynamic header data split configuration,
 *          as mentioned in @ref rmax_in_buffer_attr.hdr documentation.
 * @param[in] flow_id - flow id of the packet provided by application upon rmax_in_attach_flow() call
 * @param[in] timestamp - timestamp of the packet as described in @ref rmax_in_timestamp_format
 */
struct rmax_in_packet_info {
    uint16_t data_size;
    uint16_t hdr_size;
    uint32_t flow_id;
    uint64_t timestamp;
};

/**
 * Completion returned by input stream that describes the incoming packets.
 * @ref chunk_size - number of packet received
 * @ref seqn_first - first packet sequence number
 *     This parameter is valid only if input stream was created with RMAX_IN_BUFFER_ATTER_STREAM_RTP_SEQN_PLACEMENT_ORDER
 *     or with RMAX_IN_BUFFER_ATTER_STREAM_RTP_EXT_SEQN_PLACEMENT_ORDER flag enabled, see @rmax_in_buffer_attr_flags
 * @ref flags - completion flags, @ref rmax_in_comp_flags;
 * @ref reserved - padding
 * @ref timestamp_first - the timestamp of the first packet
 * @ref timestamp_last - the timestamp of the last packet
 * @ref data_ptr - a pointer to the beginning of the payload as described by the user when creating
 *     the stream in @ref rmax_in_buffer_attr. Offset between packets is data_stride_size
 * @ref hdr_ptr - a pointer to the beginning of the header as described by the user when
 *     creating the stream in @ref rmax_in_buffer_attr. Offset between headers is hdr_stride_size
 * @ref packet_info_arr - array of size chunk_size describing each packet properties
 *     This array is valid only if input stream was created with RMAX_IN_CREATE_STREAM_INFO_PER_PACKET
 *     flag enabled.
 */
struct rmax_in_completion {
    uint32_t chunk_size;
    uint32_t seqn_first;
    uint32_t flags;
    uint8_t  reserved[4];
    uint64_t timestamp_first;
    uint64_t timestamp_last;
    void *data_ptr;
    void *hdr_ptr;
    struct rmax_in_packet_info *packet_info_arr;
};

/**
 * Rivermax memory key flags.
 * RMAX_MKEY_FLAG_NONE - no flag is set
 * RMAX_MKEY_FLAG_ZERO_BASED - Memory key address space starts at 0 (zero-based)
 */
typedef enum rmax_memory_key_flags_t {
    RMAX_MKEY_FLAG_NONE = 0x00,
    RMAX_MKEY_FLAG_ZERO_BASED = (1UL << RMX_MEM_REGISTRY_ZERO_BASED),
} rmax_memory_key_flags;

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Get Rivermax SDK version.
 *
 * This routine returns the Rivermax SDK version.
 *
 * @param [out] major_version       Filled with Rivermax SDK major version.
 * @param [out] minor_version       Filled with Rivermax SDK minor version.
 * @param [out] patch_version       Filled with Rivermax SDK patch version.
 */
__export
rmax_status_t rmax_get_version(unsigned int *major_version,
                               unsigned int *minor_version, unsigned int *patch_version);


/**
 * @brief Get Rivermax SDK version as a string.
 *
 * This routine returns the Rivermax SDK version as a string which
 * consists of:"major.minor.patch".
 */
__export
const char *rmax_get_version_string(void);

/**
 * @brief Rivermax library initialization.
 *
 * This routine initializes Rivermax library.
 *
 * @warning This routine must be called before any other Rivermax routine
 * call in the application.
 *
 * This routine checks SDK version compatibility and then initializes the
 * library.
 *
 * @return Status code as defined by @ref rmax_status_t
 * */
 __export
rmax_status_t rmax_init(const struct rmax_init_config *init_config);

/**
 * @brief Performs Rivermax library cleanups.
 *
 * This routine finalizes and releases the resources allocated
 * by Rivermax library.
 *
 * @warning An application cannot call any Rivermax routine
 * after rmax_cleanup call.
 * @return Status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_cleanup(void);

/**
 * @brief: Query device capabilities.
 *
 * @param [in] ip - The IP address of device to query
 * @param [in] caps_mask - Capabilities bit mask, using bits from @ref rmax_device_caps_mask_t
 * @param [out] caps - Structure to store supported device capabilities @ref rmax_device_caps_t
 */
__export
rmax_status_t rmax_device_get_caps(const struct in_addr ip, uint64_t caps_mask, rmax_device_caps_t *caps);

/**
 * @brief: Set device configuration.
 *
 * @param [in] device_config - The device to set.
 */
__export
rmax_status_t rmax_set_device_config(const rmax_device_config_t *device_config);

/**
 * @brief: Unset device configuration.
 *
 * This method unset device configuration previously set by @ref rmax_set_device_config.
 *
 * @param [in] device_config - The device to set.
 */
__export
rmax_status_t rmax_unset_device_config(const rmax_device_config_t *device_config);

#ifdef __linux__
/**
 * @brief request stream notification.
 *
 * This Linux routine request stream notification to generate an event upon read/write.
 * @param id is the id given in @ref rmax_in_create_stream/rmax_out_create_stream
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_request_notification(rmax_stream_id id);
#else
/**
 * @brief request stream notification.
 *
 * This windows routine request stream notification to generate an event upon read/write.
 * @param id the id given in @ref rmax_in_create_stream/rmax_out_create_stream.
 * @param pOverlapped is a pointer to an OVERLAPPED structure that must remain valid for the
 *        duration of the operation.
 * @return Status code as defined by @ref rmax_status_t
 * Note:
 *       If the stream does have available chunks @ref rmax_request_notification returns
 *       RMAX_ERR_BUSY. The user should call @ref rmax_out_get_next_chunk/rmax_in_get_next_chunk to
 *       handle them (notification will not be generated).
 */
__export
rmax_status_t rmax_request_notification(rmax_stream_id id, OVERLAPPED *overlapped);
#endif

#ifdef __linux__
typedef int rmax_event_channel_t;
#else
typedef HANDLE rmax_event_channel_t;
#endif

/**
 * @brief get stream event channel.
 *
 * This routine returns the event channel handle of the stream to get read/write notification event.
 * @param id the id given in @ref rmax_in_create_stream/rmax_out_create_stream.
 * @param event_channel is a pointer to stream event channel handle.
 * Note:
 *       For windows users: it's strongly recommended to use Input/output completion port (IOCP)
 *       GetQueuedCompletionStatus() function to handle the notification.
 * @return Status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_get_event_channel(rmax_stream_id id, rmax_event_channel_t *event_channel);

/**
 * @brief Registers memory region
 *
 * This function registers virtual memory @addr with size @length with device
 * specified by @dev_addr.
 *
 * @note: @ref addr must be aligned to page size and @ref length must be a
 * multiple of page size.
 *
 * @param [in] addr - memory region virtual address
 * @param [in] length - memory region lengths in Bytes
 * @param [in] dev_addr - register memory for dev with dev_addr network address
 * @param [out] id - ID of memory key that represents the registered memory region
 *
 * @return Status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_register_memory(void *addr, size_t length,
                                  struct in_addr dev_addr, rmax_mkey_id *id);

/**
 * @brief Registers memory region (extended)
 *
 * This function registers virtual memory @addr with size @length with device
 * specified by @dev_addr.
 *
 * @note: @ref addr must be aligned to page size and @ref length must be a
 * multiple of page size.
 *
 * @param [in] addr - memory region virtual address
 * @param [in] length - memory region length in Bytes
 * @param [in] dev_addr - register memory for dev with dev_addr network address
 * @param [in] flags - Memory key flags, see @ref rmax_memory_key_flags.
 * @param [out] id - ID of memory key that represents the registered memory region
 *
 * @return Status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_register_memory_ex(void *addr, size_t length,
                                  struct in_addr dev_addr, rmax_memory_key_flags flags,
                                  rmax_mkey_id *id);

/**
 * @brief Unregisters memory region
 *
 * This function unregisters memory region specified by @id from device
 * specified by @dev_addr.
 * @param [in] id - ID of memory key that represents the registered memory region
 * @param [in] dev_addr - unregister memory for dev with dev_addr address
 *
 * @return Status code as defined by @ref rmax_status_t
 * */
__export
rmax_status_t rmax_deregister_memory(rmax_mkey_id id, struct in_addr dev_addr);

/* Output Stream Routines */

/**
 *  @brief Creates an output stream.
 *
 * This routine creates an output stream with media attributes set by @sdp_chr,
 *  with buffer attributes set by @buffer_attr, QoS attributes set by @qos
 *  and number of packets per frame/field set by @rtp_payload_size .
 * @param sdp_chr - media attributes in SDP format
 *   List of required SDP attributes:
 *     - protocol version, v=, must be set to zero
 *     - Connection Data c= field
 *     - a=fmtp: format, sampling, width, height, exactframerate,
 *        depth, colorimetry, PM, TP
 *     - a=source-filter
 *     -Media name and transport address, m=, "video" must be provided
 *   List of Optional supported SDP parameters and their defaults:
 *     a=fmtp:
 *     interlace: default 0, segmented- default 0, MAXUDP default: 1460
 *     a=group:DUP.
 *     Notice: if DUP exists then the number of identification tags
 *     (tokens following the "DUP" semantic) has to correspond to the number
 *     of m=video blocks.
 *  Example of SDP string:
 *    char *sdp =
 *    "v=0\n"
 *    "m=video 50000 RTP/AVP 112\n"
 *    "c=IN IP4 239.100.9.10/32\n"
 *    "a=source-filter:incl IN IP4 239.100.9.10 192.168.100.7
 *    "a=fmtp:112 sampling=YCbCr-4:2:2; width=1280; height=720;
 *    "exactframerate=60000/1001; depth=10; TCS=SDR; "
 *    "colorimetry=BT709; PM=2110GPM; TP=2110TPN; "
 *    "SSN=ST2110-20:2017; \n";
 * @param buffer_attr - buffer attributes
 * @param qos - QoS attributes.
 * @param num_packets_per_frame - number of packets in a single frame
 * @media_block_index - index of the media block in SDP file (starting at zero)
 *   in order of appearance for which to open an output stream.
 *   Media streams may be grouped by the session-level "a=group" attribute.
 *   note: currently only grouping with the DUP semantic is supported.
 *         In case of DUP video streams, Rivermax creates the duplicate video
 *         stream implicitly and manages the data duplication internally.
 *         From the user's perspective there is always a single output stream.
 *         The index of either of the DUP session's media blocks may be used in
 *         this case (note that each media block, starting at the "m="
 *         directive, has its own index - according to its position within the
 *         SDP file).
 * @stream_id - output argument that returns rmax_stream_id to use for this
 *   stream operations
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_out_create_stream(char *sdp_chr,
                                     struct rmax_buffer_attr *buffer_attr,
                                     struct rmax_qos_attr *qos,
                                     uint32_t num_packets_per_frame,
                                     uint32_t media_block_index,
                                     rmax_stream_id *stream_id);

/* @brief Creates an output stream - extended.
 *
 * @note: This is an extended operation prone to changes.
 *
 * @param [in] params - Output stream create parameters.
 * @param [out] id - Returned output stream id.
 *
 * @return status code as defined by @ref rmax_status_t.
 */
__export
rmax_status_t rmax_out_create_stream_ex(struct rmax_out_stream_params *params,
                                        rmax_stream_id *id);

/**
 * @brief Returns source port of the stream
 *
 * @param [in] id - Stream id.
 * @param [out] source_address - Socket source address of the stream.
 * @param [out] destination_address - Socket destination address of the stream.
 *
 * @note: User can supply NULL for source_address or destination_address,
 *        in case one of them is not needed.
 *
 * @return status code as defined by @ref rmax_status_t.
 */
__export
rmax_status_t rmax_out_query_address(rmax_stream_id id, uint32_t media_block_index,
                                        struct sockaddr_in *source_address,
                                        struct sockaddr_in *destination_address);

/*@brief Creates a generic output stream
 * @param [in] output stream create parameters
 * @param [out]returned output stream id
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_out_create_gen_stream(struct rmax_out_gen_stream_params *params,
                                         rmax_stream_id *stream_id);

/* @brief Modifies rate for a generic output stream
 * @param [in] the id given in @ref rmax_out_create_gen_stream
 * @param [in] new rate
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_out_modify_gen_stream_rate(rmax_stream_id stream_id,
                                  struct rmax_out_rate_attr *new_rate);

/**
 * @brief Modifies QoS DSCP attribute for media or generic output stream.
 * Changes will be applied on the next commit.
 * @param [in] stream_id Stream id.
 * @param [in] dscp New QoS DSCP attribute.
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_out_modify_common_stream_qos_dscp(rmax_stream_id stream_id,
                                  uint8_t dscp);

/**
 * @brief Modifies ECN attribute for media or generic output stream.
 * Changes will be applied on the next commit.
 * @param [in] stream_id Stream id.
 * @param [in] ecn New ECN attribute value.
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_out_modify_common_stream_ecn(rmax_stream_id stream_id,
                                  uint8_t ecn);

/**
 * @brief Returns number of chunks in out stream
 * @param [in] the id given in @ref rmax_out_create_gen_stream or in rmax_out_create_stream
 * @param [out] result - number of chunks in @ref stream_id
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_out_query_chunk_num(rmax_stream_id stream_id, size_t *result);

/**
 * @brief Destroys an output stream.
 *
 * This routine destroys an output stream.
 * @param id the id given in @ref rmax_out_create_stream
 * @return Status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_out_destroy_stream(rmax_stream_id id);

/**
 * @brief Sends a chunk.
 *
 * Sends packets written in a chunk that was returned by the oldest
 * rmax_out_get_next_chunk call.
 * @param id the id given in @ref rmax_out_create_stream
 * @param time:
 *   if RMAX_DELTA_TIME is disabled @time represents system/user(@ref rmax_user_clock_handler)/
 *   PTP (@ref rmax_ptp_clock) timestamp in nanoseconds of latest time to send the first packet of the next frame/field
 *   (e.g. SMPTE ST 2110-21 Tvd time).
 *   if RMAX_DELTA_TIME flag is enabled it represents delta in nanoseconds
 *   between the latest time to send the first packet of the next frame/field
 *   (e.g. SMPTE ST 2110-21 Tvd time) and current time.
 *   Must be set only for the first chunk of each frame/field.
 * @param flags see @ref rmax_commit_flags_t.
 * @return Status code as defined by @ref rmax_status_t
 *  Notice:
 *   - RMAX_OK status means that send request of the chunks was posted to HW.
 *   - If no available chunks to post were found (e.g. since
 *   @ref rmax_out_get_next_chunk was not called before) RMAX_ERR_NO_CHUNK_TO_SEND
 *   is returned. Application is responsible in this case to call
 *   ref @rmax_out_commit again after successful call to @ref rmax_out_get_next_chunk
 *   and after data is written to the chunk.
 *   - If HW send queue is full rmax_out_commit returns
 *   RMAX_ERR_HW_SEND_QUEUE_FULL.
 *   Application is responsible to retry in this case till it
 *   gets status != RMAX_ERR_HW_SEND_QUEUE_FULL.
 *   - RMAX_ERR_BUSY may be returned for generic streams if @ref rmax_out_modify_gen_stream_rate
 *   was previously called and there are still outstanding committed chunks from before the request
 *   for rate change queued to be sent to the wire.
 *   It is the application's responsibility to retry sending until all chunks with the previous rate
 *   have been sent, after which RMAX_OUT_COMMIT will complete successfully.
 *   @note In dynamic mode RMAX_ERR_NO_MEMORY is returned if packet sized asked for is more then
 *   @data_stride_size.
 */
__export
rmax_status_t rmax_out_commit(rmax_stream_id id, uint64_t time,
                              rmax_commit_flags_t flags);

/**
 * @brief This function sends chunk of packets
 * @param id - output stream id
 * @param time_ns - represents time in ns when to start transmission of the chunk.
 * if 0 the chunk is sent immediately, when there are previously committed unsent chunks,
 * chunk is sent immediately after previously committed chunks are sent.
 * Notice if packet pacing is enabled the chunk transmission may be delayed also due to rate limit enforcement.
 * @param chunk - chunk to commit
 * @param flags - commit flags see @ref rmax_out_commit_flags
 *
 * @return Status code as defined by @ref rmax_status_t
 * Note: if HW send queue is full rmax_out_commit_chunk returns RMAX_ERR_HW_SEND_QUEUE_FULL.
 * Application is responsible to retry in this case till it gets status != RMAX_ERR_HW_SEND_QUEUE_FULL.
 */
__export
rmax_status_t rmax_out_commit_chunk(rmax_stream_id id, uint64_t time,
                                        struct rmax_chunk *chunk,
                                        rmax_commit_flags_t flags);

/**
 * @brief This function sends chunk of packets
 * @param id - output stream id
 * @param time_ns - represents time in ns when to start transmission of the chunk.
 * if 0 the chunk is sent immediately, when there are previously committed unsent chunks,
 * chunk is sent immediately after previously committed chunks are sent.
 * Notice if packet pacing is enabled the chunk transmission may be delayed also due to rate limit enforcement.
 * @param chunk - chunk to commit
 * @param flags - commit flags see @ref rmax_out_commit_flags
 * @param to_addr - indicates the chunk's destination address
 *
 * @return Status code as defined by @ref rmax_status_t
 * Note: if HW send queue is full rmax_out_commit_chunk returns RMAX_ERR_HW_SEND_QUEUE_FULL.
 * Application is responsible to retry in this case till it gets status != RMAX_ERR_HW_SEND_QUEUE_FULL.
 */
__export
rmax_status_t rmax_out_commit_chunk_to(rmax_stream_id id, uint64_t time,
                                        struct rmax_chunk *chunk,
                                        rmax_commit_flags_t flags,
                                        struct sockaddr *to_addr);

/**
 * @brief Returns the next free chunk.
 *
 * Get a pointer to the beginning of the chunk to write packets with
 * data_stride_size/app_hdr_stride_size gap between them
 * @param id the id given in @ref rmax_out_create_stream
 * @param data_ptr out parameter to where to put the first data to be sent.
 *  Next data should be written at a gap of data_stride_size.
 *  Number of packets to send and their sizes are defined by @ref data_size_arr
 *  field of @ref rmax_mem_block provided on
 *  @ref rmax_out_create_stream call.
 * @param app_hdr_ptr out parameter to where to put the first app header
 *  to be sent. Next header should be written at a gap of app_hdr_stride_size.
 *  Number of headers to create and their sizes are defined by
 *  @ref app_hdr_size_arr field of @ref app_hdr_mem_block_array provided on
 *  @ref rmax_out_create_stream call.
 *  If @ref app_hdr_stride_size of the @ref rmax_buffer_attr provided in the call to
 *  @ref rmax_out_create_stream is 0, app_hdr_ptr must be NULL
 *
 * @return Status code as defined by @ref rmax_status_t
 * Note: If the stream doesn't have available chunks @ref rmax_out_get_next_chunk
 *   returns RMAX_ERR_NO_FREE_CHUNK. The application is responsible to retry in
 *   this case until it gets status != RMAX_ERR_NO_FREE_CHUNK.
 */
__export
rmax_status_t rmax_out_get_next_chunk(rmax_stream_id id, void **data_ptr, void **app_hdr_ptr);

/**
 * @brief Returns the next free chunk for dynamic packets sizes.
 *
 * Get a pointer to the beginning of the chunk to write packets with
 * data_stride_size/app_hdr_stride_size gap between them
 *
 * Note:
 * - dynamic mode is currently limited to ancillary (SMPTE2110-40) streams
 * - dynamic mode must be used if and only if the data_size_arr provided
 *   at stream creation time is NULL
 *
 * @param id the id given in @ref rmax_out_create_stream
 * @param data_ptr out parameter to where to put the first data to be sent.
 *  - Following data should be written at gaps of @ref data_stride_size.
 *  - The number of packets to send is defined by the @ref chunk_size_in_strides
 *  - The size of each packet is defined by its corresponding entry in @ref data_size_arr
 * @param app_hdr_ptr out parameter
 *  If @ref app_hdr_stride_size of the @ref rmax_buffer_attr provided in the call to
 *    @ref rmax_out_create_stream is 0, it is set to NULL
 *  Otherwise, it is set to the address in which to put the first app header to be sent.
 *  - Following headers should be written at gaps corresponding to their entries in
 *    @ref app_hdr_stride_size
 *  - The number of headers to send is defined by the @ref chunk_size_in_strides
 *  - The size of each header is defined by its corresponding entry in @ref hdr_stride_size
 * @param chunk_size_in_strides - how many strides fit in this memory block
 * @param data_size_arr - out array for describing the actual data size used in each stride
 *    The array values define how many bytes of each stride must be sent to
 *    the wire, the remaining bytes in each stride are ignored.
 *    The values must be filled by the user before calling @ref rmax_out_commit and
 *    must contain exactly one entry per each stride of the chunk.
 *    The values may not be zero unless they represent the proper suffix of
 *    the last chunk of a frame/field in which case these remaining strides are ignored.
 *    The chunk cannot start with zeros.
 *
 *    Example:
 *    Entry #0 holds data size of stride #0 of the chunk
 *    Entry #1 holds data size of stride #1 of the chunk
 *    Entry #N holds data size of stride #N of the chunk
 *
 *    The Size of the array is defined by: @ref chunk_size_in_strides which must be <= the
 *    @ref chunk_size_in_strides provided during @ref rmax_out_create_stream.
 * @param app_hdr_size_arr same as @ref data_size_arr for application headers
 *    If the app_hdr_stride_size provided during the call to @ref rmax_out_create_stream is
 *    zero, the user should set this parameter to NULL.
 *    Otherwise,
 *    - If the array was supplied statically during the call to @ref rmax_out_create_stream
 *      Rivermax already has the header size information and the user should set this
 *      parameter to NULL.
 *    - If the array was NOT supplied statically during the call to @ref rmax_out_create_stream
 *      Rivermax allocates an array for the header sizes and sets its location at
 *      *app_hdr_stride_size for the user to fill before calling @ref rmax_out_commit.
 * @return Status code as defined by @ref rmax_status_t
 *
 * Note: If the stream doesn't have available chunks @ref rmax_out_get_next_chunk_dynamic
 *   returns RMAX_ERR_NO_FREE_CHUNK. The application is responsible to retry in
 *   this case until it gets status != RMAX_ERR_NO_FREE_CHUNK.
 */
__export
rmax_status_t rmax_out_get_next_chunk_dynamic(rmax_stream_id id, void **data_ptr, void **app_hdr_ptr,
                                              size_t chunk_size_in_strides, uint16_t **data_size_arr,
                                              uint16_t **app_hdr_size_arr);

/**
 * @brief Cancels all unsent chunks.
 *
 * Cancels all requested (via @ref max_get_next_chunk) but not committed chunks
 * @returns Status code as defined by @ref rmax_status_t
 * @param [in] id - the id of the stream to be paused as returned by @ref rmax_out_create_stream
 */
__export
rmax_status_t rmax_out_cancel_unsent_chunks(rmax_stream_id id);

/*
* @brief Skip ahead to a memory chunk which is due to be sent in the future
*
* This function allows the application to skip ahead to some chunk which is
* destined to be sent in the future.
* Chunks which have already been committed by previous calls to
* @rmax_out_commit will be still sent by the hardware.
* Chunks which have been provided to the application by previous calls to
* @ref rmax_out_get_next_chunk but have not yet been committed will be
* revoked to Rivermax ownership and may not be used by the application.
* In subsequent calls to @ref rmax_out_get_next_chunk following the call to
* this function Rivermax will return chunks starting from num_chunks chunks ahead.
* In the subsequent call to @ref rmax_out_commit the application must
* provide the time at which to schedule the sending of the chunk.
* Thereafter, obtaining and committing chunks by the application continues
* as usual.
*
* @param [in] id - the stream id obtained by @ref rmax_out_create_stream.
* @param [in] num_chunks - the amount of chunks to skip ahead
* @return Status code as defined by @ref rmax_status_t
* Note: Skip ahead must be done to the beginning of a frame
*       The application can skip N frames ahead, but the amount to skip
*       is provided in chunk resolution.
*/
__export
rmax_status_t rmax_out_skip_chunks(rmax_stream_id id, uint64_t num_chunks);

/* Input Stream Routines */

/**
 * @brief return the memory size that matched the given parameters
 * This function can be used to calculate the memory size needed by Rivermax to create
 * the rx session. After getting the size the user can allocate this amount and pass the pointer
 * to Rivermax using the @ref rmax_in_create_stream function in the @ref buffer_attr struct
 * @param [in] rx_type - the stream type
 * @param [in] flags - as described in rmax_in_flags
 * @param [in] local_nic_addr - IP address of the local NIC. Rivermax uses @local_nic_addr
 *     in order to resolve the local NIC.
 * @param [in] buffer_attr - the buffer parameters
 * @param [out] payload_size - the payload buffer size to use
 * @param [out] header_size - if not NULL Rivermax will return the size of the user header buffer
 *     to allocate. Otherwise, the returned payload_size size will be large enough to include both
 *     header and payload sizes.
 *     Rivermax will know to use this size if user provides a pointer in hdr_ptr or the
 *     @ref buffer_attr when creating the session.
 *     In case of rx_type = @ref rmax_in_stream_type.RMAX_APP_PROTOCOL_PAYLOAD, it should be
 *     set to NULL, the return payload_size in this case will include only payload size.
 * @return status code as defined by @ref rmax_status_t
 * @note the pointer given in @ref rmax_in_create_stream must be aligned to cache line unless
 *     - RMAX_IN_BUFFER_ATTER_STREAM_RTP_SEQN_PLACEMENT_ORDER
 *     - RMAX_IN_BUFFER_ATTER_STREAM_RTP_EXT_SEQN_PLACEMENT_ORDER
 *     flags are being used, in which case memory must be aligned to page size, see @rmax_in_buffer_attr_flags
 */
__export
rmax_status_t rmax_in_query_buffer_size(rmax_in_stream_type rx_type,
                                        struct sockaddr_in *local_nic_addr,
                                        struct rmax_in_buffer_attr *buffer_attr,
                                        size_t *payload_size, size_t *header_size);

/**
 * @brief Creates an input stream.
 * @param [in] type - input stream type, see @ref rmax_in_stream_type
 * @param [in] local_nic_addr - IP address of the local NIC. Rivermax uses @local_nic_addr in order
 *     to resolve the local NIC. Input stream is configured on that NIC.
 * @param [in] buffer_attr - buffer attributes as described in @ref rmax_in_buffer_attr
 * @param [in] timestamp_format - the timestamp format as described in @ref rmax_in_timestamp_format
 * @param [in] flags - as described in rmax_in_flags
 * @stream_id [out] - output argument that returns rmax_stream_id to use for this
 *   stream operations
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_in_create_stream(rmax_in_stream_type rx_type, struct sockaddr_in *local_nic_addr,
                                    struct rmax_in_buffer_attr *buffer_attr,
                                    rmax_in_timestamp_format timestamp_format, rmax_in_flags flags,
                                    rmax_stream_id *stream_id);

/**
 * @brief attach a network flow to a Rivermax stream
 * @param [in] id - the id given in @ref rmax_in_create_stream
 * @param [in] flow_attr - network flow attributes see @rmax_in_flow_attr
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_in_attach_flow(rmax_stream_id id, const struct rmax_in_flow_attr *flow_attr);

/**
 * @brief detach a network from a Rivermax stream
 * @param [in] id -the id given in @ref rmax_in_create_stream
 * @param [in] flow_attr - network flow attributes see @rmax_in_flow_attr
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_in_detach_flow(rmax_stream_id id, const struct rmax_in_flow_attr *flow_attr);

/**
 * @brief gets a pointer to the input stream buffer containing data received by Rivermax
 * @param [in] id - the id given in @ref rmax_in_create_stream
 * @param [in] min_chunk_size_in_strides - minimal number of packets that input stream must return.
 *     Rivermax will not return to application until @min_chunk_size_in_strides number
 *     of packets are ready or exception occurred.
 * @param [in] max_chunk_size_in_strides - maximal number of packets to return.
 * @param [in] timeout - specifies the number of usecs that rmax_in_get_next_chunk would do
 *     busy wait (polling) for reception of at least `min_chunk_size_in_strides` number of packets.
 * @param [in] flags - not supported yet
 * @param [in/out] rx_completion - holds incoming packets info as described in @ref rmax_in_completion
 * @return status code as defined by @ref rmax_status_t
 * @note: Chunk remains valid only until the next call to rmax_in_get_next_chunk()
 * @note: this function will block until min_chunk_size_in_strides packets arrive or processes is
 *     interrupted by signal.
 * @note calling this function without attach will return RMAX_ERR_NO_ATTACH
 * @note call can return a failure code but still contain data:
 *      e.g. if Rivermax detected a checksum issue it will return RMAX_ERR_CHECKSUM_ISSUE
 *      but all packets up to this packet will contain valid data
 */
__export
rmax_status_t rmax_in_get_next_chunk(rmax_stream_id id, size_t min_chunk_size_in_strides,
                                     size_t max_chunk_size_in_strides, int timeout, int flags,
                                     struct rmax_in_completion *rx_completion);
/**
 * @brief Destroys an input stream.
 * @param id the id given in @ref rmax_in_create_stream
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_in_destroy_stream(rmax_stream_id id);


typedef enum rmax_time_type_t {
    RMAX_CLOCK_PTP        = RMX_TIME_PTP,
    RMAX_CLOCK_RAW_NANO   = RMX_TIME_RAW_NANO,
    RMAX_CLOCK_RAW_CYCLES = RMX_TIME_RAW_CYCLES,
} rmax_time_type;

/**
 * @brief Set global clock
 *
 * @param [in] clock - @ref rmax_clock_t structure to describe clock configuration
 */
__export
rmax_status_t rmax_set_clock(struct rmax_clock_t *clock);

/**
* @brief Get time
* @param clock_type as defined by @ref rmax_time_type
* @param return time value
* @return status code as defined by @ref rmax_status_t
*/
__export
rmax_status_t rmax_get_time(rmax_time_type time_type, uint64_t *p_time);

typedef struct rmax_ip_addr {
    uint16_t family;
    union {
        struct in_addr ipv4_addr;
        uint8_t reserved[16];
    } addr;
} rmax_ip_addr_t;

/**
 * @brief Data structure to describe device
 */
typedef struct rmax_device {
    /* Interface device name */
    const char *ifa_name;
    /* Array of IP addresses for device @ref rmax_ip_addr_t */
    const rmax_ip_addr_t *ip_addrs;
    /* Size of array IP addresses */
    size_t ip_addrs_count;
    /* Byte array with device MAC address */
    const uint8_t *mac_addr;
    /* Device ID */
    uint32_t device_id;
    /* Device serial number */
    const char *serial_number;
} rmax_device_t;

/**
 * @brief Obtain a list of devices supported by Rivermax
 * @param [out] supported_devices - pointer to an array of @ref rmax_device_t
 *     devices supported by Rivermax
 * @param [out] num_devices - length of the supported devices array
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_get_supported_devices_list(rmax_device_t **supported_devices, size_t *num_devices);

/**
 * @brief Free a supported devices list array obtained by @ref rmax_get_supported_devices_list
 * @param [in] supported_devices - pointer to an array of supported devices obtained by @ref rmax_get_supported_devices_list
 * @return status code as defined by @ref rmax_status_t
 */
__export
rmax_status_t rmax_free_supported_devices_list(rmax_device_t *supported_devices);

#ifdef __cplusplus
}
#endif

#endif /* SRC_RIVERMAX_DEPRECATED_H_ */


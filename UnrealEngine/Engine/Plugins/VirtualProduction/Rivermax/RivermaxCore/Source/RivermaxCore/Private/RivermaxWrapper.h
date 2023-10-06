// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RivermaxHeader.h"


namespace UE::RivermaxCore::Private
{
	typedef rmax_status_t(*PFN_RMAX_GET_VERSION) (unsigned* major_version, unsigned* minor_version, unsigned* release_number, unsigned* build);
	typedef const char* (*PFN_RMAX_GET_VERSION_STRING) (void);
	typedef rmax_status_t(*PFN_RMAX_INIT_VERSION) (unsigned api_major_version, unsigned api_minor_version, struct rmax_init_config* init_config);
	typedef rmax_status_t(*PFN_RMAX_CLEANUP) (void);
	typedef rmax_status_t(*PFN_RMAX_DEVICE_GET_CAPS) (const struct in_addr ip, uint64_t caps_mask, rmax_device_caps_t* caps);
	typedef rmax_status_t(*PFN_RMAX_SET_DEVICE_CONFIG) (const rmax_device_config_t* device_config);
	typedef rmax_status_t(*PFN_RMAX_UNSET_DEVICE_CONFIG) (const rmax_device_config_t* device_config);
	typedef rmax_status_t(*PFN_RMAX_REQUEST_NOTIFICATION) (rmax_stream_id id);
	typedef rmax_status_t(*PFN_RMAX_REQUEST_NOTIFICATION_OVERLAPPED) (rmax_stream_id id, OVERLAPPED* overlapped);
	typedef rmax_status_t(*PFN_RMAX_GET_EVENT_CHANNEL) (rmax_stream_id id, rmax_event_channel_t* event_channel);
	typedef rmax_status_t(*PFN_RMAX_REGISTER_MEMORY) (void* addr, size_t length, struct in_addr dev_addr, rmax_mkey_id* id);
	typedef rmax_status_t(*PFN_RMAX_REGISTER_MEMORY_EX) (void* addr, size_t length, struct in_addr dev_addr, rmax_memory_key_flags flags, rmax_mkey_id* id);
	typedef rmax_status_t(*PFN_RMAX_DEREGISTER_MEMORY) (rmax_mkey_id id, struct in_addr dev_addr);
	typedef rmax_status_t(*PFN_RMAX_OUT_CREATE_STREAM) (char* sdp_chr, struct rmax_buffer_attr* buffer_attr, struct rmax_qos_attr* qos, uint32_t num_packets_per_frame, uint32_t media_block_index, rmax_stream_id* stream_id);
	typedef rmax_status_t(*PFN_RMAX_OUT_CREATE_STREAM_EX) (struct rmax_out_stream_params* params, rmax_stream_id* id);
	typedef rmax_status_t(*PFN_RMAX_OUT_QUERY_ADDRESS) (rmax_stream_id id, uint32_t media_block_index, struct sockaddr_in* source_address, struct sockaddr_in* destination_address);
	typedef rmax_status_t(*PFN_RMAX_OUT_CREATE_GEN_STREAM) (struct rmax_out_gen_stream_params* params, rmax_stream_id* stream_id);
	typedef rmax_status_t(*PFN_RMAX_OUT_MODIFY_GEN_STREAM_RATE) (rmax_stream_id stream_id, struct rmax_out_rate_attr* new_rate);
	typedef rmax_status_t(*PFN_RMAX_OUT_QUERY_CHUNK_NUM) (rmax_stream_id stream_id, size_t* result);
	typedef rmax_status_t(*PFN_RMAX_OUT_DESTROY_STREAM) (rmax_stream_id id);
	typedef rmax_status_t(*PFN_RMAX_OUT_COMMIT) (rmax_stream_id id, uint64_t time, rmax_commit_flags_t flags);
	typedef rmax_status_t(*PFN_RMAX_OUT_COMMIT_CHUNK) (rmax_stream_id id, uint64_t time, struct rmax_chunk* chunk, rmax_commit_flags_t flags);
	typedef rmax_status_t(*PFN_RMAX_OUT_COMMIT_CHUNK_TO) (rmax_stream_id id, uint64_t time, struct rmax_chunk* chunk, rmax_commit_flags_t flags, struct sockaddr* to_addr);
	typedef rmax_status_t(*PFN_RMAX_OUT_GET_NEXT_CHUNK) (rmax_stream_id id, void** data_ptr, void** app_hdr_ptr);
	typedef rmax_status_t(*PFN_RMAX_OUT_GET_NEXT_CHUNK_DYNAMIC) (rmax_stream_id id, void** data_ptr, void** app_hdr_ptr, size_t chunk_size_in_strides, uint16_t** data_size_arr, uint16_t** app_hdr_size_arr);
	typedef rmax_status_t(*PFN_RMAX_OUT_CANCEL_UNSENT_CHUNKS) (rmax_stream_id id);
	typedef rmax_status_t(*PFN_RMAX_OUT_SKIP_CHUNKS) (rmax_stream_id id, uint64_t skip);
	typedef rmax_status_t(*PFN_RMAX_IN_QUERY_BUFFER_SIZE) (rmax_in_stream_type rx_type, struct sockaddr_in* local_nic_addr, struct rmax_in_buffer_attr* buffer_attr, size_t* payload_size, size_t* header_size);
	typedef rmax_status_t(*PFN_RMAX_IN_CREATE_STREAM) (rmax_in_stream_type rx_type, struct sockaddr_in* local_nic_addr, struct rmax_in_buffer_attr* buffer_attr, rmax_in_timestamp_format timestamp_format, rmax_in_flags flags, rmax_stream_id* stream_id);
	typedef rmax_status_t(*PFN_RMAX_IN_ATTACH_FLOW) (rmax_stream_id id, struct rmax_in_flow_attr* flow_attr);
	typedef rmax_status_t(*PFN_RMAX_IN_DETACH_FLOW) (rmax_stream_id id, struct rmax_in_flow_attr* flow_attr);
	typedef rmax_status_t(*PFN_RMAX_IN_GET_NEXT_CHUNK) (rmax_stream_id id, size_t min_chunk_size_in_strides, size_t max_chunk_size_in_strides, int timeout, int flags, struct rmax_in_completion* rx_completion);
	typedef rmax_status_t(*PFN_RMAX_IN_DESTROY_STREAM) (rmax_stream_id id);
	typedef rmax_status_t(*PFN_RMAX_SET_CLOCK) (struct rmax_clock_t* clock);
	typedef rmax_status_t(*PFN_RMAX_GET_TIME) (rmax_time_type time_type, uint64_t* p_time);
	typedef rmax_status_t(*PFN_RMAX_GET_SUPPORTED_DEVICES_LIST) (rmax_device_t** supported_devices, size_t* num_devices);
	typedef rmax_status_t(*PFN_RMAX_FREE_SUPPORTED_DEVICES_LIST) (rmax_device_t* supported_devices);


	struct RIVERMAX_API_FUNCTION_LIST
	{
		PFN_RMAX_GET_VERSION rmax_get_version;
		PFN_RMAX_GET_VERSION_STRING rmax_get_version_string;
		PFN_RMAX_INIT_VERSION rmax_init_version;
		PFN_RMAX_CLEANUP rmax_cleanup;
		PFN_RMAX_DEVICE_GET_CAPS rmax_device_get_caps;
		PFN_RMAX_SET_DEVICE_CONFIG rmax_set_device_config;
		PFN_RMAX_UNSET_DEVICE_CONFIG rmax_unset_device_config;
		PFN_RMAX_REQUEST_NOTIFICATION rmax_request_notification;
		//PFN_RMAX_REQUEST_NOTIFICATION_OVERLAPPED rmax_request_notification; not supported because of overload
		PFN_RMAX_GET_EVENT_CHANNEL rmax_get_event_channel;
		PFN_RMAX_REGISTER_MEMORY rmax_register_memory;
		PFN_RMAX_REGISTER_MEMORY_EX rmax_register_memory_ex;
		PFN_RMAX_DEREGISTER_MEMORY rmax_deregister_memory;
		PFN_RMAX_OUT_CREATE_STREAM rmax_out_create_stream;
		PFN_RMAX_OUT_CREATE_STREAM_EX rmax_out_create_stream_ex;
		PFN_RMAX_OUT_QUERY_ADDRESS rmax_out_query_address;
		PFN_RMAX_OUT_CREATE_GEN_STREAM rmax_out_create_gen_stream;
		PFN_RMAX_OUT_MODIFY_GEN_STREAM_RATE rmax_out_modify_gen_stream_rate;
		PFN_RMAX_OUT_QUERY_CHUNK_NUM rmax_out_query_chunk_num;
		PFN_RMAX_OUT_DESTROY_STREAM rmax_out_destroy_stream;
		PFN_RMAX_OUT_COMMIT rmax_out_commit;
		PFN_RMAX_OUT_COMMIT_CHUNK rmax_out_commit_chunk;
		PFN_RMAX_OUT_COMMIT_CHUNK_TO rmax_out_commit_chunk_to;
		PFN_RMAX_OUT_GET_NEXT_CHUNK rmax_out_get_next_chunk;
		PFN_RMAX_OUT_GET_NEXT_CHUNK_DYNAMIC rmax_out_get_next_chunk_dynamic;
		PFN_RMAX_OUT_CANCEL_UNSENT_CHUNKS rmax_out_cancel_unsent_chunks;
		PFN_RMAX_OUT_SKIP_CHUNKS rmax_out_skip_chunks;
		PFN_RMAX_IN_QUERY_BUFFER_SIZE rmax_in_query_buffer_size;
		PFN_RMAX_IN_CREATE_STREAM rmax_in_create_stream;
		PFN_RMAX_IN_ATTACH_FLOW rmax_in_attach_flow;
		PFN_RMAX_IN_DETACH_FLOW rmax_in_detach_flow;
		PFN_RMAX_IN_GET_NEXT_CHUNK rmax_in_get_next_chunk;
		PFN_RMAX_IN_DESTROY_STREAM rmax_in_destroy_stream;
		PFN_RMAX_SET_CLOCK rmax_set_clock;
		PFN_RMAX_GET_TIME rmax_get_time;
		PFN_RMAX_GET_SUPPORTED_DEVICES_LIST rmax_get_supported_devices_list;
		PFN_RMAX_FREE_SUPPORTED_DEVICES_LIST rmax_free_supported_devices_list;
	};

}
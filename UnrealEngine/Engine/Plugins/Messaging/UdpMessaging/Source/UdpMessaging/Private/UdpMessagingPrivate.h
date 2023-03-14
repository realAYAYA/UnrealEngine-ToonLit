// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Logging/LogMacros.h"


/** Declares a log category for this module. */
DECLARE_LOG_CATEGORY_EXTERN(LogUdpMessaging, Log, All);


/** Defines the default IP endpoint for multicast traffic. */
#define UDP_MESSAGING_DEFAULT_MULTICAST_ENDPOINT FIPv4Endpoint(FIPv4Address(230, 0, 0, 1), 6666)

/** Defines the maximum number of annotations a message can have. */
#define UDP_MESSAGING_MAX_ANNOTATIONS 128

/** Defines the maximum number of recipients a message can have. */
#define UDP_MESSAGING_MAX_RECIPIENTS 1024

/** Defines the desired size of socket receive buffers (in bytes). */
#define UDP_MESSAGING_RECEIVE_BUFFER_SIZE 2 * 1024 * 1024

/** Define the desired size for the message segments */
#define UDP_MESSAGING_SEGMENT_SIZE 1024

/**
 * Defines the protocol version of the UDP message transport.
 * @note When changing the version, ensure to update the serialization/deserialization code in UdpSerializeMessageTask.cpp/UdpDeserializedMessage.cpp and the supported versions in FUdpMessageProcessor::Init().
 */
#define UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION 16

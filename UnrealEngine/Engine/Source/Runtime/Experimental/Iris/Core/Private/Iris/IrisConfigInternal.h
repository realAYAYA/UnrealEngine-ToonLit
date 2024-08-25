// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/IrisConfig.h"

// Log config for Iris subsystems
#ifndef UE_NET_ENABLE_REPLICATIONREADER_LOG
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define UE_NET_ENABLE_REPLICATIONREADER_LOG 0
#else
#	define UE_NET_ENABLE_REPLICATIONREADER_LOG 0
#endif
#endif


#ifndef UE_NET_ENABLE_REPLICATIONWRITER_LOG
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define UE_NET_ENABLE_REPLICATIONWRITER_LOG 0
#else
#	define UE_NET_ENABLE_REPLICATIONWRITER_LOG 0
#endif
#endif

// Misc config
#ifndef UE_NET_VALIDATE_REPLICATION_RECORD
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define UE_NET_VALIDATE_REPLICATION_RECORD 0
#else
#	define UE_NET_VALIDATE_REPLICATION_RECORD 1
#endif 
#endif

// When enabled will add bit stream guards to detect overflow or serialization errors.
// Increases bandwidth and replication time so should only be enabled for local or unit testing.
#ifndef UE_NET_USE_READER_WRITER_SENTINEL
#	define UE_NET_USE_READER_WRITER_SENTINEL 0
#endif

#ifndef UE_NET_VALIDATE_DC_BASELINES
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define UE_NET_VALIDATE_DC_BASELINES 0
#else
#	define UE_NET_VALIDATE_DC_BASELINES 1
#endif 
#endif

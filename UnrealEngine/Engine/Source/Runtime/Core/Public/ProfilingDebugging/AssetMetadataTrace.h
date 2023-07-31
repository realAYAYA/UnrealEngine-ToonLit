// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetadataTrace.h"
#include "ProfilingDebugging/StringsTrace.h"
#include "Trace/Detail/EventNode.h"
#include "Trace/Trace.h"

namespace UE { namespace Trace { class FChannel; } }

////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(UE_TRACE_ASSET_METADATA_ENABLED) 
	#define UE_TRACE_ASSET_METADATA_ENABLED UE_TRACE_METADATA_ENABLED
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
#if UE_TRACE_ASSET_METADATA_ENABLED

/**
 * Channel that asset meta data is output on
 */
CORE_API UE_TRACE_CHANNEL_EXTERN(AssetMetadataChannel);

/**
 * Metadata scope to instrument operations belonging to a certain asset.
 */
CORE_API UE_TRACE_METADATA_EVENT_BEGIN_EXTERN(Asset)
	UE_TRACE_METADATA_EVENT_REFERENCE_FIELD(Strings, FName, Name)
	UE_TRACE_METADATA_EVENT_REFERENCE_FIELD(Strings, FName, Class)
UE_TRACE_METADATA_EVENT_END()

/**
 * Utility macro to use asset scope
 * @note When using this macro in an module outside of Core, make sure a dependency to the "TraceLog" module
 *		 is added.
 */
#define UE_TRACE_METADATA_SCOPE_ASSET(Object, ObjClass) \
	auto MetaNameRef = UE_TRACE_CHANNELEXPR_IS_ENABLED(MetadataChannel) ? FStringTrace::GetNameRef(Object->GetFName()) : UE::Trace::FEventRef32(0,0);\
	auto ClassNameRef = UE_TRACE_CHANNELEXPR_IS_ENABLED(MetadataChannel) ? FStringTrace::GetNameRef(ObjClass->GetFName()) : UE::Trace::FEventRef32(0,0);\
	UE_TRACE_METADATA_SCOPE(Asset, AssetMetadataChannel)\
		<< Asset.Name(MetaNameRef)\
		<< Asset.Class(ClassNameRef);

#else

#define UE_TRACE_METADATA_SCOPE_ASSET(...)

#endif // UE_TRACE_ASSET_METADATA_ENABLED

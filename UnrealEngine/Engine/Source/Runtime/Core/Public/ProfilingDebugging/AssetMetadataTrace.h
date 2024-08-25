// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetadataTrace.h"
#include "ProfilingDebugging/StringsTrace.h"
#include "Trace/Detail/EventNode.h"
#include "Trace/Trace.h"

namespace UE { namespace Trace { class FChannel; } }

#if !defined(UE_TRACE_ASSET_METADATA_ENABLED)
	#define UE_TRACE_ASSET_METADATA_ENABLED UE_TRACE_METADATA_ENABLED
#endif

#if UE_TRACE_ASSET_METADATA_ENABLED

/**
 * Channel that asset metadata is output on
 */
CORE_API UE_TRACE_CHANNEL_EXTERN(AssetMetadataChannel);

/**
 * Metadata scope to instrument operations belonging to a certain asset
 */
CORE_API UE_TRACE_METADATA_EVENT_BEGIN_EXTERN(Asset)
	UE_TRACE_METADATA_EVENT_REFERENCE_FIELD(Strings, FName, Name)
	UE_TRACE_METADATA_EVENT_REFERENCE_FIELD(Strings, FName, Class)
	UE_TRACE_METADATA_EVENT_REFERENCE_FIELD(Strings, FName, Package)
UE_TRACE_METADATA_EVENT_END()

/**
 * Utility macro to create asset scope from an object and object class
 * @note When using this macro in a module outside of Core, make sure a dependency to the "TraceLog" module is added.
 */
#define UE_TRACE_METADATA_SCOPE_ASSET(Object, ObjClass) \
	UE_TRACE_METADATA_SCOPE_ASSET_FNAME(Object->GetFName(), ObjClass->GetFName(), Object->GetPackage()->GetFName())

/**
 * Utility macro to create an asset scope by specifying object name, class name and package name explicitly
 */
#define UE_TRACE_METADATA_SCOPE_ASSET_FNAME(ObjectName, ObjClassName, PackageName) \
	auto MetaNameRef = UE_TRACE_CHANNELEXPR_IS_ENABLED(MetadataChannel) ? FStringTrace::GetNameRef(ObjectName) : UE::Trace::FEventRef32(0,0);\
	auto ClassNameRef = UE_TRACE_CHANNELEXPR_IS_ENABLED(MetadataChannel) ? FStringTrace::GetNameRef(ObjClassName) : UE::Trace::FEventRef32(0,0);\
	auto PackageNameRef = UE_TRACE_CHANNELEXPR_IS_ENABLED(MetadataChannel) ? FStringTrace::GetNameRef(PackageName) : UE::Trace::FEventRef32(0,0);\
	UE_TRACE_METADATA_SCOPE(Asset, AssetMetadataChannel)\
		<< Asset.Name(MetaNameRef)\
		<< Asset.Class(ClassNameRef)\
		<< Asset.Package(PackageNameRef);

#else // UE_TRACE_ASSET_METADATA_ENABLED

#define UE_TRACE_METADATA_SCOPE_ASSET(...)
#define UE_TRACE_METADATA_SCOPE_ASSET_FNAME(...)

#endif // UE_TRACE_ASSET_METADATA_ENABLED

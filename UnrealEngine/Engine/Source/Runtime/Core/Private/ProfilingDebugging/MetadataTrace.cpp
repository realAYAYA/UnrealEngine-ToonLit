// Copyright Epic Games, Inc. All Rights Reserved.
#include "ProfilingDebugging/MetadataTrace.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
#if UE_TRACE_METADATA_ENABLED

UE_TRACE_CHANNEL_DEFINE(MetadataChannel);
UE_TRACE_EVENT_DEFINE(MetadataStack, ClearScope);
UE_TRACE_EVENT_DEFINE(MetadataStack, SaveStack);
UE_TRACE_EVENT_DEFINE(MetadataStack, RestoreStack);

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32 FMetadataTrace::SaveStack()
{
	static std::atomic_uint32_t Counter(1);
	const uint32 Id = Counter.fetch_add(1u);
	UE_TRACE_LOG(MetadataStack, SaveStack, MetadataChannel)
			<< SaveStack.Id(Id);
	return Id;
}

#endif // UE_TRACE_METADATA_ENABLED



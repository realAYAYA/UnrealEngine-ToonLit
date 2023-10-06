// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationState/ReplicationStateDescriptorConfig.h"
#include "Containers/ArrayView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReplicationStateDescriptorConfig)

UReplicationStateDescriptorConfig::UReplicationStateDescriptorConfig()
: Super()
{
}

TConstArrayView<FSupportsStructNetSerializerConfig> UReplicationStateDescriptorConfig::GetSupportsStructNetSerializerList() const
{
	return MakeArrayView(SupportsStructNetSerializerList);
}

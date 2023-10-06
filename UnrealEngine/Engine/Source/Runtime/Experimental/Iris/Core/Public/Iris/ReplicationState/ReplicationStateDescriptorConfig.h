// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "UObject/ObjectMacros.h"
#include "ReplicationStateDescriptorConfig.generated.h"

USTRUCT()
struct FSupportsStructNetSerializerConfig
{
	GENERATED_BODY()

	/** Struct name. */
	UPROPERTY()
	FName StructName;

	/** If the named struct works with the default Iris StructNetSerializer. */
	UPROPERTY()
	bool bCanUseStructNetSerializer = true;
};

UCLASS(transient, config=Engine)
class UReplicationStateDescriptorConfig : public UObject
{
	GENERATED_BODY()

public:
	IRISCORE_API TConstArrayView<FSupportsStructNetSerializerConfig> GetSupportsStructNetSerializerList() const;

protected:
	UReplicationStateDescriptorConfig();

private:
	/**
	 * Structs that works using the default struct NetSerializer when running iris replication even though they implement a custom NetSerialize or NetDeltaSerialize method.
	 */
	UPROPERTY(Config)
	TArray<FSupportsStructNetSerializerConfig> SupportsStructNetSerializerList;
};

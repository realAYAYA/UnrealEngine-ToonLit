// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "ILevelSequenceMetaData.generated.h"

UINTERFACE()
class ULevelSequenceMetaData : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface that may be implemented by meta-data added to a ULevelSequence that can extend the default behavior
 * such as adding asset registry tags and other meta-data.
 */
class ILevelSequenceMetaData
{
public:

	GENERATED_BODY()

	/**
	 * Called from ULevelSequence::GetAssetRegistryTags in order to
	 * extend its default set of tags to include any from this meta-data object.
	 */
	virtual void ExtendAssetRegistryTags(TArray<UObject::FAssetRegistryTag>& OutTags) const {}

#if WITH_EDITOR

	/**
	 * Called from ULevelSequence::GetAssetRegistryTagMetadata in order to
	 * extend its default set of tag meta-data to include any from this meta-data object.
	 */
	virtual void ExtendAssetRegistryTagMetaData(TMap<FName, UObject::FAssetRegistryTagMetadata>& OutMetadata) const {}

#endif
};
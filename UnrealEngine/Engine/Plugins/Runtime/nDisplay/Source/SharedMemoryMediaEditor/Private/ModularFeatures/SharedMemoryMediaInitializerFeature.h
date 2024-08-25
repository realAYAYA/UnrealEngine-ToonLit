// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDisplayClusterModularFeatureMediaInitializer.h"


/**
 * ShareMemory media source/output initializer for nDisplay
 */
class FSharedMemoryMediaInitializerFeature
	: public IDisplayClusterModularFeatureMediaInitializer
{
public:

	//~ Begin IDisplayClusterModularFeatureMediaInitializer
	virtual bool IsMediaSubjectSupported(const UObject* MediaSubject) override;
	virtual void InitializeMediaSubjectForTile(UObject* MediaSubject, const FMediaSubjectOwnerInfo& OnwerInfo, const FIntPoint& TilePos) override;
	virtual void InitializeMediaSubjectForFullFrame(UObject* MediaSubject, const FMediaSubjectOwnerInfo& OnwerInfo) override;
	//~ End IDisplayClusterModularFeatureMediaInitializer
};

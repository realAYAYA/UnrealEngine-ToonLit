// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDisplayClusterModularFeatureMediaInitializer.h"


/**
 * Rivermax media source/output initializer for nDisplay
 */
class FRivermaxMediaInitializerFeature
	: public IDisplayClusterModularFeatureMediaInitializer
{
public:

	//~ Begin IDisplayClusterModularFeatureMediaInitializer
	virtual bool IsMediaSubjectSupported(const UObject* MediaSubject) override;
	virtual void InitializeMediaSubjectForTile(UObject* MediaSubject, const FMediaSubjectOwnerInfo& OnwerInfo, const FIntPoint& TilePos) override;
	virtual void InitializeMediaSubjectForFullFrame(UObject* MediaSubject, const FMediaSubjectOwnerInfo& OnwerInfo) override;
	//~ End IDisplayClusterModularFeatureMediaInitializer

private:

	/** Returns first interface address if available */
	FString GetRivermaxInterfaceAddress() const;

	/** Generates stream address based on the function parameters */
	FString GenerateStreamAddress(uint8 OwnerUniqueIdx, const FIntPoint& TilePos) const;

	/** Generates stream address based on the function parameters */
	FString GenerateStreamAddress(uint8 ClusterNodeUniqueIdx, uint8 OwnerUniqueIdx, const FMediaSubjectOwnerInfo::EMediaSubjectOwnerType OwnerType) const;
};

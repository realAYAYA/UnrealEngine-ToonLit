// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"


/**
 * Base class for nDisplay media initializer implementations.
 */
class DISPLAYCLUSTERMODULARFEATURESEDITOR_API IDisplayClusterModularFeatureMediaInitializer
	: public IModularFeature
{
public:
	/** Public feature name */
	static const FName ModularFeatureName;

public:

	/**
	 * Container to carrier the info about media subject's owner
	 */
	struct FMediaSubjectOwnerInfo
	{
		/**
		 * Type of the media subject's owner
		 */
		enum class EMediaSubjectOwnerType : uint8
		{
			ICVFXCamera = 0,
			Viewport,
			Backbuffer
		};


		/** Owner name (ICVFX camera component name, viewport or node name) */
		FString OwnerName;

		/** Owner type (ICVFX camera component name, viewport or node name) */
		EMediaSubjectOwnerType OwnerType;

		/** Optional unique index of the cluster node holding the owner object */
		TOptional<uint8> ClusterNodeUniqueIdx = 0;

		/** 
		 * Unique index of the owner
		 *   Camera     - within a config
		 *   Viewport   - within a cluster node
		 *   Backbuffer - within a config
		 */
		uint8 OwnerUniqueIdx = 0;
	};

public:

	virtual ~IDisplayClusterModularFeatureMediaInitializer() = default;

public:

	/**
	 * Checks if media subject is supported by the initializer
	 *
	 * @param MediaSubject - UMediaSource or UMediaOutput instance
	 * @return true if media subject supported
	 */
	virtual bool IsMediaSubjectSupported(const UObject* MediaSubject) = 0;

	/**
	 * Performs initialization of a media subject for tiled input/output
	 *
	 * @param MediaSubject - UMediaSource or UMediaOutput instance
	 * @param OwnerInfo    - Additional information about the object holding the media subject
	 * @param TilePos      - TIle XY-position
	 */
	virtual void InitializeMediaSubjectForTile(UObject* MediaSubject, const FMediaSubjectOwnerInfo& OnwerInfo, const FIntPoint& TilePos) = 0;

	/**
	 * Performs initialization of a media subject for full frame input/output
	 *
	 * @param MediaSubject - UMediaSource or UMediaOutput instance
	 * @param OwnerInfo    - Additional information about the object holding the media subject
	 */
	virtual void InitializeMediaSubjectForFullFrame(UObject* MediaSubject, const FMediaSubjectOwnerInfo& OnwerInfo) = 0;
};

// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GeometryCacheTrack.h"
#include "GeometryCacheMeshData.h"

#include "GeometryCacheTrackTransformAnimation.generated.h"

/** Derived GeometryCacheTrack class, used for Transform animation. */
UCLASS(collapsecategories, hidecategories = Object, BlueprintType, config = Engine, deprecated)
class GEOMETRYCACHE_API UDEPRECATED_GeometryCacheTrack_TransformAnimation : public UGeometryCacheTrack
{
	GENERATED_UCLASS_BODY()
		
	//~ Begin UObject Interface.
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~ End UObject Interface.
	
	//~ Begin UGeometryCacheTrack Interface.
	virtual const bool UpdateMeshData(const float Time, const bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData*& OutMeshData) override;
	//~ End UGeometryCacheTrack Interface.

	/**
	* Sets/updates the MeshData for this track
	*
	* @param NewMeshData - GeometryCacheMeshData instance later used as the rendered mesh	
	*/
	UFUNCTION()
	void SetMesh(const FGeometryCacheMeshData& NewMeshData);	
private:
	FGeometryCacheMeshData MeshData;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

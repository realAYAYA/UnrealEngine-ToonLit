// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class UObject;

class FAbcFile;
struct FGeometryCacheMeshData;

class ALEMBICLIBRARY_API FAbcUtilities
{
public:
	/*  Populates a FGeometryCacheMeshData instance (with merged meshes) for the given frame of an Alembic 
	 *  with ConcurrencyIndex used as the concurrent read index
	 */
	static void GetFrameMeshData(FAbcFile& AbcFile, int32 FrameIndex, FGeometryCacheMeshData& OutMeshData, int32 ConcurrencyIndex = 0);

	/** Sets up materials from an AbcFile to a GeometryCache and moves them into the given package */
	static void SetupGeometryCacheMaterials(FAbcFile& AbcFile, class UGeometryCache* GeometryCache, UObject* Package);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

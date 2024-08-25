// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeMeshUtilities.generated.h"

class UInterchangeSourceData;

UCLASS(Experimental, MinimalAPI)
class UInterchangeMeshUtilities : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * This function ask to select a file and import a mesh from it. After it add/replace the MeshObject LOD (at LodIndex) with the imported mesh LOD data.
	 *
	 * @Param MeshObject - The Mesh we want to add the lod
	 * @Param LodIndex - The index of the LOD we want to replace or add
	 * @Return - return true if it successfully add or replace the MeshObject LOD at LodIndex with the imported data.
	 *
	 * @Note - This function will sort out the SourceData and call ImportCustomLodAsync and wait until the result is available.
	 */
	static INTERCHANGEENGINE_API TFuture<bool> ImportCustomLod(UObject* MeshObject, const int32 LodIndex);


	/**
	 * This function import a mesh from the source data and add/replace the MeshObject LOD (at LodIndex) with the imported mesh LOD data.
	 *
	 * @Param MeshObject - The Mesh we want to add the lod
	 * @Param LodIndex - The index of the LOD we want to replace or add
	 * @Param SourceData - The source to import the custom LOD
	 * @Return - return future boolean which will be true if it successfully add or replace the MeshObject LOD at LodIndex with the imported data.
	 *
	 * @Note - Since this is the async version, the SourceData parameter is required.
	 * @Note - This function will search for an available interchange asset factory that can do the job for the MeshObject class
	 */
	static INTERCHANGEENGINE_API TFuture<bool> ImportCustomLod(UObject* MeshObject, const int32 LodIndex, const UInterchangeSourceData* SourceData);

private:

	static TFuture<bool> InternalImportCustomLod(TSharedPtr<TPromise<bool>> Promise, UObject* MeshObject, const int32 LodIndex, const UInterchangeSourceData* SourceData);
};



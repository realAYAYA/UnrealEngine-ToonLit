// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IGeometryProcessingInterfacesModule.h"


class IGeometryProcessing_ApproximateActors;
class IGeometryProcessing_CombineMeshInstances;
class IGeometryProcessing_MeshAutoUV;


/**
 * Implementation of IGeometryProcessingInterfacesModule (which extends the standard IModuleInterface)
 * to provide access to various "Operation" interfaces
 */
class FGeometryProcessingInterfacesModule : public IGeometryProcessingInterfacesModule
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	

	/**
	 * @return implementation of IGeometryProcessing_ApproximateActors, if available, or nullptr (result is cached internally)
	 */
	virtual IGeometryProcessing_ApproximateActors* GetApproximateActorsImplementation() override;

	/**
	 * @return implementation of IGeometryProcessing_CombineMeshInstances, if available, or nullptr (result is cached internally)
	 */
	virtual IGeometryProcessing_CombineMeshInstances* GetCombineMeshInstancesImplementation() override;

	/**
	 * @return implementation of IGeometryProcessing_MeshAutoUV, if available, or nullptr (result is cached internally)
	 */
	virtual IGeometryProcessing_MeshAutoUV* GetMeshAutoUVImplementation() override;

private:
	IGeometryProcessing_ApproximateActors* ApproximateActors = nullptr;
	IGeometryProcessing_CombineMeshInstances* CombineMeshInstances = nullptr;
	IGeometryProcessing_MeshAutoUV* MeshAutoUV = nullptr;
};

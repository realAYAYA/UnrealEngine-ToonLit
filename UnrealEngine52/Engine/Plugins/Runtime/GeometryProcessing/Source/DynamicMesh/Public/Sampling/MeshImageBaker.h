// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sampling/MeshImageBakingCache.h"

namespace UE
{
namespace Geometry
{

class DYNAMICMESH_API FMeshImageBaker
{
public:
	virtual ~FMeshImageBaker() {}

	void SetCache(const FMeshImageBakingCache* CacheIn)
	{
		ImageBakeCache = CacheIn;
	}

	const FMeshImageBakingCache* GetCache() const
	{
		return ImageBakeCache;
	}

	virtual void Bake()
	{

	}


protected:

	const FMeshImageBakingCache* ImageBakeCache;

};

} // end namespace UE::Geometry
} // end namespace UE

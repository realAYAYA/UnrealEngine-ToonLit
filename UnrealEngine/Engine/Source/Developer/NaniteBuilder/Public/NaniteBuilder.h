// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "StaticMeshResources.h"

struct FStaticMeshBuildVertex;
struct FMeshNaniteSettings;

namespace Nanite
{

struct FResources;

class IBuilderModule : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IBuilderModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IBuilderModule>("NaniteBuilder");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("NaniteBuilder");
	}

	virtual const FString& GetVersionString() const = 0;

	virtual bool Build(
		FResources& Resources,
		TArray<FStaticMeshBuildVertex>& Vertices, // TODO: Do not require this vertex type for all users of Nanite
		TArray<uint32>& TriangleIndices,
		TArray<int32>& MaterialIndices,
		TArray<uint32>& MeshTriangleCounts,			// Split into multiple triangle ranges with separate hierarchy roots.
		uint32 NumTexCoords,
		const FMeshNaniteSettings& Settings)
	{
		return false;
	}

	struct FVertexMeshData
	{
		TArray<FStaticMeshBuildVertex> Vertices;
		TArray<uint32> TriangleIndices;
		FStaticMeshSectionArray Sections;
		float PercentTriangles;
		float MaxDeviation;
	};

	virtual bool Build(
		FResources& Resources,
		FVertexMeshData& InputMeshData,
		TArrayView< FVertexMeshData > OutputLODMeshData,
		uint32 NumTexCoords,
		const FMeshNaniteSettings& Settings)
	{
		return false;
	}
};

} // namespace Nanite
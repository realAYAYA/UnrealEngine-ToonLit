// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "StaticMeshResources.h"
#include "Math/Bounds.h"

struct FMeshBuildVertexView;
struct FMeshNaniteSettings;

namespace Nanite
{

const int32 MaxSectionArraySize = 64;

struct FResources;

class IBuilderModule : public IModuleInterface
{
public:
	typedef TDelegate<void(bool bFallbackIsReduced)> FOnFreeInputMeshData;

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

	struct FInputMeshData
	{
		FMeshBuildVertexData Vertices;
		TArray<uint32> TriangleIndices;
		TArray<uint32> TriangleCounts;
		TArray<int32>  MaterialIndices;
		FStaticMeshSectionArray Sections;
		FBounds3f VertexBounds;
		uint32 NumTexCoords;
		float PercentTriangles;
		float MaxDeviation;
	};

	struct FOutputMeshData
	{
		FMeshBuildVertexData Vertices;
		TArray<uint32> TriangleIndices;
		FStaticMeshSectionArray Sections;
		float PercentTriangles;
		float MaxDeviation;
	};

	virtual bool Build(
		FResources& Resources,
		FInputMeshData& InputMeshData,
		TArrayView<FOutputMeshData> OutputLODMeshData,
		const FMeshNaniteSettings& Settings,
		FOnFreeInputMeshData OnFreeInputMeshData)
	{
		return false;
	}

	virtual bool BuildMaterialIndices(
		const FStaticMeshSectionArray& SectionArray,
		const uint32 TriangleCount,
		TArray<int32>& OutMaterialIndices)
	{
		return false;
	}
};

} // namespace Nanite
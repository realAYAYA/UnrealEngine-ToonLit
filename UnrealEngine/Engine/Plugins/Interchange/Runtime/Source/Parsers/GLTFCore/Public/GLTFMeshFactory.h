// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshTypes.h"
#include "GLTFLogger.h"

struct FVertexID;

struct FMeshDescription;

namespace GLTF
{
	struct FMesh;
	class FMeshFactoryImpl;

	class GLTFCORE_API FMeshFactory
	{
	public:
		FMeshFactory();
		~FMeshFactory();

		using FIndexVertexIdMap = TMap<int32, FVertexID>;

		void FillMeshDescription(const GLTF::FMesh &Mesh, const FTransform& MeshGlobalTransform /*In UE Space*/, FMeshDescription* MeshDescription, const TArray<float>& MorphTargetWeights = TArray<float>());

		float GetUniformScale() const;
		void  SetUniformScale(float Scale);

		const TArray<FLogMessage>&  GetLogMessages() const;

		void SetReserveSize(uint32 Size);

		TArray<FMeshFactory::FIndexVertexIdMap>& GetPositionIndexToVertexIdPerPrim() const;

		void CleanUp();

	private:
		TUniquePtr<FMeshFactoryImpl> Impl;
	};
}

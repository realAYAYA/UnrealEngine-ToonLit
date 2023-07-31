// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GLTFLogger.h"
#include "MeshTypes.h"

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

		void FillMeshDescription(const GLTF::FMesh &Mesh, FMeshDescription* MeshDescription);

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
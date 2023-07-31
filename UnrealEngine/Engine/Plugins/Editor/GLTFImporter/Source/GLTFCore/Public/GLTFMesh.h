// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GLTFAccessor.h"

#include "CoreMinimal.h"

struct FMD5Hash;

namespace GLTF
{
	// for storing triangle indices as a unit
	struct GLTFCORE_API FTriangle
	{
		uint32 A, B, C;

		FTriangle()
		    : A(0)
		    , B(0)
		    , C(0)
		{
		}
	};

	struct GLTFCORE_API FJointInfluence
	{
		FVector4 Weight;
		uint16   ID[4];

		FJointInfluence(const FVector4& InWeight)
		    : Weight(InWeight)
		    , ID {0, 0, 0, 0}
		{
		}
	};

	struct GLTFCORE_API FVariantMapping
	{
		int32 MaterialIndex;
		TArray<int32> VariantIndices;
	};

	struct GLTFCORE_API FPrimitive
	{
		enum class EMode
		{
			// valid but unsupported
			Points    = 0,
			Lines     = 1,
			LineLoop  = 2,
			LineStrip = 3,
			// initially supported
			Triangles = 4,
			// will be supported prior to release
			TriangleStrip = 5,
			TriangleFan   = 6
		};

		const EMode Mode;
		const int32 MaterialIndex;
		TArray<FVariantMapping>	VariantMappings;

		FPrimitive(EMode InMode, int32 InMaterial, const FAccessor& InIndices, const FAccessor& InPosition, const FAccessor& InNormal,
		           const FAccessor& InTangent, const FAccessor& InTexCoord0, const FAccessor& InTexCoord1, const FAccessor& InColor0,
		           const FAccessor& InJoints0, const FAccessor& InWeights0);

		bool IsValid() const;
		FMD5Hash GetHash() const;

		void GetPositions(TArray<FVector3f>& Buffer) const;
		bool HasNormals() const;
		void GetNormals(TArray<FVector3f>& Buffer) const;
		bool HasTangents() const;
		void GetTangents(TArray<FVector4f>& Buffer) const;
		bool HasTexCoords(uint32 Index) const;
		void GetTexCoords(uint32 Index, TArray<FVector2f>& Buffer) const;
		void GetColors(TArray<FVector4f>& Buffer) const;
		bool HasColors() const;
		bool HasJointWeights() const;
		void GetJointInfluences(TArray<FJointInfluence>& Buffer) const;

		FTriangle TriangleVerts(uint32 T) const;
		void      GetTriangleIndices(TArray<uint32>& Buffer) const;

		uint32 VertexCount() const;
		uint32 TriangleCount() const;

	private:
		// index buffer
		const FAccessor& Indices;

		// common attributes
		const FAccessor& Position;  // always required
		const FAccessor& Normal;
		const FAccessor& Tangent;
		const FAccessor& TexCoord0;
		const FAccessor& TexCoord1;
		const FAccessor& Color0;
		// skeletal mesh attributes
		const FAccessor& Joints0;
		const FAccessor& Weights0;
	};


	struct GLTFCORE_API FMesh
	{
		FString				Name;
		TArray<FPrimitive>	Primitives;
	
		bool HasNormals() const;
		bool HasTangents() const;
		bool HasTexCoords(uint32 Index) const;
		bool HasColors() const;
		bool HasJointWeights() const;

		bool IsValid() const;
		FMD5Hash GetHash() const;
	};

	//

	inline bool FPrimitive::HasNormals() const
	{
		return Normal.IsValid();
	}

	inline bool FPrimitive::HasTangents() const
	{
		return Tangent.IsValid();
	}

	inline bool FPrimitive::HasTexCoords(uint32 Index) const
	{
		switch (Index)
		{
			case 0:
				return TexCoord0.IsValid();
			case 1:
				return TexCoord1.IsValid();
			default:
				return false;
		}
	}

	inline bool FPrimitive::HasColors() const
	{
		return Color0.IsValid();
	}

	inline void FPrimitive::GetPositions(TArray<FVector3f>& Buffer) const
	{
		Position.GetCoordArray(Buffer);
	}

	inline void FPrimitive::GetNormals(TArray<FVector3f>& Buffer) const
	{
		Normal.GetCoordArray(Buffer);
	}

	inline void FPrimitive::GetTexCoords(uint32 Index, TArray<FVector2f>& Buffer) const
	{
		switch (Index)
		{
			case 0:
				return TexCoord0.GetVec2Array(Buffer);
			case 1:
				return TexCoord1.GetVec2Array(Buffer);
			default:
				check(false);
				break;
		}
	}

	inline bool FPrimitive::HasJointWeights() const
	{
		return Joints0.IsValid() && Weights0.IsValid();
	}

	//

	inline bool FMesh::HasNormals() const
	{
		return Primitives.FindByPredicate([](const FPrimitive& Prim) { return Prim.HasNormals(); }) != nullptr;
	}

	inline bool FMesh::HasTangents() const
	{
		return Primitives.FindByPredicate([](const FPrimitive& Prim) { return Prim.HasTangents(); }) != nullptr;
	}

	inline bool FMesh::HasTexCoords(uint32 Index) const
	{
		return Primitives.FindByPredicate([Index](const FPrimitive& Prim) { return Prim.HasTexCoords(Index); }) != nullptr;
	}

	inline bool FMesh::HasColors() const
	{
		return Primitives.FindByPredicate([](const FPrimitive& Prim) { return Prim.HasColors(); }) != nullptr;
	}

	inline bool FMesh::HasJointWeights() const
	{
		const bool Result = Primitives.FindByPredicate([](const auto& Prim) { return Prim.HasJointWeights(); }) != nullptr;

		if (Result)
		{
			// According to spec, *all* primitives of a skinned mesh must have joint weights.
			int32 Count = 0;
			for (const FPrimitive& Prim : Primitives)
			{
				Count += Prim.HasJointWeights();
			}
			check(Primitives.Num() == Count);
		}

		return Result;
	}

	inline bool FMesh::IsValid() const
	{
		return Primitives.FindByPredicate([](const FPrimitive& Prim) { return !Prim.IsValid(); }) == nullptr;
	}

}  // namespace GLTF

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

	struct GLTFCORE_API FMorphTarget
	{
		FMorphTarget(const FAccessor& InPositionDisplacements, const FAccessor& InNormalDisplacements,
			const FAccessor& InTangentDisplacements, const FAccessor& InTexCoord0Displacements, const FAccessor& InTexCoord1Displacements, const FAccessor& InColor0Deltas);

		bool IsValid() const;
		FMD5Hash GetHash() const;

		bool HasPositionDisplacements() const;
		void GetPositionDisplacements(TArray<FVector3f>& Buffer) const;
		int32 GetNumberOfPositionDisplacements() const;

		bool HasNormalDisplacements() const;
		void GetNormalDisplacements(TArray<FVector3f>& Buffer) const;
		int32 GetNumberOfNormalDisplacements() const;

		bool HasTangentDisplacements() const;
		void GetTangentDisplacements(TArray<FVector4f>& Buffer) const;
		int32 GetNumberOfTangentDisplacements() const;

		bool HasTexCoordDisplacements(uint32 Index) const;
		void GetTexCoordDisplacements(uint32 Index, TArray<FVector2f>& Buffer) const;
		int32 GetNumberOfTexCoordDisplacements(uint32 Index) const;

		bool HasColorDeltas() const;
		void GetColorDeltas(TArray<FVector4f>& Buffer) const;
		int32 GetNumberOfColorDeltas() const;

	private:
		const FAccessor& PositionDisplacements;
		const FAccessor& NormalDisplacements;
		const FAccessor& TangentDisplacements;
		const FAccessor& TexCoord0Displacements;
		const FAccessor& TexCoord1Displacements;
		const FAccessor& Color0Deltas;
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
			TriangleFan   = 6,

			//
			Unknown = 7
		};

		const EMode Mode;
		const int32 MaterialIndex;
		TArray<FVariantMapping>	VariantMappings;

		FPrimitive(EMode InMode, int32 InMaterial, const FAccessor& InIndices, const FAccessor& InPosition, const FAccessor& InNormal,
		           const FAccessor& InTangent, const FAccessor& InTexCoord0, const FAccessor& InTexCoord1, const FAccessor& InColor0,
		           const FAccessor& InJoints0, const FAccessor& InWeights0);

		void GenerateIsValidCache();
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

		//
		TArray<FMorphTarget> MorphTargets;

	private:
		bool IsValidPrivate() const;

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

		//Validity cache:
		TOptional<bool> bIsValidCache;
	};


	struct GLTFCORE_API FMesh
	{
		FString				Name;
		TArray<FPrimitive>	Primitives;

		TArray<float>		MorphTargetWeights;
		TArray<FString>		MorphTargetNames;

		FString				UniqueId; //will be generated in FAsset::GenerateNames
	
		bool HasNormals() const;
		bool HasTangents() const;
		bool HasTexCoords(uint32 Index) const;
		bool HasColors() const;
		bool HasJointWeights() const;

		void GenerateIsValidCache(bool GenerateIsValidCacheForPrimitives = true);
		bool IsValid() const;
		FMD5Hash GetHash() const;

		int32 NumberOfMorphTargetsPerPrimitive() const;

	private:
		bool IsValidPrivate() const;
		//Validity cache:
		TOptional<bool> bIsValidCache;
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
		return Color0.IsValid() && (Color0.Type == FAccessor::EType::Vec3 || Color0.Type == FAccessor::EType::Vec4);
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
				ensure(false);
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
			ensure(Primitives.Num() == Count);
		}

		return Result;
	}

	inline void FMesh::GenerateIsValidCache(bool GenerateIsValidCacheForPrimitives)
	{
		if (GenerateIsValidCacheForPrimitives)
		{
			for (FPrimitive& Primitive : Primitives)
			{
				Primitive.GenerateIsValidCache();
			}
		}

		bIsValidCache = IsValidPrivate();
	}
	inline bool FMesh::IsValid() const
	{
		if (bIsValidCache.IsSet())
		{
			return bIsValidCache.GetValue();
		}
		else
		{
			return IsValidPrivate();
		}
	}

	inline bool FMesh::IsValidPrivate() const
	{
		//Validate Primitives:
		bool bIsValid = Primitives.FindByPredicate([](const FPrimitive& Prim) { return !Prim.IsValid(); }) == nullptr;

		//if MorphTargetNames are not set, but mesh has morph targets (which is likely), this will generate isValid false;
		//IsValidCache will have to be (re-)genereated post FAsset::GenerateNames (which is called at the end of GLTF::FReader) to overcome this issue.

		//Validate Morph Target Names and Morph Target Weights:
		if ((MorphTargetNames.Num() > 0) && (MorphTargetWeights.Num() > 0))
		{
			if (MorphTargetNames.Num() != MorphTargetWeights.Num())
			{
				bIsValid = false;
			}
		}

		//Validate Morph Target (and Morph Target Names) Counts:
		int32 MorphTargetCounter = MorphTargetNames.Num();

		for (const FPrimitive& Primitive : Primitives)
		{
			if (MorphTargetCounter != Primitive.MorphTargets.Num())
			{
				bIsValid = false;
				break;
			}
		}

		return bIsValid;
	}

}  // namespace GLTF

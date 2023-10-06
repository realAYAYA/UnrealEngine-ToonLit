// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFMesh.h"

#include "ConversionUtilities.h"

namespace GLTF
{
	FMorphTarget::FMorphTarget(const FAccessor& InPositionDisplacements, const FAccessor& InNormalDisplacements,
		const FAccessor& InTangentDisplacements, const FAccessor& InTexCoord0Displacements, const FAccessor& InTexCoord1Displacements, const FAccessor& InColor0Deltas)
		: PositionDisplacements(InPositionDisplacements)
		, NormalDisplacements(InNormalDisplacements)
		, TangentDisplacements(InTangentDisplacements)
		, TexCoord0Displacements(InTexCoord0Displacements)
		, TexCoord1Displacements(InTexCoord1Displacements)
		, Color0Deltas(InColor0Deltas)
	{
	}

	bool FMorphTarget::IsValid() const
	{
		bool bHasData = false;
		const bool bMorphTarget = true;

		if (HasPositionDisplacements())
		{
			bHasData = true;
			if (!PositionDisplacements.IsValidDataType(FAccessor::EDataType::Position, bMorphTarget))
			{
				return false;
			}
		}
		
		if (HasNormalDisplacements())
		{
			bHasData = true;
			if (!NormalDisplacements.IsValidDataType(FAccessor::EDataType::Normal, bMorphTarget))
			{
				return false;
			}
		}
		
		if (HasTangentDisplacements())
		{
			bHasData = true;
			if (!TangentDisplacements.IsValidDataType(FAccessor::EDataType::Tangent, bMorphTarget))
			{
				return false;
			}
		}
		
		if (HasTexCoordDisplacements(0))
		{
			bHasData = true;
			if (!TexCoord0Displacements.IsValidDataType(FAccessor::EDataType::Texcoord, bMorphTarget))
			{
				return false;
			}
		}
		if (HasTexCoordDisplacements(1))
		{
			bHasData = true;
			if (!TexCoord1Displacements.IsValidDataType(FAccessor::EDataType::Texcoord, bMorphTarget))
			{
				return false;
			}
		}

		if (HasColorDeltas())
		{
			bHasData = true;
			if (Color0Deltas.IsValidDataType(FAccessor::EDataType::Color, bMorphTarget))
			{
				return false;
			}
		}
		return bHasData;
	}
	FMD5Hash FMorphTarget::GetHash() const
	{
		if (!IsValid())
		{
			return FMD5Hash();
		}

		auto HashAccessor = [](FMD5& MD5, const FAccessor& Accessor)
		{
			if (!Accessor.IsValid())
			{
				return;
			}

			FMD5Hash MD5Hash = Accessor.GetHash();
			if (MD5Hash.IsValid())
			{
				MD5.Update(MD5Hash.GetBytes(), MD5Hash.GetSize());
			}
		};

		FMD5 MD5;

		HashAccessor(MD5, PositionDisplacements);
		HashAccessor(MD5, NormalDisplacements);
		HashAccessor(MD5, TangentDisplacements);
		HashAccessor(MD5, TexCoord0Displacements);
		HashAccessor(MD5, TexCoord1Displacements);
		HashAccessor(MD5, Color0Deltas);

		FMD5Hash Hash;
		Hash.Set(MD5);

		return Hash;
	}

	bool FMorphTarget::HasPositionDisplacements() const
	{
		return PositionDisplacements.IsValid();
	}
	void FMorphTarget::GetPositionDisplacements(TArray<FVector3f>& Buffer) const
	{
		PositionDisplacements.GetCoordArray(Buffer);
	}
	int32 FMorphTarget::GetNumberOfPositionDisplacements() const
	{
		return PositionDisplacements.Count;
	}

	bool FMorphTarget::HasNormalDisplacements() const
	{
		return NormalDisplacements.IsValid();
	}
	void FMorphTarget::GetNormalDisplacements(TArray<FVector3f>& Buffer) const
	{
		NormalDisplacements.GetCoordArray(Buffer);
	}
	int32 FMorphTarget::GetNumberOfNormalDisplacements() const
	{
		return NormalDisplacements.Count;
	}

	bool FMorphTarget::HasTangentDisplacements() const
	{
		return TangentDisplacements.IsValid();
	}
	void FMorphTarget::GetTangentDisplacements(TArray<FVector4f>& Buffer) const
	{
		const int32 N = TangentDisplacements.Count;
		Buffer.Reserve(N);
		for (int32 Index = 0; Index < N; ++Index)
		{
			const FVector TangentVector = TangentDisplacements.GetVec3(Index);
			Buffer.Emplace((FVector3f)GLTF::ConvertVec3(TangentVector), 0.f);
		}
	}
	int32 FMorphTarget::GetNumberOfTangentDisplacements() const
	{
		return TangentDisplacements.Count;
	}

	bool FMorphTarget::HasTexCoordDisplacements(uint32 Index) const
	{
		switch (Index)
		{
			case 0:
				return TexCoord0Displacements.IsValid();
			case 1:
				return TexCoord1Displacements.IsValid();
			default:
				return false;
		}
	}
	void FMorphTarget::GetTexCoordDisplacements(uint32 Index, TArray<FVector2f>& Buffer) const
	{
		switch (Index)
		{
		case 0:
			return TexCoord0Displacements.GetVec2Array(Buffer);
		case 1:
			return TexCoord1Displacements.GetVec2Array(Buffer);
		default:
			break;
		}
	}
	int32 FMorphTarget::GetNumberOfTexCoordDisplacements(uint32 Index) const
	{
		switch (Index)
		{
		case 0:
			return TexCoord0Displacements.Count;
		case 1:
			return TexCoord1Displacements.Count;
		default:
			return 0;
		}
	}

	bool FMorphTarget::HasColorDeltas() const
	{
		return Color0Deltas.IsValid() && (Color0Deltas.Type == FAccessor::EType::Vec3 || Color0Deltas.Type == FAccessor::EType::Vec4);
	}
	void FMorphTarget::GetColorDeltas(TArray<FVector4f>& Buffer) const
	{
		if (Color0Deltas.Type == FAccessor::EType::Vec4)
		{
			Color0Deltas.GetVec4Array(Buffer);
		}
		else if (Color0Deltas.Type == FAccessor::EType::Vec3)
		{
			const int32 N = Color0Deltas.Count;
			Buffer.Reserve(N);
			for (int32 Index = 0; Index < N; ++Index)
			{
				const FVector Vec = Color0Deltas.GetVec3(Index);
				Buffer.Emplace((FVector3f)Vec, 0.f);
			}
		}
	}
	int32 FMorphTarget::GetNumberOfColorDeltas() const
	{
		return (Color0Deltas.Type == FAccessor::EType::Vec3 || Color0Deltas.Type == FAccessor::EType::Vec4) ? Color0Deltas.Count : 0;
	}


	FPrimitive::FPrimitive(EMode InMode, int32 InMaterial, const FAccessor& InIndices, const FAccessor& InPosition, const FAccessor& InNormal,
	                       const FAccessor& InTangent, const FAccessor& InTexCoord0, const FAccessor& InTexCoord1, const FAccessor& InColor0,
	                       const FAccessor& InJoints0, const FAccessor& InWeights0)
	    : Mode(InMode)
	    , MaterialIndex(InMaterial)
	    , Indices(InIndices)
	    , Position(InPosition)
	    , Normal(InNormal)
	    , Tangent(InTangent)
	    , TexCoord0(InTexCoord0)
	    , TexCoord1(InTexCoord1)
	    , Color0(InColor0)
	    , Joints0(InJoints0)
	    , Weights0(InWeights0)
	{
	}

	void FPrimitive::GetTangents(TArray<FVector4f>& Buffer) const
	{
		Buffer.Reserve(Tangent.Count);
		for (uint32 Index = 0; Index < Tangent.Count; ++Index)
		{
			Buffer.Push((FVector4f)GLTF::ConvertTangent(Tangent.GetVec4(Index)));
		}
	}

	void FPrimitive::GetColors(TArray<FVector4f>& Buffer) const
	{
		if (Color0.Type == FAccessor::EType::Vec4)
		{
			Color0.GetVec4Array(Buffer);
		}
		else if (Color0.Type == FAccessor::EType::Vec3)
		{
			const int32 N = Color0.Count;
			Buffer.Reserve(N);
			for (int32 Index = 0; Index < N; ++Index)
			{
				const FVector Vec = Color0.GetVec3(Index);
				Buffer.Emplace((FVector3f)Vec, 1.f);
			}
		}
	}

	void FPrimitive::GetJointInfluences(TArray<FJointInfluence>& Buffer) const
	{
		// return a flat array that corresponds 1-to-1 with vertex positions
		const int32 N = Joints0.Count;
		Buffer.Reserve(N);
		for (int32 Index = 0; Index < N; ++Index)
		{
			FJointInfluence& Joint = Buffer.Emplace_GetRef(Weights0.GetVec4(Index));
			Joints0.GetUnsignedInt16x4(Index, Joint.ID);
		}
	}

	FTriangle FPrimitive::TriangleVerts(uint32 T) const
	{
		FTriangle Result;
		if (T >= TriangleCount())
			return Result;

		const bool Indexed = Indices.IsValid();
		switch (Mode)
		{
			case EMode::Triangles:
				if (Indexed)
				{
					Result.A = Indices.GetUnsignedInt(3 * T);
					Result.B = Indices.GetUnsignedInt(3 * T + 1);
					Result.C = Indices.GetUnsignedInt(3 * T + 2);
				}
				else
				{
					Result.A = 3 * T;
					Result.B = 3 * T + 1;
					Result.C = 3 * T + 2;
				}
				break;
			case EMode::TriangleStrip:
				// are indexed TriangleStrip & TriangleFan valid?
				// I don't see anything in the spec that says otherwise...
				if (Indexed)
				{
					if (T % 2 == 0)
					{
						Result.A = Indices.GetUnsignedInt(T);
						Result.B = Indices.GetUnsignedInt(T + 1);
					}
					else
					{
						Result.A = Indices.GetUnsignedInt(T + 1);
						Result.B = Indices.GetUnsignedInt(T);
					}
					Result.C = Indices.GetUnsignedInt(T + 2);
				}
				else
				{
					if (T % 2 == 0)
					{
						Result.A = T;
						Result.B = T + 1;
					}
					else
					{
						Result.A = T + 1;
						Result.B = T;
					}
					Result.C = T + 2;
				}
				break;
			case EMode::TriangleFan:
				if (Indexed)
				{
					Result.A = Indices.GetUnsignedInt(0);
					Result.B = Indices.GetUnsignedInt(T + 1);
					Result.C = Indices.GetUnsignedInt(T + 2);
				}
				else
				{
					Result.A = 0;
					Result.B = T + 1;
					Result.C = T + 2;
				}
				break;
			default:
				break;
		}

		return Result;
	}

	void FPrimitive::GetTriangleIndices(TArray<uint32>& Buffer) const
	{
		if (Mode == EMode::Triangles)
		{
			if (Indices.IsValid())
			{
				return Indices.GetUnsignedIntArray(Buffer);
			}
			else
			{
				// generate indices [0 1 2][3 4 5]...
				const uint32 N = TriangleCount() * 3;
				Buffer.Reserve(N);
				for (uint32 Index = 0; Index < N; ++Index)
				{
					Buffer.Push(Index);
				}
			}
		}
		else
		{
			const uint32 N = TriangleCount() * 3;
			Buffer.Reserve(N);
			for (uint32 Index = 0; Index < TriangleCount(); ++Index)
			{
				FTriangle Tri = TriangleVerts(Index);
				Buffer.Push(Tri.A);
				Buffer.Push(Tri.B);
				Buffer.Push(Tri.C);
			}
		}
	}

	uint32 FPrimitive::VertexCount() const
	{
		if (Indices.IsValid())
			return Indices.Count;
		else
			return Position.Count;
	}

	uint32 FPrimitive::TriangleCount() const
	{
		switch (Mode)
		{
			case EMode::Triangles:
				return VertexCount() / 3;
			case EMode::TriangleStrip:
			case EMode::TriangleFan:
				return VertexCount() - 2;
			default:
				return 0;
		}
	}

	void FPrimitive::GenerateIsValidCache()
	{
		bIsValidCache = IsValidPrivate();
	}

	bool FPrimitive::IsValid() const
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

	bool FPrimitive::IsValidPrivate() const
	{
		// make sure all semantic attributes meet the spec
		const bool bMorphTarget = false;

		if (!(Position.IsValid() && Position.IsValidDataType(FAccessor::EDataType::Position, bMorphTarget)))
		{
			return false;
		}

		const uint32 VertexCount = Position.Count;

		if (Normal.IsValid())
		{
			if (Normal.Count != VertexCount)
			{
				return false;
			}

			if (!Normal.IsValidDataType(FAccessor::EDataType::Normal, bMorphTarget))
			{
				return false;
			}
		}

		if (Tangent.IsValid())
		{
			if (Tangent.Count != VertexCount)
			{
				return false;
			}

			if (!Tangent.IsValidDataType(FAccessor::EDataType::Tangent, bMorphTarget))
			{
				return false;
			}
		}

		const FAccessor* TexCoords[] = { &TexCoord0, &TexCoord1 };
		for (const FAccessor* TexCoord : TexCoords)
		{
			if (TexCoord->IsValid())
			{
				if (TexCoord->Count != VertexCount)
				{
					return false;
				}

				if (!TexCoord->IsValidDataType(FAccessor::EDataType::Texcoord, bMorphTarget))
				{
					return false;
				}
			}
		}

		if (Color0.IsValid())
		{
			if (Color0.Count != VertexCount)
			{
				return false;
			}

			if (!Color0.IsValidDataType(FAccessor::EDataType::Color, bMorphTarget))
			{
				return false;
			}
		}

		//Validate Ranges:
		{
			TArray<uint32> AttributesSizes;
			const FAccessor* AttributeAccessors[] = { &Position, &Normal, &Tangent, &TexCoord0, &TexCoord1, &Color0, &Joints0, &Weights0 };
			for (const FAccessor* AttributeAccessor : AttributeAccessors)
			{
				if (AttributeAccessor->IsValid() && (AttributeAccessor->Count > 0))
				{
					AttributesSizes.Add(AttributeAccessor->Count);
				}
			}

			TArray<uint32> IndicesValues;
			GetTriangleIndices(IndicesValues);

			for (uint32 Index : IndicesValues)
			{
				for (const uint32& AttributeSize : AttributesSizes)
				{
					if (AttributeSize <= Index)
					{
						return false;
					}
				}
			}
		}

		//Morph Targets:
		for (const FMorphTarget& MorphTarget : MorphTargets)
		{
			if (!MorphTarget.IsValid())
			{
				return false;
			}

			if (MorphTarget.HasPositionDisplacements() && Position.Count != MorphTarget.GetNumberOfPositionDisplacements())
			{
				return false;
			}

			if (MorphTarget.HasNormalDisplacements() && Normal.Count != MorphTarget.GetNumberOfNormalDisplacements())
			{
				return false;
			}

			if (MorphTarget.HasTangentDisplacements() && Tangent.Count != MorphTarget.GetNumberOfTangentDisplacements())
			{
				return false;
			}

			if (MorphTarget.HasTexCoordDisplacements(0) && TexCoord0.Count != MorphTarget.GetNumberOfTexCoordDisplacements(0))
			{
				return false;
			}
			if (MorphTarget.HasTexCoordDisplacements(1) && TexCoord0.Count != MorphTarget.GetNumberOfTexCoordDisplacements(1))
			{
				return false;
			}

			if (MorphTarget.HasColorDeltas() && Color0.Count != MorphTarget.GetNumberOfColorDeltas())
			{
				return false;
			}
		}

		return true;
	}

	FMD5Hash FPrimitive::GetHash() const
	{
		if (!IsValid())
		{
			return FMD5Hash();
		}

		auto HashAccessor = [](FMD5& MD5, const FAccessor& Accessor)
		{
			if (!Accessor.IsValid())
			{
				return;
			}

			FMD5Hash MD5Hash = Accessor.GetHash();
			if (MD5Hash.IsValid())
			{
				MD5.Update(MD5Hash.GetBytes(), MD5Hash.GetSize());
			}
		};

		FMD5 MD5;

		uint8 ModeInt = static_cast<uint8>(Mode);
		MD5.Update(&ModeInt, sizeof(ModeInt));
		MD5.Update(reinterpret_cast<const uint8*>(&MaterialIndex), sizeof(MaterialIndex));

		HashAccessor(MD5, Indices);
		HashAccessor(MD5, Position);

		if (HasNormals())
		{
			HashAccessor(MD5, Normal);
		}

		if (HasTangents())
		{
			HashAccessor(MD5, Tangent);
		}

		if (HasTexCoords(0))
		{
			HashAccessor(MD5, TexCoord0);
		}

		if (HasTexCoords(1))
		{
			HashAccessor(MD5, TexCoord1);
		}

		if (HasColors())
		{
			HashAccessor(MD5, Color0);
		}

		if (HasJointWeights())
		{
			HashAccessor(MD5, Joints0);
			HashAccessor(MD5, Weights0);
		}

		//Morph Targets:
		for (const FMorphTarget& MorphTarget : MorphTargets)
		{
			FMD5Hash MorphTargetHash = MorphTarget.GetHash();
			if (MorphTargetHash.IsValid())
			{
				MD5.Update(MorphTargetHash.GetBytes(), MorphTargetHash.GetSize());
			}
		}

		FMD5Hash Hash;
		Hash.Set(MD5);
		return Hash;
	}

	FMD5Hash FMesh::GetHash() const
	{
		if (!IsValid())
		{
			return FMD5Hash();
		}

		FMD5 MD5;

		for (const GLTF::FPrimitive& Primitive : Primitives)
		{
			FMD5Hash PrimitiveHash = Primitive.GetHash();
			if (PrimitiveHash.IsValid())
			{
				MD5.Update(PrimitiveHash.GetBytes(), PrimitiveHash.GetSize());
			}
		}

		FMD5Hash Hash;
		Hash.Set(MD5);
		return Hash;
	}

	//This is a helper functions for the validators.
	//We are allowed to check only the first primitive's morph target count,
	// because if there is a incosonsitency acorss primitive targets then the mesh IsValid will report false already.
	int32 FMesh::NumberOfMorphTargetsPerPrimitive() const
	{
		int32 MorphTargetCounter = Primitives.Num() > 0 ? Primitives[0].MorphTargets.Num() : 0;

		return MorphTargetCounter;
	}

}  // namespace GLTF

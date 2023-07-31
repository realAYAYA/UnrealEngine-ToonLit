// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFMesh.h"

#include "ConversionUtilities.h"
#include "Misc/SecureHash.h"

namespace GLTF
{
	namespace
	{
		// convertible to 0.0 .. 1.0 factor
		// (colors, tex coords, weights, etc.)
		bool IsConvertibleToNormalizedFloat(const FAccessor& Attrib)
		{
			return Attrib.ComponentType == FAccessor::EComponentType::F32 ||
			       (Attrib.Normalized &&
			        (Attrib.ComponentType == FAccessor::EComponentType::U8 || Attrib.ComponentType == FAccessor::EComponentType::U16));
		}
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
		else
			check(false);
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

	bool FPrimitive::IsValid() const
	{
		// make sure all semantic attributes meet the spec

		if (!Position.IsValid())
		{
			return false;
		}

		const uint32 VertexCount = Position.Count;

		if (Position.Type != FAccessor::EType::Vec3 || Position.ComponentType != FAccessor::EComponentType::F32)
		{
			return false;
		}

		if (Normal.IsValid())
		{
			if (Normal.Count != VertexCount)
			{
				return false;
			}

			if (Normal.Type != FAccessor::EType::Vec3 || Normal.ComponentType != FAccessor::EComponentType::F32)
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

			if (Tangent.Type != FAccessor::EType::Vec4 || Tangent.ComponentType != FAccessor::EComponentType::F32)
			{
				return false;
			}
		}

		const FAccessor* TexCoords[] = {&TexCoord0, &TexCoord1};
		for (const FAccessor* TexCoord : TexCoords)
		{
			if (TexCoord->IsValid())
			{
				if (TexCoord->Count != VertexCount)
				{
					return false;
				}

				if (TexCoord->Type != FAccessor::EType::Vec2 || !IsConvertibleToNormalizedFloat(*TexCoord))
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

			if (!(Color0.Type == FAccessor::EType::Vec3 || Color0.Type == FAccessor::EType::Vec4) || !IsConvertibleToNormalizedFloat(Color0))
			{
				return false;
			}
		}

		// TODO: validate ranges? index buffer values?

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
}  // namespace GLTF

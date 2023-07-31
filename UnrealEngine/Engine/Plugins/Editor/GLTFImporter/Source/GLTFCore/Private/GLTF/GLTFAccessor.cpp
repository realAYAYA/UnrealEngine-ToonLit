// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFAccessor.h"

#include "ConversionUtilities.h"
#include "Misc/SecureHash.h"

namespace GLTF
{
	namespace
	{
		uint32 GetElementSize(FAccessor::EType Type, FAccessor::EComponentType ComponentType)
		{
			static const uint8 ComponentSize[]      = {0, 1, 1, 2, 2, 4, 4};      // keep in sync with EComponentType
			static const uint8 ComponentsPerValue[] = {0, 1, 2, 3, 4, 4, 9, 16};  // keep in sync with EType

			static_assert(
			    (int)FAccessor::EType::Unknown == 0 && ((int)FAccessor::EType::Count) == (sizeof(ComponentsPerValue) / sizeof(ComponentsPerValue[0])),
			    "EType doesn't match!");
			static_assert((int)FAccessor::EComponentType::None == 0 &&
			                  ((int)FAccessor::EComponentType::Count) == (sizeof(ComponentSize) / sizeof(ComponentSize[0])),
			              "EComponentType doesn't match!");

			return ComponentsPerValue[(int)Type] * ComponentSize[(int)ComponentType];
		}

		template <class ReturnType, uint32 Count>
		ReturnType GetNormalized(FAccessor::EComponentType ComponentType, const void* Pointer)
		{
			ReturnType Res;

			// convert to 0..1
			if (ComponentType == FAccessor::EComponentType::U8)
			{
				const uint8*    P = static_cast<const uint8*>(Pointer);
				constexpr float S = 1.0f / 255.0f;
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					Res[Index] = P[Index] * S;
				}
			}
			else if (ComponentType == FAccessor::EComponentType::U16)
			{
				const uint16*   P = static_cast<const uint16*>(Pointer);
				constexpr float S = 1.0f / 65535.0f;
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					Res[Index] = P[Index] * S;
				}
			}
			else
				check(false);

			return Res;
		}

		FMatrix GetMatrix(const void* Pointer)
		{
			// copy float mat4 directly from buffer
			const float* P = static_cast<const float*>(Pointer);

			FMatrix Matrix;
			for (int32 Row = 0; Row < 4; ++Row)
			{
				for (int32 Col = 0; Col < 4; ++Col)
				{
					// glTF stores matrix elements in column major order
					// Unreal's FMatrix is row major
					Matrix.M[Row][Col] = P[Col * 4 + Row];
				}
			}
			return Matrix;
		}

		template <typename DstT, typename SrcT>
		void Copy(DstT* Dst, const SrcT* Src, uint32 Count)
		{
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				*(Dst++) = *(Src++);
			}
		}

		template <typename DstT, uint32 ElementCount>
		void CopyNormalized(DstT* Dst, const void* Src, FAccessor::EComponentType ComponentType, uint32 Count)
		{
			// convert to 0..1
			if (ComponentType == FAccessor::EComponentType::U8)
			{
				const uint8*    P = static_cast<const uint8*>(Src);
				constexpr float S = 1.0f / 255.0f;
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					DstT& VecDst = *Dst++;
					for (uint32 J = 0; J < ElementCount; ++J)
					{
						VecDst[J] = P[J] * S;
					}
					P += ElementCount;
				}
			}
			else if (ComponentType == FAccessor::EComponentType::U16)
			{
				const uint16*   P = static_cast<const uint16*>(Src);
				constexpr float S = 1.0f / 65535.0f;
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					DstT& VecDst = *Dst++;
					for (uint32 J = 0; J < ElementCount; ++J)
					{
						VecDst[J] = P[J] * S;
					}
					P += ElementCount;
				}
			}
			else
				check(false);
		}

		// Copy data items that don't need conversion/expansion(i.e. Vec3 to Vec3, uint8 to uint8(not uint16)
		// but including normalized from fixed-point types
		template<typename ItemType, uint32 ItemElementCount>
		void CopyWithoutConversion(const FValidAccessor& Accessor, ItemType* Buffer)
		{
			// Stride equals item size => use simpler copy
			if ((Accessor.ByteStride == 0) || Accessor.ByteStride == sizeof(ItemType))
			{
				const void* Src = Accessor.DataAt(0);
				if (Accessor.ComponentType == FAccessor::EComponentType::F32)
				{
					memcpy(Buffer, Src, Accessor.Count * sizeof(ItemType));
				}
				else if (Accessor.Normalized)
				{
					CopyNormalized<ItemType, ItemElementCount>(Buffer, Src, Accessor.ComponentType, Accessor.Count);
				}
			}
			else
			{
				if (Accessor.ComponentType == FAccessor::EComponentType::F32)
				{
					for (uint32 Index = 0; Index < Accessor.Count; ++Index)
					{
						const void* Pointer = Accessor.DataAt(Index);
						Buffer[Index] = *static_cast<const ItemType*>(Pointer);
					}
				}
				else if (Accessor.Normalized)
				{
					for (uint32 Index = 0; Index < Accessor.Count; ++Index)
					{
						const void* Pointer = Accessor.DataAt(Index);
						Buffer[Index] = GetNormalized<ItemType, ItemElementCount>(Accessor.ComponentType, Pointer);
					}
				}
			}
		}
	}

	FAccessor::FAccessor(uint32 InCount, EType InType, EComponentType InCompType, bool InNormalized)
	    : Count(InCount)
	    , Type(InType)
	    , ComponentType(InCompType)
	    , Normalized(InNormalized)
	{
	}

	uint32 FAccessor::GetUnsignedInt(uint32 Index) const
	{
		return 0;
	}

	void FAccessor::GetUnsignedInt16x4(uint32 Index, uint16 Values[4]) const {}

	float FAccessor::GetFloat(uint32 Index) const
	{
		return 0.f;
	}

	FVector2D FAccessor::GetVec2(uint32 Index) const
	{
		return FVector2D::ZeroVector;
	}

	FVector FAccessor::GetVec3(uint32 Index) const
	{
		return FVector::ZeroVector;
	}

	FVector4 FAccessor::GetVec4(uint32 Index) const
	{
		return FVector4();
	}

	FMatrix FAccessor::GetMat4(uint32 Index) const
	{
		return FMatrix::Identity;
	}

	void FAccessor::GetUnsignedIntArray(uint32* Buffer) const {}

	void FAccessor::GetFloatArray(float* Buffer) const {}

	void FAccessor::GetVec2Array(FVector2f* Buffer) const {}

	void FAccessor::GetVec3Array(FVector3f* Buffer) const {}

	void FAccessor::GetVec4Array(FVector4f* Buffer) const {}

	void FAccessor::GetMat4Array(FMatrix44f* Buffer) const {}

	void FAccessor::GetCoordArray(FVector3f* Buffer) const
	{
		GetVec3Array(Buffer);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			Buffer[Index] = (FVector3f)ConvertVec3((FVector)Buffer[Index]);
		}
	}

	void FAccessor::GetQuatArray(FVector4f* Buffer) const
	{
		GetVec4Array(Buffer);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FVector4f& Value = Buffer[Index];
			FQuat4f Quat(Value[0], Value[1], Value[2], Value[3]);
			Quat = GLTF::ConvertQuat(Quat);
			Value.X = Quat.X;
			Value.Y = Quat.Y;
			Value.Z = Quat.Z;
			Value.W = Quat.W;
		}
	}

	//

	FValidAccessor::FValidAccessor(FBufferView& InBufferView, uint32 InOffset, uint32 InCount, EType InType, EComponentType InCompType,
	                               bool InNormalized)
	    : FAccessor(InCount, InType, InCompType, InNormalized)
	    , BufferView(InBufferView)
	    , ByteOffset(InOffset)
	    , ElementSize(GetElementSize(Type, ComponentType))
		// BufferView.ByteStride zero means elements are tightly packed
		, ByteStride(InBufferView.ByteStride ? InBufferView.ByteStride : ElementSize)
	{
	}

	bool FValidAccessor::IsValid() const
	{
		return BufferView.IsValid();
	}

	FMD5Hash FValidAccessor::GetHash() const
	{
		if (!IsValid())
		{
			return FMD5Hash();
		}

		FMD5 MD5;

		// Contigous view into a buffer: Can hash all at once
		if (ByteStride == ElementSize)
		{
			MD5.Update(DataAt(0), Count * ElementSize);
		}
		// Shared buffer: Have to hash in strides
		else
		{
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				MD5.Update(DataAt(Index), ElementSize);
			}
		}

		uint8 TypeInt = static_cast<uint8>(Type);
		uint8 ComponentInt = static_cast<uint8>(ComponentType);
		uint8 NormalizedInt = static_cast<uint8>(Normalized);

		MD5.Update(&TypeInt, sizeof(TypeInt));
		MD5.Update(&ComponentInt, sizeof(ComponentInt));
		MD5.Update(&NormalizedInt, sizeof(NormalizedInt));

		FMD5Hash Hash;
		Hash.Set(MD5);
		return Hash;
	}

	uint32 FValidAccessor::GetUnsignedInt(uint32 Index) const
	{
		// should be Scalar, not Normalized, unsigned integer (8, 16 or 32 bit)

		if (Index < Count)
		{
			if (Type == EType::Scalar && !Normalized)
			{
				const uint8* ValuePtr = DataAt(Index);
				switch (ComponentType)
				{
					case EComponentType::U8:
						return *ValuePtr;
					case EComponentType::U16:
						return *reinterpret_cast<const uint16*>(ValuePtr);
					case EComponentType::U32:
						return *reinterpret_cast<const uint32*>(ValuePtr);
					default:
						break;
				}
			}
		}

		check(false);
		return 0;
	}

	void FValidAccessor::GetUnsignedInt16x4(uint32 Index, uint16 Values[4]) const
	{
		// should be Vec4, not Normalized, unsigned integer (8 or 16 bit)

		if (Index < Count)
		{
			if (Type == EType::Vec4 && !Normalized)
			{
				const void* ValuePtr = DataAt(Index);
				switch (ComponentType)
				{
					case EComponentType::U8:
						for (int i = 0; i < 4; ++i)
						{
							Values[i] = ((uint8*)ValuePtr)[i];
						}
						return;
					case EComponentType::U16:
						for (int i = 0; i < 4; ++i)
						{
							Values[i] = ((uint16*)ValuePtr)[i];
						}
						return;
					default:
						break;
				}
			}
		}
		check(false);
	}

	float FValidAccessor::GetFloat(uint32 Index) const
	{
		// should be Scalar float

		if (Index < Count)
		{
			if (Type == EType::Scalar && !Normalized)
			{
				const uint8* ValuePtr = DataAt(Index);
				switch (ComponentType)
				{
					case EComponentType::F32:
						return *ValuePtr;
					default:
						break;
				}
			}
		}

		check(false);
		return 0.f;
	}

	FVector2D FValidAccessor::GetVec2(uint32 Index) const
	{
		// Spec-defined attributes (TEXCOORD_0, TEXCOORD_1) use only these formats:
		// - F32
		// - U8 normalized
		// - U16 normalized
		// Custom attributes can use any CompType, so add support for those when needed.

		if (Index < Count)
		{
			if (Type == EType::Vec2)  // strict format match, unlike GPU shader fetch
			{
				const void* Pointer = DataAt(Index);

				if (ComponentType == EComponentType::F32)
				{
					// copy float vec2 directly from buffer
					return FVector2D(*reinterpret_cast<const FVector2f*>(Pointer));
				}
				else if (Normalized)
				{
					return FVector2D(GetNormalized<FVector2f, 2>(ComponentType, Pointer));
				}
			}
		}

		check(false);
		return FVector2D::ZeroVector;
	}

	FVector FValidAccessor::GetVec3(uint32 Index) const
	{
		// Spec-defined attributes (POSITION, NORMAL, COLOR_0) use only these formats:
		// - F32
		// - U8 normalized
		// - U16 normalized
		// Custom attributes can use any CompType, so add support for those when needed.

		if (Index < Count)
		{
			if (Type == EType::Vec3)  // strict format match, unlike GPU shader fetch
			{
				const void* Pointer = DataAt(Index);

				if (ComponentType == EComponentType::F32)
				{
					// copy float vec3 directly from buffer
					return FVector(*reinterpret_cast<const FVector3f*>(Pointer));
				}
				else if (Normalized)
				{
					return FVector(GetNormalized<FVector3f, 3>(ComponentType, Pointer));
				}
			}
		}

		check(false);
		return FVector::ZeroVector;
	}

	FVector4 FValidAccessor::GetVec4(uint32 Index) const
	{
		// Spec-defined attributes (TANGENT, COLOR_0) use only these formats:
		// - F32
		// - U8 normalized
		// - U16 normalized
		// Custom attributes can use any CompType, so add support for those when needed.

		if (Index < Count)
		{
			if (Type == EType::Vec4)  // strict format match, unlike GPU shader fetch
			{
				const void* Pointer = DataAt(Index);

				if (ComponentType == EComponentType::F32)
				{
					// copy float vec4 directly from buffer
					return FVector4(*reinterpret_cast<const FVector4f*>(Pointer));
				}
				else if (Normalized)
				{
					return FVector4(GetNormalized<FVector4f, 4>(ComponentType, Pointer));
				}
			}
		}

		check(false);
		return FVector4();
	}

	FMatrix FValidAccessor::GetMat4(uint32 Index) const
	{
		// Focus on F32 for now, add other types as needed.

		if (Index < Count)
		{
			if (Type == EType::Mat4)  // strict format match, unlike GPU shader fetch
			{
				if (ComponentType == EComponentType::F32)
				{
					const void* Pointer = DataAt(Index);
					return GetMatrix(Pointer);
				}
			}
		}

		check(false);
		return FMatrix();
	}

	void FValidAccessor::GetUnsignedIntArray(uint32* Buffer) const
	{
		if (Type == EType::Scalar && !Normalized)
		{
			const uint8* Src = DataAt(0);
			switch (ComponentType)
			{
				case EComponentType::U8:
					Copy(Buffer, reinterpret_cast<const uint8*>(Src), Count);
					return;
				case EComponentType::U16:
					Copy(Buffer, reinterpret_cast<const uint16*>(Src), Count);
					return;
				case EComponentType::U32:
					memcpy(Buffer, Src, Count * sizeof(uint32));
					return;
				default:
					break;
			}
		}

		check(false);
	}

	void FValidAccessor::GetFloatArray(float* Buffer) const
	{
		if (Type == EType::Scalar && !Normalized)
		{
			const uint8* Src = DataAt(0);
			switch (ComponentType)
			{
				case EComponentType::F32:
					memcpy(Buffer, Src, Count * sizeof(float));
					return;
				default:
					break;
			}
		}

		check(false);
	}

	void FValidAccessor::GetVec2Array(FVector2f* Buffer) const
	{
		if (!ensure(Type == EType::Vec2))
		{
			return;
		}
		CopyWithoutConversion<FVector2f, 2>(*this, Buffer);
	}

	void FValidAccessor::GetVec3Array(FVector3f* Buffer) const
	{
		if (!ensure(Type == EType::Vec3))
		{
			return;
		}
		CopyWithoutConversion<FVector3f, 3>(*this, Buffer);
	}

	void FValidAccessor::GetVec4Array(FVector4f* Buffer) const
	{
		if (!ensure(Type == EType::Vec4))
		{
			return;
		}
		CopyWithoutConversion<FVector4f, 4>(*this, Buffer);
	}

	void FValidAccessor::GetMat4Array(FMatrix44f* Buffer) const
	{
		if (Type == EType::Mat4 && ComponentType == EComponentType::F32)  // strict format match, unlike GPU shader fetch
		{
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				const void* Pointer = DataAt(Index);
				Buffer[Index]       = FMatrix44f(GetMatrix(Pointer));
			}
			return;
		}

		check(false);
	}

	inline const uint8* FValidAccessor::DataAt(uint32 Index) const
	{
		const uint32 Offset = Index * ByteStride;
		return BufferView.DataAt(Offset + ByteOffset);
	}

	//

	bool FVoidAccessor::IsValid() const
	{
		return false;
	}

	FMD5Hash FVoidAccessor::GetHash() const
	{
		return FMD5Hash();
	}
}  // namespace GLTF

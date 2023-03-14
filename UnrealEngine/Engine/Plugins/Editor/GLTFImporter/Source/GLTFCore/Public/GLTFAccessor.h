// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FMD5Hash;

namespace GLTF
{
	struct GLTFCORE_API FBuffer
	{
		const uint32 ByteLength;
		const uint8* Data;

		explicit FBuffer(uint32 InByteLength)
		    : ByteLength(InByteLength)
		    , Data(nullptr)
		{
		}

		bool IsValid() const
		{
			return Data != nullptr;
		}

		const uint8* DataAt(uint32 Offset) const
		{
			checkSlow(Data);
			return Data + Offset;
		}
	};

	struct GLTFCORE_API FBufferView
	{
		const FBuffer& Buffer;
		const uint32 ByteOffset;
		const uint32 ByteLength;
		// if zero then accessor elements are tightly packed, i.e., effective stride equals the size of the element
		const uint32 ByteStride;  // range 4..252

		explicit FBufferView(const FBuffer& InBuffer, uint32 InOffset, uint32 InLength, uint32 InStride)
		    : Buffer(InBuffer)
		    , ByteOffset(InOffset)
		    , ByteLength(InLength)
		    , ByteStride(InStride)
		{
			// check that view fits completely inside the buffer
		}

		bool IsValid() const
		{
			return Buffer.IsValid();
		}

		const uint8* DataAt(uint32 Offset) const
		{
			return Buffer.DataAt(Offset + ByteOffset);
		}
	};

	struct GLTFCORE_API FAccessor
	{
		// accessor stores the data but has no usage semantics

		enum class EType
		{
			Unknown,
			Scalar,
			Vec2,
			Vec3,
			Vec4,
			Mat2,
			Mat3,
			Mat4,
			Count
		};

		enum class EComponentType
		{
			None,
			S8,   // signed byte
			U8,   // unsigned byte
			S16,  // signed short
			U16,  // unsigned short
			U32,  // unsigned int -- only valid for indices, not attributes
			F32,  // float
			Count
		};

		const uint32         Count;
		const EType          Type;
		const EComponentType ComponentType;
		const bool           Normalized;

		FAccessor(uint32 InCount, EType InType, EComponentType InComponentType, bool InNormalized);

		virtual bool IsValid() const = 0;
		virtual FMD5Hash GetHash() const = 0;

		virtual uint32 GetUnsignedInt(uint32 Index) const;
		virtual void   GetUnsignedInt16x4(uint32 Index, uint16 Values[4]) const;

		virtual float     GetFloat(uint32 Index) const;
		virtual FVector2D GetVec2(uint32 Index) const;
		virtual FVector   GetVec3(uint32 Index) const;
		virtual FVector4  GetVec4(uint32 Index) const;
		virtual FMatrix   GetMat4(uint32 Index) const;

		void         GetUnsignedIntArray(TArray<uint32>& Buffer) const;
		virtual void GetUnsignedIntArray(uint32* Buffer) const;
		void         GetFloatArray(TArray<float>& Buffer) const;
		virtual void GetFloatArray(float* Buffer) const;
		void         GetVec2Array(TArray<FVector2f>& Buffer) const;
		virtual void GetVec2Array(FVector2f* Buffer) const;
		void         GetVec3Array(TArray<FVector3f>& Buffer) const;
		virtual void GetVec3Array(FVector3f* Buffer) const;
		///@note Performs axis conversion for vec3s(i.e. from glTF right-handed and Y-up to left-handed and Z-up).
		void         GetCoordArray(TArray<FVector3f>& Buffer) const;
		void         GetCoordArray(FVector3f* Buffer) const;
		void         GetVec4Array(TArray<FVector4f>& Buffer) const;
		virtual void GetVec4Array(FVector4f* Buffer) const;
		///@note Performs axis conversion for quaternion(i.e. from glTF right-handed and Y-up to left-handed and Z-up).
		void         GetQuatArray(TArray<FVector4f>& Buffer) const;
		void         GetQuatArray(FVector4f* Buffer) const;
		void         GetMat4Array(TArray<FMatrix44f>& Buffer) const;
		virtual void GetMat4Array(FMatrix44f* Buffer) const;
	};

	struct GLTFCORE_API FValidAccessor final : FAccessor
	{
		FValidAccessor(FBufferView& InBufferView, uint32 InOffset, uint32 InCount, EType InType, EComponentType InCompType, bool InNormalized);

		bool IsValid() const override;
		FMD5Hash GetHash() const override;

		uint32 GetUnsignedInt(uint32 Index) const override;
		void   GetUnsignedInt16x4(uint32 Index, uint16 Values[4]) const override;

		float     GetFloat(uint32 Index) const override;
		FVector2D GetVec2(uint32 Index) const override;
		FVector   GetVec3(uint32 Index) const override;
		FVector4  GetVec4(uint32 Index) const override;

		FMatrix GetMat4(uint32 Index) const override;

		void GetUnsignedIntArray(uint32* Buffer) const override;
		void GetFloatArray(float* Buffer) const override;
		void GetVec2Array(FVector2f* Buffer) const override;
		void GetVec3Array(FVector3f* Buffer) const override;
		void GetVec4Array(FVector4f* Buffer) const override;
		void GetMat4Array(FMatrix44f* Buffer) const override;

		const uint8* DataAt(uint32 Index) const;

		const FBufferView& BufferView;
		const uint32       ByteOffset;
		const uint32       ElementSize;
		const uint32	   ByteStride;
	};

	struct GLTFCORE_API FVoidAccessor final : FAccessor
	{
		FVoidAccessor()
		    : FAccessor(0, EType::Scalar, EComponentType::S8, false)
		{
		}

		bool IsValid() const override;
		FMD5Hash GetHash() const override;
	};

	//

	inline void FAccessor::GetUnsignedIntArray(TArray<uint32>& Buffer) const
	{
		if (IsValid())
			Buffer.SetNumUninitialized(Count, false);
		GetUnsignedIntArray(Buffer.GetData());
	}

	inline void FAccessor::GetFloatArray(TArray<float>& Buffer) const
	{
		if (IsValid())
			Buffer.SetNumUninitialized(Count, false);
		GetFloatArray(Buffer.GetData());
	}

	inline void FAccessor::GetVec2Array(TArray<FVector2f>& Buffer) const
	{
		if (IsValid())
			Buffer.SetNumUninitialized(Count, false);
		GetVec2Array(Buffer.GetData());
	}

	inline void FAccessor::GetVec3Array(TArray<FVector3f>& Buffer) const
	{
		if (IsValid())
			Buffer.SetNumUninitialized(Count, false);
		GetVec3Array(Buffer.GetData());
	}

	inline void FAccessor::GetCoordArray(TArray<FVector3f>& Buffer) const
	{
		if (IsValid())
			Buffer.SetNumUninitialized(Count, false);
		GetCoordArray(Buffer.GetData());
	}

	inline void FAccessor::GetVec4Array(TArray<FVector4f>& Buffer) const
	{
		if (IsValid())
			Buffer.SetNumUninitialized(Count, false);
		GetVec4Array(Buffer.GetData());
	}

	inline void FAccessor::GetQuatArray(TArray<FVector4f>& Buffer) const
	{
		if (IsValid())
			Buffer.SetNumUninitialized(Count, false);
		GetQuatArray(Buffer.GetData());
	}

	inline void FAccessor::GetMat4Array(TArray<FMatrix44f>& Buffer) const
	{
		if (IsValid())
			Buffer.SetNumUninitialized(Count, false);
		GetMat4Array(Buffer.GetData());
	}

}  // namespace GLTF

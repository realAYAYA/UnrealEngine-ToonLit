// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FMD5Hash;

namespace GLTF
{
	struct GLTFCORE_API FBuffer
	{
		const uint64 ByteLength;
		const uint8* Data;

		explicit FBuffer(uint64 InByteLength)
		    : ByteLength(InByteLength)
		    , Data(nullptr)
		{
		}

		bool IsValid() const
		{
			return Data != nullptr;
		}

		const uint8* DataAt(uint64 Offset) const
		{
			checkSlow(Data);
			return Data + Offset;
		}
	};

	struct GLTFCORE_API FBufferView
	{
		const FBuffer& Buffer;
		const uint64 ByteOffset;
		const uint64 ByteLength;
		// if zero then accessor elements are tightly packed, i.e., effective stride equals the size of the element
		const uint32 ByteStride;  // range 4..252

		explicit FBufferView(const FBuffer& InBuffer, uint64 InOffset, uint64 InLength, uint32 InStride)
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

		const uint8* DataAt(uint64 Offset) const
		{
			return Buffer.DataAt(Offset + ByteOffset);
		}
	};

	struct GLTFCORE_API FVoidBufferView final : FBufferView
	{
		FVoidBufferView()
			: FBufferView(FBuffer(0), 0, 0, 0)
		{

		}
		static FVoidBufferView& GetVoidBufferView()
		{
			static FVoidBufferView Void;
			return Void;
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

		struct GLTFCORE_API FSparse
		{
			bool                     bHasSparse;

			const uint32              Count;

			//Indices:
			struct FIndices
			{
				const int32          Count; //Helper for creating cache, equals to FSparse.Count

				const FBufferView&   BufferView;
				const uint64         ByteOffset;
				const EComponentType ComponentType;

				FIndices()
					: Count(0)
					, BufferView(FVoidBufferView::GetVoidBufferView())
					, ByteOffset(0)
					, ComponentType(EComponentType::None)
				{
				}

				FIndices(uint32 InCount, const FBufferView& InBufferView, uint64 InByteOffset, EComponentType InComponentType);
			} Indices;
			
			//Values:
			struct FValues
			{
				const FBufferView&   BufferView;
				const uint64         ByteOffset;
				
				FValues()
					: BufferView(FVoidBufferView::GetVoidBufferView())
					, ByteOffset(0)
				{
				}

				FValues(const FBufferView& InBufferView, uint64 InByteOffset)
					: BufferView(InBufferView)
					, ByteOffset(InByteOffset)
				{
				}
			} Values;
			
			FSparse()
				: bHasSparse(false)
				, Count(0)
				, Indices(FIndices())
				, Values(FValues())
			{
			}

			FSparse(uint32 InCount,
				const FBufferView& InIndicesBufferView, uint64 InIndicesByteOffset, EComponentType InIndicesComponentType,
				const FBufferView& InValuesBufferView, uint64 InValuesByteOffset)
				: bHasSparse(true)
				, Count(InCount)
				, Indices(InCount, InIndicesBufferView, InIndicesByteOffset, InIndicesComponentType)
				, Values(InValuesBufferView, InValuesByteOffset)
			{
			}
		};

		const uint32         Count;
		const EType          Type;
		const EComponentType ComponentType;
		const bool           bNormalized;
		bool                 bQuantized;
		const FSparse        Sparse;


		FAccessor(uint32 InCount, EType InType, EComponentType InComponentType, bool bInNormalized, const FSparse& InSparse);

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

		enum class EDataType
		{
			Position,
			Normal,
			Tangent,
			Texcoord,
			Color,
			Joints,
			Weights
		};
		bool IsValidDataType(EDataType DataType, bool bMorphTargetProperty) const;
		bool CheckAccessorTypeForDataType(EDataType DataType, bool bMorphTargetProperty) const;
		bool CheckNonQuantizedComponentTypeForDataType(EDataType DataType, bool bMorphTargetProperty) const;
		bool CheckQuantizedComponentTypeForDataType(EDataType DataType, bool bMorphTargetProperty) const;
	};

	struct GLTFCORE_API FValidAccessor final : FAccessor
	{
		FValidAccessor(FBufferView& InBufferView, uint64 InOffset, uint32 InCount, EType InType, EComponentType InCompType, bool bInNormalized, const FSparse& InSparse);

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
		const uint64       ByteOffset;
		const uint32       ElementSize;
		const uint32	   ByteStride;

		//Sparse related helpers:
		void UpdateUnsignedIntWithSparse(uint32 Index, uint32& Data) const;
		void UpdateUnsignedInt16x4WithSparse(uint32 Index, uint16 Data[4]) const;

		void UpdateFloatWithSparse(uint32 Index, float& Data) const;
		void UpdateVec2WithSparse(uint32 Index, FVector2D& Data) const;
		void UpdateVec3WithSparse(uint32 Index, FVector& Data) const;
		void UpdateVec4WithSparse(uint32 Index, FVector4& Data) const;

		void UpdateMat4WithSparse(uint32 Index, FMatrix& Data) const;

		void UpdateFloatArrayWithSparse(float* Data) const;
		void UpdateUnsignedIntArrayWithSparse(uint32* Data) const;

		template<typename ItemType, uint32 ItemElementCount>
		void UpdateArrayWithSparse(ItemType* Data) const;
	};

	struct GLTFCORE_API FVoidAccessor final : FAccessor
	{
		FVoidAccessor()
		    : FAccessor(0, EType::Scalar, EComponentType::S8, false, FSparse())
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

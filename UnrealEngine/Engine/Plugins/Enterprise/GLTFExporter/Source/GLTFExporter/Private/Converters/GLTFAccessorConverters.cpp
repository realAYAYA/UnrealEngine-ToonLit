// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFAccessorConverters.h"
#include "Utilities/GLTFCoreUtilities.h"
#include "Converters/GLTFBufferAdapter.h"
#include "Builders/GLTFConvertBuilder.h"

#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "RawIndexBuffer.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"

// TODO: Unreal-style implementation of std::conditional to avoid mixing in STL. Should be added to the engine.
template <bool Condition, class TypeIfTrue, class TypeIfFalse>
class TConditional
{
public:
	typedef TypeIfFalse Type;
};

template <class TypeIfTrue, class TypeIfFalse>
class TConditional<true, TypeIfTrue, TypeIfFalse>
{
public:
	typedef TypeIfTrue Type;
};

FGLTFJsonAccessor* FGLTFPositionBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FPositionVertexBuffer* VertexBuffer)
{
	if (VertexBuffer == nullptr || VertexBuffer->GetNumVertices() == 0)
	{
		return nullptr;
	}

	const TUniquePtr<IGLTFBufferAdapter> SourceBuffer = IGLTFBufferAdapter::GetPositions(VertexBuffer);
	const uint8* SourceData = SourceBuffer->GetData();

	if (SourceData == nullptr)
	{
		// TODO: report error
		return nullptr;
	}

	const TArray<uint32>& IndexMap = MeshSection->IndexMap;
	const uint32 VertexCount = IndexMap.Num();
	const uint32 Stride = VertexBuffer->GetStride();

	TArray<FGLTFVector3> Positions;
	Positions.AddUninitialized(VertexCount);

	for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const uint32 MappedVertexIndex = IndexMap[VertexIndex];
		const FVector3f& Position = *reinterpret_cast<const FVector3f*>(SourceData + Stride * MappedVertexIndex);
		Positions[VertexIndex] = FGLTFCoreUtilities::ConvertPosition(Position, Builder.ExportOptions->ExportUniformScale);
	}

	FGLTFJsonAccessor* JsonAccessor = Builder.AddAccessor();
	JsonAccessor->BufferView = Builder.AddBufferView(Positions, EGLTFJsonBufferTarget::ArrayBuffer);
	JsonAccessor->ComponentType = EGLTFJsonComponentType::Float;
	JsonAccessor->Count = VertexCount;
	JsonAccessor->Type = EGLTFJsonAccessorType::Vec3;

	if (VertexCount > 0)
	{
		// Calculate accurate bounding box based on raw vertex values
		JsonAccessor->MinMaxLength = 3;

		for (int32 ComponentIndex = 0; ComponentIndex < JsonAccessor->MinMaxLength; ComponentIndex++)
		{
			JsonAccessor->Min[ComponentIndex] = Positions[0].Components[ComponentIndex];
			JsonAccessor->Max[ComponentIndex] = Positions[0].Components[ComponentIndex];
		}

		for (uint32 VertexIndex = 1; VertexIndex < VertexCount; ++VertexIndex)
		{
			const FGLTFVector3& Position = Positions[VertexIndex];
			for (int32 ComponentIndex = 0; ComponentIndex < JsonAccessor->MinMaxLength; ComponentIndex++)
			{
				JsonAccessor->Min[ComponentIndex] = FMath::Min(JsonAccessor->Min[ComponentIndex], Position.Components[ComponentIndex]);
				JsonAccessor->Max[ComponentIndex] = FMath::Max(JsonAccessor->Max[ComponentIndex], Position.Components[ComponentIndex]);
			}
		}
	}

	return JsonAccessor;
}

FGLTFJsonAccessor* FGLTFColorBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FColorVertexBuffer* VertexBuffer)
{
	if (VertexBuffer == nullptr || VertexBuffer->GetNumVertices() == 0)
	{
		return nullptr;
	}

	const TUniquePtr<IGLTFBufferAdapter> SourceBuffer = IGLTFBufferAdapter::GetColors(VertexBuffer);
	const uint8* SourceData = SourceBuffer->GetData();

	if (SourceData == nullptr)
	{
		// TODO: report error
		return nullptr;
	}

	const TArray<uint32>& IndexMap = MeshSection->IndexMap;
	const uint32 VertexCount = IndexMap.Num();
	const uint32 Stride = VertexBuffer->GetStride();

	TArray<FGLTFUInt8Color4> Colors;
	Colors.AddUninitialized(VertexCount);

	for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const uint32 MappedVertexIndex = IndexMap[VertexIndex];
		const FColor& Color = *reinterpret_cast<const FColor*>(SourceData + Stride * MappedVertexIndex);
		Colors[VertexIndex] = FGLTFCoreUtilities::ConvertColor(Color);
	}

	FGLTFJsonAccessor* JsonAccessor = Builder.AddAccessor();
	JsonAccessor->BufferView = Builder.AddBufferView(Colors, EGLTFJsonBufferTarget::ArrayBuffer);
	JsonAccessor->ComponentType = EGLTFJsonComponentType::UInt8;
	JsonAccessor->Count = VertexCount;
	JsonAccessor->Type = EGLTFJsonAccessorType::Vec4;
	JsonAccessor->bNormalized = true;

	return JsonAccessor;
}

FGLTFJsonAccessor* FGLTFNormalBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer)
{
	if (VertexBuffer == nullptr || VertexBuffer->GetNumVertices() == 0)
	{
		return nullptr;
	}

	const TUniquePtr<IGLTFBufferAdapter> SourceBuffer = IGLTFBufferAdapter::GetTangents(VertexBuffer);
	const uint8* SourceData = SourceBuffer->GetData();

	if (SourceData == nullptr)
	{
		// TODO: report error
		return nullptr;
	}

	FGLTFJsonBufferView* BufferView;
	EGLTFJsonComponentType ComponentType;

	const bool bMeshQuantization = Builder.ExportOptions->bUseMeshQuantization;
	const bool bHighPrecision = VertexBuffer->GetUseHighPrecisionTangentBasis();

	if (bMeshQuantization)
	{
		Builder.AddExtension(EGLTFJsonExtension::KHR_MeshQuantization, true);

		if (bHighPrecision)
		{
			ComponentType = EGLTFJsonComponentType::Int16;
			BufferView = ConvertBufferView<FGLTFInt16Vector4, FPackedRGBA16N>(MeshSection, SourceData);
			BufferView->ByteStride = sizeof(FGLTFInt16Vector4);
		}
		else
		{
			ComponentType = EGLTFJsonComponentType::Int8;
			BufferView = ConvertBufferView<FGLTFInt8Vector4, FPackedNormal>(MeshSection, SourceData);
			BufferView->ByteStride = sizeof(FGLTFInt8Vector4);
		}
	}
	else
	{
		ComponentType = EGLTFJsonComponentType::Float;
		BufferView = bHighPrecision
			? ConvertBufferView<FGLTFVector3, FPackedRGBA16N>(MeshSection, SourceData)
			: ConvertBufferView<FGLTFVector3, FPackedNormal>(MeshSection, SourceData);
	}

	FGLTFJsonAccessor* JsonAccessor = Builder.AddAccessor();
	JsonAccessor->BufferView = BufferView;
	JsonAccessor->ComponentType = ComponentType;
	JsonAccessor->Count = MeshSection->IndexMap.Num();
	JsonAccessor->Type = EGLTFJsonAccessorType::Vec3;
	JsonAccessor->bNormalized = bMeshQuantization;

	return JsonAccessor;
}

template <typename DestinationType, typename SourceType>
FGLTFJsonBufferView* FGLTFNormalBufferConverter::ConvertBufferView(const FGLTFMeshSection* MeshSection, const void* SourceData)
{
	const TArray<uint32>& IndexMap = MeshSection->IndexMap;
	const uint32 VertexCount = IndexMap.Num();

	typedef TStaticMeshVertexTangentDatum<SourceType> VertexTangentType;
	const VertexTangentType* TangentData= static_cast<const VertexTangentType*>(SourceData);

	TArray<DestinationType> Normals;
	Normals.AddUninitialized(VertexCount);

	for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const uint32 MappedVertexIndex = IndexMap[VertexIndex];
		const FVector3f SafeNormal = TangentData[MappedVertexIndex].TangentZ.ToFVector3f().GetSafeNormal();

		typedef typename TConditional<TIsSame<DestinationType, FGLTFVector3>::Value, FVector3f, SourceType>::Type IntermediateType;
		Normals[VertexIndex] = FGLTFCoreUtilities::ConvertNormal(IntermediateType(SafeNormal));
	}

	return Builder.AddBufferView(Normals, EGLTFJsonBufferTarget::ArrayBuffer);
}

FGLTFJsonAccessor* FGLTFTangentBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer)
{
	if (VertexBuffer == nullptr || VertexBuffer->GetNumVertices() == 0)
	{
		return nullptr;
	}

	const TUniquePtr<IGLTFBufferAdapter> SourceBuffer = IGLTFBufferAdapter::GetTangents(VertexBuffer);
	const uint8* SourceData = SourceBuffer->GetData();

	if (SourceData == nullptr)
	{
		// TODO: report error
		return nullptr;
	}

	FGLTFJsonBufferView* BufferView;
	EGLTFJsonComponentType ComponentType;

	const bool bMeshQuantization = Builder.ExportOptions->bUseMeshQuantization;
	const bool bHighPrecision = VertexBuffer->GetUseHighPrecisionTangentBasis();

	if (bMeshQuantization)
	{
		Builder.AddExtension(EGLTFJsonExtension::KHR_MeshQuantization, true);

		if (bHighPrecision)
		{
			ComponentType = EGLTFJsonComponentType::Int16;
			BufferView = ConvertBufferView<FGLTFInt16Vector4, FPackedRGBA16N>(MeshSection, SourceData);
		}
		else
		{
			ComponentType = EGLTFJsonComponentType::Int8;
			BufferView = ConvertBufferView<FGLTFInt8Vector4, FPackedNormal>(MeshSection, SourceData);
		}
	}
	else
	{
		ComponentType = EGLTFJsonComponentType::Float;
		BufferView = bHighPrecision
			? ConvertBufferView<FGLTFVector4, FPackedRGBA16N>(MeshSection, SourceData)
			: ConvertBufferView<FGLTFVector4, FPackedNormal>(MeshSection, SourceData);
	}

	FGLTFJsonAccessor* JsonAccessor = Builder.AddAccessor();
	JsonAccessor->BufferView = BufferView;
	JsonAccessor->ComponentType = ComponentType;
	JsonAccessor->Count = MeshSection->IndexMap.Num();
	JsonAccessor->Type = EGLTFJsonAccessorType::Vec4;
	JsonAccessor->bNormalized = bMeshQuantization;

	return JsonAccessor;
}

template <typename DestinationType, typename SourceType>
FGLTFJsonBufferView* FGLTFTangentBufferConverter::ConvertBufferView(const FGLTFMeshSection* MeshSection, const void* SourceData)
{
	const TArray<uint32>& IndexMap = MeshSection->IndexMap;
	const uint32 VertexCount = IndexMap.Num();

	typedef TStaticMeshVertexTangentDatum<SourceType> VertexTangentType;
	const VertexTangentType* VertexTangents = static_cast<const VertexTangentType*>(SourceData);

	TArray<DestinationType> Tangents;
	Tangents.AddUninitialized(VertexCount);

	for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const uint32 MappedVertexIndex = IndexMap[VertexIndex];
		const FVector3f SafeTangent = VertexTangents[MappedVertexIndex].TangentX.ToFVector3f().GetSafeNormal();

		typedef typename TConditional<TIsSame<DestinationType, FGLTFVector4>::Value, FVector3f, SourceType>::Type IntermediateType;
		Tangents[VertexIndex] = FGLTFCoreUtilities::ConvertTangent(IntermediateType(SafeTangent));
	}

	return Builder.AddBufferView(Tangents, EGLTFJsonBufferTarget::ArrayBuffer);
}

FGLTFJsonAccessor* FGLTFUVBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer, uint32 UVIndex)
{
	if (VertexBuffer == nullptr || VertexBuffer->GetNumVertices() == 0)
	{
		return nullptr;
	}

	const uint32 UVCount = VertexBuffer->GetNumTexCoords();
	if (UVIndex >= UVCount)
	{
		// TODO: report warning
		return nullptr;
	}

	// TODO: report warning or add support for half float precision UVs, i.e. !VertexBuffer->GetUseFullPrecisionUVs()?

	const TUniquePtr<IGLTFBufferAdapter> SourceBuffer = IGLTFBufferAdapter::GetUVs(VertexBuffer);
	const uint8* SourceData = SourceBuffer->GetData();

	if (SourceData == nullptr)
	{
		// TODO: report error
		return nullptr;
	}

	return VertexBuffer->GetUseFullPrecisionUVs()
		? Convert<FVector2f>(MeshSection, VertexBuffer, UVIndex, SourceData)
		: Convert<FVector2DHalf>(MeshSection, VertexBuffer, UVIndex, SourceData);
}

template <typename SourceType>
FGLTFJsonAccessor* FGLTFUVBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer, uint32 UVIndex, const uint8* SourceData) const
{
	const TArray<uint32>& IndexMap = MeshSection->IndexMap;
	const uint32 VertexCount = IndexMap.Num();

	TArray<FGLTFVector2> UVs;
	UVs.AddUninitialized(VertexCount);

	const uint32 UVCount = VertexBuffer->GetNumTexCoords();
	const SourceType* UVData = reinterpret_cast<const SourceType*>(SourceData);

	for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const uint32 MappedVertexIndex = IndexMap[VertexIndex];
		const SourceType& UV = UVData[UVCount * MappedVertexIndex + UVIndex];
		UVs[VertexIndex] = FGLTFCoreUtilities::ConvertUV(UV);
	}

	FGLTFJsonAccessor* JsonAccessor = Builder.AddAccessor();
	JsonAccessor->BufferView = Builder.AddBufferView(UVs, EGLTFJsonBufferTarget::ArrayBuffer);
	JsonAccessor->ComponentType = EGLTFJsonComponentType::Float;
	JsonAccessor->Count = VertexCount;
	JsonAccessor->Type = EGLTFJsonAccessorType::Vec2;

	return JsonAccessor;
}

FGLTFJsonAccessor* FGLTFBoneIndexBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset)
{
	if (VertexBuffer == nullptr || VertexBuffer->GetNumVertices() == 0)
	{
		return nullptr;
	}

	const TUniquePtr<IGLTFBufferAdapter> SourceBuffer = IGLTFBufferAdapter::GetInfluences(VertexBuffer);
	const uint8* SourceData = SourceBuffer->GetData();

	if (SourceData == nullptr)
	{
		// TODO: report error
		return nullptr;
	}

	return MeshSection->MaxBoneIndex > UINT8_MAX
		? Convert<uint16>(MeshSection, VertexBuffer, InfluenceOffset, SourceData)
		: Convert<uint8>(MeshSection, VertexBuffer, InfluenceOffset, SourceData);
}

template <typename DestinationType>
FGLTFJsonAccessor* FGLTFBoneIndexBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset, const uint8* SourceData) const
{
	return VertexBuffer->Use16BitBoneIndex()
		? Convert<DestinationType, uint16>(MeshSection, VertexBuffer, InfluenceOffset, SourceData)
		: Convert<DestinationType, uint8>(MeshSection, VertexBuffer, InfluenceOffset, SourceData);
}

template <typename DestinationType, typename SourceType>
FGLTFJsonAccessor* FGLTFBoneIndexBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset, const uint8* SourceData) const
{
	if (VertexBuffer->GetVariableBonesPerVertex())
	{
		const TUniquePtr<IGLTFBufferAdapter> LookupBuffer = IGLTFBufferAdapter::GetLookups(VertexBuffer);
		const uint32* LookupData = reinterpret_cast<const uint32*>(LookupBuffer->GetData());

		if (LookupData == nullptr)
		{
			// TODO: report error
			return nullptr;
		}

		return Convert<DestinationType, SourceType>(MeshSection, VertexBuffer, InfluenceOffset, SourceData, [LookupData](uint32 VertexIndex, uint32& VertexDataOffset, uint32& VertexInfluenceCount)
		{
			const uint32 Value = LookupData[VertexIndex];
			VertexDataOffset = Value >> 8;
			VertexInfluenceCount = Value & 0xff;
		});
	}

	const uint32 MaxBoneInfluences = VertexBuffer->GetMaxBoneInfluences();
	return Convert<DestinationType, SourceType>(MeshSection, VertexBuffer, InfluenceOffset, SourceData, [MaxBoneInfluences](uint32 VertexIndex, uint32& VertexDataOffset, uint32& VertexInfluenceCount)
	{
		VertexDataOffset = (sizeof(SourceType) + sizeof(uint8)) * MaxBoneInfluences * VertexIndex;
		VertexInfluenceCount = MaxBoneInfluences;
	});
}

template <typename DestinationType, typename SourceType, typename CallbackType>
FGLTFJsonAccessor* FGLTFBoneIndexBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset, const uint8* SourceData, CallbackType GetVertexInfluenceOffsetCount) const
{
	const TArray<uint32>& IndexMap = MeshSection->IndexMap;
	const uint32 VertexCount = IndexMap.Num();

	struct FVertexBoneIndices
	{
		DestinationType Index[4];
	};

	TArray<FVertexBoneIndices> BoneIndices;
	BoneIndices.AddUninitialized(VertexCount);

	for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const TArray<FBoneIndexType>& BoneMap = MeshSection->BoneMaps[MeshSection->BoneMapLookup[VertexIndex]];
		const uint32 MappedVertexIndex = IndexMap[VertexIndex];
		FVertexBoneIndices& VertexBoneIndices = BoneIndices[VertexIndex];

		uint32 VertexDataOffset;
		uint32 VertexInfluenceCount;
		GetVertexInfluenceOffsetCount(MappedVertexIndex, VertexDataOffset, VertexInfluenceCount);

		const SourceType* VertexBoneIndexData = reinterpret_cast<const SourceType*>(SourceData + VertexDataOffset);
		const int32 InfluenceCount = FMath::Min(static_cast<int32>(VertexInfluenceCount - InfluenceOffset), 4);

		for (int32 InfluenceIndex = 0; InfluenceIndex < InfluenceCount; ++InfluenceIndex)
		{
			const uint32 UnmappedBoneIndex = VertexBoneIndexData[InfluenceOffset + InfluenceIndex];
			VertexBoneIndices.Index[InfluenceIndex] = static_cast<DestinationType>(BoneMap[UnmappedBoneIndex]);
		}

		for (int32 InfluenceIndex = InfluenceCount; InfluenceIndex < 4; ++InfluenceIndex)
		{
			VertexBoneIndices.Index[InfluenceIndex] = 0;
		}
	}

	FGLTFJsonAccessor* JsonAccessor = Builder.AddAccessor();
	JsonAccessor->BufferView = Builder.AddBufferView(BoneIndices, EGLTFJsonBufferTarget::ArrayBuffer);
	JsonAccessor->ComponentType = FGLTFCoreUtilities::GetComponentType<DestinationType>();
	JsonAccessor->Count = VertexCount;
	JsonAccessor->Type = EGLTFJsonAccessorType::Vec4;

	return JsonAccessor;
}

FGLTFJsonAccessor* FGLTFBoneWeightBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset)
{
	if (VertexBuffer == nullptr || VertexBuffer->GetNumVertices() == 0)
	{
		return nullptr;
	}

	const TUniquePtr<IGLTFBufferAdapter> SourceBuffer = IGLTFBufferAdapter::GetInfluences(VertexBuffer);
	const uint8* SourceData = SourceBuffer->GetData();

	if (SourceData == nullptr)
	{
		// TODO: report error
		return nullptr;
	}

	return MeshSection->MaxBoneIndex > UINT8_MAX
		? Convert<uint16>(MeshSection, VertexBuffer, InfluenceOffset, SourceData)
		: Convert<uint8>(MeshSection, VertexBuffer, InfluenceOffset, SourceData);
}

template <typename BoneIndexType>
FGLTFJsonAccessor* FGLTFBoneWeightBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset, const uint8* SourceData) const
{
	if (VertexBuffer->GetVariableBonesPerVertex())
	{
		const TUniquePtr<IGLTFBufferAdapter> LookupBuffer = IGLTFBufferAdapter::GetLookups(VertexBuffer);
		const uint32* LookupData = reinterpret_cast<const uint32*>(LookupBuffer->GetData());

		if (LookupData == nullptr)
		{
			// TODO: report error
			return nullptr;
		}

		return Convert<BoneIndexType>(MeshSection, VertexBuffer, InfluenceOffset, SourceData, [LookupData](uint32 VertexIndex, uint32& VertexDataOffset, uint32& VertexInfluenceCount)
		{
			const uint32 Value = LookupData[VertexIndex];
			VertexDataOffset = Value >> 8;
			VertexInfluenceCount = Value & 0xff;
		});
	}

	const uint32 MaxBoneInfluences = VertexBuffer->GetMaxBoneInfluences();
	return Convert<BoneIndexType>(MeshSection, VertexBuffer, InfluenceOffset, SourceData, [MaxBoneInfluences](uint32 VertexIndex, uint32& VertexDataOffset, uint32& VertexInfluenceCount)
	{
		VertexDataOffset = (sizeof(BoneIndexType) + sizeof(uint8)) * MaxBoneInfluences * VertexIndex;
		VertexInfluenceCount = MaxBoneInfluences;
	});
}

template <typename BoneIndexType, typename CallbackType>
FGLTFJsonAccessor* FGLTFBoneWeightBufferConverter::Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset, const uint8* SourceData, CallbackType GetVertexInfluenceOffsetCount) const
{
	const TArray<uint32>& IndexMap = MeshSection->IndexMap;
	const uint32 VertexCount = IndexMap.Num();

	struct FVertexBoneWeights
	{
		uint8 Weights[4];
	};

	TArray<FVertexBoneWeights> BoneWeights;
	BoneWeights.AddUninitialized(VertexCount);

	for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const uint32 MappedVertexIndex = IndexMap[VertexIndex];
		FVertexBoneWeights& VertexBoneWeights = BoneWeights[VertexIndex];

		uint32 VertexDataOffset;
		uint32 VertexInfluenceCount;
		GetVertexInfluenceOffsetCount(MappedVertexIndex, VertexDataOffset, VertexInfluenceCount);

		const uint8* VertexBoneWeightsData = SourceData + VertexDataOffset + sizeof(BoneIndexType) * VertexInfluenceCount;
		const int32 InfluenceCount = FMath::Min(static_cast<int32>(VertexInfluenceCount - InfluenceOffset), 4);

		for (int32 InfluenceIndex = 0; InfluenceIndex < InfluenceCount; ++InfluenceIndex)
		{
			VertexBoneWeights.Weights[InfluenceIndex] = VertexBoneWeightsData[InfluenceOffset + InfluenceIndex];
		}

		for (int32 InfluenceIndex = InfluenceCount; InfluenceIndex < 4; ++InfluenceIndex)
		{
			VertexBoneWeights.Weights[InfluenceIndex] = 0;
		}
	}

	FGLTFJsonAccessor* JsonAccessor = Builder.AddAccessor();
	JsonAccessor->BufferView = Builder.AddBufferView(BoneWeights, EGLTFJsonBufferTarget::ArrayBuffer);
	JsonAccessor->ComponentType = EGLTFJsonComponentType::UInt8;
	JsonAccessor->Count = VertexCount;
	JsonAccessor->Type = EGLTFJsonAccessorType::Vec4;
	JsonAccessor->bNormalized = true;

	return JsonAccessor;
}

FGLTFJsonAccessor* FGLTFIndexBufferConverter::Convert(const FGLTFMeshSection* MeshSection)
{
	const uint32 MaxVertexIndex = MeshSection->IndexMap.Num() - 1;
	if (MaxVertexIndex <= UINT8_MAX) return Convert<uint8>(MeshSection);
	if (MaxVertexIndex <= UINT16_MAX) return Convert<uint16>(MeshSection);
	return Convert<uint32>(MeshSection);
}

template <typename IndexType>
FGLTFJsonAccessor* FGLTFIndexBufferConverter::Convert(const FGLTFMeshSection* MeshSection) const
{
	const TArray<uint32>& IndexBuffer = MeshSection->IndexBuffer;
	const uint32 IndexCount = IndexBuffer.Num();
	if (IndexCount == 0)
	{
		return nullptr;
	}

	TArray<IndexType> Indices;
	Indices.AddUninitialized(IndexCount);

	for (uint32 Index = 0; Index < IndexCount; ++Index)
	{
		Indices[Index] = static_cast<IndexType>(IndexBuffer[Index]);
	}

	FGLTFJsonAccessor* JsonAccessor = Builder.AddAccessor();
	JsonAccessor->BufferView = Builder.AddBufferView(Indices, EGLTFJsonBufferTarget::ElementArrayBuffer, sizeof(IndexType));
	JsonAccessor->ComponentType = FGLTFCoreUtilities::GetComponentType<IndexType>();
	JsonAccessor->Count = IndexCount;
	JsonAccessor->Type = EGLTFJsonAccessorType::Scalar;

	return JsonAccessor;
}

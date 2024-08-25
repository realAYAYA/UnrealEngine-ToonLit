// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/GLTFDelayedMeshTasks.h"
#include "Converters/GLTFMeshUtilities.h"
#include "Converters/GLTFBufferAdapter.h"
#include "Builders/GLTFConvertBuilder.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "StaticMeshComponentLODInfo.h"
#include "StaticMeshResources.h"
#include "Rendering/SkeletalMeshRenderData.h"

#include "Converters/GLTFMaterialUtilities.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Components/SplineMeshComponent.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "Converters/GLTFNormalArray.h"
#include "Converters/GLTFUVArray.h"
#include "Utilities/GLTFLandscapeComponentDataInterface.h"

namespace
{
	template <typename VectorType>
	void CheckTangentVectors(const void* SourceData, uint32 VertexCount, bool& bOutZeroNormals, bool& bOutZeroTangents)
	{
		bool bZeroNormals = false;
		bool bZeroTangents = false;

		typedef TStaticMeshVertexTangentDatum<VectorType> VertexTangentType;
		const VertexTangentType* VertexTangents = static_cast<const VertexTangentType*>(SourceData);

		for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
		{
			const VertexTangentType& VertexTangent = VertexTangents[VertexIndex];
			bZeroNormals |= VertexTangent.TangentZ.ToFVector().IsNearlyZero();
			bZeroTangents |= VertexTangent.TangentX.ToFVector().IsNearlyZero();
		}

		bOutZeroNormals = bZeroNormals;
		bOutZeroTangents = bZeroTangents;
	}

	void ValidateVertexBuffer(FGLTFConvertBuilder& Builder, const FStaticMeshVertexBuffer* VertexBuffer, const TCHAR* MeshName)
	{
		if (VertexBuffer == nullptr)
		{
			return;
		}

		const TUniquePtr<IGLTFBufferAdapter> SourceBuffer = IGLTFBufferAdapter::GetTangents(VertexBuffer);
		const uint8* SourceData = SourceBuffer->GetData();

		if (SourceData == nullptr)
		{
			return;
		}

		const uint32 VertexCount = VertexBuffer->GetNumVertices();
		bool bZeroNormals;
		bool bZeroTangents;

		if (VertexBuffer->GetUseHighPrecisionTangentBasis())
		{
			CheckTangentVectors<FPackedRGBA16N>(SourceData, VertexCount, bZeroNormals, bZeroTangents);
		}
		else
		{
			CheckTangentVectors<FPackedNormal>(SourceData, VertexCount, bZeroNormals, bZeroTangents);
		}

		if (bZeroNormals)
		{
			Builder.LogSuggestion(FString::Printf(
				TEXT("Mesh %s has some nearly zero-length normals which may not be supported in some glTF applications. Consider checking 'Recompute Normals' in the asset settings"),
				MeshName));
		}

		if (bZeroTangents)
		{
			Builder.LogSuggestion(FString::Printf(
				TEXT("Mesh %s has some nearly zero-length tangents which may not be supported in some glTF applications. Consider checking 'Recompute Tangents' in the asset settings"),
				MeshName));
		}
	}

	bool HasVertexColors(const FColorVertexBuffer* VertexBuffer)
	{
		if (VertexBuffer == nullptr)
		{
			return false;
		}

		const TUniquePtr<IGLTFBufferAdapter> SourceBuffer = IGLTFBufferAdapter::GetColors(VertexBuffer);
		const uint8* SourceData = SourceBuffer->GetData();

		if (SourceData == nullptr)
		{
			return false;
		}

		const uint32 VertexCount = VertexBuffer->GetNumVertices();
		const uint32 Stride = VertexBuffer->GetStride();

		for (uint32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
		{
			const FColor& Color = *reinterpret_cast<const FColor*>(SourceData + Stride * VertexIndex);
			if (Color != FColor::White)
			{
				return true;
			}
		}

		return false;
	}
}

FString FGLTFDelayedStaticMeshTask::GetName()
{
	return StaticMeshComponent != nullptr ? FGLTFNameUtilities::GetName(StaticMeshComponent) : StaticMesh->GetName();
}

void FGLTFDelayedStaticMeshTask::Process()
{
	FGLTFMeshUtilities::FullyLoad(StaticMesh);
	JsonMesh->Name = StaticMeshComponent != nullptr ? FGLTFNameUtilities::GetName(StaticMeshComponent) : StaticMesh->GetName();

	const FStaticMeshLODResources& RenderData = FGLTFMeshUtilities::GetRenderData(StaticMesh, LODIndex);
	const FPositionVertexBuffer& PositionBuffer = RenderData.VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& VertexBuffer = RenderData.VertexBuffers.StaticMeshVertexBuffer;
	const FColorVertexBuffer* ColorBuffer = &RenderData.VertexBuffers.ColorVertexBuffer; // TODO: add support for overriding color buffer by component

	if (Builder.ExportOptions->bExportVertexColors && HasVertexColors(ColorBuffer))
	{
		Builder.LogSuggestion(FString::Printf(
			TEXT("Vertex colors in mesh %s will act as a multiplier for base color in glTF, regardless of material, which may produce undesirable results"),
			*StaticMesh->GetName()));
	}
	else
	{
		ColorBuffer = nullptr;
	}

	if (StaticMeshComponent != nullptr && StaticMeshComponent->LODData.IsValidIndex(LODIndex))
	{
		const FStaticMeshComponentLODInfo& LODInfo = StaticMeshComponent->LODData[LODIndex];
		ColorBuffer = LODInfo.OverrideVertexColors != nullptr ? LODInfo.OverrideVertexColors : ColorBuffer;
	}

	const FGLTFMeshData* MeshData = Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::UseMeshData ?
		Builder.AddUniqueMeshData(StaticMesh, StaticMeshComponent, LODIndex) : nullptr;

#if WITH_EDITOR
	if (MeshData != nullptr)
	{
		if (MeshData->Description.IsEmpty())
		{
			// TODO: report warning in case the mesh actually has data, which means we failed to extract a mesh description.
			MeshData = nullptr;
		}
		else if (MeshData->BakeUsingTexCoord < 0)
		{
			// TODO: report warning (about missing texture coordinate for baking with mesh data).
			MeshData = nullptr;
		}
	}
#endif

	ValidateVertexBuffer(Builder, &VertexBuffer, *StaticMesh->GetName());

	const TArray<FStaticMaterial>& MaterialSlots = FGLTFMeshUtilities::GetMaterials(StaticMesh);
	const int32 MaterialCount = MaterialSlots.Num();

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		const FGLTFIndexArray SectionIndices = FGLTFMeshUtilities::GetSectionIndices(RenderData, MaterialIndex);
		const FGLTFMeshSection* ConvertedSection = MeshSectionConverter.GetOrAdd(StaticMesh, LODIndex, SectionIndices);

		FGLTFJsonPrimitive& JsonPrimitive = JsonMesh->Primitives[MaterialIndex];
		JsonPrimitive.Indices = Builder.AddUniqueIndexAccessor(ConvertedSection);

		JsonPrimitive.Attributes.Position = Builder.AddUniquePositionAccessor(ConvertedSection, &PositionBuffer);
		if (JsonPrimitive.Attributes.Position == nullptr)
		{
			Builder.LogError(
				FString::Printf(TEXT("Failed to export vertex positions related to material slot %d (%s) in static mesh %s"),
					MaterialIndex,
					*MaterialSlots[MaterialIndex].MaterialSlotName.ToString(),
					*ConvertedSection->ToString()
				));
		}

		if (ColorBuffer != nullptr)
		{
			JsonPrimitive.Attributes.Color0 = Builder.AddUniqueColorAccessor(ConvertedSection, ColorBuffer);
		}

		// TODO: report warning if both Mesh Quantization (export options) and Use High Precision Tangent Basis (vertex buffer) are disabled
		JsonPrimitive.Attributes.Normal = Builder.AddUniqueNormalAccessor(ConvertedSection, &VertexBuffer);
		JsonPrimitive.Attributes.Tangent = Builder.AddUniqueTangentAccessor(ConvertedSection, &VertexBuffer);

		const uint32 UVCount = VertexBuffer.GetNumTexCoords();
		// TODO: report warning or option to limit UV channels since most viewers don't support more than 2?
		JsonPrimitive.Attributes.TexCoords.AddUninitialized(UVCount);

		for (uint32 UVIndex = 0; UVIndex < UVCount; ++UVIndex)
		{
			JsonPrimitive.Attributes.TexCoords[UVIndex] = Builder.AddUniqueUVAccessor(ConvertedSection, &VertexBuffer, UVIndex);
		}

		const UMaterialInterface* Material = Materials[MaterialIndex];
		JsonPrimitive.Material = Builder.AddUniqueMaterial(Material, MeshData, SectionIndices);
	}
}

FString FGLTFDelayedSkeletalMeshTask::GetName()
{
	return SkeletalMeshComponent != nullptr ? FGLTFNameUtilities::GetName(SkeletalMeshComponent) : SkeletalMesh->GetName();
}

void FGLTFDelayedSkeletalMeshTask::Process()
{
	FGLTFMeshUtilities::FullyLoad(SkeletalMesh);
	JsonMesh->Name = SkeletalMeshComponent != nullptr ? FGLTFNameUtilities::GetName(SkeletalMeshComponent) : SkeletalMesh->GetName();

	const FSkeletalMeshLODRenderData& RenderData = FGLTFMeshUtilities::GetRenderData(SkeletalMesh, LODIndex);
	const FPositionVertexBuffer& PositionBuffer = RenderData.StaticVertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& VertexBuffer = RenderData.StaticVertexBuffers.StaticMeshVertexBuffer;
	const FColorVertexBuffer* ColorBuffer = &RenderData.StaticVertexBuffers.ColorVertexBuffer; // TODO: add support for overriding color buffer by component
	const FSkinWeightVertexBuffer* SkinWeightBuffer = RenderData.GetSkinWeightVertexBuffer(); // TODO: add support for overriding skin weight buffer by component
	// TODO: add support for skin weight profiles?
	// TODO: add support for morph targets

	if (Builder.ExportOptions->bExportVertexColors && HasVertexColors(ColorBuffer))
	{
		Builder.LogSuggestion(FString::Printf(
			TEXT("Vertex colors in mesh %s will act as a multiplier for base color in glTF, regardless of material, which may produce undesirable results"),
			*SkeletalMesh->GetName()));
	}
	else
	{
		ColorBuffer = nullptr;
	}

	if (SkeletalMeshComponent != nullptr && SkeletalMeshComponent->LODInfo.IsValidIndex(LODIndex))
	{
		const FSkelMeshComponentLODInfo& LODInfo = SkeletalMeshComponent->LODInfo[LODIndex];
		ColorBuffer = LODInfo.OverrideVertexColors != nullptr ? LODInfo.OverrideVertexColors : ColorBuffer;
		SkinWeightBuffer = LODInfo.OverrideSkinWeights != nullptr ? LODInfo.OverrideSkinWeights : SkinWeightBuffer;
	}

	const FGLTFMeshData* MeshData = Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::UseMeshData ?
		Builder.AddUniqueMeshData(SkeletalMesh, SkeletalMeshComponent, LODIndex) : nullptr;

#if WITH_EDITOR
	if (MeshData != nullptr)
	{
		if (MeshData->Description.IsEmpty())
		{
			// TODO: report warning in case the mesh actually has data, which means we failed to extract a mesh description.
			MeshData = nullptr;
		}
		else if (MeshData->BakeUsingTexCoord < 0)
		{
			// TODO: report warning (about missing texture coordinate for baking with mesh data).
			MeshData = nullptr;
		}
	}
#endif

	ValidateVertexBuffer(Builder, &VertexBuffer, *SkeletalMesh->GetName());

	const TArray<FSkeletalMaterial>& MaterialSlots = FGLTFMeshUtilities::GetMaterials(SkeletalMesh);
	const int32 MaterialCount = MaterialSlots.Num();

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		const FGLTFIndexArray SectionIndices = FGLTFMeshUtilities::GetSectionIndices(RenderData, MaterialIndex);
		const FGLTFMeshSection* ConvertedSection = MeshSectionConverter.GetOrAdd(SkeletalMesh, LODIndex, SectionIndices);

		FGLTFJsonPrimitive& JsonPrimitive = JsonMesh->Primitives[MaterialIndex];
		JsonPrimitive.Indices = Builder.AddUniqueIndexAccessor(ConvertedSection);

		JsonPrimitive.Attributes.Position = Builder.AddUniquePositionAccessor(ConvertedSection, &PositionBuffer);
		if (JsonPrimitive.Attributes.Position == nullptr)
		{
			Builder.LogError(
				FString::Printf(TEXT("Failed to export vertex positions related to material slot %d (%s) in skeletal mesh %s"),
					MaterialIndex,
					*MaterialSlots[MaterialIndex].MaterialSlotName.ToString(),
					*ConvertedSection->ToString()
				));
		}

		if (ColorBuffer != nullptr)
		{
			JsonPrimitive.Attributes.Color0 = Builder.AddUniqueColorAccessor(ConvertedSection, ColorBuffer);
		}

		// TODO: report warning if both Mesh Quantization (export options) and Use High Precision Tangent Basis (vertex buffer) are disabled
		JsonPrimitive.Attributes.Normal = Builder.AddUniqueNormalAccessor(ConvertedSection, &VertexBuffer);
		JsonPrimitive.Attributes.Tangent = Builder.AddUniqueTangentAccessor(ConvertedSection, &VertexBuffer);

		const uint32 UVCount = VertexBuffer.GetNumTexCoords();
		// TODO: report warning or option to limit UV channels since most viewers don't support more than 2?
		JsonPrimitive.Attributes.TexCoords.AddUninitialized(UVCount);

		for (uint32 UVIndex = 0; UVIndex < UVCount; ++UVIndex)
		{
			JsonPrimitive.Attributes.TexCoords[UVIndex] = Builder.AddUniqueUVAccessor(ConvertedSection, &VertexBuffer, UVIndex);
		}

		if (Builder.ExportOptions->bExportVertexSkinWeights)
		{
			const uint32 GroupCount = (SkinWeightBuffer->GetMaxBoneInfluences() + 3) / 4;
			// TODO: report warning or option to limit groups (of joints and weights) since most viewers don't support more than one?
			JsonPrimitive.Attributes.Joints.AddUninitialized(GroupCount);
			JsonPrimitive.Attributes.Weights.AddUninitialized(GroupCount);

			for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
			{
				JsonPrimitive.Attributes.Joints[GroupIndex] = Builder.AddUniqueJointAccessor(ConvertedSection, SkinWeightBuffer, GroupIndex * 4);
				JsonPrimitive.Attributes.Weights[GroupIndex] = Builder.AddUniqueWeightAccessor(ConvertedSection, SkinWeightBuffer, GroupIndex * 4);
			}
		}

		const UMaterialInterface* Material = Materials[MaterialIndex];
		JsonPrimitive.Material = Builder.AddUniqueMaterial(Material, MeshData, SectionIndices);
	}
}


FString FGLTFDelayedSplineMeshTask::GetName()
{
	return FGLTFNameUtilities::GetName(&SplineMeshComponent);
}

void FGLTFDelayedSplineMeshTask::Process()
{
	FGLTFMeshUtilities::FullyLoad(&StaticMesh);
	JsonMesh->Name = GetName();

	const FStaticMeshLODResources& RenderData = FGLTFMeshUtilities::GetRenderData(&StaticMesh, LODIndex);
	const FPositionVertexBuffer& PositionBuffer = RenderData.VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& VertexBuffer = RenderData.VertexBuffers.StaticMeshVertexBuffer;
	const FColorVertexBuffer* ColorBuffer = &RenderData.VertexBuffers.ColorVertexBuffer; // TODO: add support for overriding color buffer by component

	if (Builder.ExportOptions->bExportVertexColors && HasVertexColors(ColorBuffer))
	{
		Builder.LogSuggestion(FString::Printf(
			TEXT("Vertex colors in mesh %s will act as a multiplier for base color in glTF, regardless of material, which may produce undesirable results"),
			*StaticMesh.GetName()));
	}
	else
	{
		ColorBuffer = nullptr;
	}

	if (SplineMeshComponent.LODData.IsValidIndex(LODIndex))
	{
		const FStaticMeshComponentLODInfo& LODInfo = SplineMeshComponent.LODData[LODIndex];
		ColorBuffer = LODInfo.OverrideVertexColors != nullptr ? LODInfo.OverrideVertexColors : ColorBuffer;
	}

	const FGLTFMeshData* MeshData = Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::UseMeshData ?
	Builder.AddUniqueMeshData(&StaticMesh, &SplineMeshComponent, LODIndex) : nullptr;

	#if WITH_EDITOR
	if (MeshData != nullptr)
	{
		if (MeshData->Description.IsEmpty())
		{
			// TODO: report warning in case the mesh actually has data, which means we failed to extract a mesh description.
			MeshData = nullptr;
		}
		else if (MeshData->BakeUsingTexCoord < 0)
		{
			// TODO: report warning (about missing texture coordinate for baking with mesh data).
			MeshData = nullptr;
		}
	}
	#endif

	ValidateVertexBuffer(Builder, &VertexBuffer, *StaticMesh.GetName());

	const TArray<FStaticMaterial>& MaterialSlots = FGLTFMeshUtilities::GetMaterials(&StaticMesh);
	const int32 MaterialCount = MaterialSlots.Num();

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		const FGLTFIndexArray SectionIndices = FGLTFMeshUtilities::GetSectionIndices(RenderData, MaterialIndex);
		const FGLTFMeshSection* ConvertedSection = MeshSectionConverter.GetOrAdd(&StaticMesh, LODIndex, SectionIndices);

		FGLTFJsonPrimitive& JsonPrimitive = JsonMesh->Primitives[MaterialIndex];
		JsonPrimitive.Indices = Builder.AddUniqueIndexAccessor(ConvertedSection);

		FPositionVertexBuffer* TransformedPositionBuffer = new FPositionVertexBuffer();
		
		{//fix for Splines:
			TransformedPositionBuffer->Init(PositionBuffer.GetNumVertices(), true);

			const uint32 VertexCount = PositionBuffer.GetNumVertices();
			const uint32 Stride = PositionBuffer.GetStride();

			const TUniquePtr<IGLTFBufferAdapter> SourceBuffer = IGLTFBufferAdapter::GetPositions(&PositionBuffer);
			const uint8* SourceData = SourceBuffer->GetData();

			for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
			{
				FVector3f Position = *reinterpret_cast<const FVector3f*>(SourceData + Stride * VertexIndex);

				const FTransform3f SliceTransform = FTransform3f(SplineMeshComponent.CalcSliceTransform(USplineMeshComponent::GetAxisValueRef(Position, SplineMeshComponent.ForwardAxis)));
				USplineMeshComponent::GetAxisValueRef(Position, SplineMeshComponent.ForwardAxis) = 0;
				Position = SliceTransform.TransformPosition(Position);

				TransformedPositionBuffer->VertexPosition(VertexIndex) = FVector3f(Position.X, Position.Y, Position.Z);
			}
		}

		JsonPrimitive.Attributes.Position = Builder.AddUniquePositionAccessor(ConvertedSection, TransformedPositionBuffer);
		if (JsonPrimitive.Attributes.Position == nullptr)
		{
			Builder.LogError(
				FString::Printf(TEXT("Failed to export vertex positions related to material slot %d (%s) in static mesh %s"),
					MaterialIndex,
					*MaterialSlots[MaterialIndex].MaterialSlotName.ToString(),
					*ConvertedSection->ToString()
				));
		}

		if (ColorBuffer != nullptr)
		{
			JsonPrimitive.Attributes.Color0 = Builder.AddUniqueColorAccessor(ConvertedSection, ColorBuffer);
		}

		// TODO: report warning if both Mesh Quantization (export options) and Use High Precision Tangent Basis (vertex buffer) are disabled
		JsonPrimitive.Attributes.Normal = Builder.AddUniqueNormalAccessor(ConvertedSection, &VertexBuffer);
		JsonPrimitive.Attributes.Tangent = Builder.AddUniqueTangentAccessor(ConvertedSection, &VertexBuffer);

		const uint32 UVCount = VertexBuffer.GetNumTexCoords();
		// TODO: report warning or option to limit UV channels since most viewers don't support more than 2?
		JsonPrimitive.Attributes.TexCoords.AddUninitialized(UVCount);

		for (uint32 UVIndex = 0; UVIndex < UVCount; ++UVIndex)
		{
			JsonPrimitive.Attributes.TexCoords[UVIndex] = Builder.AddUniqueUVAccessor(ConvertedSection, &VertexBuffer, UVIndex);
		}

		const UMaterialInterface* Material = Materials[MaterialIndex];
		JsonPrimitive.Material = Builder.AddUniqueMaterial(Material, MeshData, SectionIndices);
	}
}


FGLTFDelayedLandscapeTask::FGLTFDelayedLandscapeTask(FGLTFConvertBuilder& Builder, const ULandscapeComponent& LandscapeComponent, FGLTFJsonMesh* JsonMesh, const UMaterialInterface& LandscapeMaterial)
	: FGLTFDelayedTask(EGLTFTaskPriority::Mesh)
	, Builder(Builder)
	, LandscapeComponent(LandscapeComponent)
	, JsonMesh(JsonMesh)
	, LandscapeMaterial(LandscapeMaterial)
{
}

FString FGLTFDelayedLandscapeTask::GetName()
{
	return LandscapeComponent.GetName();
}

void FGLTFDelayedLandscapeTask::Process()
{
	const ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(LandscapeComponent.GetOwner());
	JsonMesh->Name = LandscapeComponent.GetName();

	int32 MinX = MAX_int32, MinY = MAX_int32;
	int32 MaxX = MIN_int32, MaxY = MIN_int32;

	// Create and fill in the vertex position data source.
	int32 ExportLOD = 0;
#if WITH_EDITOR
	ExportLOD = Landscape->ExportLOD;
#endif
	const int32 ComponentSizeQuads = ((Landscape->ComponentSizeQuads + 1) >> ExportLOD) - 1;
	const float ScaleFactor = (float)Landscape->ComponentSizeQuads / (float)ComponentSizeQuads;
	const int32 VertexCount = FMath::Square(ComponentSizeQuads + 1);
	const int32 TriangleCount = FMath::Square(ComponentSizeQuads) * 2;

	FPositionVertexBuffer* PositionBuffer = new FPositionVertexBuffer();
	PositionBuffer->Init(VertexCount, true);

	FGLTFJsonPrimitive& JsonPrimitive = JsonMesh->Primitives[0];
	TArray<uint8> VisibilityData;
	VisibilityData.Empty(VertexCount);
	VisibilityData.AddZeroed(VertexCount);

	int OffsetX = Landscape->LandscapeSectionOffset.X;
	int OffsetY = Landscape->LandscapeSectionOffset.Y;
	
	FGLTFLandscapeComponentDataInterface CDI(LandscapeComponent, ExportLOD);

	TArray<uint8> CompVisData;
	const TArray<FWeightmapLayerAllocationInfo>& ComponentWeightmapLayerAllocations = LandscapeComponent.GetWeightmapLayerAllocations();

	for (int32 AllocIdx = 0; AllocIdx < ComponentWeightmapLayerAllocations.Num(); AllocIdx++)
	{
		const FWeightmapLayerAllocationInfo& AllocInfo = ComponentWeightmapLayerAllocations[AllocIdx];
		//Landscape Visibility Layer is named: __LANDSCAPE_VISIBILITY__
		//based on: Engine/Source/Runtime/Landscape/Private/Materials/MaterialExpressionLandscapeVisibilityMask.cpp
		//		FName UMaterialExpressionLandscapeVisibilityMask::ParameterName = FName("__LANDSCAPE_VISIBILITY__");
		FString LayerName = AllocInfo.LayerInfo->LayerName.ToString();
		if (LayerName == TEXT("__LANDSCAPE_VISIBILITY__"))
		{
			CDI.GetWeightmapTextureData(AllocInfo.LayerInfo, CompVisData);
		}
	}
	CompVisData.AddZeroed((ComponentSizeQuads + 1) * (ComponentSizeQuads + 1));

	if (CompVisData.Num() > 0)
	{
		for (int32 i = 0; i < VertexCount; ++i)
		{
			VisibilityData[i] = CompVisData[CDI.VertexIndexToTexel(i)];
		}
	}

	FGLTFNormalArray* Normals = new FGLTFNormalArray();
	Normals->SetNumZeroed(VertexCount);
	FGLTFUVArray* UVs = new FGLTFUVArray(); //UVIndex==0
	UVs->SetNumZeroed(VertexCount);

	for (int32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
	{
		int32 VertX, VertY;
		CDI.VertexIndexToXY(VertexIndex, VertX, VertY);

		FVector3f Position;
		FVector3f Normal;
		FVector2f UV;
		CDI.GetPositionNormalUV(VertX, VertY, Position, Normal, UV);

		PositionBuffer->VertexPosition(VertexIndex) = Position;
		(*Normals)[VertexIndex] = Normal;
		(*UVs)[VertexIndex] = UV;
	}

	const int32 VisThreshold = 170;
	
	FGLTFIndexArray* LandscapeIndices = new FGLTFIndexArray();
	LandscapeIndices->Reserve(FMath::Square(ComponentSizeQuads) * 2 * 3);

	for (int32 Y = 0; Y < ComponentSizeQuads; Y++)
	{
		for (int32 X = 0; X < ComponentSizeQuads; X++)
		{
			if (VisibilityData[Y * (ComponentSizeQuads + 1) + X] < VisThreshold)
			{
				LandscapeIndices->Push((X + 0) + (Y + 0) * (ComponentSizeQuads + 1));
				LandscapeIndices->Push((X + 1) + (Y + 1) * (ComponentSizeQuads + 1));
				LandscapeIndices->Push((X + 1) + (Y + 0) * (ComponentSizeQuads + 1));

				LandscapeIndices->Push((X + 0) + (Y + 0) * (ComponentSizeQuads + 1));
				LandscapeIndices->Push((X + 0) + (Y + 1) * (ComponentSizeQuads + 1));
				LandscapeIndices->Push((X + 1) + (Y + 1) * (ComponentSizeQuads + 1));
			}
		}
	}

	JsonPrimitive.Attributes.Position = Builder.AddUniquePositionAccessor(PositionBuffer);
	JsonPrimitive.Attributes.Normal = Builder.AddUniqueNormalAccessor(Normals);
	JsonPrimitive.Attributes.TexCoords.AddUninitialized(1);
	JsonPrimitive.Attributes.TexCoords[0] = Builder.AddUniqueUVAccessor(UVs);
	JsonPrimitive.Indices = Builder.AddUniqueIndexAccessor(LandscapeIndices, JsonMesh->Name);
	JsonPrimitive.Material = Builder.AddUniqueMaterial(&LandscapeMaterial);
}
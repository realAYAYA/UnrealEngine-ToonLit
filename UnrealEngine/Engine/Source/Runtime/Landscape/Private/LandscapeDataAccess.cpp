// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeDataAccess.h"
#include "LandscapeComponent.h"

#if WITH_EDITOR


FLandscapeComponentDataInterfaceBase::FLandscapeComponentDataInterfaceBase(ULandscapeComponent* InComponent, int32 InMipLevel, bool InWorkOnEditingLayer): 
	MipLevel(InMipLevel)
{
	UTexture2D* HeightMapTexture = InComponent->GetHeightmap(InWorkOnEditingLayer);
	HeightmapStride = HeightMapTexture->Source.GetSizeX() >> InMipLevel;
	HeightmapComponentOffsetX = FMath::RoundToInt((HeightMapTexture->Source.GetSizeX() >> InMipLevel) * InComponent->HeightmapScaleBias.Z);
	HeightmapComponentOffsetY = FMath::RoundToInt((HeightMapTexture->Source.GetSizeY() >> InMipLevel) * InComponent->HeightmapScaleBias.W);
	HeightmapSubsectionOffset = (InComponent->SubsectionSizeQuads + 1) >> InMipLevel;

	ComponentSizeQuads = InComponent->ComponentSizeQuads;
	ComponentSizeVerts = (InComponent->ComponentSizeQuads + 1) >> InMipLevel;
	SubsectionSizeVerts = (InComponent->SubsectionSizeQuads + 1) >> InMipLevel;
	ComponentNumSubsections = InComponent->NumSubsections;
}

LANDSCAPE_API FLandscapeComponentDataInterface::FLandscapeComponentDataInterface(ULandscapeComponent* InComponent, int32 InMipLevel, bool InWorkOnEditingLayer) :
	FLandscapeComponentDataInterfaceBase(InComponent, InMipLevel, InWorkOnEditingLayer),
	Component(InComponent),
	bWorkOnEditingLayer(InWorkOnEditingLayer),
	HeightMipData(NULL),
	XYOffsetMipData(NULL)
{
	UTexture2D* HeightMapTexture = Component->GetHeightmap(bWorkOnEditingLayer);
	if (MipLevel < HeightMapTexture->Source.GetNumMips())
	{
		HeightMipData = (FColor*)DataInterface.LockMip(HeightMapTexture, MipLevel);
		if (Component->XYOffsetmapTexture)
		{
			XYOffsetMipData = (FColor*)DataInterface.LockMip(Component->XYOffsetmapTexture, MipLevel);
		}
	}
}

LANDSCAPE_API FLandscapeComponentDataInterface::~FLandscapeComponentDataInterface()
{
	if (HeightMipData)
	{
		UTexture2D* HeightMapTexture = Component->GetHeightmap(bWorkOnEditingLayer);
		DataInterface.UnlockMip(HeightMapTexture, MipLevel);
		if (Component->XYOffsetmapTexture)
		{
			DataInterface.UnlockMip(Component->XYOffsetmapTexture, MipLevel);
		}
	}
}

LANDSCAPE_API void FLandscapeComponentDataInterface::GetHeightmapTextureData(TArray<FColor>& OutData, bool bOkToFail)
{
	if (bOkToFail && !HeightMipData)
	{
		OutData.Empty();
		return;
	}
#if LANDSCAPE_VALIDATE_DATA_ACCESS
	check(HeightMipData);
#endif
	int32 HeightmapSize = ((Component->SubsectionSizeQuads + 1) * Component->NumSubsections) >> MipLevel;
	OutData.Empty(FMath::Square(HeightmapSize));
	OutData.AddUninitialized(FMath::Square(HeightmapSize));

	for (int32 SubY = 0; SubY < HeightmapSize; SubY++)
	{
		// X/Y of the vertex we're looking at in component's coordinates.
		int32 CompY = SubY;

		// UV coordinates of the data offset into the texture
		int32 TexV = SubY + HeightmapComponentOffsetY;

		// Copy the data
		FMemory::Memcpy(&OutData[CompY * HeightmapSize], &HeightMipData[HeightmapComponentOffsetX + TexV * HeightmapStride], HeightmapSize * sizeof(FColor));
	}
}

LANDSCAPE_API bool FLandscapeComponentDataInterface::GetWeightmapTextureData(ULandscapeLayerInfoObject* InLayerInfo, TArray<uint8>& OutData, bool bInUseEditingWeightmap, bool bInRemoveSubsectionDuplicates)
{
	int32 LayerIdx = INDEX_NONE;
	const TArray<FWeightmapLayerAllocationInfo>& ComponentWeightmapLayerAllocations = Component->GetWeightmapLayerAllocations(bInUseEditingWeightmap);
	const TArray<UTexture2D*>& ComponentWeightmapTextures = Component->GetWeightmapTextures(bInUseEditingWeightmap);

	for (int32 Idx = 0; Idx < ComponentWeightmapLayerAllocations.Num(); Idx++)
	{
		if (ComponentWeightmapLayerAllocations[Idx].LayerInfo == InLayerInfo)
		{
			LayerIdx = Idx;
			break;
		}
	}
	if (LayerIdx < 0)
	{
		return false;
	}
	if (ComponentWeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex >= ComponentWeightmapTextures.Num())
	{
		return false;
	}
	if (ComponentWeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel >= 4)
	{
		return false;
	}

	// If requested to skip the duplicate row/col of texture data
	int32 WeightmapSize = bInRemoveSubsectionDuplicates ?
		((Component->SubsectionSizeQuads * Component->NumSubsections) + 1) >> MipLevel :
		((Component->SubsectionSizeQuads + 1) * Component->NumSubsections) >> MipLevel;
	
	OutData.Empty(FMath::Square(WeightmapSize));
	OutData.AddUninitialized(FMath::Square(WeightmapSize));

	// DataInterface Lock is a LockMipReadOnly on the texture
	const FColor* WeightMipData = (const FColor*)DataInterface.LockMip(ComponentWeightmapTextures[ComponentWeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex], MipLevel);

	// Channel remapping
	int32 ChannelOffsets[4] = { (int32)STRUCT_OFFSET(FColor, R), (int32)STRUCT_OFFSET(FColor, G), (int32)STRUCT_OFFSET(FColor, B), (int32)STRUCT_OFFSET(FColor, A) };

	const uint8* SrcTextureData = (const uint8*)WeightMipData + ChannelOffsets[ComponentWeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel];

	for (int32 i = 0; i < FMath::Square(WeightmapSize); i++)
	{
		// If removing subsection duplicates, convert vertex to texel index
		OutData[i] = bInRemoveSubsectionDuplicates ? SrcTextureData[VertexIndexToTexel(i) * sizeof(FColor)] : SrcTextureData[i * sizeof(FColor)];
	}

	DataInterface.UnlockMip(ComponentWeightmapTextures[ComponentWeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex], MipLevel);
	return true;
}

LANDSCAPE_API FColor* FLandscapeComponentDataInterface::GetXYOffsetData(int32 LocalX, int32 LocalY) const
{
#if LANDSCAPE_VALIDATE_DATA_ACCESS
	check(Component);
	check(LocalX >= 0 && LocalY >= 0 && LocalX < ComponentSizeQuads + 1 && LocalY < ComponentSizeQuads + 1);
#endif

	const int32 WeightmapSize = ((Component->SubsectionSizeQuads + 1) * Component->NumSubsections) >> MipLevel;
	int32 SubNumX;
	int32 SubNumY;
	int32 SubX;
	int32 SubY;
	ComponentXYToSubsectionXY(LocalX, LocalY, SubNumX, SubNumY, SubX, SubY);

	return &XYOffsetMipData[SubX + SubNumX*SubsectionSizeVerts + (SubY + SubNumY*SubsectionSizeVerts)*WeightmapSize];
}

LANDSCAPE_API FVector FLandscapeComponentDataInterface::GetLocalVertex(int32 LocalX, int32 LocalY) const
{
	const float ScaleFactor = (float)ComponentSizeQuads / (float)(ComponentSizeVerts - 1);
	float XOffset, YOffset;
	GetXYOffset(LocalX, LocalY, XOffset, YOffset);
	return FVector(LocalX * ScaleFactor + XOffset, LocalY * ScaleFactor + YOffset, LandscapeDataAccess::GetLocalHeight(GetHeight(LocalX, LocalY)));
}

LANDSCAPE_API FVector FLandscapeComponentDataInterface::GetWorldVertex(int32 LocalX, int32 LocalY) const
{
	return Component->GetComponentTransform().TransformPosition(GetLocalVertex(LocalX, LocalY));
}

LANDSCAPE_API void FLandscapeComponentDataInterface::GetWorldTangentVectors(int32 LocalX, int32 LocalY, FVector& WorldTangentX, FVector& WorldTangentY, FVector& WorldTangentZ) const
{
	FColor* Data = GetHeightData(LocalX, LocalY);
	WorldTangentZ = LandscapeDataAccess::UnpackNormal(*Data);
	WorldTangentX = FVector(-WorldTangentZ.Z, 0.f, WorldTangentZ.X);
	WorldTangentY = FVector(0.f, WorldTangentZ.Z, -WorldTangentZ.Y);

	WorldTangentX = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentX);
	WorldTangentY = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentY);
	WorldTangentZ = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentZ);
}

LANDSCAPE_API void FLandscapeComponentDataInterface::GetWorldPositionTangents(int32 LocalX, int32 LocalY, FVector& WorldPos, FVector& WorldTangentX, FVector& WorldTangentY, FVector& WorldTangentZ) const
{
	FColor* Data = GetHeightData(LocalX, LocalY);
	WorldTangentZ = LandscapeDataAccess::UnpackNormal(*Data);
	WorldTangentX = FVector(WorldTangentZ.Z, 0.f, -WorldTangentZ.X);
	WorldTangentY = WorldTangentZ ^ WorldTangentX;

	float Height = LandscapeDataAccess::UnpackHeight(*Data);

	const float ScaleFactor = (float)ComponentSizeQuads / (float)(ComponentSizeVerts - 1);
	float XOffset, YOffset;
	GetXYOffset(LocalX, LocalY, XOffset, YOffset);
	WorldPos = Component->GetComponentTransform().TransformPosition(FVector(LocalX * ScaleFactor + XOffset, LocalY * ScaleFactor + YOffset, Height));
	WorldTangentX = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentX);
	WorldTangentY = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentY);
	WorldTangentZ = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentZ);
}

int32 FLandscapeComponentDataInterface::GetHeightmapSizeX(int32 MipIndex) const
{
	return Component->GetHeightmap(bWorkOnEditingLayer)->Source.GetSizeX() >> MipIndex;
}

int32 FLandscapeComponentDataInterface::GetHeightmapSizeY(int32 MipIndex) const
{
	return Component->GetHeightmap(bWorkOnEditingLayer)->Source.GetSizeY() >> MipIndex;
}

#endif // WITH_EDITOR

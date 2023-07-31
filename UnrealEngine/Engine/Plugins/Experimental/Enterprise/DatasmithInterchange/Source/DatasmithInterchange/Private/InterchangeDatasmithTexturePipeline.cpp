// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeDatasmithTexturePipeline.h"

#include "InterchangeDatasmithTextureData.h"
#include "InterchangeDatasmithUtils.h"

#include "InterchangeDatasmithSceneNode.h"

#include "InterchangeTexture2DFactoryNode.h"
#include "InterchangeTextureNode.h"
#include "IDatasmithSceneElements.h"

void UInterchangeDatasmithTexturePipeline::ExecutePreImportPipeline(UInterchangeBaseNodeContainer* InNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas)
{
	using namespace UE::DatasmithInterchange;

	Super::ExecutePreImportPipeline(InNodeContainer, InSourceDatas);

	for (UInterchangeTextureFactoryNode* TextureFactoryNode : NodeUtils::GetNodes<UInterchangeTextureFactoryNode>(InNodeContainer))
	{
		PreImportTextureFactoryNode(InNodeContainer, TextureFactoryNode);
	}
}

void UInterchangeDatasmithTexturePipeline::PreImportTextureFactoryNode(UInterchangeBaseNodeContainer* InNodeContainer, UInterchangeTextureFactoryNode* TextureFactoryNode) const
{
	using namespace UE::DatasmithInterchange;
	
	TArray<FString> TargetNodes;
	TextureFactoryNode->GetTargetNodeUids(TargetNodes);
	if (TargetNodes.Num() == 0)
	{
		return;
	}

	const UInterchangeBaseNode* TargetNode = InNodeContainer->GetNode(TargetNodes[0]);
	if (!FInterchangeDatasmithTextureData::HasData(TargetNode))
	{
		return;
	}

	FInterchangeDatasmithTextureDataConst TextureData(TargetNode);
	TOptional< bool > bLocalFlipNormalMapGreenChannel;
	TOptional< TextureMipGenSettings > MipGenSettings;
	TOptional< TextureGroup > LODGroup;
	TOptional< TextureCompressionSettings > CompressionSettings;

	// Make sure to set the proper LODGroup as it's used to determine the CompressionSettings when using TEXTUREGROUP_WorldNormalMap
	EDatasmithTextureMode TextureMode;
	if (TextureData.GetCustomTextureMode(TextureMode))
	{
		switch (TextureMode)
		{
		case EDatasmithTextureMode::Diffuse:
			LODGroup = TEXTUREGROUP_World;
			break;
		case EDatasmithTextureMode::Specular:
			LODGroup = TEXTUREGROUP_WorldSpecular;
			break;
		case EDatasmithTextureMode::Bump:
		case EDatasmithTextureMode::Normal:
			LODGroup = TEXTUREGROUP_WorldNormalMap;
			CompressionSettings = TC_Normalmap;
			break;
		case EDatasmithTextureMode::NormalGreenInv:
			LODGroup = TEXTUREGROUP_WorldNormalMap;
			CompressionSettings = TC_Normalmap;
			bLocalFlipNormalMapGreenChannel = true;
			break;
		}
	}

	const TOptional< float > RGBCurve = [&TextureData]() -> TOptional< float >
	{
		float ElementRGBCurve;

		if (TextureData.GetCustomRGBCurve(ElementRGBCurve)
			&& FMath::IsNearlyEqual(ElementRGBCurve, 1.0f) == false
			&& ElementRGBCurve > 0.f)
		{
			return ElementRGBCurve;
		}

		return {};
	}();

	static_assert(TextureAddress::TA_Wrap == (int)EDatasmithTextureAddress::Wrap && TextureAddress::TA_Mirror == (int)EDatasmithTextureAddress::Mirror, "Texture Address enum doesn't match!");

	TOptional< TextureFilter > TexFilter;
	EDatasmithTextureFilter TextureFilter;
	if (TextureData.GetCustomTextureFilter(TextureFilter))
	{
		switch (TextureFilter)
		{
		case EDatasmithTextureFilter::Nearest:
			TexFilter = TextureFilter::TF_Nearest;
			break;
		case EDatasmithTextureFilter::Bilinear:
			TexFilter = TextureFilter::TF_Bilinear;
			break;
		case EDatasmithTextureFilter::Trilinear:
			TexFilter = TextureFilter::TF_Trilinear;
			break;
		}
	}

	TOptional< bool > bSrgb;
	EDatasmithColorSpace ColorSpace;
	if (TextureData.GetCustomSRGB(ColorSpace))
	{
		if (ColorSpace == EDatasmithColorSpace::sRGB)
		{
			bSrgb = true;
		}
		else if (ColorSpace == EDatasmithColorSpace::Linear)
		{
			bSrgb = false;
		}
	}

	EDatasmithTextureAddress AddressX;
	EDatasmithTextureAddress AddressY;
	UInterchangeTexture2DFactoryNode* Texture2DFactoryNode = Cast<UInterchangeTexture2DFactoryNode>(TextureFactoryNode);
	if (Texture2DFactoryNode
		&& TextureData.GetCustomTextureAddressX(AddressX)
		&& TextureData.GetCustomTextureAddressY(AddressY))
	{
		Texture2DFactoryNode->SetCustomAddressX((TextureAddress)AddressX);
		Texture2DFactoryNode->SetCustomAddressY((TextureAddress)AddressY);
	}

	if (bSrgb.IsSet())
	{
		TextureFactoryNode->SetCustomSRGB(bSrgb.GetValue());
	}

	if (bLocalFlipNormalMapGreenChannel.IsSet())
	{
		TextureFactoryNode->SetCustombFlipGreenChannel(bLocalFlipNormalMapGreenChannel.GetValue());
	}

	if (MipGenSettings.IsSet())
	{
		TextureFactoryNode->SetCustomMipGenSettings(MipGenSettings.GetValue());
	}

	if (LODGroup.IsSet())
	{
		TextureFactoryNode->SetCustomLODGroup(LODGroup.GetValue());
	}

	if (CompressionSettings.IsSet())
	{
		TextureFactoryNode->SetCustomCompressionSettings(CompressionSettings.GetValue());
	}

	if (RGBCurve.IsSet())
	{
		TextureFactoryNode->SetCustomAdjustRGBCurve(RGBCurve.GetValue());
	}

	if (TexFilter.IsSet())
	{
		TextureFactoryNode->SetCustomFilter(TexFilter.GetValue());
	}
}
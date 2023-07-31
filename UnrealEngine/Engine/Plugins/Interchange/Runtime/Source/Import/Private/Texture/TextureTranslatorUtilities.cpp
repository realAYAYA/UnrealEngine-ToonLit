// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Texture/TextureTranslatorUtilities.h"

#include "CoreMinimal.h"
#include "InterchangeSourceData.h"
#include "InterchangeTexture2DArrayNode.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeTextureCubeArrayNode.h"
#include "InterchangeTextureCubeNode.h"
#include "InterchangeTextureLightProfileNode.h"
#include "InterchangeTextureNode.h"
#include "InterchangeVolumeTextureNode.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

namespace UE::Interchange::Private
{
	bool GenericTextureTranslate(const UInterchangeSourceData* SourceData, UInterchangeBaseNodeContainer& BaseNodeContainer, const TSubclassOf<UInterchangeTextureNode>& TextureNodeClass)
	{
		FString Filename = SourceData->GetFilename();
		FPaths::NormalizeFilename(Filename);
		if (!FPaths::FileExists(Filename))
		{
			return false;
		}

		UClass* Class = TextureNodeClass.Get();
		if (!ensure(Class))
		{
			return false;
		}

		FString DisplayLabel = FPaths::GetBaseFilename(Filename);
		FString NodeUID(Filename);
		UInterchangeTextureNode* TextureNode = NewObject<UInterchangeTextureNode>(&BaseNodeContainer, Class);
		if (!ensure(TextureNode))
		{
			return false;
		}

		TextureNode->InitializeNode(NodeUID, DisplayLabel, EInterchangeNodeContainerType::TranslatedAsset);
		TextureNode->SetPayLoadKey(Filename);

		BaseNodeContainer.AddNode(TextureNode);

		return true;
	}
}

bool UE::Interchange::FTextureTranslatorUtilities::Generic2DTextureTranslate(const UInterchangeSourceData* SourceData, UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	return Private::GenericTextureTranslate(SourceData, BaseNodeContainer, UInterchangeTexture2DNode::StaticClass());
}

bool UE::Interchange::FTextureTranslatorUtilities::GenericTextureCubeTranslate(const UInterchangeSourceData* SourceData, UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	return Private::GenericTextureTranslate(SourceData, BaseNodeContainer, UInterchangeTextureCubeNode::StaticClass());
}

bool UE::Interchange::FTextureTranslatorUtilities::GenericTextureCubeArrayTranslate(const UInterchangeSourceData* SourceData, UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	return Private::GenericTextureTranslate(SourceData, BaseNodeContainer, UInterchangeTextureCubeArrayNode::StaticClass());
}

bool UE::Interchange::FTextureTranslatorUtilities::GenericTexture2DArrayTranslate(const UInterchangeSourceData* SourceData, UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	return Private::GenericTextureTranslate(SourceData, BaseNodeContainer, UInterchangeTexture2DArrayNode::StaticClass());
}

bool UE::Interchange::FTextureTranslatorUtilities::GenericVolumeTextureTranslate(const UInterchangeSourceData* SourceData, UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	return Private::GenericTextureTranslate(SourceData, BaseNodeContainer, UInterchangeVolumeTextureNode::StaticClass());
}

bool UE::Interchange::FTextureTranslatorUtilities::GenericTextureLightProfileTranslate(const UInterchangeSourceData* SourceData, UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	return Private::GenericTextureTranslate(SourceData, BaseNodeContainer, UInterchangeTextureLightProfileNode::StaticClass());
}

// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeGenericTexturePipeline.h"

#include "Engine/Texture.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureLightProfile.h"
#include "InterchangePipelineLog.h"
#include "InterchangeTexture2DArrayFactoryNode.h"
#include "InterchangeTexture2DArrayNode.h"
#include "InterchangeTexture2DFactoryNode.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeTextureCubeArrayFactoryNode.h"
#include "InterchangeTextureCubeArrayNode.h"
#include "InterchangeTextureCubeFactoryNode.h"
#include "InterchangeTextureCubeNode.h"
#include "InterchangeTextureFactoryNode.h"
#include "InterchangeTextureLightProfileFactoryNode.h"
#include "InterchangeTextureLightProfileNode.h"
#include "InterchangeTextureNode.h"
#include "InterchangeVolumeTextureNode.h"
#include "InterchangeVolumeTextureFactoryNode.h"
#include "Misc/Paths.h"
#include "Engine/Texture.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGenericTexturePipeline)

#if WITH_EDITOR
#include "NormalMapIdentification.h"
#include "TextureCompiler.h"
#include "UDIMUtilities.h"
#endif //WITH_EDITOR

namespace UE::Interchange::Private
{
	UClass* GetDefaultFactoryClassFromTextureNodeClass(UClass* NodeClass)
	{
		if (UInterchangeTexture2DNode::StaticClass() == NodeClass)
		{
			return UInterchangeTexture2DFactoryNode::StaticClass();
		}

		if (UInterchangeTextureCubeNode::StaticClass() == NodeClass)
		{
			return UInterchangeTextureCubeFactoryNode::StaticClass();
		}

		if (UInterchangeTextureCubeArrayNode::StaticClass() == NodeClass)
		{
			return UInterchangeTextureCubeArrayFactoryNode::StaticClass();
		}

		if (UInterchangeTexture2DArrayNode::StaticClass() == NodeClass)
		{
			return UInterchangeTexture2DArrayFactoryNode::StaticClass();
		}

		if (UInterchangeTextureLightProfileNode::StaticClass() == NodeClass)
		{
			return UInterchangeTextureLightProfileFactoryNode::StaticClass();
		}

		if (UInterchangeVolumeTextureNode::StaticClass() == NodeClass)
		{
			return UInterchangeVolumeTextureFactoryNode::StaticClass();
		}

		return nullptr;
	}

#if WITH_EDITOR
	void AdjustTextureForNormalMap(UTexture* Texture, bool bFlipNormalMapGreenChannel)
	{
		if (Texture)
		{
			Texture->PreEditChange(nullptr);
			if (UE::NormalMapIdentification::HandleAssetPostImport(Texture))
			{
				UE_LOG(LogInterchangePipeline, Display, TEXT("Auto-detected normal map"));

				if (bFlipNormalMapGreenChannel)
				{
					Texture->bFlipGreenChannel = true;
				}
			}
			Texture->PostEditChange();
		}
	}
#endif

	TextureAddress ConvertWrap(const EInterchangeTextureWrapMode WrapMode)
	{
		switch (WrapMode)
		{
		case EInterchangeTextureWrapMode::Wrap:
			return TA_Wrap;
		case EInterchangeTextureWrapMode::Clamp:
			return TA_Clamp;
		case EInterchangeTextureWrapMode::Mirror:
			return TA_Mirror;

		default:
			ensureMsgf(false, TEXT("Unkown Interchange Texture Wrap Mode"));
			return TA_Wrap;
		}
	}
}

void UInterchangeGenericTexturePipeline::AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset)
{
	Super::AdjustSettingsForContext(ImportType, ReimportAsset);

	TArray<FString> HideCategories;
	bool bIsObjectATexture = !ReimportAsset ? false : ReimportAsset.IsA(UTexture::StaticClass());
	if( (!bIsObjectATexture && ImportType == EInterchangePipelineContext::AssetReimport)
		|| ImportType == EInterchangePipelineContext::AssetCustomLODImport
		|| ImportType == EInterchangePipelineContext::AssetCustomLODReimport
		|| ImportType == EInterchangePipelineContext::AssetAlternateSkinningImport
		|| ImportType == EInterchangePipelineContext::AssetAlternateSkinningReimport)
	{
		bImportTextures = false;
		HideCategories.Add(TEXT("Textures"));
	}
	if (UInterchangePipelineBase* OuterMostPipeline = GetMostPipelineOuter())
	{
		for (const FString& HideCategoryName : HideCategories)
		{
			HidePropertiesOfCategory(OuterMostPipeline, this, HideCategoryName);
		}
	}
}

void UInterchangeGenericTexturePipeline::ExecutePreImportPipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas)
{
	if (!InBaseNodeContainer)
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericTexturePipeline: Cannot execute pre-import pipeline because InBaseNodeContrainer is null"));
		return;
	}

	BaseNodeContainer = InBaseNodeContainer;
	SourceDatas.Empty(InSourceDatas.Num());
	for (const UInterchangeSourceData* SourceData : InSourceDatas)
	{
		SourceDatas.Add(SourceData);
	}
	
	//Find all translated node we need for this pipeline
	BaseNodeContainer->IterateNodes([this](const FString& NodeUid, UInterchangeBaseNode* Node)
	{
		switch(Node->GetNodeContainerType())
		{
			case EInterchangeNodeContainerType::TranslatedAsset:
			{
				if (UInterchangeTextureNode* TextureNode = Cast<UInterchangeTextureNode>(Node))
				{
					TextureNodes.Add(TextureNode);
				}
			}
			break;
		}
	});

	if (bImportTextures)
	{
		for (const UInterchangeTextureNode* TextureNode : TextureNodes)
		{
			HandleCreationOfTextureFactoryNode(TextureNode);
		}
	}
}

void UInterchangeGenericTexturePipeline::ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport)
{
	//We do not use the provided base container since ExecutePreImportPipeline cache it
	//We just make sure the same one is pass in parameter
	if (!InBaseNodeContainer || !ensure(BaseNodeContainer == InBaseNodeContainer) || !CreatedAsset)
	{
		return;
	}

	const UInterchangeFactoryBaseNode* Node = BaseNodeContainer->GetFactoryNode(NodeKey);
	if (!Node)
	{
		return;
	}

	PostImportTextureAssetImport(CreatedAsset, bIsAReimport);
}

UInterchangeTextureFactoryNode* UInterchangeGenericTexturePipeline::HandleCreationOfTextureFactoryNode(const UInterchangeTextureNode* TextureNode)
{
	UClass* FactoryClass = UE::Interchange::Private::GetDefaultFactoryClassFromTextureNodeClass(TextureNode->GetClass());

	TOptional<FString> SourceFile = TextureNode->GetPayLoadKey();
#if WITH_EDITORONLY_DATA
	if (FactoryClass == UInterchangeTexture2DFactoryNode::StaticClass())
	{ 
		if (SourceFile)
		{
			const FString Extension = FPaths::GetExtension(SourceFile.GetValue()).ToLower();
			if (FileExtensionsToImportAsLongLatCubemap.Contains(Extension))
			{
				FactoryClass = UInterchangeTextureCubeFactoryNode::StaticClass();
			}
		}
	}
#endif

	UInterchangeTextureFactoryNode* InterchangeTextureFactoryNode =  CreateTextureFactoryNode(TextureNode, FactoryClass);

	if (FactoryClass == UInterchangeTexture2DFactoryNode::StaticClass() && InterchangeTextureFactoryNode)
	{
		// Forward the UDIM from the translator to the factory node
		TMap<int32, FString> SourceBlocks;
		UInterchangeTexture2DFactoryNode* Texture2DFactoryNode = static_cast<UInterchangeTexture2DFactoryNode*>(InterchangeTextureFactoryNode);
		if (const UInterchangeTexture2DNode* Texture2DNode = Cast<UInterchangeTexture2DNode>(TextureNode))
		{
			SourceBlocks = Texture2DNode->GetSourceBlocks();

			EInterchangeTextureWrapMode WrapU;
			if (Texture2DNode->GetCustomWrapU(WrapU))
			{
				Texture2DFactoryNode->SetCustomAddressX(UE::Interchange::Private::ConvertWrap(WrapU));
			}

			EInterchangeTextureWrapMode WrapV;
			if (Texture2DNode->GetCustomWrapV(WrapV))
			{
				Texture2DFactoryNode->SetCustomAddressY(UE::Interchange::Private::ConvertWrap(WrapV));
			}
		}

#if WITH_EDITOR
		if (SourceBlocks.IsEmpty() && bImportUDIMs && SourceFile)
		{
			FString PrettyAssetName;
			SourceBlocks = UE::TextureUtilitiesCommon::GetUDIMBlocksFromSourceFile(SourceFile.GetValue(), UE::TextureUtilitiesCommon::DefaultUdimRegexPattern, &PrettyAssetName);
			if (!PrettyAssetName.IsEmpty())
			{
				InterchangeTextureFactoryNode->SetAssetName(PrettyAssetName);
			}
		}
#endif

		if (!SourceBlocks.IsEmpty())
		{
			Texture2DFactoryNode->SetSourceBlocks(MoveTemp(SourceBlocks));
		}
	}

	return InterchangeTextureFactoryNode;
}

UInterchangeTextureFactoryNode* UInterchangeGenericTexturePipeline::CreateTextureFactoryNode(const UInterchangeTextureNode* TextureNode, const TSubclassOf<UInterchangeTextureFactoryNode>& FactorySubclass)
{
	FString DisplayLabel = TextureNode->GetDisplayLabel();
	FString NodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(TextureNode->GetUniqueID());
	UInterchangeTextureFactoryNode* TextureFactoryNode = nullptr;
	if (BaseNodeContainer->IsNodeUidValid(NodeUid))
	{
		TextureFactoryNode = Cast<UInterchangeTextureFactoryNode>(BaseNodeContainer->GetFactoryNode(NodeUid));
		if (!ensure(TextureFactoryNode))
		{
			//Log an error
			return nullptr;
		}
	}
	else
	{
		UClass* FactoryClass = FactorySubclass.Get();
		if (!ensure(FactoryClass))
		{
			// Log an error
			return nullptr;
		}

		TextureFactoryNode = NewObject<UInterchangeTextureFactoryNode>(BaseNodeContainer, FactoryClass);
		if (!ensure(TextureFactoryNode))
		{
			return nullptr;
		}
		//Creating a Texture
		TextureFactoryNode->InitializeTextureNode(NodeUid, DisplayLabel, TextureNode->GetDisplayLabel());
		TextureFactoryNode->SetCustomTranslatedTextureNodeUid(TextureNode->GetUniqueID());
		BaseNodeContainer->AddNode(TextureFactoryNode);
		TextureFactoryNodes.Add(TextureFactoryNode);

		TextureFactoryNode->AddTargetNodeUid(TextureNode->GetUniqueID());
		TextureNode->AddTargetNodeUid(TextureFactoryNode->GetUniqueID());

		if (bAllowNonPowerOfTwo)
		{
			TextureFactoryNode->SetCustomAllowNonPowerOfTwo(bAllowNonPowerOfTwo);
		}
	}

	if (bool bSRGB; TextureNode->GetCustomSRGB(bSRGB))
	{
		TextureFactoryNode->SetCustomSRGB(bSRGB);
	}
	if (bool bFlipGreenChannel; TextureNode->GetCustombFlipGreenChannel(bFlipGreenChannel))
	{
		TextureFactoryNode->SetCustombFlipGreenChannel(bFlipGreenChannel);
	}

	using FInterchangeTextureFilterMode = std::underlying_type_t<EInterchangeTextureFilterMode>;
	using FTextureFilter = std::underlying_type_t<TextureFilter>;
	using FCommonTextureFilterModes = std::common_type_t<FInterchangeTextureFilterMode, FTextureFilter>;

	static_assert(FCommonTextureFilterModes(EInterchangeTextureFilterMode::Nearest) == FCommonTextureFilterModes(TextureFilter::TF_Nearest), "EInterchangeTextureFilterMode::Nearest differs from TextureFilter::TF_Nearest");
	static_assert(FCommonTextureFilterModes(EInterchangeTextureFilterMode::Bilinear) == FCommonTextureFilterModes(TextureFilter::TF_Bilinear), "EInterchangeTextureFilterMode::Bilinear differs from TextureFilter::TF_Bilinear");
	static_assert(FCommonTextureFilterModes(EInterchangeTextureFilterMode::Trilinear) == FCommonTextureFilterModes(TextureFilter::TF_Trilinear), "EInterchangeTextureFilterMode::Trilinear differs from TextureFilter::TF_Trilinear");
	static_assert(FCommonTextureFilterModes(EInterchangeTextureFilterMode::Default) == FCommonTextureFilterModes(TextureFilter::TF_Default), "EInterchangeTextureFilterMode::Default differs from TextureFilter::TF_Default");

	if (EInterchangeTextureFilterMode TextureFilter; TextureNode->GetCustomFilter(TextureFilter))
	{

		TextureFactoryNode->SetCustomFilter(uint8(TextureFilter));
	}

#if WITH_EDITORONLY_DATA
	if (bPreferCompressedSourceData)
	{
		TextureFactoryNode->SetCustomPreferCompressedSourceData(true);
	}
#endif // WITH_EDITORONLY_DATA

	return TextureFactoryNode;
}

void UInterchangeGenericTexturePipeline::PostImportTextureAssetImport(UObject* CreatedAsset, bool bIsAReimport)
{
#if WITH_EDITOR
	if (!bIsAReimport && bDetectNormalMapTexture)
	{
		// Verify if the texture is a normal map
		if (UTexture* Texture = Cast<UTexture>(CreatedAsset))
		{
			if (!Texture->IsNormalMap())
			{
				// This can create 2 build of the texture (we should revisit this at some point)
				if (FTextureCompilingManager::Get().IsCompilingTexture(Texture))
				{
					TWeakObjectPtr<UTexture> WeakTexturePtr = Texture;
					TSharedRef<FDelegateHandle> HandlePtr = MakeShared<FDelegateHandle>();
					HandlePtr.Get() = FTextureCompilingManager::Get().OnTexturePostCompileEvent().AddLambda([this, WeakTexturePtr, HandlePtr](const TArrayView<UTexture* const>&)
						{
							if (UTexture* TextureToTest = WeakTexturePtr.Get())
							{
								if (FTextureCompilingManager::Get().IsCompilingTexture(TextureToTest))
								{
									return;
								}

								UE::Interchange::Private::AdjustTextureForNormalMap(TextureToTest, bFlipNormalMapGreenChannel);
							}

							FTextureCompilingManager::Get().OnTexturePostCompileEvent().Remove(HandlePtr.Get());
						});
				}
				else
				{
					UE::Interchange::Private::AdjustTextureForNormalMap(Texture, bFlipNormalMapGreenChannel);
				}
			}
		}
	}
#endif //WITH_EDITOR
}



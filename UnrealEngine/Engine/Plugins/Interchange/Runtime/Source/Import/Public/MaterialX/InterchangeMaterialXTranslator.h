// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "InterchangeShaderGraphNode.h"
#include "InterchangeTranslatorBase.h"
#include "InterchangeTextureNode.h"
#include "Texture/InterchangeTexturePayloadInterface.h"

#include "InterchangeMaterialXTranslator.generated.h"

class UInterchangeTextureNode;
class UInterchangeBaseLightNode;
class UInterchangeSceneNode;

UCLASS(BlueprintType)
class INTERCHANGEIMPORT_API UInterchangeMaterialXTranslator : public UInterchangeTranslatorBase, public IInterchangeTexturePayloadInterface
{
	GENERATED_BODY()

public:

	/** Begin UInterchangeTranslatorBase API*/

	virtual EInterchangeTranslatorType GetTranslatorType() const override;

	virtual EInterchangeTranslatorAssetType GetSupportedAssetTypes() const override;

	virtual TArray<FString> GetSupportedFormats() const override;

	/**
	 * Translate the associated source data into a node hold by the specified nodes container.
	 *
	 * @param BaseNodeContainer - The container where to add the translated Interchange nodes.
	 * @return true if the translator can translate the source data, false otherwise.
	 */
	virtual bool Translate( UInterchangeBaseNodeContainer& BaseNodeContainer ) const override;
	/** End UInterchangeTranslatorBase API*/

	virtual TOptional<UE::Interchange::FImportImage> GetTexturePayloadData(const FString& PayloadKey, TOptional<FString>& AlternateTexturePath) const override;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "InterchangeTranslatorBase.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Mesh/InterchangeStaticMeshPayload.h"
#include "Mesh/InterchangeStaticMeshPayloadInterface.h"
#include "Texture/InterchangeTexturePayloadInterface.h"

#include "InterchangeOBJTranslator.generated.h"

class UInterchangeShaderGraphNode;
struct FObjData;


UCLASS(BlueprintType, Experimental)
class INTERCHANGEIMPORT_API UInterchangeOBJTranslator : public UInterchangeTranslatorBase,
	                                                    public IInterchangeStaticMeshPayloadInterface,
	                                                    public IInterchangeTexturePayloadInterface
{
	GENERATED_BODY()

public:

	UInterchangeOBJTranslator();
	virtual ~UInterchangeOBJTranslator();


	virtual TArray<FString> GetSupportedFormats() const override;
	virtual EInterchangeTranslatorAssetType GetSupportedAssetTypes() const override;
	/**
	 * Translate the associated source data into a node hold by the specified nodes container.
	 *
	 * @param BaseNodeContainer - The unreal objects descriptions container where to put the translated source data.
	 * @return true if the translator can translate the source data, false otherwise.
	 */
	virtual bool Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const override;


	/* IInterchangeStaticMeshPayloadInterface Begin */

	/**
	 * Once the translation is done, the import process need a way to retrieve payload data.
	 * This payload will be use by the factories to create the asset.
	 *
	 * @param PayloadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return a PayloadData containing the the data ask with the key.
	 */
	virtual TFuture<TOptional<UE::Interchange::FStaticMeshPayloadData>> GetStaticMeshPayloadData(const FString& PayloadKey) const override;

	/* IInterchangeStaticMeshPayloadInterface End */

	//////////////////////////////////////////////////////////////////////////
	/* IInterchangeTexturePayloadInterface Begin */

	/**
	 * Once the translation is done, the import process need a way to retrieve payload data.
	 * This payload will be use by the factories to create the asset.
	 *
	 * @param InSourceData - The source data containing the data to translate
	 * @param PayloadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return a PayloadData containing the imported data. The TOptional will not be set if there is an error.
	 */
	virtual TOptional<UE::Interchange::FImportImage> GetTexturePayloadData(const UInterchangeSourceData* InSourceData, const FString& PayLoadKey) const override;

	/* IInterchangeTexturePayloadInterface End */

private:
	void AddMaterialNodes(UInterchangeBaseNodeContainer& BaseNodeContainer, UInterchangeShaderGraphNode* ShaderGraphNode, const FString& InputType, const FVector3f& Color, const FString& TexturePath) const;

	TPimplPtr<FObjData> ObjDataPtr;
};

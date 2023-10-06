// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Animation/InterchangeAnimationPayloadInterface.h"
#include "CoreMinimal.h"
#include "InterchangeTranslatorBase.h"
#include "InterchangeDispatcher.h"
#include "Mesh/InterchangeMeshPayload.h"
#include "Mesh/InterchangeMeshPayloadInterface.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Texture/InterchangeTexturePayloadInterface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeFbxTranslator.generated.h"

/* Fbx translator class support import of texture, material, static mesh, skeletal mesh, */

UCLASS(BlueprintType, Experimental)
class INTERCHANGEIMPORT_API UInterchangeFbxTranslator : public UInterchangeTranslatorBase
, public IInterchangeTexturePayloadInterface
, public IInterchangeMeshPayloadInterface
, public IInterchangeAnimationPayloadInterface
{
	GENERATED_BODY()
public:
	UInterchangeFbxTranslator();

	/** Begin UInterchangeTranslatorBase API*/
	virtual EInterchangeTranslatorType GetTranslatorType() const override;
	virtual EInterchangeTranslatorAssetType GetSupportedAssetTypes() const override;
	virtual TArray<FString> GetSupportedFormats() const override;
	virtual bool Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const override;
	virtual void ReleaseSource() override;
	virtual void ImportFinish() override;
	/** End UInterchangeTranslatorBase API*/


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
	virtual TOptional<UE::Interchange::FImportImage> GetTexturePayloadData(const FString& PayloadKey, TOptional<FString>& AlternateTexturePath) const override;

	/* IInterchangeTexturePayloadInterface End */


	//////////////////////////////////////////////////////////////////////////
	/* IInterchangeMeshPayloadInterface Begin */

	/**
	 * Once the translation is done, the import process need a way to retrieve payload data.
	 * This payload will be use by the factories to create the asset.
	 *
	 * @param SourceData - The source data containing the data to translate
	 * @param PayloadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return a PayloadData containing the imported data. The TOptional will not be set if there is an error.
	 */
	virtual TFuture<TOptional<UE::Interchange::FMeshPayloadData>> GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const FTransform& MeshGlobalTransform) const override;

	///* IInterchangeMeshPayloadInterface End */

	//////////////////////////////////////////////////////////////////////////
	/* IInterchangeAnimationPayloadInterface Begin */
	virtual TFuture<TOptional<UE::Interchange::FAnimationPayloadData>> GetAnimationPayloadData(const FInterchangeAnimationPayLoadKey& PayLoadKey, const double BakeFrequency = 0, const double RangeStartSecond = 0, const double RangeStopSecond = 0) const override;
	/* IInterchangeAnimationPayloadInterface End */
private:
	FString CreateLoadFbxFileCommand(const FString& FbxFilePath) const;

	FString CreateFetchMeshPayloadFbxCommand(const FString& FbxPayloadKey, const FTransform& MeshGlobalTransform) const;

	FString CreateFetchPayloadFbxCommand(const FString& FbxPayloadKey) const;

	FString CreateFetchAnimationBakeTransformPayloadFbxCommand(const FString& FbxPayloadKey, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime) const;
	
	//Dispatcher is mutable since it is create during the Translate operation
	//We do not want to allocate the dispatcher and start the InterchangeWorker process
	//in the constructor because Archetype, CDO and registered translators will
	//never translate a source.
	mutable TUniquePtr<UE::Interchange::FInterchangeDispatcher> Dispatcher;
};



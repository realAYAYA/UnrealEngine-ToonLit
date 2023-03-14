// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Animation/InterchangeAnimationPayloadInterface.h"
#include "CoreMinimal.h"
#include "InterchangeTranslatorBase.h"
#include "InterchangeDispatcher.h"
#include "Mesh/InterchangeSkeletalMeshPayload.h"
#include "Mesh/InterchangeSkeletalMeshPayloadInterface.h"
#include "Mesh/InterchangeStaticMeshPayload.h"
#include "Mesh/InterchangeStaticMeshPayloadInterface.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Texture/InterchangeTexturePayloadInterface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeFbxTranslator.generated.h"

/* Fbx translator class support import of texture, material, static mesh, skeletal mesh, */

UCLASS(BlueprintType, Experimental)
class INTERCHANGEIMPORT_API UInterchangeFbxTranslator : public UInterchangeTranslatorBase
, public IInterchangeTexturePayloadInterface
, public IInterchangeStaticMeshPayloadInterface
, public IInterchangeSkeletalMeshPayloadInterface
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
	virtual TOptional<UE::Interchange::FImportImage> GetTexturePayloadData(const UInterchangeSourceData* InSourceData, const FString& PayLoadKey) const override;

	/* IInterchangeTexturePayloadInterface End */


	//////////////////////////////////////////////////////////////////////////
	/* IInterchangeStaticMeshPayloadInterface Begin */

	/**
	 * Once the translation is done, the import process need a way to retrieve payload data.
	 * This payload will be use by the factories to create the asset.
	 *
	 * @param SourceData - The source data containing the data to translate
	 * @param PayloadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return a PayloadData containing the imported data. The TOptional will not be set if there is an error.
	 */
	virtual TFuture<TOptional<UE::Interchange::FStaticMeshPayloadData>> GetStaticMeshPayloadData(const FString& PayLoadKey) const override;

	/* IInterchangeStaticMeshPayloadInterface End */


	//////////////////////////////////////////////////////////////////////////
	/* IInterchangeSkeletalMeshPayloadInterface Begin */

	virtual TFuture<TOptional<UE::Interchange::FSkeletalMeshLodPayloadData>> GetSkeletalMeshLodPayloadData(const FString& PayLoadKey) const override;
	virtual TFuture<TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>> GetSkeletalMeshMorphTargetPayloadData(const FString& PayLoadKey) const override;
	
	/* IInterchangeSkeletalMeshPayloadInterface End */

	//////////////////////////////////////////////////////////////////////////
	/* IInterchangeAnimationPayloadInterface Begin */

	virtual TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>> GetAnimationCurvePayloadData(const FString& PayLoadKey) const override;

	virtual TFuture<TOptional<UE::Interchange::FAnimationStepCurvePayloadData>> GetAnimationStepCurvePayloadData(const FString& PayLoadKey) const override;

	virtual TFuture<TOptional<UE::Interchange::FAnimationBakeTransformPayloadData>> GetAnimationBakeTransformPayloadData(const FString& PayLoadKey, const double BakeFrequency, const double RangeStartSecond, const double RangeStopSecond) const override;

	/* IInterchangeAnimationPayloadInterface End */
private:
	FString CreateLoadFbxFileCommand(const FString& FbxFilePath) const;

	FString CreateFetchPayloadFbxCommand(const FString& FbxPayloadKey) const;

	FString CreateFetchAnimationBakeTransformPayloadFbxCommand(const FString& FbxPayloadKey, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime) const;
	
	//Dispatcher is mutable since it is create during the Translate operation
	//We do not want to allocate the dispatcher and start the InterchangeWorker process
	//in the constructor because Archetype, CDO and registered translators will
	//never translate a source.
	mutable TUniquePtr<UE::Interchange::FInterchangeDispatcher> Dispatcher;
};



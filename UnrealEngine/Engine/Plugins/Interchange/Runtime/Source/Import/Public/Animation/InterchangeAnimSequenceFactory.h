// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeCommonAnimationPayload.h"
#include "InterchangeFactoryBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeAnimSequenceFactory.generated.h"

class UAnimSequence;
class UInterchangeAnimSequenceFactoryNode;
class UInterchangeSceneNode;

UCLASS(BlueprintType)
class INTERCHANGEIMPORT_API UInterchangeAnimSequenceFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()
public:

	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;
	virtual EInterchangeFactoryAssetType GetFactoryAssetType() override { return EInterchangeFactoryAssetType::Animations; }
	
	virtual FImportAssetResult BeginImportAsset_GameThread(const FImportAssetObjectParams& Arguments) override;
	
	virtual FImportAssetResult ImportAsset_Async(const FImportAssetObjectParams& Arguments) override;
	
	virtual FImportAssetResult EndImportAsset_GameThread(const FImportAssetObjectParams& Arguments) override;

	virtual void SetupObject_GameThread(const FSetupObjectParams& Arguments) override;

	virtual bool GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const override;
	virtual bool SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const override;
	
	struct FBoneTrackData
	{
		TMap<const UInterchangeSceneNode*, UE::Interchange::FAnimationPayloadData> PreProcessedAnimationPayloads;
		double MergedRangeStart = 0.0;
		double MergedRangeEnd = 0.0;
	};

	struct FMorphTargetData
	{
		TMap<FString, UE::Interchange::FAnimationPayloadData> CurvesPayloads;
		TMap<FString, FString> CurveNodeNamePerPayloadKey;
	};

private:
	bool IsBoneTrackAnimationValid(const UInterchangeAnimSequenceFactoryNode* AnimSequenceFactoryNode, const FImportAssetObjectParams& Arguments);

	//The imported AnimSequence
	TObjectPtr<UAnimSequence> AnimSequence = nullptr;

	//Bone track animations payload data
	FBoneTrackData BoneTrackData;

	//Morph target curves payload data
	FMorphTargetData MorphTargetData;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////
};



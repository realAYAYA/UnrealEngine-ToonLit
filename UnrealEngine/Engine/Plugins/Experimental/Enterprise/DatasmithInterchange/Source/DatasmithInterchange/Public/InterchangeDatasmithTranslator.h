// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Animation/InterchangeAnimationPayloadInterface.h"
#include "InterchangeTranslatorBase.h"
#include "Mesh/InterchangeStaticMeshPayloadInterface.h"
#include "Texture/InterchangeTexturePayloadData.h"
#include "Texture/InterchangeTexturePayloadInterface.h"
#include "Mesh/InterchangeStaticMeshPayloadInterface.h"
#include "Scene/InterchangeVariantSetPayloadInterface.h"

#include "Async/Async.h"
#include "ExternalSource.h"
#include "UObject/GCObjectScopeGuard.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeDatasmithTranslator.generated.h"

class IDatasmithActorElement;
class IDatasmithBaseAnimationElement;
class IDatasmithCameraActorElement;
class IDatasmithLightActorElement;
class IDatasmithScene;
class IDatasmithTransformAnimationElement;
class UInterchangeCameraNode;
class UInterchangeBaseLightNode;
class UInterchangeSceneNode;

namespace UE::Interchange
{
	struct FImportImage;
	struct FStaticMeshPayloadData;
	struct FAnimationTransformPayloadData;
	struct FAnimationBakeTransformPayloadData;
}

namespace UE::DatasmithImporter
{
	class FExternalSource;
}

namespace UE::DatasmithInterchange::AnimUtils
{
	typedef TPair<float, TSharedPtr<IDatasmithBaseAnimationElement>> FAnimationPayloadDesc;
	extern bool GetAnimationPayloadData(const IDatasmithBaseAnimationElement& AnimationElement, float FrameRate, UE::Interchange::FAnimationCurvePayloadData& PayLoadData);
	extern bool GetAnimationPayloadData(const IDatasmithBaseAnimationElement& AnimationElement, float FrameRate, UE::Interchange::FAnimationStepCurvePayloadData& PayLoadData);
}

UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGE_API UInterchangeDatasmithTranslator : public UInterchangeTranslatorBase
	, public IInterchangeTexturePayloadInterface
	, public IInterchangeStaticMeshPayloadInterface
	, public IInterchangeAnimationPayloadInterface
	, public IInterchangeVariantSetPayloadInterface
{
	GENERATED_BODY()

public:

	virtual bool CanImportSourceData(const UInterchangeSourceData* InSourceData) const override;

	virtual bool Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const override;

	virtual TArray<FString> GetSupportedFormats() const override { return TArray<FString>(); }
	virtual EInterchangeTranslatorAssetType GetSupportedAssetTypes() const override { return EInterchangeTranslatorAssetType::Materials | EInterchangeTranslatorAssetType::Meshes;}

	virtual void ReleaseSource() override
	{
		return;
	}

	virtual void ImportFinish() override;

	/* IInterchangeTexturePayloadInterface Begin */
	virtual TOptional<UE::Interchange::FImportImage> GetTexturePayloadData(const UInterchangeSourceData* InPayloadSourceData, const FString& PayloadKey) const override;
	/* IInterchangeTexturePayloadInterface End */

	/* IInterchangeStaticMeshPayloadInterface Begin */
	virtual TFuture<TOptional<UE::Interchange::FStaticMeshPayloadData>> GetStaticMeshPayloadData(const FString& PayloadKey) const override;
	/* IInterchangeStaticMeshPayloadInterface End */

	/* IInterchangeAnimationPayloadInterface Begin */
	virtual TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>> GetAnimationCurvePayloadData(const FString& PayLoadKey) const override;
	virtual TFuture<TOptional<UE::Interchange::FAnimationStepCurvePayloadData>> GetAnimationStepCurvePayloadData(const FString& PayLoadKey) const override;
	virtual TFuture<TOptional<UE::Interchange::FAnimationBakeTransformPayloadData>> GetAnimationBakeTransformPayloadData(const FString& PayLoadKey, const double BakeFrequency, const double RangeStartSecond, const double RangeStopSecond) const override;
	/* IInterchangeAnimationPayloadInterface End */

	/* IInterchangeVariantSetPayloadInterface Begin */
	virtual TFuture<TOptional<UE::Interchange::FVariantSetPayloadData>> GetVariantSetPayloadData(const FString& PayloadKey) const override;
	/* IInterchangeVariantSetPayloadInterface End */

	/** Returns a unique file path to */ 
	static FString BuildConfigFilePath(const FString& FilePath);

private:

	void HandleDatasmithActor(UInterchangeBaseNodeContainer& BaseNodeContainer, const TSharedRef<IDatasmithActorElement>& ActorElement, const UInterchangeSceneNode* ParentNode) const;

	UInterchangeCameraNode* AddCameraNode(UInterchangeBaseNodeContainer& BaseNodeContainer, const TSharedRef<IDatasmithCameraActorElement>& CameraActor) const;

	UInterchangeBaseLightNode* AddLightNode(UInterchangeBaseNodeContainer& BaseNodeContainer, const TSharedRef<IDatasmithLightActorElement>& LightActor) const;

	TSharedPtr<UE::DatasmithImporter::FExternalSource> LoadedExternalSource;

	template<typename T>
	TFuture<TOptional<T>> GetAnimationPayloadDataAsCurve(const FString& PayLoadKey) const
	{
		using namespace UE::DatasmithInterchange::AnimUtils;

		TPromise<TOptional<T>> EmptyPromise;
		EmptyPromise.SetValue(TOptional<T>());

		if (!LoadedExternalSource || !LoadedExternalSource->GetDatasmithScene())
		{
			return EmptyPromise.GetFuture();
		}

		TSharedPtr<IDatasmithBaseAnimationElement> AnimationElement;
		float FrameRate = 0.f;
		if (FAnimationPayloadDesc* PayloadDescPtr = AnimationPayLoadMapping.Find(PayLoadKey))
		{
			AnimationElement = PayloadDescPtr->Value;
			if (!ensure(AnimationElement))
			{
				// #ueent_logwarning:
				return EmptyPromise.GetFuture();
			}

			FrameRate = PayloadDescPtr->Key;
		}

		return Async(EAsyncExecution::TaskGraph, [this, AnimationElement = MoveTemp(AnimationElement), FrameRate]
			{
				T TransformPayloadData;
				TOptional<T> Result;

				if (GetAnimationPayloadData(*AnimationElement, FrameRate, TransformPayloadData))
				{
					Result.Emplace(MoveTemp(TransformPayloadData));
				}

				return Result;
			}
		);
	}

	mutable uint64 StartTime = 0;
	mutable FString FileName;

	mutable TMap<FString, UE::DatasmithInterchange::AnimUtils::FAnimationPayloadDesc> AnimationPayLoadMapping;
};
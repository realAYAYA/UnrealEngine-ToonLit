// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Animation/InterchangeAnimationPayloadInterface.h"
#include "InterchangeTranslatorBase.h"
#include "Mesh/InterchangeMeshPayloadInterface.h"
#include "Texture/InterchangeTexturePayloadData.h"
#include "Texture/InterchangeTexturePayloadInterface.h"
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
class UInterchangePhysicalCameraNode;
class UInterchangeBaseLightNode;
class UInterchangeSceneNode;

namespace UE::Interchange
{
	struct FImportImage;
	struct FMeshPayloadData;
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
	extern bool GetAnimationPayloadData(const IDatasmithBaseAnimationElement& AnimationElement, float FrameRate, TArray<FRichCurve>& Curves);
	extern bool GetAnimationPayloadData(const IDatasmithBaseAnimationElement& AnimationElement, float FrameRate, TArray<FInterchangeStepCurve>& StepCurves);
}

UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGE_API UInterchangeDatasmithTranslator : public UInterchangeTranslatorBase
	, public IInterchangeTexturePayloadInterface
	, public IInterchangeMeshPayloadInterface
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
	virtual TOptional<UE::Interchange::FImportImage> GetTexturePayloadData(const FString& PayloadKey, TOptional<FString>& AlternateTexturePath) const override;
	/* IInterchangeTexturePayloadInterface End */

	/* IInterchangeStaticMeshPayloadInterface Begin */
	virtual TFuture<TOptional<UE::Interchange::FMeshPayloadData>> GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const FTransform& MeshGlobalTransform) const override;
	/* IInterchangeStaticMeshPayloadInterface End */

	/* IInterchangeAnimationPayloadInterface Begin */
	virtual TFuture<TOptional<UE::Interchange::FAnimationPayloadData>> GetAnimationPayloadData(const FInterchangeAnimationPayLoadKey& PayLoadKey, const double BakeFrequency = 0, const double RangeStartSecond = 0, const double RangeStopSecond = 0) const override;
	/* IInterchangeAnimationPayloadInterface End */

	/* IInterchangeVariantSetPayloadInterface Begin */
	virtual TFuture<TOptional<UE::Interchange::FVariantSetPayloadData>> GetVariantSetPayloadData(const FString& PayloadKey) const override;
	/* IInterchangeVariantSetPayloadInterface End */

	/** Returns a unique file path to */ 
	static FString BuildConfigFilePath(const FString& FilePath);

private:

	void HandleDatasmithActor(UInterchangeBaseNodeContainer& BaseNodeContainer, const TSharedRef<IDatasmithActorElement>& ActorElement, const UInterchangeSceneNode* ParentNode) const;

	UInterchangePhysicalCameraNode* AddCameraNode(UInterchangeBaseNodeContainer& BaseNodeContainer, const TSharedRef<IDatasmithCameraActorElement>& CameraActor) const;

	UInterchangeBaseLightNode* AddLightNode(UInterchangeBaseNodeContainer& BaseNodeContainer, const TSharedRef<IDatasmithLightActorElement>& LightActor) const;

	TSharedPtr<UE::DatasmithImporter::FExternalSource> LoadedExternalSource;

	mutable uint64 StartTime = 0;
	mutable FString FileName;

	mutable TMap<FString, UE::DatasmithInterchange::AnimUtils::FAnimationPayloadDesc> AnimationPayLoadMapping;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkFrameTranslator.h"
#include "LiveLinkAnimationRoleToTransform.generated.h"


/**
 * Basic object to translate data from one role to another
 */
UCLASS(meta=(DisplayName="Animation To Transform"))
class LIVELINK_API ULiveLinkAnimationRoleToTransform : public ULiveLinkFrameTranslator
{
	GENERATED_BODY()

public:
	class FLiveLinkAnimationRoleToTransformWorker : public ILiveLinkFrameTranslatorWorker
	{
	public:
		FName BoneName;
		virtual TSubclassOf<ULiveLinkRole> GetFromRole() const override;
		virtual TSubclassOf<ULiveLinkRole> GetToRole() const override;
		virtual bool Translate(const FLiveLinkStaticDataStruct& InStaticData, const FLiveLinkFrameDataStruct& InFrameData, FLiveLinkSubjectFrameData& OutTranslatedFrame) const override;
	};

protected:
	UPROPERTY(EditAnywhere, Category="LiveLink")
	FName BoneName;

public:
	virtual TSubclassOf<ULiveLinkRole> GetFromRole() const override;
	virtual TSubclassOf<ULiveLinkRole> GetToRole() const override;
	virtual ULiveLinkFrameTranslator::FWorkerSharedPtr FetchWorker() override;

public:
	//~ UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
	//~ End UObject interface

private:
	TSharedPtr<FLiveLinkAnimationRoleToTransformWorker, ESPMode::ThreadSafe> Instance;
};

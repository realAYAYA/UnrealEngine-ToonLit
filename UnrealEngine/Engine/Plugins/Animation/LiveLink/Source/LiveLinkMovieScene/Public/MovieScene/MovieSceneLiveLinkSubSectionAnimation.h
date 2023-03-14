// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneLiveLinkSubSection.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Containers/Array.h"
#include "LiveLinkRole.h"
#include "LiveLinkTypes.h"
#include "MovieScene/MovieSceneLiveLinkTransformHandler.h"

#include "MovieSceneLiveLinkSubSectionAnimation.generated.h"


/**
 * A LiveLinkSubSection managing array of transforms contained in the Animation Frame Data structure
 */
UCLASS()
class LIVELINKMOVIESCENE_API UMovieSceneLiveLinkSubSectionAnimation : public UMovieSceneLiveLinkSubSection
{
	GENERATED_BODY()

public:

	UMovieSceneLiveLinkSubSectionAnimation(const FObjectInitializer& ObjectInitializer);

	virtual void Initialize(TSubclassOf<ULiveLinkRole> InSubjectRole, const TSharedPtr<FLiveLinkStaticDataStruct>& InStaticData);
	virtual int32 CreateChannelProxy(int32 InChannelIndex, TArray<bool>& OutChannelMask, FMovieSceneChannelProxyData& OutChannelData);
	virtual void RecordFrame(FFrameNumber InFrameNumber, const FLiveLinkFrameDataStruct& InFrameData) override;
	virtual void FinalizeSection(bool bReduceKeys, const FKeyDataOptimizationParams& OptimizationParams) override;

public:

	virtual bool IsRoleSupported(const TSubclassOf<ULiveLinkRole>& RoleToSupport) const override;

private:
	void CreatePropertiesChannel();

protected:
	
	/** Helper struct to manage filling channels from transforms */
	TSharedPtr<FMovieSceneLiveLinkTransformHandler> TransformHandler;
};

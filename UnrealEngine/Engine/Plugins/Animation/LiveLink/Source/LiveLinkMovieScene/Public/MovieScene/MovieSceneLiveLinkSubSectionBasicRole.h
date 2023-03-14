// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneLiveLinkSubSection.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Containers/Array.h"
#include "LiveLinkRole.h"
#include "LiveLinkTypes.h"
#include "MovieScene/MovieSceneLiveLinkPropertyHandler.h"

#include "MovieSceneLiveLinkSubSectionBasicRole.generated.h"


/**
 * A LiveLinkSubSection managing special properties of the BasicRole
 */
UCLASS()
class LIVELINKMOVIESCENE_API UMovieSceneLiveLinkSubSectionBasicRole : public UMovieSceneLiveLinkSubSection
{
	GENERATED_BODY()

public:

	UMovieSceneLiveLinkSubSectionBasicRole(const FObjectInitializer& ObjectInitializer);

	virtual void Initialize(TSubclassOf<ULiveLinkRole> InSubjectRole, const TSharedPtr<FLiveLinkStaticDataStruct>& InStaticData) override;
	virtual int32 CreateChannelProxy(int32 InChannelIndex, TArray<bool>& OutChannelMask, FMovieSceneChannelProxyData& OutChannelData) override;
	virtual void RecordFrame(FFrameNumber InFrameNumber, const FLiveLinkFrameDataStruct& InFrameData) override;
	virtual void FinalizeSection(bool bReduceKeys, const FKeyDataOptimizationParams& OptimizationParams) override;

public:

	virtual bool IsRoleSupported(const TSubclassOf<ULiveLinkRole>& RoleToSupport) const override;

private:
	void CreatePropertiesChannel();

protected:

	/** Helper struct to manage filling channels from property array */
	TSharedPtr<IMovieSceneLiveLinkPropertyHandler> PropertyHandler;
};

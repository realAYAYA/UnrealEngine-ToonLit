// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneLiveLinkSubSection.h"


#include "MovieSceneLiveLinkSubSectionProperties.generated.h"


/**
 * A LiveLinkSubSection managing properties marked as Interp in the data struct associated with the subject role
 */
UCLASS()
class LIVELINKMOVIESCENE_API UMovieSceneLiveLinkSubSectionProperties : public UMovieSceneLiveLinkSubSection
{
	GENERATED_BODY()

public:

	UMovieSceneLiveLinkSubSectionProperties(const FObjectInitializer& ObjectInitializer);

	virtual void Initialize(TSubclassOf<ULiveLinkRole> InSubjectRole, const TSharedPtr<FLiveLinkStaticDataStruct>& InStaticData) override;
	virtual int32 CreateChannelProxy(int32 InChannelIndex, TArray<bool>& OutChannelMask, FMovieSceneChannelProxyData& OutChannelData) override;
	virtual void RecordFrame(FFrameNumber InFrameNumber, const FLiveLinkFrameDataStruct& InFrameData) override;
	virtual void FinalizeSection(bool bReduceKeys, const FKeyDataOptimizationParams& OptimizationParams) override;

public:

	virtual bool IsRoleSupported(const TSubclassOf<ULiveLinkRole>& RoleToSupport) const override;

protected:

private:
	void CreatePropertyList(UScriptStruct* InScriptStruct, bool bCheckInterpFlag, const FString& InOwner);
	void CreatePropertiesChannel(UScriptStruct* InScriptStruct);
	bool IsPropertyTypeSupported(const FProperty* InProperty) const;
	int32 CreateChannelProxyInternal(FProperty* InPropertyPtr, FLiveLinkPropertyData& OutPropertyData, int32 InPropertyIndex, int32 GlobalIndex, TArray<bool>& OutChannelMask, FMovieSceneChannelProxyData& OutChannelData, const FText& InPropertyName);

protected:

	/** Helper struct to manage filling channels for each properties */
	TArray<TSharedPtr<IMovieSceneLiveLinkPropertyHandler>> PropertyHandlers;
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "LiveLinkRole.h"
#include "MovieScene/MovieSceneLiveLinkPropertyHandler.h"
#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "LiveLinkPresetTypes.h"
#include "LiveLinkRole.h"

#include "MovieSceneLiveLinkSection.generated.h"

class UMovieScenePropertyTrack;
struct FKeyDataOptimizationParams;
struct FLiveLinkSubjectPreset;
class UMovieSceneLiveLinkSubSection;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/**
* A movie scene section for all live link recorded data
*/
UCLASS()
class LIVELINKMOVIESCENE_API UMovieSceneLiveLinkSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:

	UMovieSceneLiveLinkSection(const FObjectInitializer& ObjectInitializer);

	void Initialize(const FLiveLinkSubjectPreset& SubjectPreset, const TSharedPtr<FLiveLinkStaticDataStruct>& InStaticData);
	void SetSubjectName(const FName& InSubjectName) { SubjectPreset.Key.SubjectName = InSubjectName; }

public:
	
	/**
	 * Called when first created. Creates the channels required to represent this section
	 */
	int32 CreateChannelProxy();

	/**
	 * Called during loading. 
	 */
	void UpdateChannelProxy();

	void SetMask(const TArray<bool>& InChannelMask);

	void RecordFrame(FFrameNumber InFrameNumber, const FLiveLinkFrameDataStruct& InFrameData);
	void FinalizeSection(bool bReduceKeys, const FKeyDataOptimizationParams& OptimizationParams);

public:
	static TArray<TSubclassOf<UMovieSceneLiveLinkSection>> GetMovieSectionForRole(const TSubclassOf<ULiveLinkRole>& RoleToSupport);

	virtual FMovieSceneEvalTemplatePtr CreateSectionTemplate(const UMovieScenePropertyTrack& InTrack) const;

public:
	//~ Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditImport() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

protected:

	virtual int32 GetChannelCount() const;

private:
	void ConvertPreRoleData();

public:

	UPROPERTY()
	FLiveLinkSubjectPreset SubjectPreset;

	// Channels that we may not send to live link or they are sent but not priority (MattH to do).
	UPROPERTY()
	TArray<bool> ChannelMask;

	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneLiveLinkSubSection>> SubSections;

	TSharedPtr<FLiveLinkStaticDataStruct> StaticData;

	UPROPERTY()
	FName SubjectName_DEPRECATED;
	UPROPERTY()
	FLiveLinkFrameData TemplateToPush_DEPRECATED;
	UPROPERTY()
	FLiveLinkRefSkeleton RefSkeleton_DEPRECATED;
	UPROPERTY()
	TArray<FName> CurveNames_DEPRECATED;
	UPROPERTY()
	TArray <FMovieSceneFloatChannel> PropertyFloatChannels_DEPRECATED;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

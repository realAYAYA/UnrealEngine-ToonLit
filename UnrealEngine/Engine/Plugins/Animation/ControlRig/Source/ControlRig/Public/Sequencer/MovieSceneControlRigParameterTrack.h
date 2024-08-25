// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sections/MovieSceneParameterSection.h"
#include "MovieSceneNameableTrack.h"
#include "ControlRig.h"
#include "Compilation/IMovieSceneTrackTemplateProducer.h"
#include "INodeAndChannelMappings.h"
#include "MovieSceneControlRigParameterSection.h"
#include "EulerTransform.h"
#include "MovieSceneControlRigParameterTrack.generated.h"

struct FEndLoadPackageContext;
struct FRigControlElement;

USTRUCT()
struct CONTROLRIG_API FControlRotationOrder
{
	GENERATED_BODY()

	FControlRotationOrder();

	/* Rotation Order*/
	UPROPERTY()
	EEulerRotationOrder RotationOrder;

	/** Override the default control setting*/
	UPROPERTY()
	bool bOverrideSetting;
};

/**
 * Handles animation of skeletal mesh actors using animation ControlRigs
 */

UCLASS(MinimalAPI)
class UMovieSceneControlRigParameterTrack
	: public UMovieSceneNameableTrack
	, public IMovieSceneTrackTemplateProducer
	, public INodeAndChannelMappings
{
	GENERATED_UCLASS_BODY()

public:
	// UMovieSceneTrack interface
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual void RemoveSectionAt(int32 SectionIndex) override;
	virtual bool IsEmpty() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual FName GetTrackName() const override { return TrackName; }
	// UObject
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif
	virtual void PostEditImport() override;
#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

	//INodeAndMappingsInterface
	virtual TArray<FRigControlFBXNodeAndChannels>* GetNodeAndChannelMappings(UMovieSceneSection* InSection)  override;
	virtual void GetSelectedNodes(TArray<FName>& OutSelectedNodes) override;
#if WITH_EDITOR
	virtual bool GetFbxCurveDataFromChannelMetadata(const FMovieSceneChannelMetaData& MetaData, FControlRigFbxCurveData& OutCurveData) override;
#endif // WITH_EDITOR

	//UControlRig Delegates
	void HandleOnPostConstructed_GameThread();
	void HandleOnPostConstructed(UControlRig* Subject, const FName& InEventName);

#if WITH_EDITOR
	void HandlePackageDone(const FEndLoadPackageContext& Context);
	// control Rigs are ready only after its package is fully end-loaded
	void HandleControlRigPackageDone(URigVMHost* InControlRig);
#endif

public:
	/** Add a section at that start time*/
	CONTROLRIG_API UMovieSceneSection* CreateControlRigSection(FFrameNumber StartTime, UControlRig* InControlRig, bool bInOwnsControlRig);

	CONTROLRIG_API void ReplaceControlRig(UControlRig* NewControlRig, bool RecreateChannels);

	CONTROLRIG_API void RenameParameterName(const FName& OldParameterName, const FName& NewParameterName);

public:
	CONTROLRIG_API UControlRig* GetControlRig() const { return ControlRig; }

	/**
	* Find all sections at the current time.
	*
	*@param Time  The Time relative to the owning movie scene where the section should be
	*@Return All sections at that time
	*/
	CONTROLRIG_API TArray<UMovieSceneSection*, TInlineAllocator<4>> FindAllSections(FFrameNumber Time);

	/**
	 * Finds a section at the current time.
	 *
	 * @param Time The time relative to the owning movie scene where the section should be
	 * @return The found section.
	 */
	CONTROLRIG_API class UMovieSceneSection* FindSection(FFrameNumber Time);

	/**
	 * Finds a section at the current time or extends an existing one
	 *
	 * @param Time The time relative to the owning movie scene where the section should be
	 * @param OutWeight The weight of the section if found
	 * @return The found section.
	 */
	CONTROLRIG_API class UMovieSceneSection* FindOrExtendSection(FFrameNumber Time, float& OutWeight);

	/**
	 * Finds a section at the current time.
	 *
	 * @param Time The time relative to the owning movie scene where the section should be
	 * @param bSectionAdded Whether a section was added or not
	 * @return The found section, or the new section.
	 */
	CONTROLRIG_API class UMovieSceneSection* FindOrAddSection(FFrameNumber Time, bool& bSectionAdded);

	/**
	 * Set the section we want to key and recieve globally changed values.
	 *
	 * @param Section The section that changes.
	 */

	virtual void SetSectionToKey(UMovieSceneSection* Section) override;

	/**
	 * Finds a section we want to key and recieve globally changed values.
	 * @return The Section that changes.
	 */
	virtual UMovieSceneSection* GetSectionToKey() const override;

	CONTROLRIG_API void SetTrackName(FName InName) { TrackName = InName; }

	UMovieSceneControlRigParameterSection::FSpaceChannelAddedEvent& SpaceChannelAdded() { return OnSpaceChannelAdded; }
	IMovieSceneConstrainedSection::FConstraintChannelAddedEvent& ConstraintChannelAdded() { return OnConstraintChannelAdded; }

public:
	CONTROLRIG_API TArray<FName> GetControlsWithDifferentRotationOrders() const;
	CONTROLRIG_API void ResetControlsToSettingsRotationOrder(const TArray<FName>& Names,
		EMovieSceneKeyInterpolation Interpolation = EMovieSceneKeyInterpolation::SmartAuto);
	// if order is not set then it uses the default FRotator conversions
	CONTROLRIG_API void ChangeControlRotationOrder(const FName& InControlName, const TOptional<EEulerRotationOrder>& NewOrder,
		EMovieSceneKeyInterpolation Interpolation = EMovieSceneKeyInterpolation::SmartAuto);

	CONTROLRIG_API int32 GetPriorityOrder() const;
	CONTROLRIG_API void SetPriorityOrder(int32 InPriorityIndex);
private:
	//get the rotation orderm will not be set if it's default FRotator order. If bCurrent is true, it uses what's set
	//if false it uses the default setting from the current control rig.
	TOptional<EEulerRotationOrder> GetControlRotationOrder(const FRigControlElement* ControlElement, bool bCurrent) const;
private:

	//we register this with sections.
	void HandleOnSpaceAdded(UMovieSceneControlRigParameterSection* Section, const FName& InControlName, FMovieSceneControlRigSpaceChannel* Channel);
	//we'll register this against all space channels
	void HandleOnSpaceNoLongerUsed(FMovieSceneControlRigSpaceChannel* InChannel, const TArray<FRigElementKey>& InSpaces, FName InControlName);
	//then send this event out to the track editor.
	UMovieSceneControlRigParameterSection::FSpaceChannelAddedEvent OnSpaceChannelAdded;

	void HandleOnConstraintAdded(
		IMovieSceneConstrainedSection* InSection,
		FMovieSceneConstraintChannel* InChannel) const;
	IMovieSceneConstrainedSection::FConstraintChannelAddedEvent OnConstraintChannelAdded;
	void ReconstructControlRig();

private:


	/** Control Rig we control*/
	UPROPERTY()
	TObjectPtr<UControlRig> ControlRig;

	/** Section we should Key */
	UPROPERTY()
	TObjectPtr<UMovieSceneSection> SectionToKey;

	/** The sections owned by this track .*/
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneSection>> Sections;

	/** Unique Name*/
	UPROPERTY()
	FName TrackName;

	/** Uses Rotation Order*/
	UPROPERTY()
	TMap<FName,FControlRotationOrder> ControlsRotationOrder;

	UPROPERTY()
	int32 PriorityOrder;

public:
	static CONTROLRIG_API FColor AbsoluteRigTrackColor;
	static CONTROLRIG_API FColor LayeredRigTrackColor;

public:
	UControlRig* GetGameWorldControlRig(UWorld* InWorld);
private:
	/** copy of the controlled control rig that we use in the game world so editor control rig doesn't conflict*/
	UPROPERTY(transient)
	TMap<TWeakObjectPtr<UWorld>,TObjectPtr<UControlRig>> GameWorldControlRigs;
};




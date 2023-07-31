// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Abstract base class of animation sequence that can be played and evaluated to produce a pose.
 *
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimData/AnimDataModelNotifyCollector.h"
#include "Animation/AnimData/IAnimationDataController.h"

#include "AnimSequenceBase.generated.h"

UENUM()
enum ETypeAdvanceAnim
{
	ETAA_Default,
	ETAA_Finished,
	ETAA_Looped
};

struct FAnimationPoseData;
struct FAnimDataModelNotifPayload;
class UAnimDataModel;
enum class EAnimDataModelNotifyType : uint8;

UCLASS(abstract, BlueprintType)
class ENGINE_API UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_UCLASS_BODY()

public:
	/** Animation notifies, sorted by time (earliest notification first). */
	UPROPERTY()
	TArray<struct FAnimNotifyEvent> Notifies;

	/** Length (in seconds) of this AnimSequence if played back with a speed of 1.0. */
	UE_DEPRECATED(5.0, "Public access to SequenceLength is deprecated, use GetPlayLength or UAnimDataController::SetPlayLength instead")
	UPROPERTY(Category=Length, AssetRegistrySearchable, VisibleAnywhere, BlueprintReadOnly)
	float SequenceLength;

	UE_DEPRECATED(5.0, "SetSequenceLength is deprecated use UAnimDataController::SetPlayLength instead")
	virtual void SetSequenceLength(float NewLength);

	/** Number for tweaking playback rate of this animation globally. */
	UPROPERTY(EditAnywhere, Category=Animation)
	float RateScale;
	
	/** 
	 * The default looping behavior of this animation.
	 * Asset players can override this
	 */
	UPROPERTY(EditAnywhere, Category=Animation)
	bool bLoop;

	/**
	 * Raw uncompressed float curve data 
	 */
	UE_DEPRECATED(5.0, "Public access to RawCurveData is deprecated, see UAnimDataModel for source data or use GetCurveData for runtime instead")
	UPROPERTY()
	struct FRawCurveTracks RawCurveData;

#if WITH_EDITORONLY_DATA
	// if you change Notifies array, this will need to be rebuilt
	UPROPERTY()
	TArray<FAnimNotifyTrack> AnimNotifyTracks;
#endif // WITH_EDITORONLY_DATA

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual bool IsPostLoadThreadSafe() const override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif
	//~ End UObject Interface

	/** Returns the total play length of the montage, if played back with a speed of 1.0. */
	virtual float GetPlayLength() const override;

	/** Sort the Notifies array by time, earliest first. */
	void SortNotifies();	

	/** Remove the notifies specified */
	bool RemoveNotifies(const TArray<FName>& NotifiesToRemove);
	
	/** Remove all notifies */
	void RemoveNotifies();

	/** 
	 * Retrieves AnimNotifies given a StartTime and a DeltaTime.
	 * Time will be advanced and support looping if bAllowLooping is true.
	 * Supports playing backwards (DeltaTime<0).
	 * Returns notifies between StartTime (exclusive) and StartTime+DeltaTime (inclusive)
	 */
	UE_DEPRECATED(4.19, "Use the GetAnimNotifies that takes FAnimNotifyEventReferences instead")
	void GetAnimNotifies(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<const FAnimNotifyEvent *>& OutActiveNotifies) const;

	UE_DEPRECATED(5.0, "Use the other GetAnimNotifies that takes FAnimNotifyContext instead")
	void GetAnimNotifies(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<FAnimNotifyEventReference>& OutActiveNotifies) const;

	/**
	* Retrieves AnimNotifies given a StartTime and a DeltaTime.
	* Time will be advanced and support looping if bAllowLooping is true.
	* Supports playing backwards (DeltaTime<0).
	* Returns notifies between StartTime (exclusive) and StartTime+DeltaTime (inclusive)
	*/
	void GetAnimNotifies(const float& StartTime, const float& DeltaTime, FAnimNotifyContext& NotifyContext) const;

	/** 
	 * Retrieves AnimNotifies between two time positions. ]PreviousPosition, CurrentPosition]
	 * Between PreviousPosition (exclusive) and CurrentPosition (inclusive).
	 * Supports playing backwards (CurrentPosition<PreviousPosition).
	 * Only supports contiguous range, does NOT support looping and wrapping over.
	 */
	UE_DEPRECATED(4.19, "Use the GetAnimNotifiesFromDeltaPositions that takes FAnimNotifyEventReferences instead")
	void GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float & CurrentPosition, TArray<const FAnimNotifyEvent *>& OutActiveNotifies) const;

	UE_DEPRECATED(5.0, "Use the other GetAnimNotifiesFromDeltaPositions that takes FAnimNotifyContext instead")
	virtual void GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float & CurrentPosition, TArray<FAnimNotifyEventReference>& OutActiveNotifies) const;
	/**
	* Retrieves AnimNotifies between two time positions. ]PreviousPosition, CurrentPosition]
	* Between PreviousPosition (exclusive) and CurrentPosition (inclusive).
	* Supports playing backwards (CurrentPosition<PreviousPosition).
	* Only supports contiguous range, does NOT support looping and wrapping over.
	*/
	virtual void GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float & CurrentPosition, FAnimNotifyContext& NotifyContext) const;

	/** Evaluate curve data to Instance at the time of CurrentTime **/
	virtual void EvaluateCurveData(FBlendedCurve& OutCurve, float CurrentTime, bool bForceUseRawData = false) const;
	virtual float EvaluateCurveData(SmartName::UID_Type CurveUID, float CurrentTime, bool bForceUseRawData = false) const;

	virtual const FRawCurveTracks& GetCurveData() const;
	virtual bool HasCurveData(SmartName::UID_Type CurveUID, bool bForceUseRawData = false) const;

	/** Return Number of Keys **/
	UE_DEPRECATED(4.19, "Use GetNumberOfSampledKeys instead")
	virtual int32 GetNumberOfFrames() const;

	/** Return the total number of keys sampled for this animation, including the T0 key **/
	virtual int32 GetNumberOfSampledKeys() const;

	/** Return rate at which the animation is sampled **/
	virtual const FFrameRate& GetSamplingFrameRate() const;

#if WITH_EDITOR
	/** Get the frame number for the provided time */
	virtual int32 GetFrameAtTime(const float Time) const;

	/** Get the time at the given frame */
	virtual float GetTimeAtFrame(const int32 Frame) const;
	
	// @todo document
	void InitializeNotifyTrack();

	/** Fix up any notifies that are positioned beyond the end of the sequence */
	void ClampNotifiesAtEndOfSequence();

	/** Calculates what (if any) offset should be applied to the trigger time of a notify given its display time */ 
	virtual EAnimEventTriggerOffsets::Type CalculateOffsetForNotify(float NotifyDisplayTime) const;

	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	
	// Get a pointer to the data for a given Anim Notify
	uint8* FindNotifyPropertyData(int32 NotifyIndex, FArrayProperty*& ArrayProperty);

	// Get a pointer to the data for a given array property item
	uint8* FindArrayProperty(const TCHAR* PropName, FArrayProperty*& ArrayProperty, int32 ArrayIndex);

protected:
	virtual void RefreshParentAssetData() override;
#endif	//WITH_EDITORONLY_DATA
public: 
	// update cache data (notify tracks, sync markers)
	virtual void RefreshCacheData();

	//~ Begin UAnimationAsset Interface
#if WITH_EDITOR
	virtual void RemapTracksToNewSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces) override;
#endif
	virtual void TickAssetPlayer(FAnimTickRecord& Instance, struct FAnimNotifyQueue& NotifyQueue, FAnimAssetTickContext& Context) const override;

	void TickByMarkerAsFollower(FMarkerTickRecord &Instance, FMarkerTickContext &MarkerContext, float& CurrentTime, float& OutPreviousTime, const float MoveDelta, const bool bLooping, const UMirrorDataTable* MirrorTable = nullptr) const;
	void TickByMarkerAsLeader(FMarkerTickRecord& Instance, FMarkerTickContext& MarkerContext, float& CurrentTime, float& OutPreviousTime, const float MoveDelta, const bool bLooping, const UMirrorDataTable* MirrorTable = nullptr) const;
	//~ End UAnimationAsset Interface

	/**
	* Get Bone Transform of the Time given, relative to Parent for all RequiredBones
	* This returns different transform based on additive or not. Or what kind of additive.
	*
	* @param	OutPose				Pose object to fill
	* @param	OutCurve			Curves to fill
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	*/
	UE_DEPRECATED(4.26, "Use other GetAnimationPose signature")
	virtual void GetAnimationPose(struct FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const;
	
	virtual void GetAnimationPose(FAnimationPoseData& OutPoseData, const FAnimExtractContext& ExtractionContext) const
		PURE_VIRTUAL(UAnimSequenceBase::GetAnimationPose, );
	
	virtual void HandleAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, struct FAnimNotifyQueue& NotifyQueue) const;

	virtual bool HasRootMotion() const { return false; }

	virtual void Serialize(FArchive& Ar) override;

	UE_DEPRECATED(5.0, "Use other AdvanceMarkerPhaseAsLeader signature")
	virtual void AdvanceMarkerPhaseAsLeader(bool bLooping, float MoveDelta, const TArray<FName>& ValidMarkerNames, float& CurrentTime, FMarkerPair& PrevMarker, FMarkerPair& NextMarker, TArray<FPassedMarker>& MarkersPassed) const {AdvanceMarkerPhaseAsLeader(bLooping, MoveDelta, ValidMarkerNames, CurrentTime, PrevMarker, NextMarker, MarkersPassed, nullptr); }
	virtual void AdvanceMarkerPhaseAsLeader(bool bLooping, float MoveDelta, const TArray<FName>& ValidMarkerNames, float& CurrentTime, FMarkerPair& PrevMarker, FMarkerPair& NextMarker, TArray<FPassedMarker>& MarkersPassed, const UMirrorDataTable* MirrorTable) const { check(false); /*Should never call this (either missing override or calling on unsupported asset */ }

	UE_DEPRECATED(5.0, "Use other AdvanceMarkerPhaseAsFollower signature")
	virtual void AdvanceMarkerPhaseAsFollower(const FMarkerTickContext& Context, float DeltaRemaining, bool bLooping, float& CurrentTime, FMarkerPair& PreviousMarker, FMarkerPair& NextMarker) const { AdvanceMarkerPhaseAsFollower(Context, DeltaRemaining, bLooping, CurrentTime, PreviousMarker, NextMarker, nullptr);}
	virtual void AdvanceMarkerPhaseAsFollower(const FMarkerTickContext& Context, float DeltaRemaining, bool bLooping, float& CurrentTime, FMarkerPair& PreviousMarker, FMarkerPair& NextMarker, const UMirrorDataTable* MirrorTable) const { check(false); /*Should never call this (either missing override or calling on unsupported asset */ }
	
	virtual void GetMarkerIndicesForTime(float CurrentTime, bool bLooping, const TArray<FName>& ValidMarkerNames, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker) const { check(false); /*Should never call this (either missing override or calling on unsupported asset */ }

	UE_DEPRECATED(5.0, "Use other GetMarkerSyncPositionfromMarkerIndicies signature")
	virtual FMarkerSyncAnimPosition GetMarkerSyncPositionfromMarkerIndicies(int32 PrevMarker, int32 NextMarker, float CurrentTime) const { return UAnimSequenceBase::GetMarkerSyncPositionFromMarkerIndicies(PrevMarker, NextMarker, CurrentTime, nullptr); }
	virtual FMarkerSyncAnimPosition GetMarkerSyncPositionFromMarkerIndicies(int32 PrevMarker, int32 NextMarker, float CurrentTime, const UMirrorDataTable* MirrorTable) const { check(false); return FMarkerSyncAnimPosition(); /*Should never call this (either missing override or calling on unsupported asset */ }

	UE_DEPRECATED(5.0, "Use other GetMarkerIndicesForPosition signature")
	virtual void GetMarkerIndicesForPosition(const FMarkerSyncAnimPosition& SyncPosition, bool bLooping, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker, float& CurrentTime) const { GetMarkerIndicesForPosition(SyncPosition, bLooping, OutPrevMarker, OutNextMarker, CurrentTime, nullptr); }
	virtual void GetMarkerIndicesForPosition(const FMarkerSyncAnimPosition& SyncPosition, bool bLooping, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker, float& CurrentTime, const UMirrorDataTable* MirrorTable ) const { check(false); /*Should never call this (either missing override or calling on unsupported asset */ }
	
	virtual float GetFirstMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition) const { return 0.f; }
	virtual float GetNextMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition, const float& StartingPosition) const { return 0.f; }
	virtual float GetPrevMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition, const float& StartingPosition) const { return 0.f; }

	// default implementation, no additive
	virtual EAdditiveAnimationType GetAdditiveAnimType() const { return AAT_None; }
	virtual bool CanBeUsedInComposition() const { return true;  }

	// to support anim sequence base to montage
	virtual void EnableRootMotionSettingFromMontage(bool bInEnableRootMotion, const ERootMotionRootLock::Type InRootMotionRootLock) {};

#if WITH_EDITOR
	// Store that our raw data has changed so that we can get correct compressed data later on
	UE_DEPRECATED(5.0, "MarkRawDataAsModified has been deprecated, any (Raw Data) modification should be applied using the UAnimDataController API instead. This will handle updating the GUID instead.")
	virtual void MarkRawDataAsModified(bool bForceNewRawDatGuid = true) {}

private:
	DECLARE_MULTICAST_DELEGATE( FOnNotifyChangedMulticaster );
	FOnNotifyChangedMulticaster OnNotifyChanged;

public:
	typedef FOnNotifyChangedMulticaster::FDelegate FOnNotifyChanged;

	/** Registers a delegate to be called after notification has changed*/
	void RegisterOnNotifyChanged(const FOnNotifyChanged& Delegate);
	void UnregisterOnNotifyChanged(void* Unregister);
	virtual bool IsValidToPlay() const { return true; }
	// ideally this would be animsequcnebase, but we might have some issue with that. For now, just allow AnimSequence
	virtual class UAnimSequence* GetAdditiveBasePose() const { return nullptr; }
#endif

	// return true if anim notify is available 
	virtual bool IsNotifyAvailable() const;
protected:
	template <typename DataType>
	void VerifyCurveNames(USkeleton& Skeleton, const FName& NameContainer, TArray<DataType>& CurveList)
	{
		for (DataType& Curve : CurveList)
		{
			Skeleton.VerifySmartName(NameContainer, Curve.Name);
		}
	}
#if WITH_EDITOR
public:
	/** Returns the UAnimDataModel object embedded in this UAnimSequenceBase */
	UAnimDataModel* GetDataModel() const;

	/** Returns the transient UAnimDataController set to operate on DataModel */
	IAnimationDataController& GetController();
protected:
	/** Populates the UAnimDataModel object according to any pre-existing data. (overrides expect to populate the model according to their data) */
	virtual void PopulateModel();

	/** Callback registered to UAnimDatModel::GetModifiedEvent for the embedded object */
	virtual void OnModelModified(const EAnimDataModelNotifyType& NotifyType, UAnimDataModel* Model, const FAnimDataModelNotifPayload& Payload);

	/** Validates that DataModel contains a valid UAnimDataModel object */
	void ValidateModel() const;
	
	/** Binds to DataModel its modification delegate */
	void BindToModelModificationEvent();

	/** Replaces the current DataModel, if any, with the provided one */
	void CopyDataModel(const UAnimDataModel* ModelToDuplicate);
private:
	/** Creates a new UAnimDataModel instance and sets DataModel accordingly */
	void CreateModel();

public:
	bool ShouldDataModelBeValid() const;
	bool IsDataModelValid() const
	{
		if(ShouldDataModelBeValid())
		{
			ValidateModel();
			return DataModel != nullptr;
		}

		return false;
	}	
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
protected:
	/** UAnimDataModel instance containing source animation data */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Model")
	TObjectPtr<UAnimDataModel> DataModel;

	/** Flag set whenever the data-model is initially populated (during upgrade path) */
	bool bPopulatingDataModel;

	/** UAnimDataController instance set to operate on DataModel */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, DuplicateTransient, Category = "Animation Model")
	TScriptInterface<IAnimationDataController> Controller;
	
	/** Helper object that keeps track of any controller brackets, and all unique notify types that are broadcasted during it */
	UE::Anim::FAnimDataModelNotifyCollector NotifyCollector;
#endif // WITH_EDITORONLY_DATA

};

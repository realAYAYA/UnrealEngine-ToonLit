// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Sections/MovieSceneParameterSection.h"
#include "Animation/AnimData/AnimDataModelNotifyCollector.h"

#include "AnimSequencerDataModel.generated.h"

class UControlRig;
class UMovieSceneControlRigParameterTrack;
class UMovieSceneControlRigParameterSection;
class UFKControlRig;

USTRUCT()
struct FAnimationCurveMetaData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Flags = 0;

	UPROPERTY()
	FLinearColor Color = FLinearColor::White;

	UPROPERTY()
	FString Comment;
};

UCLASS()
class ANIMATIONDATA_API UAnimationSequencerDataModel : public UMovieSceneSequence, public IAnimationDataModel
{
private:
	GENERATED_BODY()
public:
	static int32 RetainFloatCurves;
	static int32 ValidationMode;
	static int32 UseDirectFKControlRigMode;
	
	/** Begin UObject overrides*/
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual bool IsEditorOnly() const override { return true; }
#if WITH_EDITOR
	virtual void WillNeverCacheCookedPlatformDataAgain() override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
#endif
	/** End UObject overrides */

	/** Begin UMovieSceneSequence overrides */
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override {}
	virtual bool CanPossessObject(UObject& Object, UObject* InPlaybackContext) const override { return false; }
	virtual void LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override {}
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override {}
	virtual void UnbindObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject* Context) override {}
	virtual void UnbindInvalidObjects(const FGuid& ObjectId, UObject* Context) override {}
	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif
	/** End UMovieSceneSequence overrides */
	
	/** Begin IAnimationDataModel overrides*/
	virtual double GetPlayLength() const override;
	virtual int32 GetNumberOfFrames() const override;	
	virtual int32 GetNumberOfKeys() const override;
	virtual FFrameRate GetFrameRate() const override;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS	
	virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const override;
	virtual const FBoneAnimationTrack& GetBoneTrackByIndex(int32 TrackIndex) const override;
	virtual const FBoneAnimationTrack& GetBoneTrackByName(FName TrackName) const override;
	virtual const FBoneAnimationTrack* FindBoneTrackByName(FName Name) const override;
	virtual const FBoneAnimationTrack* FindBoneTrackByIndex(int32 BoneIndex) const override;
	virtual int32 GetBoneTrackIndex(const FBoneAnimationTrack& Track) const override;
	virtual int32 GetBoneTrackIndexByName(FName TrackName) const override;	
	virtual bool IsValidBoneTrackIndex(int32 TrackIndex) const override;
	virtual int32 GetNumBoneTracks() const override;
	virtual void GetBoneTrackNames(TArray<FName>& OutNames) const override;
	virtual const FAnimCurveBase* FindCurve(const FAnimationCurveIdentifier& CurveIdentifier) const override;
	virtual const FFloatCurve* FindFloatCurve(const FAnimationCurveIdentifier& CurveIdentifier) const override;
	virtual const FTransformCurve* FindTransformCurve(const FAnimationCurveIdentifier& CurveIdentifier) const override;
	virtual const FRichCurve* FindRichCurve(const FAnimationCurveIdentifier& CurveIdentifier) const override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual bool IsValidBoneTrackName(const FName& TrackName) const override;
	virtual FTransform GetBoneTrackTransform(FName TrackName, const FFrameNumber& FrameNumber) const override;
	virtual void GetBoneTrackTransforms(FName TrackName, const TArray<FFrameNumber>& FrameNumbers, TArray<FTransform>& OutTransforms) const override;	
	virtual void GetBoneTrackTransforms(FName TrackName, TArray<FTransform>& OutTransforms) const override;
	virtual void GetBoneTracksTransform(const TArray<FName>& TrackNames, const FFrameNumber& FrameNumber, TArray<FTransform>& OutTransforms) const override;	
	virtual FTransform EvaluateBoneTrackTransform(FName TrackName, const FFrameTime& FrameTime, const EAnimInterpolationType& Interpolation) const override;	
	virtual const FAnimationCurveData& GetCurveData() const override;
	virtual int32 GetNumberOfTransformCurves() const override;
	virtual int32 GetNumberOfFloatCurves() const override;
	virtual const TArray<struct FFloatCurve>& GetFloatCurves() const override;
	virtual const TArray<struct FTransformCurve>& GetTransformCurves() const override;
	virtual const FAnimCurveBase& GetCurve(const FAnimationCurveIdentifier& CurveIdentifier) const override;
	virtual const FFloatCurve& GetFloatCurve(const FAnimationCurveIdentifier& CurveIdentifier) const override;
	virtual const FTransformCurve& GetTransformCurve(const FAnimationCurveIdentifier& CurveIdentifier) const override;
	virtual const FRichCurve& GetRichCurve(const FAnimationCurveIdentifier& CurveIdentifier) const override;
	virtual TArrayView<const FAnimatedBoneAttribute> GetAttributes() const override;
	virtual int32 GetNumberOfAttributes() const override;
	virtual int32 GetNumberOfAttributesForBoneIndex(const int32 BoneIndex) const override;
	virtual void GetAttributesForBone(const FName& BoneName, TArray<const FAnimatedBoneAttribute*>& OutBoneAttributes) const override;
	virtual const FAnimatedBoneAttribute& GetAttribute(const FAnimationAttributeIdentifier& AttributeIdentifier) const override;
	virtual const FAnimatedBoneAttribute* FindAttribute(const FAnimationAttributeIdentifier& AttributeIdentifier) const override;
	virtual UAnimSequence* GetAnimationSequence() const override;
	virtual FAnimDataModelModifiedEvent& GetModifiedEvent() override { return ModifiedEvent; }
	virtual FGuid GenerateGuid() const override;
	virtual TScriptInterface<IAnimationDataController> GetController() override;
	virtual IAnimationDataModel::FModelNotifier& GetNotifier() override;
	virtual FAnimDataModelModifiedDynamicEvent& GetModifiedDynamicEvent() override { return ModifiedEventDynamic; }
    virtual void Evaluate(FAnimationPoseData& InOutPoseData, const UE::Anim::DataModel::FEvaluationContext& EvaluationContext) const override;
	virtual bool HasBeenPopulated() const override { return bPopulated; }
	virtual void IterateBoneKeys(const FName& BoneName, TFunction<bool(const FVector3f& Pos, const FQuat4f&, const FVector3f, const FFrameNumber&)> IterationFunction) const override;
protected:
	virtual void OnNotify(const EAnimDataModelNotifyType& NotifyType, const FAnimDataModelNotifPayload& Payload) override final;

	virtual void LockEvaluationAndModification() const override final
	{
		EvaluationLock.Lock();
	}

	virtual bool TryLockEvaluationAndModification() const override final
	{
		return EvaluationLock.TryLock();
	}
	
	virtual void UnlockEvaluationAndModification() const override final
	{
		EvaluationLock.Unlock();
	}
	/** End IAnimationDataModel overrides */
	
	/** Controller helper functionality */
	FTransformCurve* FindMutableTransformCurveById(const FAnimationCurveIdentifier& CurveIdentifier);
	FFloatCurve* FindMutableFloatCurveById(const FAnimationCurveIdentifier& CurveIdentifier);
	FAnimCurveBase* FindMutableCurveById(const FAnimationCurveIdentifier& CurveIdentifier);
	FRichCurve* GetMutableRichCurve(const FAnimationCurveIdentifier& CurveIdentifier);

	void RegenerateLegacyCurveData();
	void UpdateLegacyCurveData();

	void ValidateData() const;
	void ValidateSequencerData() const;
	void ValidateControlRigData() const;
	void ValidateLegacyAgainstControlRigData() const;
	
	void GeneratePoseData(UControlRig* ControlRig, FAnimationPoseData& InOutPoseData, const UE::Anim::DataModel::FEvaluationContext& EvaluationContext) const;
	void EvaluateTrack(UMovieSceneControlRigParameterTrack* CR_Track, const UE::Anim::DataModel::FEvaluationContext& EvaluationContext) const;

	UMovieSceneControlRigParameterTrack* GetControlRigTrack() const;
	UMovieSceneControlRigParameterSection* GetFKControlRigSection() const;
	USkeleton* GetSkeleton() const;
	void InitializeFKControlRig(UFKControlRig* FKControlRig, USkeleton* Skeleton) const;
	UControlRig* GetControlRig() const;
	
	void IterateTransformControlCurve(const FName& BoneName, TFunction<void(const FTransform&, const FFrameNumber&)> IterationFunction, const TArray<FFrameNumber>* InFrameNumbers = nullptr) const;
	void GenerateTransformKeysForControl(const FName& BoneName, TArray<FTransform>& InOutTransforms, TArray<FFrameNumber>& InOutFrameNumbers) const;
	void RemoveOutOfDateControls() const;
	
	void GenerateTransformKeysForControl(const FName& BoneName, const TArray<FFrameNumber>& FrameNumbers, TArray<FTransform>& InOutTransforms) const;
private:	
	/** Dynamic delegate event allows scripting to register to any broadcast-ed notify. */
	UPROPERTY(BlueprintAssignable, Transient, Category = AnimationDataModel, meta = (ScriptName = "ModifiedEvent", AllowPrivateAccess = "true"))
	FAnimDataModelModifiedDynamicEvent ModifiedEventDynamic;	
	FAnimDataModelModifiedEvent ModifiedEvent;
	TUniquePtr<IAnimationDataModel::FModelNotifier> Notifier;
	UE::Anim::FAnimDataModelNotifyCollector Collector;

	UPROPERTY(VisibleAnywhere, Category=AnimSequencer)
	FAnimationCurveData LegacyCurveData;

	UPROPERTY(VisibleAnywhere, Category=AnimSequencer)
	TArray<FAnimatedBoneAttribute> AnimatedBoneAttributes;

	// Movie scene instance containing FK Control rig and section representing the animation data
	UPROPERTY(VisibleAnywhere, Category=AnimSequencer)
	TObjectPtr<UMovieScene> MovieScene = nullptr;

	// Per-curve information holding flags/color, due to be deprecated in the future
	UPROPERTY(VisibleAnywhere, Category=AnimSequencer)
	TMap<FAnimationCurveIdentifier, FAnimationCurveMetaData> CurveIdentifierToMetaData;

	UPROPERTY(VisibleAnywhere, Category=AnimSequencer)
	bool bPopulated = false;

	// Raw data GUID taken from UAnimSequence when initially populating - this allows for retaining compressed data state initially
	UPROPERTY(VisibleAnywhere, Category=AnimSequencer)
	FGuid CachedRawDataGUID;

	// Scope lock to prevent contention around ControlRig->Evaluation()
	mutable FCriticalSection EvaluationLock;

	friend class UAnimSequencerController;
};



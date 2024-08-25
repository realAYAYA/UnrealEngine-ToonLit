// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/GCObject.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimationRecordingSettings.h"
#include "Components/SkinnedMeshComponent.h"
#include "Animation/AnimNotifyQueue.h"
#include "Serializers/MovieSceneAnimationSerialization.h"
#include "Misc/QualifiedFrameTime.h"
#include "Animation/AnimTypes.h"
#include "AnimationRecorder.generated.h"

class UAnimBoneCompressionSettings;
class UAnimNotify;
class UAnimNotifyState;
class UAnimSequence;
class USkeletalMeshComponent;

DECLARE_LOG_CATEGORY_EXTERN(AnimationSerialization, Verbose, All);

UENUM(BlueprintType)
enum class ETimecodeBoneMode : uint8
{
	All,
	Root,
	UserDefined,
	MAX UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FTimecodeBoneMethod
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor, initializing with default values */
	FTimecodeBoneMethod() : BoneMode(ETimecodeBoneMode::Root) { }

	/** The timecode bone mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timecode")
	ETimecodeBoneMode BoneMode;

	/** Name of the bone to assign timecode values to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timecode")
	FName BoneName;
};

//////////////////////////////////////////////////////////////////////////
// FAnimationRecorder

// records the mesh pose to animation input
struct SEQUENCERECORDER_API FAnimationRecorder : public FGCObject
{
private:
	/** Frame count used to signal an unbounded animation */
	static const int32 UnBoundedFrameCount = -1;

private:
	FFrameRate RecordingRate;
	FFrameNumber MaxFrame;
	FFrameNumber LastFrame;
	double TimePassed;
	TObjectPtr<UAnimSequence> AnimationObject;
	TArray<FTransform> PreviousSpacesBases;
	FBlendedHeapCurve PreviousAnimCurves;
	FTransform PreviousComponentToWorld;
	FTransform InvInitialRootTransform;
	FTransform InitialRootTransform;
	int32 SkeletonRootIndex;

	/** Unique notifies added to this sequence during recording */
	TMap<UAnimNotify*, UAnimNotify*> UniqueNotifies;

	/** Unique notify states added to this sequence during recording */
	TMap<UAnimNotifyState*, UAnimNotifyState*> UniqueNotifyStates;

	struct FRecordedAnimNotify
	{
		FRecordedAnimNotify(const FAnimNotifyEvent& InNewNotifyEvent, const FAnimNotifyEvent* InOriginalNotifyEvent, float InAnimNotifyStartTime, float InAnimNotifyEndTime)
			: NewNotifyEvent(InNewNotifyEvent)
			, OriginalNotifyEvent(InOriginalNotifyEvent)
			, AnimNotifyStartTime(InAnimNotifyStartTime)
			, AnimNotifyEndTime(InAnimNotifyEndTime)
			, bWasActive(true)
		{}

		/** Notify which will be added to this sequence */
		FAnimNotifyEvent NewNotifyEvent;

		/** Notify which was called on the sequence being recorded */
		const FAnimNotifyEvent* OriginalNotifyEvent;

		/** The time in the recorded animation at which the recorded notify started and ended */
		float AnimNotifyStartTime;
		float AnimNotifyEndTime;

		/** Whether this notify was active this frame */
		bool bWasActive;
	};

	/** Notify events recorded at any point, processed and inserted into animation when recording has finished */
	TArray<FRecordedAnimNotify> RecordedAnimNotifies;

	/** Currently recording notify events that have duration */
	TArray<FRecordedAnimNotify> RecordingAnimNotifies;

	static float DefaultSampleRate;

	/** Array of times recorded */
	TArray<FQualifiedFrameTime> RecordedTimes;

public:
	FAnimationRecorder();
	~FAnimationRecorder();

	// FGCObject interface start
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FAnimationRecorder");
	}
	// FGCObject interface end

	/** Starts recording an animation. Prompts for asset path and name via dialog if none provided */
	bool TriggerRecordAnimation(USkeletalMeshComponent* Component);
	bool TriggerRecordAnimation(USkeletalMeshComponent* Component, const FString& AssetPath, const FString& AssetName);

	void StartRecord(USkeletalMeshComponent* Component, UAnimSequence* InAnimationObject);
	UAnimSequence* StopRecord(bool bShowMessage);
	void UpdateRecord(USkeletalMeshComponent* Component, float DeltaTime);
	UAnimSequence* GetAnimationObject() const { return AnimationObject; }
	bool InRecording() const { return AnimationObject != nullptr; }
	double GetTimeRecorded() const { return TimePassed; }

	/** Sets a new sample rate & max length for this recorder. Don't call while recording. */
	void SetSampleRateAndLength(FFrameRate SampleFrameRate, float LengthInSeconds);

	bool SetAnimCompressionScheme(UAnimBoneCompressionSettings* Settings);

	const FTransform& GetInitialRootTransform() const { return InitialRootTransform; }

	void ProcessRecordedTimes(UAnimSequence* AnimSequence, USkeletalMeshComponent* SkeletalMeshComponent, const FString& HoursName, const FString& MinutesName, const FString& SecondsName, const FString& FramesName, const FString& SubFramesName, const FString& SlateName, const FString& Slate, const FTimecodeBoneMethod& TimecodeBoneMethod);

	/** If true, it will record root to include LocalToWorld */
	uint8 bRecordLocalToWorld :1;
	/** If true, asset will be saved to disk after recording. If false, asset will remain in mem and can be manually saved. */
	uint8 bAutoSaveAsset : 1;
	/** If true, the root bone transform will be removed from all bone transforms */
	uint8 bRemoveRootTransform : 1;
	/** If true we check delta time at beginning of recording */
	uint8 bCheckDeltaTimeAtBeginning : 1;
	/** Interpolation type for the recorded sequence */
	EAnimInterpolationType Interpolation;
	/** The interpolation mode for the recorded keys */
	ERichCurveInterpMode InterpMode;
	/** The tangent mode for the recorded keys*/
	ERichCurveTangentMode TangentMode;
	/** Serializer, if set we also store data out incrementally while running*/
	FAnimationSerializer* AnimationSerializer;
	/** Whether or not to record transforms*/
	uint8 bRecordTransforms : 1;
	/** Whether or not to record morph targets*/
	uint8 bRecordMorphTargets : 1;
	/** Whether or not to record attribute curves*/
	uint8 bRecordAttributeCurves : 1;
	/** Whether or not to record material curves*/
	uint8 bRecordMaterialCurves : 1;
	/** Include list */
	TArray<FString> IncludeAnimationNames;
	/** Exclude list */
	TArray<FString> ExcludeAnimationNames;
	/** Whether or not to transact any IAnimationDataController changes */
	bool bTransactRecording;
public:
	/** Helper function to get space bases depending on leader pose component */
	static void GetBoneTransforms(USkeletalMeshComponent* Component, TArray<FTransform>& BoneTransforms);

private:
	bool Record(USkeletalMeshComponent* Component, FTransform const& ComponentToWorld, const TArray<FTransform>& SpacesBases, const FBlendedHeapCurve& AnimationCurves, int32 FrameToAdd);

	void RecordNotifies(USkeletalMeshComponent* Component, const TArray<FAnimNotifyEventReference>& AnimNotifies, float DeltaTime, float RecordTime);

	void ProcessNotifies();

	bool ShouldSkipName(const FName& InName) const;

	TArray<FBlendedHeapCurve> RecordedCurves;
	TArray<FRawAnimSequenceTrack> RawTracks;
};

//////////////////////////////////////////////////////////////////////////
// FAnimRecorderInstance

struct SEQUENCERECORDER_API FAnimRecorderInstance
{
public:
	FAnimRecorderInstance();
	~FAnimRecorderInstance();

	void Init(USkeletalMeshComponent* InComponent, const FString& InAssetPath, const FString& InAssetName, const FAnimationRecordingSettings& InSettings);

	void Init(USkeletalMeshComponent* InComponent, UAnimSequence* InSequence, FAnimationSerializer *InAnimationSerializer, const FAnimationRecordingSettings& InSettings);

	bool BeginRecording();
	void Update(float DeltaTime);
	void FinishRecording(bool bShowMessage = true);
	void ProcessRecordedTimes(UAnimSequence* AnimSequence, USkeletalMeshComponent* SkeletalMeshComponent, const FString& HoursName, const FString& MinutesName, const FString& SecondsName, const FString& FramesName, const FString& SubFramesName, const FString& SlateName, const FString& Slate, const FTimecodeBoneMethod& TimecodeBoneMethod);

private:
	void InitInternal(USkeletalMeshComponent* InComponent, const FAnimationRecordingSettings& Settings, FAnimationSerializer *InAnimationSerializer = nullptr);

public:
	TWeakObjectPtr<USkeletalMeshComponent> SkelComp;
	TWeakObjectPtr<UAnimSequence> Sequence;
	FString AssetPath;
	FString AssetName;

	/** Original ForcedLodModel setting on the SkelComp, so we can modify it and restore it when we are done. */
	int CachedSkelCompForcedLodModel;

	TSharedPtr<FAnimationRecorder> Recorder;

	/** Used to store/restore update flag when recording */
	EVisibilityBasedAnimTickOption CachedVisibilityBasedAnimTickOption;

	/** Used to store/restore URO when recording */
	bool bCachedEnableUpdateRateOptimizations;
};


//////////////////////////////////////////////////////////////////////////
// FAnimationRecorderManager

struct SEQUENCERECORDER_API FAnimationRecorderManager
{
public:
	/** Singleton accessor */
	static FAnimationRecorderManager& Get();

	/** Destructor */
	virtual ~FAnimationRecorderManager();

	/** Starts recording an animation. */
	bool RecordAnimation(USkeletalMeshComponent* Component, const FString& AssetPath = FString(), const FString& AssetName = FString(), const FAnimationRecordingSettings& Settings = FAnimationRecordingSettings());

	bool RecordAnimation(USkeletalMeshComponent* Component, UAnimSequence* Sequence, const FAnimationRecordingSettings& Settings = FAnimationRecordingSettings());
	
	bool RecordAnimation(USkeletalMeshComponent* Component, UAnimSequence* Sequence, FAnimationSerializer *InAnimationSerializer,  const FAnimationRecordingSettings& Settings = FAnimationRecordingSettings());

	bool IsRecording(USkeletalMeshComponent* Component);

	bool IsRecording();

	UAnimSequence* GetCurrentlyRecordingSequence(USkeletalMeshComponent* Component);
	float GetCurrentRecordingTime(USkeletalMeshComponent* Component);
	void StopRecordingAnimation(USkeletalMeshComponent* Component, bool bShowMessage = true);
	void StopRecordingAllAnimations();
	const FTransform& GetInitialRootTransform(USkeletalMeshComponent* Component) const;

	void Tick(float DeltaTime);

	void Tick(USkeletalMeshComponent* Component, float DeltaTime);

	void StopRecordingDeadAnimations(bool bShowMessage = true);

private:
	/** Constructor, private - use Get() function */
	FAnimationRecorderManager();

	TArray<FAnimRecorderInstance> RecorderInstances;

	void HandleEndPIE(bool bSimulating);
};


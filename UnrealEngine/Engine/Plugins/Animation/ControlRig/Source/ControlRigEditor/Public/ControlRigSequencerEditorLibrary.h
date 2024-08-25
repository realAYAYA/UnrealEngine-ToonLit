// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * Control Rig Sequencer Exposure
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneBindingProxy.h"
#include "Tools/ControlRigSnapper.h"
#include "TransformNoScale.h"
#include "EulerTransform.h"
#include "MovieSceneToolsUserSettings.h"
#include "Tools/ControlRigSnapSettings.h"
#include "MovieSceneTimeUnit.h"
#include "RigSpacePickerBakeSettings.h"
#include "ControlRigSequencerEditorLibrary.generated.h"

class ULevelSequence;
class UTickableConstraint;
class UTickableTransformConstraint;
class UTransformableHandle;
struct FBakingAnimationKeySettings;
class UControlRig;

USTRUCT(BlueprintType)
struct FControlRigSequencerBindingProxy
{
	GENERATED_BODY()

	FControlRigSequencerBindingProxy()
		: ControlRig(nullptr)
		, Track(nullptr)
	{}

	FControlRigSequencerBindingProxy(const FMovieSceneBindingProxy& InProxy, UControlRig* InControlRig, UMovieSceneControlRigParameterTrack* InTrack)
		: Proxy(InProxy)
		, ControlRig(InControlRig)
		, Track(InTrack)
	{}

	UPROPERTY(BlueprintReadOnly, Category = ControlRig)
	FMovieSceneBindingProxy Proxy;

	UPROPERTY(BlueprintReadOnly, Category = ControlRig)
	TObjectPtr<UControlRig> ControlRig;

	UPROPERTY(BlueprintReadOnly, Category = ControlRig)
	TObjectPtr<UMovieSceneControlRigParameterTrack> Track;
};

UENUM(BlueprintType)
enum class EAnimToolBlendOperation : uint8
{
	Tween,
	BlendToNeighbor,
	PushPull,
	BlendRelative,
	BlendToEase,
	SmoothRough,
};

/**
* This is a set of helper functions to access various parts of the Sequencer and Control Rig API via Python and Blueprints.
*/
UCLASS(meta=(Transient, ScriptName="ControlRigSequencerLibrary"))
class CONTROLRIGEDITOR_API UControlRigSequencerEditorLibrary : public UBlueprintFunctionLibrary
{

	public:
	GENERATED_BODY()

public:

	/**
	* Get all of the visible control rigs in the level
	* @return returns list of visible Control Rigs
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<UControlRig*> GetVisibleControlRigs();

	/**
	* Get all of the control rigs and their bindings in the level sequence
	* @param LevelSequence The movie scene sequence to look for Control Rigs
	* @return returns list of Control Rigs in the level sequence.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FControlRigSequencerBindingProxy> GetControlRigs(ULevelSequence* LevelSequence);

	/**
	* Find or create a Control Rig track of a specific class based upon the binding
	* @param World The world used to spawn into temporarily if binding is a spawnable
	* @param LevelSequence The LevelSequence to find or create
	* @param ControlRigClass The class of the Control Rig
	* @param InBinding The binding (actor or component binding) to find or create the Control Rig track
	* @return returns Return the found or created track
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static UMovieSceneTrack* FindOrCreateControlRigTrack(UWorld* World, ULevelSequence* LevelSequence, const UClass* ControlRigClass, const FMovieSceneBindingProxy& InBinding, bool bIsLayeredControlRig = false);

	/**
	* Find or create a Control Rig Component
	* @param World The world used to spawn into temporarily if binding is a spawnable
	* @param LevelSequence The LevelSequence to find or create
	* @param InBinding The binding (actor or component binding) to find or create the Control Rig tracks
	* @return returns Find array of component Control Rigs that were found or created
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<UMovieSceneTrack*> FindOrCreateControlRigComponentTrack(UWorld* World, ULevelSequence* LevelSequence, const FMovieSceneBindingProxy& InBinding);
	
	/**
	* Load anim sequence into this control rig section
	* @param MovieSceneSection The MovieSceneSectionto load into
	* @param AnimSequence The Sequence to load
	* @param MovieScene The MovieScene getting loaded into
	* @param SkelMeshComponent The Skeletal Mesh component getting loaded into.
	* @param InStartFrame Frame to insert the animation
	* @param TimeUnit Unit for all frame and time values, either in display rate or tick resolution
	* @param bKeyReduce If true do key reduction based upon Tolerance, if false don't
	* @param Tolerance If reducing keys, tolerance about which keys will be removed, smaller tolerance, more keys usually.
	* @param Interpolation The key interpolation type to set the keys, defaults to EMovieSceneKeyInterpolation::SmartAuto
	* @param bResetControls If true will reset all controls to initial value on every frame
	* @return returns True if successful, False otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool LoadAnimSequenceIntoControlRigSection(UMovieSceneSection* MovieSceneSection, UAnimSequence* AnimSequence, USkeletalMeshComponent* SkelMeshComp,
		FFrameNumber InStartFrame, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate, bool bKeyReduce = false, float Tolerance = 0.001f,
		EMovieSceneKeyInterpolation Interpolation = EMovieSceneKeyInterpolation::SmartAuto, bool bResetControls = true);

	/**
	* Bake the current animation in the binding to a Control Rig track
	* @param World The active world
	* @param LevelSequence The LevelSequence we are baking
	* @param ControlRigClass The class of the Control Rig
	* @param ExportOptions Export options for creating an animation sequence
	* @param bKeyReduce If true do key reduction based upon Tolerance, if false don't
	* @param Tolerance If reducing keys, tolerance about which keys will be removed, smaller tolerance, more keys usually.
	* @param Binding The binding upon which to bake
	* @param bResetControls If true will reset all controls to initial value on every frame
	* @return returns True if successful, False otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool BakeToControlRig(UWorld* World, ULevelSequence* LevelSequence, UClass* ControlRigClass, UAnimSeqExportOption* ExportOptions, bool bReduceKeys, float Tolerance,
			const FMovieSceneBindingProxy& Binding, bool bResetControls = true);

	/**
	* Bake the constraint to keys based on the passed in frames. This will use the open sequencer to bake. See ConstraintsScriptingLibrary to get the list of available constraints
	* @param World The active world
	* @param Constraint The Constraint to bake. After baking it will be keyed to be inactive of the range of frames that are baked
	* @param Frames The frames to bake, if the array is empty it will use the active time ranges of the constraint to determine where it should bake
	* @param TimeUnit Unit for all frame and time values, either in display rate or tick resolution
	* @return Returns True if successful, False otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool BakeConstraint(UWorld* World, UTickableConstraint* Constraint, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Bake the constraint to keys based on the passed in settings. This will use the open sequencer to bake. See ConstraintsScriptingLibrary to get the list of available constraints
	* @param World The active world
	* @param InConstraints The Constraints tobake.  After baking they will be keyed to be inactive of the range of frames that are baked
	* @param InSettings Settings to use for baking
	* @return Returns True if successful, False otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool BakeConstraints(UWorld* World, TArray<UTickableConstraint*>& InConstraints,const FBakingAnimationKeySettings& InSettings);
	/**
	* Add a constraint possibly adding to sequencer also if one is open.
	* @param World The active world
	* @param InType Type of constraint to create
	* @param InChild The handle to the transormable to be constrainted
	* @param InParent The handle to the parent of the constraint
	* @param bMaintainOffset Whether to maintain offset between child and parent when setting the constraint
	* @return Returns the constraint if created all nullptr if not
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static UTickableConstraint* AddConstraint(UWorld* World, ETransformConstraintType InType, UTransformableHandle* InChild, UTransformableHandle* InParent, const bool bMaintainOffset);
	
	/**
	* Get the constraint keys for the specified constraint
	* @param InConstraint The constraint to get
	* @param ConstraintSection Section containing Cosntraint Key
	* @param OutBools Array of whether or not it's active at the specified times
	* @param OutFrames The Times for the keys
	* @param TimeUnit Unit for the time params, either in display rate or tick resolution
	* @return Returns true if we got the keys from this constraint
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool GetConstraintKeys(UTickableConstraint* InConstraint, UMovieSceneSection* ConstraintSection,TArray<bool>& OutBools, TArray<FFrameNumber>& OutFrames, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);
	
	/**
	* Set the constraint active key in the current open Sequencer
	* @param InConstraint The constraint to set the key
	* @param bActive Whether or not it's active
	* @param FrameTime Time to set the value
	* @param TimeUnit Unit for the time params, either in display rate or tick resolution
	* @return Returns true if we set the constraint to be the passed in value, false if not. We may not do so if the value is the same.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool SetConstraintActiveKey(UTickableConstraint* InConstraint, bool bActive, FFrameNumber InFrame, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get all constraints for this object, which is described by a transformable handle
	* @param InChild The handle to look for constraints controlling it
	* @return Returns array of Constraints this handle is constrained to.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray <UTickableConstraint*> GetConstraintsForHandle(UWorld* InWorld, const UTransformableHandle* InChild);

	/** Move the constraint active key in the current open Sequencer
	* @param InConstraint The constraint whose key to move
	* @param ConstraintSection Section containing Cosntraint Key
	* @param InTime Original time of the constraint key
	* @param InNewTime New time for the constraint key
	* @param TimeUnit Unit for the time params, either in display rate or tick resolution
	* @return Will return false if function fails, for example if there is no key at this time it will fail, or if the new time is invalid it could fail also
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool MoveConstraintKey(UTickableConstraint* Constraint, UMovieSceneSection* ConstraintSection, FFrameNumber InTime, FFrameNumber InNewTime, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/** Delete the Key for the Constraint at the specified time. 
	* @param InConstraint The constraint whose key to move
	* @param ConstraintSection Section containing Cosntraint Key
	* @param InTime Time to delete the constraint.
	* @param TimeUnit Unit for the InTime, either in display rate or tick resolution
	* @return Will return false if function fails,  for example if there is no key at this time it will fail.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool DeleteConstraintKey(UTickableConstraint* Constraint, UMovieSceneSection* ConstraintSection, FFrameNumber InTime, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);
	/**
	* Compensate constraint at the specfied time 
	* @param InConstraint The constraint to compensate
	* @param InTime Time to compensate
	* @param TimeUnit Unit for the InTime, either in display rate or tick resolution
	* @return Returns true if it can compensate
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool Compensate(UTickableConstraint* InConstraint,  FFrameNumber InTime, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Compensate constraint at all keys
	* @param InConstraint The constraint to compensate
	* @return Returns true if it can compensate
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool CompensateAll(UTickableConstraint* InConstraint);

	/**
	* Peform a Tween operation on the current active sequencer time(must be visible).
	* @param LevelSequence The LevelSequence that's loaded in the editor
	* @param ControlRig The Control Rig to tween.
	* @param TweenValue The tween value to use, range from -1(blend to previous) to 1(blend to next)
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool TweenControlRig(ULevelSequence* LevelSequence, UControlRig* ControlRig, float TweenValue);
	
	/**
	* Peform specified blend operation based upon selected keys in the curve editor or selected control rig controls
	* @param LevelSequence The LevelSequence that's loaded in the editor
	* @param EAnimToolBlendOperation The operation to perform
	* @param BlendValue The blend value to use, range from -1(blend to previous) to 1(blend to next)
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool BlendValuesOnSelected(ULevelSequence* LevelSequence, EAnimToolBlendOperation BlendOperation, 
		float BlendValue);

	/**
	* Peform a Snap operation to snap the children to the parent. 
	* @param LevelSequence Active Sequence to snap
	* @param StartFrame Beginning of the snap
	* @param EndFrame End of the snap
	* @param ChildrenToSnap The children objects that snap and get keys set onto. They need to live in an active Sequencer in the level editor
	* @param ParentToSnap The parent object to snap relative to. If animated, it needs to live in an active Sequencer in the level editor
	* @param SnapSettings Settings to use
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param Returns True if successful
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool SnapControlRig(ULevelSequence* LevelSequence, FFrameNumber StartFrame, FFrameNumber EndFrame, const FControlRigSnapperSelection& ChildrenToSnap,
		const FControlRigSnapperSelection& ParentToSnap, const UControlRigSnapSettings* SnapSettings, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);
	
	/**
	* Get Actors World Transform at a specific time
	* @param LevelSequence Active Sequence to get transform for
	* @param Actor The actor
	* @param Frame Time to get the transform
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns World Transform
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FTransform GetActorWorldTransform(ULevelSequence* LevelSequence,AActor* Actor, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get Actors World Transforms at specific times
	* @param LevelSequence Active Sequence to get transform for
	* @param Actor The actor
	* @param Frames Times to get the transform
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns World Transforms
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FTransform> GetActorWorldTransforms(ULevelSequence* LevelSequence, AActor* Actor, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get SkeletalMeshComponent World Transform at a specific time
	* @param LevelSequence Active Sequence to get transform for
	* @param SkeletalMeshComponent The SkeletalMeshComponent
	* @param Frame Time to get the transform
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param ReferenceName Optional name of the referencer
	* @return Returns World Transform
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FTransform GetSkeletalMeshComponentWorldTransform(ULevelSequence* LevelSequence, USkeletalMeshComponent* SkeletalMeshComponent, FFrameNumber Frame,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate,FName ReferenceName = NAME_None);

	/**
	* Get SkeletalMeshComponents World Transforms at specific times
	* @param LevelSequence Active Sequence to get transform for
	* @param SkeletalMeshComponent The SkeletalMeshComponent
	* @param Frames Times to get the transform
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param ReferenceName Optional name of the referencer
	* @return Returns World Transforms
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FTransform> GetSkeletalMeshComponentWorldTransforms(ULevelSequence* LevelSequence, USkeletalMeshComponent* SkeletalMeshComponent, const TArray<FFrameNumber>& Frames, 
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate,FName ReferenceName = NAME_None);

	/**
	* Get ControlRig Control's World Transform at a specific time
	* @param LevelSequence Active Sequence to get transform for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control
	* @param Frame Time to get the transform
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns World Transform
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FTransform GetControlRigWorldTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's World Transforms at specific times
	* @param LevelSequence Active Sequence to get transform for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control 
	* @param Frames Times to get the transform
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns World Transforms
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FTransform> GetControlRigWorldTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's World Transform at a specific time
	* @param LevelSequence Active Sequence to set transforms for. Must be loaded in Level Editor.
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control
	* @param Frame Time to set the transform
	* @param WorldTransform World Transform to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey Whether or not to set a key.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetControlRigWorldTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, const FTransform& WorldTransform,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's World Transforms at a specific times.
	* @param LevelSequence Active Sequence to set transforms for. Must be loaded in Level Editor.
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control
	* @param Frames Times to set the transform
	* @param WorldTransform World Transform to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetControlRigWorldTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, 
		 const TArray<FTransform>& WorldTransforms, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);
	
	/**
	* Get ControlRig Control's float value at a specific time
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a float control
	* @param Frame Time to get the value
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Value at that time
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static float GetLocalControlRigFloat(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's float values at specific times
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a float control
	* @param Frames Times to get the values
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Values at those times
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<float> GetLocalControlRigFloats(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's float value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a float control
	* @param Frame Time to set the value
	* @param Value The value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigFloat(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, float Value,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate,bool bSetKey = true);

	/**
	* Set ControlRig Control's float values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a float control
	* @param Frames Times to set the values
	* @param Values The values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigFloats(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<float> Values,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's bool value at a specific time
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a bool control
	* @param Frame Time to get the value
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Value at that time
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool GetLocalControlRigBool(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's bool values at specific times
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a bool control
	* @param Frames Times to get the values
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Values at those times
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<bool> GetLocalControlRigBools(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's bool value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a bool control
	* @param Frame Time to set the value
	* @param Value The value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigBool(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, bool Value,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's bool values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a bool control
	* @param Frames Times to set the values
	* @param Values The values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigBools(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		const TArray<bool> Values, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's integer value at a specific time
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a integer control
	* @param Frame Time to get the value
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Value at that time
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static int32 GetLocalControlRigInt(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, 
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's integer values at specific times
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a intteger control
	* @param Frames Times to get the values
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Values at those times
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<int32> GetLocalControlRigInts(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's int value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a int control
	* @param Frame Time to set the value
	* @param Value The value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigInt(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, int32 Value,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate,bool bSetKey = true);


	/**
	* Set ControlRig Control's int values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a int control
	* @param Frames Times to set the values
	* @param Values The values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigInts(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<int32> Values,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's Vector2D value at a specific time
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Vector2D control
	* @param Frame Time to get the value
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Value at that time
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FVector2D GetLocalControlRigVector2D(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's Vector2D values at specific times
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Vector2D control
	* @param Frames Times to get the values
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Values at those times
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FVector2D> GetLocalControlRigVector2Ds(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's Vector2D value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Vector2D control
	* @param Frame Time to set the value
	* @param Value The value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigVector2D(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector2D Value,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate,bool bSetKey = true);


	/**
	* Set ControlRig Control's Vector2D values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Vector2D control
	* @param Frames Times to set the values
	* @param Values The values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigVector2Ds(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FVector2D> Values,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's Position value at a specific time
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Position control
	* @param Frame Time to get the value
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Value at that time
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FVector GetLocalControlRigPosition(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's Position values at specific times
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Position control
	* @param Frames Times to get the values
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Values at those times
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FVector> GetLocalControlRigPositions(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's Position value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Position control
	* @param Frame Time to set the value
	* @param Value The value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigPosition(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector Value,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's Position values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Position control
	* @param Frames Times to set the values
	* @param Values The values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigPositions(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FVector> Values,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's Rotator value at a specific time
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Rotator control
	* @param Frame Time to get the value
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Value at that time
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FRotator GetLocalControlRigRotator(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's Rotator values at specific times
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Rotator control
	* @param Frames Times to get the values
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Values at those times
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FRotator> GetLocalControlRigRotators(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's Rotator value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Rotator control
	* @param Frame Time to set the value
	* @param Value The value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigRotator(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FRotator Value, 
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's Rotator values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Rotator control
	* @param Frames Times to set the values
	* @param Values The values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigRotators(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FRotator> Values,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's Scale value at a specific time
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Scale control
	* @param Frame Time to get the value
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Value at that time
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FVector GetLocalControlRigScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's Scale values at specific times
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Scale control
	* @param Frames Times to get the values
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Values at those times
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FVector> GetLocalControlRigScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's Scale value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Scale control
	* @param Frame Time to set the value
	* @param Value The value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector Value, 
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's Scale values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Scale control
	* @param Frames Times to set the values
	* @param Values The values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FVector> Values,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);


	/**
	* Get ControlRig Control's EulerTransform value at a specific time
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a EulerTransfom control
	* @param Frame Time to get the value
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Value at that time
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FEulerTransform GetLocalControlRigEulerTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's EulerTransform values at specific times
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a EulerTransform control
	* @param Frames Times to get the values
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Values at those times
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FEulerTransform> GetLocalControlRigEulerTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's EulerTransform value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a EulerTransform control
	* @param Frame Time to set the value
	* @param Value The value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigEulerTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FEulerTransform Value,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's EulerTransform values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a EulerTransform control
	* @param Frames Times to set the values
	* @param Values The values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigEulerTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FEulerTransform> Values,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's TransformNoScale value at a specific time
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a TransformNoScale control
	* @param Frame Time to get the value
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Value at that time
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FTransformNoScale GetLocalControlRigTransformNoScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's TransformNoScale values at specific times
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a TransformNoScale control
	* @param Frames Times to get the values
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Values at those times
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FTransformNoScale> GetLocalControlRigTransformNoScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's TransformNoScale value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a TransformNoScale control
	* @param Frame Time to set the value
	* @param Value The value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigTransformNoScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FTransformNoScale Value, 
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's TransformNoScale values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a TransformNoScale control
	* @param Frames Times to set the values
	* @param Values The values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigTransformNoScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FTransformNoScale> Values,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's Transform value at a specific time
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Transform control
	* @param Frame Time to get the value
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Value at that time
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FTransform GetLocalControlRigTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, 
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Get ControlRig Control's Transform values at specific times
	* @param LevelSequence Active Sequence to get value for
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Transform control
	* @param Frames Times to get the values
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns Values at those times
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FTransform> GetLocalControlRigTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's Transform value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Transform control
	* @param Frame Time to set the value
	* @param Value The value to set 
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FTransform Value, 
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's Transform values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Transform control
	* @param Frames Times to set the values
	* @param Values The values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FTransform> Values,
		EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/*
	 * Import FBX onto a control rig with the specified track and section
	 *
	 * @param InWorld World to import to
	 * @param InSequence Sequence to import
	 * @param InTrack Track to import onto
	 * @param InSection Section to import onto, may be null in which case we use the track's section to key
	 * @param SelectedControlRigNames  List of selected control rig names. Will use them if  ImportFBXControlRigSettings->bImportOntoSelectedControls is true
	 * @param ImportFBXControlRigSettings Settings to control import.
	 * @param InImportFileName Path to fbx file to create
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | FBX")
	static bool ImportFBXToControlRigTrack(UWorld* World, ULevelSequence* InSequence, UMovieSceneControlRigParameterTrack* InTrack, UMovieSceneControlRigParameterSection* InSection,
			const TArray<FString>& SelectedControlRigNames,
			UMovieSceneUserImportFBXControlRigSettings* ImportFBXControlRigSettings,
			const FString& ImportFilename);

	/** Exports an FBX from the given control rig section. */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | FBX")
	static bool ExportFBXFromControlRigSection(ULevelSequence* Sequence, const UMovieSceneControlRigParameterSection* Section,
	                                    const UMovieSceneUserExportFBXControlRigSettings* ExportFBXControlRigSettings);

	/*
	 * Collapse and bake all sections and layers on a control rig track to just one section.
	 *
	 * @param InSequence Sequence that has track to collapse
	 * @param InTrack Track for layers to collapse
	 * @param bKeyReduce If true do key reduction based upon Tolerance, if false don't
	 * @param Tolerance If reducing keys, tolerance about which keys will be removed, smaller tolerance, more keys usually.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool CollapseControlRigAnimLayers(ULevelSequence* InSequence,UMovieSceneControlRigParameterTrack* InTrack, bool bKeyReduce = false, float Tolerance = 0.001f);

	/*
	 * Collapse and bake all sections and layers on a control rig track to just one section using passed in settings.
	 *
	 * @param InSequence Sequence that has track to collapse
	 * @param InTrack Track for layers to collapse
	 * @param InSettings Settings that determine how to collapse
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool CollapseControlRigAnimLayersWithSettings(ULevelSequence* InSequence, UMovieSceneControlRigParameterTrack* InTrack, const FBakingAnimationKeySettings& InSettings);

	/*
	 * Get the default parent key, can be used a parent space.
	 *
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FRigElementKey GetDefaultParentKey();

	/*
	 * Get the default world space key, can be used a world space.
	 *
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FRigElementKey GetWorldSpaceReferenceKey();

	/*
	 * Set the a key for the Control Rig Space for the Control at the specified time. If space is the same as the current no key witll be set.
	 *
	 * @param InSequence Sequence to set the space
	 * @param InControlRig ControlRig with the Control
	 * @param InControlName The name of the Control
	 * @param InSpaceKey  The new space for the Control
	 * @param InTime Time to change the space.
	 * @param TimeUnit Unit for the InTime, either in display rate or tick resolution
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool SetControlRigSpace(ULevelSequence* InSequence, UControlRig* InControlRig, FName InControlName, const FRigElementKey& InSpaceKey, FFrameNumber InTime,  EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/** Bake specified Control Rig Controls to a specified Space based upon the current settings
	* @param InSequence Sequence to bake
	* @param InControlRig ControlRig to bake
	* @param InControlNames The name of the Controls to bake
	* @param InSettings  The settings for the bake, e.g, how long to bake, to key reduce etc.
	* @param TimeUnit Unit for the start and end times in the InSettings parameter.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool BakeControlRigSpace(ULevelSequence* InSequence, UControlRig* InControlRig, const TArray<FName>& InControlNames, FRigSpacePickerBakeSettings InSettings, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);
	
	/** Delete the Control Rig Space Key for the Control at the specified time. This will delete any attached Control Rig keys at this time and will perform any needed compensation to the new space.
	*
	* @param InSequence Sequence to set the space
	* @param InControlRig ControlRig with the Control
	* @param InControlName The name of the Control
	* @param InTime Time to delete the space.
	* @param TimeUnit Unit for the InTime, either in display rate or tick resolution
	* @return Will return false if function fails,  for example if there is no key at this time it will fail.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool DeleteControlRigSpace(ULevelSequence* InSequence, UControlRig* InControlRig, FName InControlName, FFrameNumber InTime, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/** Move the Control Rig Space Key for the Control at the specified time to the new time. This will also move any Control Rig keys at this space switch boundary.
	*
	* @param InSequence Sequence to set the space
	* @param InControlRig ControlRig with the Control
	* @param InControlName The name of the Control
	* @param InTime Original time of the space key
	* @param InNewTime New time for the space key
	* @param TimeUnit Unit for the time params, either in display rate or tick resolution
	* @return Will return false if function fails, for example if there is no key at this time it will fail, or if the new time is invalid it could fail also
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool MoveControlRigSpace(ULevelSequence* InSequence, UControlRig* InControlRig, FName InControlName, FFrameNumber InTime, FFrameNumber InNewTime, EMovieSceneTimeUnit TimeUnit = EMovieSceneTimeUnit::DisplayRate);

	/** Rename the Control Rig Channels in Sequencer to the specified new control names, which should be present on the Control Rig
	* @param InSequence Sequence to rename controls
	* @param InControlRig ControlRig to rename controls
	* @param InOldControlNames The name of the old Control Rig Control Channels to change. Will be replaced by the corresponding name in the InNewControlNames array
	* @param InNewControlNames  The name of the new Control Rig Channels 
	* @return Return true if the function succeeds, false if it doesn't which can happen if the name arrays don't match in size or any of the new Control Names aren't valid
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool RenameControlRigControlChannels(ULevelSequence* InSequence, UControlRig* InControlRig, const TArray<FName>& InOldControlNames, const TArray<FName>& InNewControlNames);

	/** Get the controls mask for the given ControlName */
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool GetControlsMask(UMovieSceneSection* InSection, FName ControlName);

	/** Set the controls mask for the given ControlNames */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetControlsMask(UMovieSceneSection* InSection, const TArray<FName>& ControlNames, bool bVisible);

	/** Shows all of the controls for the given section */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void ShowAllControls(UMovieSceneSection* InSection);

	/** Hides all of the controls for the given section */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void HideAllControls(UMovieSceneSection* InSection);

	/** Set Control Rig priority order */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetControlRigPriorityOrder(UMovieSceneTrack* InSection,int32 PriorityOrder);

	/** Get Control Rig prirority order */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static int32 GetControlRigPriorityOrder(UMovieSceneTrack* InSection);

	/**	Whether or not the control rig is an FK Control Rig.
	@param InControlRig Rig to test to see if FK Control Rig
	**/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool IsFKControlRig(UControlRig* InControlRig);

	/**	Whether or not the control rig is an Layered Control Rig.
	@param InControlRig Rig to test to see if Layered Control Rig
	**/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool IsLayeredControlRig(UControlRig* InControlRig);

	/*
	 * Convert the control rig track into absolute or layered rig
	 *
	 * @param InTrack Control rig track to convert 
	 * @param bSetIsLayered Convert to layered rig if true, or absolute if false
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool SetControlRigLayeredMode(UMovieSceneControlRigParameterTrack* InTrack, bool bSetIsLayered);

	/**	Get FKControlRig Apply Mode.
	@param InControlRig Rig to test
	@return The EControlRigFKRigExecuteMode mode it is in, either Replace,Additive or Direct
	**/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static EControlRigFKRigExecuteMode  GetFKControlRigApplyMode(UControlRig* InControlRig);

	/**	Set the FK Control Rig to apply mode
	@param InControlRig Rig to set 
	@param InApplyMode Set the EControlRigFKRigExecuteMode mode (Replace,Addtiive or Direct)
	@return returns True if the mode was set, may not be set if the Control Rig doesn't support these modes currently only FKControlRig's do.
	**/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool SetControlRigApplyMode(UControlRig* InControlRig, EControlRigFKRigExecuteMode InApplyMode);

};

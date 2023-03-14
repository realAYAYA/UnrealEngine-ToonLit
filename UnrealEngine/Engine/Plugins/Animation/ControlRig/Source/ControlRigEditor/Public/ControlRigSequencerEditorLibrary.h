// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * Control Rig Sequencer Exposure
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneBindingProxy.h"
#include "ControlRig.h"
#include "Tools/ControlRigSnapper.h"
#include "TransformNoScale.h"
#include "EulerTransform.h"
#include "Tools/ControlRigSnapSettings.h"
#include "SequenceTimeUnit.h"
#include "RigSpacePickerBakeSettings.h"
#include "ControlRigSequencerEditorLibrary.generated.h"

class ULevelSequence;
class UTickableConstraint;

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
	static UMovieSceneTrack* FindOrCreateControlRigTrack(UWorld* World, ULevelSequence* LevelSequence, const UClass* ControlRigClass, const FMovieSceneBindingProxy& InBinding);

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

	* @return returns True if successful, False otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool LoadAnimSequenceIntoControlRigSection(UMovieSceneSection* MovieSceneSection, UAnimSequence* AnimSequence, USkeletalMeshComponent* SkelMeshComp,
		FFrameNumber InStartFrame, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate, bool bKeyReduce = false, float Tolerance = 0.001f);

	/**
	* Bake the current animation in the binding to a Control Rig track
	* @param World The active world
	* @param LevelSequence The LevelSequence we are baking
	* @param ControlRigClass The class of the Control Rig
	* @param ExportOptions Export options for creating an animation sequence
	* @param bKeyReduce If true do key reduction based upon Tolerance, if false don't
	* @param Tolerance If reducing keys, tolerance about which keys will be removed, smaller tolerance, more keys usually.
	* @param Binding The binding upon which to bake
	* @return returns True if successful, False otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool BakeToControlRig(UWorld* World, ULevelSequence* LevelSequence, UClass* ControlRigClass, UAnimSeqExportOption* ExportOptions, bool bReduceKeys, float Tolerance,
			const FMovieSceneBindingProxy& Binding);

	/**
	* Bake the constraint to keys based on the passed in frames. This will use the open sequencer to bake. See ConstraintsScriptingLibrary to get the list of available constraints
	* @param World The active world
	* @param Constraint The Constraint to bake. After baking it will be keyed to be inactive of the range of frames that are baked
	* @param Frames The frames to bake, if the array is empty it will use the active time ranges of the constraint to deteremine where it should bake
	* @param TimeUnit Unit for all frame and time values, either in display rate or tick resolution
	* @return returns True if successful, False otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool BakeConstraint(UWorld* World, UTickableConstraint* Constraint, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Peform a Tween operation on the current active sequencer time(must be visible).
	* @param LevelSequence The LevelSequence that's loaded in the editor
	* @param ControlRig The Control Rig to tween.
	* @param TweenValue The tween value to use, range from -1(blend to previous) to 1(blend to next)
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool TweenControlRig(ULevelSequence* LevelSequence, UControlRig* ControlRig, float TweenValue);

	/**
	* Peform a Snap operation to snap the children to the parent. 
	* @param LevelSequence Active Sequence to snap
	* @param StartFrame Beginning of the snap
	* @param EndFrame End of the snap
	* @param ChildrenToSnap The children objects that snap and get keys set onto. They need to live in an active Sequencer in the level editor
	* @param ParentToSnap The parent object to snap relative to. If animated, it needs to live in an active Sequencer in the level editor
	* @param SnapSettings Settings to use
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param Returns True if successful, 
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool SnapControlRig(ULevelSequence* LevelSequence, FFrameNumber StartFrame, FFrameNumber EndFrame, const FControlRigSnapperSelection& ChildrenToSnap,
		const FControlRigSnapperSelection& ParentToSnap, const UControlRigSnapSettings* SnapSettings, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);
	
	/**
	* Get Actors World Transform at a specific time
	* @param LevelSequence Active Sequence to get transform for
	* @param Actor The actor
	* @param Frame Time to get the transform
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns World Transform
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static FTransform GetActorWorldTransform(ULevelSequence* LevelSequence,AActor* Actor, FFrameNumber Frame, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Get Actors World Transforms at specific times
	* @param LevelSequence Active Sequence to get transform for
	* @param Actor The actor
	* @param Frames Times to get the transform
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @return Returns World Transforms
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static TArray<FTransform> GetActorWorldTransforms(ULevelSequence* LevelSequence, AActor* Actor, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate,FName ReferenceName = NAME_None);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate,FName ReferenceName = NAME_None);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's World Transform at a specific time
	* @param LevelSequence Active Sequence to set transforms for. Must be loaded in Level Editor.
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control
	* @param Frame Time to set the transform
	* @oaram WorldTransform World Transform to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey Whether or not to set a key.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetControlRigWorldTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, const FTransform& WorldTransform,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's World Transforms at a specific times.
	* @param LevelSequence Active Sequence to set transforms for. Must be loaded in Level Editor.
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control
	* @param Frames Times to set the transform
	* @oaram WorldTransform World Transform to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetControlRigWorldTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, 
		 const TArray<FTransform>& WorldTransforms, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);
	
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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's float value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a float control
	* @param Frame Time to set the value
	* @param Value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigFloat(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, float Value,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate,bool bSetKey = true);

	/**
	* Set ControlRig Control's float values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a float control
	* @param Frames Times to set the values
	* @param Values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigFloats(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<float> Values,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's bool value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a bool control
	* @param Frame Time to set the value
	* @param Value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigBool(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, bool Value,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's bool values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a bool control
	* @param Frames Times to set the values
	* @param Values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigBools(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
		const TArray<bool> Values, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's int value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a int control
	* @param Frame Time to set the value
	* @param Value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigInt(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, int32 Value,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate,bool bSetKey = true);


	/**
	* Set ControlRig Control's int values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a int control
	* @param Frames Times to set the values
	* @param Values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigInts(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<int32> Values,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's Vector2D value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Vector2D control
	* @param Frame Time to set the value
	* @param Value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigVector2D(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector2D Value,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate,bool bSetKey = true);


	/**
	* Set ControlRig Control's Vector2D values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Vector2D control
	* @param Frames Times to set the values
	* @param Values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigVector2Ds(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FVector2D> Values,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's Position value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Position control
	* @param Frame Time to set the value
	* @param Value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigPosition(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector Value,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's Position values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Position control
	* @param Frames Times to set the values
	* @param Values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigPositions(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FVector> Values,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's Rotator value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Rotator control
	* @param Frame Time to set the value
	* @param Value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigRotator(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FRotator Value, 
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's Rotator values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Rotator control
	* @param Frames Times to set the values
	* @param Values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigRotators(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FRotator> Values,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's Scale value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Scale control
	* @param Frame Time to set the value
	* @param Value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector Value, 
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's Scale values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Scale control
	* @param Frames Times to set the values
	* @param Values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FVector> Values,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);


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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's EulerTransform value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a EulerTransform control
	* @param Frame Time to set the value
	* @param Value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigEulerTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FEulerTransform Value,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's EulerTransform values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a EulerTransform control
	* @param Frames Times to set the values
	* @param Values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigEulerTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FEulerTransform> Values,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's TransformNoScale value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a TransformNoScale control
	* @param Frame Time to set the value
	* @param Value to set
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigTransformNoScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FTransformNoScale Value, 
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's TransformNoScale values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a TransformNoScale control
	* @param Frames Times to set the values
	* @param Values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigTransformNoScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FTransformNoScale> Values,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/**
	* Set ControlRig Control's Transform value at specific time
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Transform control
	* @param Frame Time to set the value
	* @param Value to set 
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	* @param bSetKey If True set a key, if not just set the value
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FTransform Value, 
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate, bool bSetKey = true);

	/**
	* Set ControlRig Control's Transform values at specific times
	* @param LevelSequence Active Sequence to set value on
	* @param ControlRig The ControlRig
	* @param ControlName Name of the Control, should be a Transform control
	* @param Frames Times to set the values
	* @param Values to set at those times
	* @param TimeUnit Unit for frame values, either in display rate or tick resolution
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static void SetLocalControlRigTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FTransform> Values,
		ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
	static bool SetControlRigSpace(ULevelSequence* InSequence, UControlRig* InControlRig, FName InControlName, const FRigElementKey& InSpaceKey, FFrameNumber InTime,  ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

	/** Bake specified Control Rig Controls to a specified Space based upon the current settings
	* @param InSequence Sequence to bake
	* @param InControlRig ControlRig to bake
	* @param InControlNames The name of the Controls to bake
	* @param InSettings  The settings for the bake, e.g, how long to bake, to key reduce etc.
	* @param TimeUnit Unit for the start and end times in the InSettings parameter.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	static bool BakeControlRigSpace(ULevelSequence* InSequence, UControlRig* InControlRig, const TArray<FName>& InControlNames, FRigSpacePickerBakeSettings InSettings, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);
	
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
	static bool DeleteControlRigSpace(ULevelSequence* InSequence, UControlRig* InControlRig, FName InControlName, FFrameNumber InTime, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);

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
	static bool MoveControlRigSpace(ULevelSequence* InSequence, UControlRig* InControlRig, FName InControlName, FFrameNumber InTime, FFrameNumber InNewTime, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate);



	/** Rename the Control Rig Channels in Sequencer to the specified new control names, which should be present on the Control Rig
	* @param InSequence Sequence to rename controls
	* @param InControlRig ControlRig to rename controls
	* @param InOldControlNames The name of the old Control Rig Control Channels to change. Will be replaced by the corresponding name in the InNewControlNames array
	* @param InNewControlNames  The name of the new Control Rig Channels 
	* @return Return true if the function succeeds, false if it doesn't which can happen if the name arrays don't match in size or any of the new Control Names aren't valid
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig")
	bool RenameControlRigControlChannels(ULevelSequence* InSequence, UControlRig* InControlRig, const TArray<FName>& InOldControlNames, const TArray<FName>& InNewControlNames);

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
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TrailHierarchy.h"
#include "ISequencer.h"
#include "Rigs/RigHierarchyDefines.h"
#include "UObject/GCObject.h"


class ISequencer;
class USceneComponent;
class USkeletalMeshComponent;
class USkeleton;
class UMovieSceneSection;
class UMovieScene3DTransformTrack;
class UMovieSceneControlRigParameterTrack;
class UMovieSceneControlRigParameterSection;
class UControlRig;
struct FRigHierarchyContainer;

namespace UE
{
namespace SequencerAnimTools
{

enum class EBindingVisibilityState
{
	AlwaysVisible,
	VisibleWhenSelected
};

class FSequencerTrailHierarchy : public FTrailHierarchy
{
public:
	FSequencerTrailHierarchy(TWeakPtr<ISequencer> InWeakSequencer)
		: FTrailHierarchy()
		, WeakSequencer(InWeakSequencer)
		, ObjectsTracked()
		, ControlsTracked()
		, HierarchyRenderer(MakeUnique<FTrailHierarchyRenderer>(this))
		, OnActorAddedToSequencerHandle()
		, OnSelectionChangedHandle()
		, OnViewOptionsChangedHandle()
		, ControlRigDelegateHandles()
	{
	}

	// FTrailHierarchy interface
	virtual void Initialize() override;
	virtual void Destroy() override;
	virtual ITrailHierarchyRenderer* GetRenderer() const override { return HierarchyRenderer.Get(); }
	virtual double GetSecondsPerFrame() const override { return 1.0 / WeakSequencer.Pin()->GetFocusedDisplayRate().AsDecimal(); }
	virtual double GetSecondsPerSegment() const override;
	virtual FFrameNumber GetFramesPerFrame() const override;
	virtual FFrameNumber GetFramesPerSegment() const override;

	virtual void RemoveTrail(const FGuid& Key) override;
	virtual void Update() override;
	// End FTrailHierarchy interface

	const TMap<UObject*, FGuid>& GetObjectsTracked() const { return ObjectsTracked; }
	const TMap<USkeletalMeshComponent*, TMap<FName, FGuid>>& GetBonesTracked() const { return BonesTracked; }
	const TMap<UControlRig*, TMap<FName, FGuid>>& GetControlsTracked() const { return ControlsTracked; }

	void OnBoneVisibilityChanged(USkeleton* Skeleton, const FName& BoneName, const bool bIsVisible);
	void OnBindingVisibilityStateChanged(UObject* BoundObject, const EBindingVisibilityState VisibilityState);
	//called when added moved, deleted, added, all of which may need a refresh of a motion trail
	void OnActorChangedSomehow(AActor* InActor);
	void OnActorsChangedSomehow(TArray<AActor*>& InActors);	

private:
	void AddControlRigTrail(USkeletalMeshComponent* Component,UControlRig* ControlRig, UMovieSceneControlRigParameterTrack* CRTrack, const FName& ControlName);

	void UpdateControlRig(const TArray<FFrameNumber>& Frames, UControlRig* ControlRig, TMap<FName, FGuid >& CompMapPair, bool bUseEditedTimes);
	void UpdateControlRig(const FTrailEvaluateTimes& EvalTimes, UControlRig* ControlRig, TMap<FName, FGuid >& CompMapPair);

	void UpdateSequencerBindings(const TArray<FGuid>& SequencerBindings, TFunctionRef<void(UObject*, FTrail*, FGuid)> OnUpdated);
	void UpdateViewAndEvalRange();

	void AddComponentToHierarchy(USceneComponent* CompToAdd, UMovieScene3DTransformTrack* TransformTrack);
	void AddSkeletonToHierarchy(USkeletalMeshComponent* CompToAdd);

	void RegisterControlRigDelegates(USkeletalMeshComponent* Component, UMovieSceneControlRigParameterTrack* CRParameterTrack);

	TWeakPtr<ISequencer> WeakSequencer;
	TMap<UObject*, FGuid> ObjectsTracked;
	TMap<USkeletalMeshComponent*, TMap<FName, FGuid>> BonesTracked; 
	TMap<UControlRig*, TMap<FName, FGuid>> ControlsTracked; 

	// TODO: components can have multiple rigs so make this a map from sections to controls instead. However, this is only part of a larger problem of handling blending

	TUniquePtr<FTrailHierarchyRenderer> HierarchyRenderer;

	FDelegateHandle OnActorAddedToSequencerHandle;
	FDelegateHandle OnSelectionChangedHandle;
	FDelegateHandle OnViewOptionsChangedHandle;

	struct FControlRigDelegateHandles
	{
		FDelegateHandle OnHierarchyModified;
		FDelegateHandle OnControlSelected;
	};
	TMap<UMovieSceneControlRigParameterTrack*, FControlRigDelegateHandles> ControlRigDelegateHandles;
};

} // namespace MovieScene
} // namespace UE

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trail.h"
#include "MotionTrailMovieSceneKey.h"
#include "TrajectoryDrawInfo.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"

class ISequencer;
class UMovieScene3DTransformTrack;
class UMovieScene3DTransformSection;
class UMovieSceneTrack;
class USceneComponent;

namespace UE
{
namespace SequencerAnimTools
{


class FMovieSceneTransformTrail : public FTrail
{
public:
	FMovieSceneTransformTrail(USceneComponent* SceneComponent,const FLinearColor& InColor, const bool bInIsVisible, TWeakObjectPtr<UMovieSceneTrack> InWeakTrack, TSharedPtr<ISequencer> InSequencer);
	// Begin FTrail interface
	virtual ETrailCacheState UpdateTrail(const FSceneContext& InSceneContext) override;
	virtual FTrajectoryCache* GetTrajectoryTransforms() override { return TrajectoryCache.Get(); }
	virtual TRange<double> GetEffectiveRange() const override { return CachedEffectiveRange; }
	virtual void Render(const FGuid& Guid, const FSceneView* View,  FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(const FSceneView* View, FCanvas* Canvas) override;

	virtual bool HandleClick(const FGuid& Guid, FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, FInputClick Click) override;
	virtual bool IsAnythingSelected() const override;
	virtual bool IsAnythingSelected(FVector& OutVectorPosition)const override;
	virtual bool IsTrailSelected() const override;
	virtual bool StartTracking() override;
	virtual bool ApplyDelta(const FVector& Pos, const FRotator& Rot, const FVector& WidgetLocation) override;
	virtual bool EndTracking() override;
	virtual void TranslateSelectedKeys(bool bRight) override;
	virtual void DeleteSelectedKeys() override;
	virtual void SelectNone() override;
	virtual int32 GetChannelOffset()const { return 0; }
	virtual bool GetEditedTimes(const FTrailHierarchy* TrailHierarchy,const FFrameNumber& LastFrame, TArray<FFrameNumber>& OutEditedTimes);
	virtual void UpdateKeysInRange(const TRange<double>& ViewRange) override;
	virtual TArray<FFrameNumber> GetKeyTimes() const override;
	virtual TArray<FFrameNumber> GetSelectedKeyTimes() const override;
	virtual void ForceEvaluateNextTick() override;
	// End FTrail interface

	TSharedPtr<ISequencer> GetSequencer() const { return WeakSequencer.Pin(); }
	FGuid GetCachedHierarchyGuid() const { return CachedHierarchyGuid; }
	UMovieSceneTrack* GetTrack() const { return WeakTrack.Get(); }
	UMovieSceneSection* GetSection() const;

public:
	TWeakPtr<ISequencer> WeakSequencer;
	// Begin FMovieSceneTransformTrail interface
	virtual void UpdateCacheTimes(const FSceneContext& InSceneContext) = 0;
protected:
	virtual bool HandleAltClick(FEditorViewportClient* InViewportClient, HMotionTrailProxy* Proxy, FInputClick Click);
	// End FMovieSceneTransformTrail interface

	// when editing interactively the tool may specify just a specify range to recalculate
	void CalculateEditedTimes(const FTrailHierarchy* TrailHierarchy, const FFrameNumber& LastFrame);
	void ClearEditedTiimes() { EditedTimes.Reset(); }

	IMovieScenePlayer* MovieScenePlayer;
	TRange<double> GetEffectiveSectionRange(int32 ChannelOffset) const;
	TRange<double> CachedEffectiveRange;

	bool bIsSelected;
	FVector SelectedPos;

	TUniquePtr<FMotionTraiMovieScenelKeyTool> KeyTool;
	TUniquePtr<FArrayTrajectoryCache> TrajectoryCache;

	FGuid LastTransformSectionSig;
	FGuid CachedHierarchyGuid;
	TWeakObjectPtr<UMovieSceneTrack> WeakTrack;

	TOptional<TArray<FFrameNumber>> EditedTimes;
};


class FMovieSceneComponentTransformTrail : public FMovieSceneTransformTrail
{
public:

	FMovieSceneComponentTransformTrail(USceneComponent* InComponent, const FLinearColor& InColor, const bool bInIsVisible, TWeakObjectPtr<UMovieSceneTrack> InWeakTrack, TSharedPtr<ISequencer> InSequencer)
		: FMovieSceneTransformTrail(InComponent, InColor, bInIsVisible, InWeakTrack, InSequencer),
		Component(InComponent)
		
	{
	}


private:
	TWeakObjectPtr<USceneComponent> Component;
	// Begin FMovieSceneTransformTrail interface
	virtual void UpdateCacheTimes(const FSceneContext& InSceneContext) override;
	virtual bool StartTracking() override;
	virtual bool ApplyDelta(const FVector& Pos, const FRotator& Rot, const FVector& WidgetLocation) override;
	virtual bool EndTracking() override;

protected:
	virtual bool HandleAltClick(FEditorViewportClient* InViewportClient, HMotionTrailProxy* Proxy, FInputClick Click) override;

	// End FMovieSceneTransformTrail interface

};


class FMovieSceneControlRigTransformTrail : public FMovieSceneTransformTrail
{
public:

	FMovieSceneControlRigTransformTrail(USceneComponent* SceneComponent, const FLinearColor& InColor, const bool bInIsVisible, TWeakObjectPtr<UMovieSceneTrack> InWeakTrack, TSharedPtr<ISequencer> InSequencer, const FName& InControlName);

	virtual void UpdateCacheTimes(const FSceneContext& InSceneContext) override;
	virtual bool StartTracking() override;
	virtual bool ApplyDelta(const FVector& Pos, const FRotator& Rot, const FVector& WidgetLocation) override;
	virtual bool EndTracking() override;
	virtual int32 GetChannelOffset() const override;
	virtual void GetTrajectoryPointsForDisplay(const FDisplayContext& InDisplayContext, TArray<FVector>& OutPoints, TArray<double>& OutSeconds) override;
	virtual void GetTickPointsForDisplay(const FDisplayContext& InDisplayContext, TArray<FVector2D>& OutTicks, TArray<FVector2D>& OutTickTangents) override;

	void SetUseKeysForTrajectory(bool bVal);
protected:
	virtual bool HandleAltClick(FEditorViewportClient* InViewportClient, HMotionTrailProxy* Proxy, FInputClick Click) override;

private:
	bool bUseKeysForTrajectory = false; //on when interatively moving
	FName ControlName;
};

} // namespace MovieScene
} // namespace UE

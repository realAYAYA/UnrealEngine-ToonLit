// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trail.h"
#include "EditorViewportClient.h"
#include "Widgets/Input/SCheckBox.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include "Misc/Timespan.h"


namespace UE
{
namespace SequencerAnimTools
{


struct FTrailVisibilityManager
{
	bool IsTrailVisible(const FGuid& Guid, const FTrail* Trail) const
	{
		return !InactiveMask.Contains(Guid) && !VisibilityMask.Contains(Guid) && (AlwaysVisible.Contains(Guid) || Selected.Contains(Guid) || ControlSelected.Contains(Guid)
			|| Trail->IsAnythingSelected()) && Guid.IsValid();
	}

	bool IsTrailAlwaysVisible(const FGuid& Guid) const
	{
		return AlwaysVisible.Contains(Guid);
	}
	void SetTrailAlwaysVisible(const FGuid& Guid, bool bSet)
	{
		if (bSet)
		{
			AlwaysVisible.Add(Guid);
		}
		else
		{
			AlwaysVisible.Remove(Guid);
		}
	}


	TSet<FGuid> InactiveMask; // Any trails whose cache state or parent's cache state has been marked as NotUpdated
	TSet<FGuid> VisibilityMask; // Any trails masked out by the user interface, ex bone trails
	TSet<FGuid> AlwaysVisible; // Any trails pinned by the user interface
	TSet<FGuid> Selected; // Any transform or bone trails selected in the user interface
	TSet<FGuid> ControlSelected;// Any control rig trails selected
};

class ITrailHierarchyRenderer
{
public:
	virtual ~ITrailHierarchyRenderer() {}
	virtual void Render(const FSceneView* View, FPrimitiveDrawInterface* PDI) = 0;
	virtual void DrawHUD(const FSceneView* View, FCanvas* Canvas) = 0;
};

class FTrailHierarchyRenderer : public ITrailHierarchyRenderer
{
public:
	FTrailHierarchyRenderer(class FTrailHierarchy* InOwningHierarchy)
		: OwningHierarchy(InOwningHierarchy)
	{}

	virtual void Render(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(const FSceneView* View, FCanvas* Canvas) override;

private:
	class FTrailHierarchy* OwningHierarchy;
};

class FTrailHierarchy
{
public:

	FTrailHierarchy()
		: ViewRange(TRange<double>::All())
		, EvalRange(TRange<double>::All())
		, SecondsPerSegment(0.1)
		, LastEvalRange(TRange<double>::All())
		, LastSecondsPerSegment(0.1)
		, AllTrails()
		, TimingStats()
		, VisibilityManager()
	{}

	virtual ~FTrailHierarchy() {}

	virtual void Initialize() = 0;
	virtual void Destroy() = 0; // TODO: make dtor?
	virtual ITrailHierarchyRenderer* GetRenderer() const = 0;
	virtual double GetSecondsPerFrame() const = 0;
	virtual double GetSecondsPerSegment() const = 0;
	virtual FFrameNumber GetFramesPerFrame() const = 0;
	virtual FFrameNumber GetFramesPerSegment() const = 0;

	// Optionally implemented methods
	virtual void CalculateEvalRangeArray();
	virtual void Update();
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, FInputClick Click);
	virtual bool IsHitByClick(HHitProxy* HitProx);
	virtual bool BoxSelect(FBox& InBox, bool InSelect = true);
	virtual bool FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect = true) ;

	//Get calculated center of all selections into one position, return false if not selected
	virtual bool IsAnythingSelected(FVector& OutVectorPosition)const;
	//Get each selected item's position, return false if not selected.
	virtual bool IsAnythingSelected(TArray<FVector>& OutVectorPositions) const;
	virtual bool IsAnythingSelected() const;
	virtual void SelectNone();
	virtual bool IsSelected(const FGuid& Key) const;
	virtual bool IsAlwaysVisible(const FGuid Key) const;

	virtual void AddTrail(const FGuid& Key, TUniquePtr<FTrail>&& TrailPtr);
	virtual void RemoveTrail(const FGuid& Key);

	virtual bool StartTracking();
	virtual bool ApplyDelta(const FVector& Pos, const FRotator& Rot, const FVector& WidgetLocation);
	virtual bool EndTracking();

	virtual void TranslateSelectedKeys(bool bRight);
	virtual void DeleteSelectedKeys();

	const TRange<double>& GetViewRange() const { return ViewRange; }
	const TMap<FGuid, TUniquePtr<FTrail>>& GetAllTrails() const { return AllTrails; }

	const TMap<FString, FTimespan>& GetTimingStats() const { return TimingStats; };
	TMap<FString, FTimespan>& GetTimingStats() { return TimingStats; }

	FTrailVisibilityManager& GetVisibilityManager() { return VisibilityManager; }

protected:
	void RemoveTrailIfNotAlwaysVisible(const FGuid& Key);

	void OpenContextMenu(const FGuid& TrailGuid);
protected:


	TRange<double> ViewRange;
	TRange<double> EvalRange;
	TArray<double> EvalTimesArr;
	double SecondsPerSegment;

	TRange<double> LastEvalRange;
	double LastSecondsPerSegment;

	TMap<FGuid, TUniquePtr<FTrail>> AllTrails;

	TMap<FString, FTimespan> TimingStats;

	FTrailVisibilityManager VisibilityManager;
};

} // namespace MovieScene
} // namespace UE

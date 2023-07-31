// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "Containers/Map.h"
#include "Containers/Array.h"
#include "Math/Range.h"
#include "Math/Color.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "GameFramework/Actor.h"

#include "TrajectoryCache.h"
#include "TrajectoryDrawInfo.h"
#include "HitProxies.h"
#include "Misc/Guid.h"

class FEditorViewportClient;
class FPrimitiveDrawInterface;
class UMotionTrailEditorMode;

namespace UE
{
namespace SequencerAnimTools
{
struct FInputClick
{
	FInputClick() {};
	FInputClick(bool bA, bool bC, bool bS) : bAltIsDown(bA), bCtrlIsDown(bC), bShiftIsDown(bS) {};
	bool bAltIsDown = false; 
	bool bCtrlIsDown = false;
	bool bShiftIsDown = false;
	bool bIsRightMouse = false;
};
struct HBaseTrailProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	FGuid Guid;
	HBaseTrailProxy(const FGuid& InGuid, EHitProxyPriority InPriority = HPP_Foreground) 
		:HHitProxy(InPriority)
		,Guid(InGuid)
	{}
};
struct HMotionTrailProxy : public HBaseTrailProxy
{
	DECLARE_HIT_PROXY();

	FVector Point;
	double Second;

	HMotionTrailProxy(const FGuid& InGuid, const FVector& InPoint, double InSecond) 
		:HBaseTrailProxy(InGuid)
		,Point(InPoint)
		,Second(InSecond)
	{}

};

enum class ETrailCacheState : uint8
{
	UpToDate = 2,
	Stale = 1,
	Dead = 0,
	NotUpdated = 3
};

class FTrail
{
public:

	FTrail(UObject* InOwner)
		: Owner(InOwner)
		,CacheState(ETrailCacheState::Stale)
		, bForceEvaluateNextTick(true)
		, DrawInfo(nullptr)
	{}

	virtual ~FTrail() {}

	// Public interface methods
	struct FSceneContext
	{
		FGuid YourNode;
		const class FTrailEvaluateTimes& EvalTimes;
		class FTrailHierarchy* TrailHierarchy;
	};

	UObject* GetOwner() const
	{ 
		return (Owner.IsValid() ? Owner.Get() : nullptr);
	}

	virtual ETrailCacheState UpdateTrail(const FSceneContext& InSceneContext) = 0;
	virtual FTrajectoryCache* GetTrajectoryTransforms() = 0;

	//Additional Render/HitTest event handling for specific trails, usually let default renderer handle it
	virtual void Render(const FGuid& Guid, const FSceneView* View, FPrimitiveDrawInterface* PDI) {};
	virtual void DrawHUD(const FSceneView* View, FCanvas* Canvas) {};
	virtual bool HandleClick(const FGuid& Guid, FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, FInputClick Click) { return false; }
	virtual bool IsAnythingSelected() const { return false; }
	virtual bool IsAnythingSelected(FVector& OutVectorPosition)const { return false;}
	virtual bool IsTrailSelected() const { return false; }
	virtual bool BoxSelect(FBox& InBox, bool InSelect = true) {return false;}
	virtual bool FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect = true)  { return false; }

	virtual bool StartTracking() { return false; }
	virtual bool ApplyDelta(const FVector& Pos, const FRotator& Rot, const FVector& WidgetLocation) { return false; }
	virtual bool EndTracking() { return false; }
	virtual void TranslateSelectedKeys(bool bRight) {};
	virtual void DeleteSelectedKeys() {};
	virtual void SelectNone() {};
	virtual bool GetEditedTimes(const FTrailHierarchy* TrailHierarchy, const FFrameNumber& LastFrame, TArray<FFrameNumber>& OutEditedTimes) { return false; }
	virtual void UpdateKeysInRange(const TRange<double>& ViewRange) {};
	// Optionally implemented methods
	virtual TRange<double> GetEffectiveRange() const { return TRange<double>::Empty(); }
	virtual TArray<FFrameNumber> GetKeyTimes() const { return TArray<FFrameNumber>(); }
	virtual TArray<FFrameNumber> GetSelectedKeyTimes() const { return TArray<FFrameNumber>(); }

	virtual void GetTrajectoryPointsForDisplay(const FDisplayContext& InDisplayContext, TArray<FVector>& OutPoints, TArray<double>& OutSeconds)
	{
		if (FTrajectoryDrawInfo* DI = GetDrawInfo())
		{
			DI->GetTrajectoryPointsForDisplay(InDisplayContext, OutPoints, OutSeconds);
		}
	}
	virtual void GetTickPointsForDisplay(const FDisplayContext& InDisplayContext, TArray<FVector2D>& OutTicks, TArray<FVector2D>& OutTickTangents)
	{
		if (FTrajectoryDrawInfo* DI = GetDrawInfo())
		{
			DI->GetTickPointsForDisplay(InDisplayContext, OutTicks, OutTickTangents);
		}
	}

	virtual void ForceEvaluateNextTick() { bForceEvaluateNextTick = true; }


	FTrajectoryDrawInfo* GetDrawInfo() { return DrawInfo.Get(); }

	ETrailCacheState GetCacheState() const { return CacheState; }
protected:

	TWeakObjectPtr<UObject> Owner;
	ETrailCacheState CacheState;

	bool bForceEvaluateNextTick;
	TUniquePtr<FTrajectoryDrawInfo> DrawInfo;
};
} // namespace MovieScene
} // namespace UE

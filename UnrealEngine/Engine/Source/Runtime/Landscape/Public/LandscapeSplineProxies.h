// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "EngineUtils.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"

class ULandscapeSplineSegment;

//////////////////////////////////////////////////////////////////////////
// LANDSCAPE SPLINES HIT PROXY

struct HLandscapeSplineProxy : public HActor
{
	DECLARE_HIT_PROXY( LANDSCAPE_API );

	HLandscapeSplineProxy(ULandscapeSplinesComponent* SplineComponent, EHitProxyPriority InPriority = HPP_Wireframe) :
		HActor(SplineComponent->GetOwner(), SplineComponent, InPriority)
	{
	}
	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

struct HLandscapeSplineProxy_Segment : public HLandscapeSplineProxy
{
	DECLARE_HIT_PROXY( LANDSCAPE_API );

	class ULandscapeSplineSegment* SplineSegment;

	HLandscapeSplineProxy_Segment(class ULandscapeSplineSegment* InSplineSegment) :
		HLandscapeSplineProxy(InSplineSegment->GetOuterULandscapeSplinesComponent()),
		SplineSegment(InSplineSegment)
	{
	}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( SplineSegment );
	}
};

struct HLandscapeSplineProxy_ControlPoint : public HLandscapeSplineProxy
{
	DECLARE_HIT_PROXY( LANDSCAPE_API );

	class ULandscapeSplineControlPoint* ControlPoint;

	HLandscapeSplineProxy_ControlPoint(class ULandscapeSplineControlPoint* InControlPoint) :
		HLandscapeSplineProxy(InControlPoint->GetOuterULandscapeSplinesComponent(), HPP_Foreground),
		ControlPoint(InControlPoint)
	{
	}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( ControlPoint );
	}
};

struct HLandscapeSplineProxy_Tangent : public HLandscapeSplineProxy
{
	DECLARE_HIT_PROXY( LANDSCAPE_API );

	ULandscapeSplineSegment* SplineSegment;
	uint32 End:1;

	HLandscapeSplineProxy_Tangent(class ULandscapeSplineSegment* InSplineSegment, bool InEnd) :
		HLandscapeSplineProxy(InSplineSegment->GetOuterULandscapeSplinesComponent(), HPP_UI),
		SplineSegment(InSplineSegment),
		End(InEnd)
	{
	}
	LANDSCAPE_API virtual void Serialize(FArchive& Ar);

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( SplineSegment );
	}
};

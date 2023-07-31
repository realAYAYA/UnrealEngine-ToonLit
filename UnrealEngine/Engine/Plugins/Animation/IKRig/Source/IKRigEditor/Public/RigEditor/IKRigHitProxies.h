// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"

struct HIKRigEditorBoneProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	FName BoneName;

	HIKRigEditorBoneProxy(const FName& InBoneName)
		: HHitProxy(HPP_World)
		, BoneName(InBoneName) {}

	virtual EMouseCursor::Type GetMouseCursor()
	{
		return EMouseCursor::Crosshairs;
	}

	virtual bool AlwaysAllowsTranslucentPrimitives() const override
	{
		return true;
	}
};

struct HIKRigEditorGoalProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	FName GoalName;
	
	HIKRigEditorGoalProxy(const FName& InGoalName)
		: HHitProxy(HPP_World)
		, GoalName(InGoalName) {}

	virtual EMouseCursor::Type GetMouseCursor()
	{
		return EMouseCursor::Crosshairs;
	}

	virtual bool AlwaysAllowsTranslucentPrimitives() const override
	{
		return true;
	}
};
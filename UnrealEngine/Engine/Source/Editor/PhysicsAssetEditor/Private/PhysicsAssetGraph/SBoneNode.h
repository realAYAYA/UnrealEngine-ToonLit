// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsAssetGraph/SPhysicsAssetGraphNode.h"

class SBoneNode : public SPhysicsAssetGraphNode
{
public:
	SLATE_BEGIN_ARGS(SBoneNode){}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, class UPhysicsAssetGraphNode_Bone* InNode);
};

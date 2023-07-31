// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetGraph/SConstraintNode.h"

#include "PhysicsAssetGraph/PhysicsAssetGraphNode_Constraint.h"

void SConstraintNode::Construct(const FArguments& InArgs, UPhysicsAssetGraphNode_Constraint* InNode)
{
	SPhysicsAssetGraphNode::Construct(SPhysicsAssetGraphNode::FArguments(), InNode);

	UpdateGraphNode();
}

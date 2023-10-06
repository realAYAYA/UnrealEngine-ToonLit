// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimTrace.h"

#if ANIM_TRACE_ENABLED

struct FAnimationBaseContext;
struct FAnimNode_BlendSpacePlayerBase;
struct FAnimNode_BlendSpaceGraphBase;

struct FAnimGraphRuntimeTrace
{
	/** Helper function to output debug info for blendspace player nodes */
	ANIMGRAPHRUNTIME_API static void OutputBlendSpacePlayer(const FAnimationBaseContext& InContext, const FAnimNode_BlendSpacePlayerBase& InNode);

	/** Helper function to output debug info for blendspace nodes */
	ANIMGRAPHRUNTIME_API static void OutputBlendSpace(const FAnimationBaseContext& InContext, const FAnimNode_BlendSpaceGraphBase& InNode);
};

#define TRACE_BLENDSPACE_PLAYER(Context, Node) \
	FAnimGraphRuntimeTrace::OutputBlendSpacePlayer(Context, Node);

#define TRACE_BLENDSPACE(Context, Node) \
	FAnimGraphRuntimeTrace::OutputBlendSpace(Context, Node);

#else

#define TRACE_BLENDSPACE_PLAYER(Context, Node)
#define TRACE_BLENDSPACE(Context, Node)

#endif
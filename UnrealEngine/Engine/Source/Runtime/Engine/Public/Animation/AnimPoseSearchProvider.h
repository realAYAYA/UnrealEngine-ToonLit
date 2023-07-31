// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

struct FAnimationBaseContext;
class UAnimSequenceBase;

namespace UE { namespace Anim {

/** Modular feature interface for PoseSearch */
class ENGINE_API IPoseSearchProvider : public IModularFeature
{
public:
	static const FName ModularFeatureName; // "AnimPoseSearch"

	virtual ~IPoseSearchProvider() {}

	static bool IsAvailable();
	static IPoseSearchProvider* Get();

	struct FSearchResult
	{
		int32 PoseIdx = -1;
		float TimeOffsetSeconds = 0.0f;
		float Dissimilarity = MAX_flt;
	};

	/**
	* Finds a matching pose in the sequence given the current graph context
	* 
	* @param	GraphContext	Graph execution context used to construct a pose search query
	* @param	Sequence		The sequence to search for the pose query
	* 
	* @return	The pose in the sequence that most closely matches the query
	*/
	virtual FSearchResult Search(const FAnimationBaseContext& GraphContext, const UAnimSequenceBase* Sequence) = 0;
};

}} // namespace UE::Anim
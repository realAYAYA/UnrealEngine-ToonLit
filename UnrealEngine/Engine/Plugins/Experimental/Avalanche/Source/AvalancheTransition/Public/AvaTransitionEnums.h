// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionEnums.generated.h"

enum class EAvaTransitionSceneFlags : uint8
{
	None = 0,
	NeedsDiscard = 1 << 0, // Flag indicating that the Scene needs to be discarded at the end of the Transition
};
ENUM_CLASS_FLAGS(EAvaTransitionSceneFlags);

enum class EAvaTransitionIterationResult : uint8
{
	Break,
	Continue,
};

UENUM(BlueprintType)
enum class EAvaTransitionType : uint8
{
	None = 0 UMETA(Hidden),
	In   = 1 << 0,
	Out  = 1 << 1,
	MAX UMETA(Hidden),
};
ENUM_CLASS_FLAGS(EAvaTransitionType);

UENUM(BlueprintType)
enum class EAvaTransitionRunState : uint8
{
	Unknown UMETA(Hidden),
	Running,
	Finished,
};

UENUM(BlueprintType)
enum class EAvaTransitionComparisonResult : uint8
{
	None,
	Different,
	Same,
};

UENUM(BlueprintType)
enum class EAvaTransitionSceneType : uint8
{
	This,
	Other,
};

UENUM(BlueprintType)
enum class EAvaTransitionLayerCompareType : uint8
{
	None UMETA(Hidden),
	Same,
	Different UMETA(DisplayName="Other"),
	MatchingTag,
	Any,
};

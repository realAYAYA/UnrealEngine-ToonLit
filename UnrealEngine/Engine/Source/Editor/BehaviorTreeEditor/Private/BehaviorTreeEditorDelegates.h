// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

struct FBlackboardEntry;
class UBlackboardData;

namespace UE::BehaviorTreeEditor::Delegates
{
/** Delegate for when a blackboard key changes (added, removed, renamed) */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBlackboardKeyChanged, const UBlackboardData& /*InBlackboardData*/, FBlackboardEntry* const /*InKey*/);
extern FOnBlackboardKeyChanged OnBlackboardKeyChanged;
} // UE::BehaviorTreeEditor::Delegates

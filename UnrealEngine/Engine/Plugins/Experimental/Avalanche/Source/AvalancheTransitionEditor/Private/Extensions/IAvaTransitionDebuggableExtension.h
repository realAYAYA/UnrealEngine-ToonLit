// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"

#if WITH_STATETREE_DEBUGGER
struct FAvaTransitionDebugInfo;
#endif

/** Extension for  View Model that can be debugged */
class IAvaTransitionDebuggableExtension
{
public:
	UE_AVA_TYPE(IAvaTransitionDebuggableExtension)

#if WITH_STATETREE_DEBUGGER
	virtual void DebugEnter(const FAvaTransitionDebugInfo& InDebugInfo) = 0;

	virtual void DebugExit(const FAvaTransitionDebugInfo& InDebugInfo) = 0;
#endif
};

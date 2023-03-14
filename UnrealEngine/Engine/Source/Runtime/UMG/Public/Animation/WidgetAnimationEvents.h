// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"

#include "WidgetAnimationEvents.generated.h"

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWidgetAnimationPlaybackStatusChanged);

UDELEGATE()
DECLARE_DYNAMIC_DELEGATE(FWidgetAnimationDynamicEvent);

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWidgetAnimationDynamicEvents);

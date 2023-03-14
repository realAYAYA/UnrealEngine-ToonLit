// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/OnlineError.h"

#define LOCTEXT_NAMESPACE "AchievementsErrors"

namespace UE::Online::Errors {
	UE_ONLINE_ERROR_CATEGORY(Achievements, Engine, 0x5, "Achievements")
	UE_ONLINE_ERROR(Achievements, AlreadyUnlocked, 1, TEXT("AlreadyUnlocked"), LOCTEXT("AlreadyUnlocked", "That achievement is already unlocked"))

/* UE::Online::Errors */ }

#undef LOCTEXT_NAMESPACE

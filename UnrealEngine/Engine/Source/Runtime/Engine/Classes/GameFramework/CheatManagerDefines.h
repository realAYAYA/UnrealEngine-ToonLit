// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


/** If set to 0, cheat manager functionality will be entirely disabled. If cheats are desired in shipping mode this can be overridden in the target definitions */
#ifndef UE_WITH_CHEAT_MANAGER
#define UE_WITH_CHEAT_MANAGER (1 && !UE_BUILD_SHIPPING)
#endif

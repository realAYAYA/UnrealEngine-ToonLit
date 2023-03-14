// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Misc/ScopedSlowTask.h"
#include "Stats/StatsMisc.h"



#if 0
	#define PROXYLOD_DEBUG_TIMER(name_) SCOPE_LOG_TIME(name_, nullptr)
#else
	#define PROXYLOD_DEBUG_TIMER(name_) (void)name_;
#endif
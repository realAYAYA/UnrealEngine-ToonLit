// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"


struct FSwitchboardCommandLineOptions;

bool SwitchboardListenerMainInit(const FSwitchboardCommandLineOptions& InOptions);
bool SwitchboardListenerMainShutdown();

int32 SwitchboardListenerMainWrapper();

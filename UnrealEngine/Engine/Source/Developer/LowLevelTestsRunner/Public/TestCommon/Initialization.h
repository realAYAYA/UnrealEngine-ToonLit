// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

void InitAllThreadPoolsEditorEx(bool MultiThreaded);

void InitOutputDevicesEx();

void InitStats();

void InitAll(bool bAllowLogging, bool bMultithreaded);

void CleanupAll();
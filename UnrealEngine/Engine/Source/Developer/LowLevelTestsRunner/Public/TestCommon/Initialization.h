// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

void SaveDefaultPlatformFile();
void UsePlatformFileStubIfRequired();
void UseDefaultPlatformFile();

void SetProjectNameAndDirectory();

void InitAllThreadPoolsEditorEx(bool MultiThreaded);

void InitOutputDevicesEx();

void InitStats();

void InitAll(bool bAllowLogging, bool bMultithreaded);

void CleanupAll();
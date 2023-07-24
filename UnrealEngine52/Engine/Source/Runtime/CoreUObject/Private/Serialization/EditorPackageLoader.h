// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IAsyncPackageLoader;
class FIoDispatcher;

#if WITH_IOSTORE_IN_EDITOR
TUniquePtr<IAsyncPackageLoader> MakeEditorPackageLoader(FIoDispatcher& InIoDispatcher);
#endif

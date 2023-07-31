// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Widgets/SWindow.h"

DECLARE_DELEGATE_TwoParams(FBakeToControlDelegate, bool, float);

struct BakeToControlRigDialog
{
	static void GetBakeParams(FBakeToControlDelegate& Delegate, const FOnWindowClosed& OnClosedDelegate);

};
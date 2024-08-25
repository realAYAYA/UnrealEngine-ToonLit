// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangePipelineBase.h"

namespace UE::Interchange::PipelineHelper
{
	void ShowModalDialog(TSharedRef<SInterchangeBaseConflictWidget> ConflictWidget, const FText& Title, const FVector2D& WindowSize);
}

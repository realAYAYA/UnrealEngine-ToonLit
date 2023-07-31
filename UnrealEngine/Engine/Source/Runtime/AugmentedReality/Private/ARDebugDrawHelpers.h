// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"

class UWorld;
class FString;
struct FColor;

namespace ARDebugHelpers
{
	void DrawDebugString(const UWorld* InWorld, FVector const& TextLocation, const FString& Text, float Scale, FColor const& TextColor, float Duration, bool bDrawShadow);
}

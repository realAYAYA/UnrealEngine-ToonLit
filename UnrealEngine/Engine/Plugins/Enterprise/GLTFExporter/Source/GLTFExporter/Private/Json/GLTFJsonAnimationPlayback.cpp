// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonAnimationPlayback.h"

void FGLTFJsonAnimationPlayback::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	if (bLoop != true)
	{
		Writer.Write(TEXT("loop"), bLoop);
	}

	if (bAutoPlay != true)
	{
		Writer.Write(TEXT("autoPlay"), bAutoPlay);
	}

	if (!FMath::IsNearlyEqual(PlayRate, 1, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("playRate"), PlayRate);
	}

	if (!FMath::IsNearlyEqual(StartTime, 0, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("startTime"), StartTime);
	}
}

bool FGLTFJsonAnimationPlayback::operator==(const FGLTFJsonAnimationPlayback& Other) const
{
	return bLoop == Other.bLoop
		&& bAutoPlay == Other.bAutoPlay
		&& PlayRate == Other.PlayRate
		&& StartTime == Other.StartTime;
}

bool FGLTFJsonAnimationPlayback::operator!=(const FGLTFJsonAnimationPlayback& Other) const
{
	return !(*this == Other);
}

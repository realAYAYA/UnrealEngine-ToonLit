// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonAnimationPlayback : IGLTFJsonObject
{
	FString Name;

	bool bLoop;
	bool bAutoPlay;

	float PlayRate;
	float StartTime;

	FGLTFJsonAnimationPlayback()
		: bLoop(true)
		, bAutoPlay(true)
		, PlayRate(1)
		, StartTime(0)
	{
	}

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

	bool operator==(const FGLTFJsonAnimationPlayback& Other) const;
	bool operator!=(const FGLTFJsonAnimationPlayback& Other) const;
};

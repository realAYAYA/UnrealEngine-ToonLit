// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EGLTFTaskPriority : uint8
{
	Animation,
	Mesh,
	Material,
	Texture,
	MAX
};

class GLTFEXPORTER_API FGLTFDelayedTask
{
public:

	const EGLTFTaskPriority Priority;

	FGLTFDelayedTask(EGLTFTaskPriority Priority)
		: Priority(Priority)
	{
	}

	virtual ~FGLTFDelayedTask() = default;

	virtual FString GetName() = 0;

	virtual void Process() = 0;
};

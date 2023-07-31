// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// includes common to all vehicle systems
#include "CoreMinimal.h"

/**
 * Code common between all vehicle systems
 */
template <typename T>
class TVehicleSystem
{
public:

	TVehicleSystem() : SetupPtr(nullptr)
	{
	}

	TVehicleSystem(const T* SetupIn) : SetupPtr(SetupIn)
	{
		check(SetupPtr != nullptr);
	}

	FORCEINLINE T& AccessSetup()
	{
		check(SetupPtr != nullptr);
		return (T&)(*SetupPtr);
	}

	FORCEINLINE const T& Setup() const
	{
		check(SetupPtr != nullptr);
		return (*SetupPtr);
	}

	/*
	 * Setup data pointer is public if you want to use it directly
	 */
	const T* SetupPtr;
};
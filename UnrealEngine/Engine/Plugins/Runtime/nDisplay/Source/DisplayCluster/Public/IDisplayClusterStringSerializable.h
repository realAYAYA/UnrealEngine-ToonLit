// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * String serialization interface
 */
class IDisplayClusterStringSerializable
{
public:
	virtual ~IDisplayClusterStringSerializable() = default;

public:
	virtual FString SerializeToString() const = 0;
	virtual bool DeserializeFromString(const FString& ar) = 0;
};

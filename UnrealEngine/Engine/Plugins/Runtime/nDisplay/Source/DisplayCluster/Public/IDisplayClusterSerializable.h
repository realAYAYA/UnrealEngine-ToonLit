// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"


/**
 * Memory serialization interface
 */
class IDisplayClusterSerializable
{
public:
	virtual ~IDisplayClusterSerializable() = default;

public:
	virtual bool Serialize  (FMemoryWriter& ar) = 0;
	virtual bool Deserialize(FMemoryReader& ar) = 0;
};

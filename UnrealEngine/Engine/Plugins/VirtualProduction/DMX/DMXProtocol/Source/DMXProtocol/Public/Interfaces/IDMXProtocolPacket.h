// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/BufferArchive.h"

struct IDMXProtocolPacket
{
public:
	virtual ~IDMXProtocolPacket() {}

	virtual TSharedPtr<FBufferArchive> Pack(const uint16 PropertiesNum) { return nullptr; };
};

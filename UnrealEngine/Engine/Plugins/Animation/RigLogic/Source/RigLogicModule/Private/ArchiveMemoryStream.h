// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "riglogic/RigLogic.h"

/** Adapter that allows using an FArchive instance as a rl4::BoundedIOStream */
class FArchiveMemoryStream: public rl4::BoundedIOStream
{
public:
	explicit FArchiveMemoryStream(FArchive* Archive);

	void seek(std::uint64_t Position) override;
	std::uint64_t tell() override;
	void open() override;
	void close() override;
	size_t read(char* ReadToBuffer, size_t Size) override;
	size_t read(Writable* Destination, size_t Size) override;
	size_t write(const char* WriteFromBuffer, size_t Size) override;
	size_t write(Readable* Source, size_t Size) override;
	std::uint64_t size() override;

private:
	FArchive* Archive;
	int64 Origin;

};

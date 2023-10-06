// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Misc/Build.h"
#include "Misc/Optional.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/StructuredArchiveSlots.h"

class FStructuredArchive;
class FStructuredArchiveFormatter;

/**
 * FStructuredArchiveChildReader
 *
 * Utility class for easily creating a structured archive that covers the data hierarchy underneath
 * the given slot
 *
 * Allows serialization code to get an archive instance for the current location, so that it can return to it
 * later on after the master archive has potentially moved on into a different location in the file.
 */
class FStructuredArchiveChildReader
{
public:
	CORE_API explicit FStructuredArchiveChildReader(FStructuredArchiveSlot InSlot);
	CORE_API ~FStructuredArchiveChildReader();

	// Non-copyable
	FStructuredArchiveChildReader(FStructuredArchiveChildReader&&) = delete;
	FStructuredArchiveChildReader(const FStructuredArchiveChildReader&) = delete;
	FStructuredArchiveChildReader& operator=(FStructuredArchiveChildReader&&) = delete;
	FStructuredArchiveChildReader& operator=(const FStructuredArchiveChildReader&) = delete;

	FORCEINLINE FStructuredArchiveSlot GetRoot() const
	{
		return Root.GetValue();
	}

private:
	FStructuredArchiveFormatter* OwnedFormatter;
	FStructuredArchive* StructuredArchive;
	TOptional<FStructuredArchiveSlot> Root;
};

#if !WITH_TEXT_ARCHIVE_SUPPORT

FORCEINLINE FStructuredArchiveChildReader::FStructuredArchiveChildReader(FStructuredArchiveSlot InSlot)
	: OwnedFormatter(nullptr)
	, StructuredArchive(nullptr)
{
	StructuredArchive = new FStructuredArchive(InSlot.StructuredArchive.Formatter);
	Root.Emplace(StructuredArchive->Open());
}

FORCEINLINE FStructuredArchiveChildReader::~FStructuredArchiveChildReader()
{
	delete StructuredArchive;
}

#endif

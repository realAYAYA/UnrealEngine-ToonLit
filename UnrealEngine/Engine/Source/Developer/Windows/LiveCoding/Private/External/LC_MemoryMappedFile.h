// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "LC_FilesystemTypes.h"


namespace Filesystem
{
	// opaque type
	struct MemoryMappedFile;

	MemoryMappedFile* OpenMemoryMappedFile(const wchar_t* path, OpenMode::Enum openMode);
	void CloseMemoryMappedFile(MemoryMappedFile*& file);

	void* GetMemoryMappedFileData(MemoryMappedFile* file);
	const void* GetMemoryMappedFileData(const MemoryMappedFile* file);

	uint64_t GetMemoryMappedFileSizeOnDisk(const MemoryMappedFile* file);
}

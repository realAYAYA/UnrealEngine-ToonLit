// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

// BEGIN EPIC MOD
#include "CoreTypes.h"
// END EPIC MOD

namespace virtualDrive
{
	void Add(const wchar_t* driveLetterPlusColon, const wchar_t* path);
	void Remove(const wchar_t* driveLetterPlusColon, const wchar_t* path);
}

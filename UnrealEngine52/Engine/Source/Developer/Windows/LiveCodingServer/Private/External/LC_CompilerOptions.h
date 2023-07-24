// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

// BEGIN EPIC MOD
#include "CoreTypes.h"
#include <string>
// END EPIC MOD

namespace compilerOptions
{
	bool CreatesPrecompiledHeader(const char* options);
	bool UsesPrecompiledHeader(const char* options);
	std::string GetPrecompiledHeaderPath(const char* options);

	bool UsesC7DebugFormat(const char* options);
	bool UsesMinimalRebuild(const char* options);
}

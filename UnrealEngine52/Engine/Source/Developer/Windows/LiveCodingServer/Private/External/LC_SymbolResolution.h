// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "LC_SymbolInfo.h"


namespace symbolResolution
{
	// Resolves the symbol information for a given address
	SymbolInfo ResolveSymbolsForAddress(const void* const address);
}

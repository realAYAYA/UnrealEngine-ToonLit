// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

// BEGIN EPIC MOD
#include "CoreTypes.h"
// END EPIC MOD

namespace Debugger
{
	bool IsConnected(void);
}


#define LC_DEBUGGER_BREAKPOINT() __debugbreak()

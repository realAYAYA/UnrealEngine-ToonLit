// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

// BEGIN EPIC MOD
#include "LC_Platform.h"
#include "LC_Debugger_Windows.h"
#include "LC_Logging.h"
// END EPIC MOD


#if LC_DEBUG
#	define LC_ASSERT_BREAKPOINT()				(Debugger::IsConnected()) ? LC_DEBUGGER_BREAKPOINT() : (void)true
#	define LC_ASSERT(_condition, _msg)			(_condition) ? (void)true : (LC_ERROR_DEV("%s", _msg), LC_ASSERT_BREAKPOINT())
#else
#	define LC_BREAKPOINT()						(void)true
#	define LC_ASSERT(_condition, _msg)			LC_UNUSED(_condition)
#endif

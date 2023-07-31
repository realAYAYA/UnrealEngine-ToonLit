// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

// BEGIN EPIC MOD
//#include PCH_INCLUDE
// END EPIC MOD
#include "LC_SymbolInfo.h"
// BEGIN EPIC MOD
#include <stdio.h>
// END EPIC MOD

SymbolInfo::SymbolInfo(const char* const function, const char* const filename, unsigned int line)
	: m_function()
	, m_filename()
	, m_line(line)
{
	strcpy_s(m_function, function);
	strcpy_s(m_filename, filename);
}

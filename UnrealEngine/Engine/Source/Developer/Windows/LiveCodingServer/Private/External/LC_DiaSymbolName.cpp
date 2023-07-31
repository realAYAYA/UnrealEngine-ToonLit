// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

// BEGIN EPIC MOD
//#include PCH_INCLUDE
// END EPIC MOD
#include "LC_DiaSymbolName.h"


dia::SymbolName::SymbolName(BSTR str)
	: m_name(str)
{
}


dia::SymbolName::~SymbolName(void)
{
	if (m_name)
	{
		::SysFreeString(m_name);
	}
}


dia::SymbolName::SymbolName(SymbolName&& other)
	: m_name(other.m_name)
{
	other.m_name = nullptr;
}

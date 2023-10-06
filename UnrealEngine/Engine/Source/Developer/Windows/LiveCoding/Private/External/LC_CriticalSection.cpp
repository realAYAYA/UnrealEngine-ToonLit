// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

// BEGIN EPIC MOD
//#include PCH_INCLUDE
// END EPIC MOD
#include "LC_CriticalSection.h"


CriticalSection::CriticalSection(void)
{
	// BEGIN EPIC MOD
	Windows::InitializeCriticalSection(&m_cs);
	// END EPIC MOD
}


CriticalSection::~CriticalSection(void)
{
	// BEGIN EPIC MOD
	Windows::DeleteCriticalSection(&m_cs);
	// END EPIC MOD
}


void CriticalSection::Enter(void)
{
	// BEGIN EPIC MOD
	Windows::EnterCriticalSection(&m_cs);
	// END EPIC MOD
}


void CriticalSection::Leave(void)
{
	// BEGIN EPIC MOD
	Windows::LeaveCriticalSection(&m_cs);
	// END EPIC MOD
}

// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

// BEGIN EPIC MOD
#include "CoreTypes.h"
#include "Windows/MinimalWindowsAPI.h"
// END EPIC MOD

class ChangeNotification
{
public:
	ChangeNotification(void);
	~ChangeNotification(void);

	void Create(const wchar_t* path);
	void Destroy(void);

	bool Check(unsigned int timeoutMs);
	bool CheckOnce(void);
	void CheckNext(unsigned int timeoutMs);

private:
	bool WaitForNotification(unsigned int timeoutMs);

	// BEGIN EPIC MOD
	Windows::HANDLE m_handle;
	// END EPIC MOD
};

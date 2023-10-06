// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

// BEGIN EPIC MOD
#include "CoreTypes.h"
// END EPIC MOD
#include "LC_DuplexPipe.h"


class DuplexPipeServer : public DuplexPipe
{
public:
	bool Create(const wchar_t* name);
	bool WaitForClient(void);
	void Disconnect(void);
};

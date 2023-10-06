// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "LPP_API.h"


namespace restart
{
	void Startup(void);
	void Shutdown(void);

	int WasRequested(void);
	void Execute(lpp::RestartBehaviour behaviour, unsigned int exitCode);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/BlockCompression/Miro/Miro.h"
#include "MuR/MutableMemory.h"

namespace mu
{
	static int s_initialized = 0;
	static int s_finalized = 0;

	void Initialize()
	{
		if (!s_initialized)
		{
			s_initialized = 1;
			s_finalized = 0;

			miro::initialize();
		}
	}

	void Finalize()
	{
		if (s_initialized && !s_finalized)
		{
			miro::finalize();

			s_finalized = 1;
			s_initialized = 0;
		}
	}

}

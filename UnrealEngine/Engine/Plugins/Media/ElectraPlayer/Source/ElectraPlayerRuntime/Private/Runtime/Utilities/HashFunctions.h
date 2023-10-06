// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"

namespace Electra
{
	namespace HashFunctions
	{

		namespace CRC
		{
			uint32 Update32(uint32 InitialCrc, const void* Data, int64 NumBytes);
			uint32 Calc32(const void* Data, int64 NumBytes);

			uint64 Update64(uint64 InitialCrc, const void* Data, int64 NumBytes);
			uint64 Calc64(const void* Data, int64 NumBytes);

		} // namespace CRC



	} // namespace HashFunctions
} // namespace Electra


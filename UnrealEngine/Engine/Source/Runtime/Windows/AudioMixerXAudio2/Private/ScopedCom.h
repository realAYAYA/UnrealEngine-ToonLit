// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Windows/AllowWindowsPlatformTypes.h"

THIRD_PARTY_INCLUDES_START
#include <combaseapi.h>
THIRD_PARTY_INCLUDES_END

namespace Audio
{
	struct FScopeComString final
	{
		LPTSTR StringPtr = nullptr;

		UE_NONCOPYABLE(FScopeComString)

		const LPTSTR Get() const
		{
			return StringPtr;
		}

		explicit operator bool() const
		{
			return Get() != nullptr;
		}

		FScopeComString(LPTSTR InStringPtr = nullptr)
			: StringPtr(InStringPtr)
		{}

		~FScopeComString()
		{
			if (StringPtr)
			{
				CoTaskMemFree(StringPtr);
			}
		}
	};
}

#include "Windows/HideWindowsPlatformTypes.h"
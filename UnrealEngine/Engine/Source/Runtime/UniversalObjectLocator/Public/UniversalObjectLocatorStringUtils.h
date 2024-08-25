// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/StringFwd.h"

namespace UE::UniversalObjectLocator
{

	UNIVERSALOBJECTLOCATOR_API bool ParseUnsignedInteger(FStringView InString, uint32& OutResult, int32* OutNumCharsParsed = nullptr);

} // namespace UE::UniversalObjectLocator
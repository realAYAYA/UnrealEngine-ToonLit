// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace UE::Core::Private
{
	[[noreturn]] CORE_API void OnInvalidArrayNum(unsigned long long NewNum);
	[[noreturn]] CORE_API void OnInvalidSetNum  (unsigned long long NewNum);
}

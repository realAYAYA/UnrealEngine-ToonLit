// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace UE
{
namespace TraceAnalyzer
{

struct FCommand
{
	const TCHAR* Name;
	const TCHAR* Description;
	int32 (*Main)(int32, TCHAR const* const*);
};

} // namespace TraceAnalyzer
} // namespace UE

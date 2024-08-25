// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

struct FAnalyticsEventAttribute;

class FShaderStatsFunctions
{
public:
	UNREALED_API static void GatherShaderAnalytics(TArray<FAnalyticsEventAttribute>& Attributes);
	static void WriteShaderStats();
};

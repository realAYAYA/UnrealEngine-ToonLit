// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE
{
namespace Insights
{

class IFilterExecutor
{
public:
	virtual bool ApplyFilters(const class FFilterContext& Context) const = 0;
};

} // namespace Insights
} // namespace UE

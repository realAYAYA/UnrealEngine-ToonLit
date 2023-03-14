// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Player/ABRRules/ABRInternal.h"

namespace Electra
{

class IABROnDemandPlus : public IABRRule
{
public:
	static IABROnDemandPlus* Create(IABRInfoInterface* InIInfo, EMediaFormatType InFormatType, EABRPresentationType InPresentationType);
	virtual ~IABROnDemandPlus() = default;
};

} // namespace Electra

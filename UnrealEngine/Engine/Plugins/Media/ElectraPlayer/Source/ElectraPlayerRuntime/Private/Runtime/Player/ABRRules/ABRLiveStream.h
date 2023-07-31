// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Player/ABRRules/ABRInternal.h"

namespace Electra
{

class IABRLiveStream : public IABRRule
{
public:
	static IABRLiveStream* Create(IABRInfoInterface* InIInfo, EMediaFormatType InFormatType, EABRPresentationType InPresentationType);
	virtual ~IABRLiveStream() = default;
};

} // namespace Electra


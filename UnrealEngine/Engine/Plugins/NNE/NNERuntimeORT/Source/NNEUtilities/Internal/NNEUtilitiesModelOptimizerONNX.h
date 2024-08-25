// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NNEUtilitiesModelOptimizerBase.h"

namespace UE::NNEUtilities::Internal
{

class FModelOptimizerONNXToONNX : public FModelOptimizerBase
{
public:
	FModelOptimizerONNXToONNX();

	virtual FString GetName() const override
	{
		return TEXT("NNEModelOptimizerFromONNXToONNX");
	}
};

class FModelOptimizerONNXToORT : public FModelOptimizerBase
{
public:
	FModelOptimizerONNXToORT();

	virtual FString GetName() const override 
	{
		return TEXT("NNEModelOptimizerONNXToORT");
	}
};

} // namespace UE::NNEUtilities::Internal

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputCoreTypes.h"
#include "UObject/Interface.h"
#include "BaseSequencerAnimTool.generated.h"

UINTERFACE(MinimalAPI)
class UBaseSequencerAnimTool : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class SEQUENCERANIMTOOLS_API IBaseSequencerAnimTool
{
	GENERATED_IINTERFACE_BODY()
	virtual bool ProcessCommandBindings(const FKey Key, const bool bRepeat) const { return false; }

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISenseEvent.h"
#include "AISenseEvent_Hearing.generated.h"

UCLASS(MinimalAPI)
class UAISenseEvent_Hearing : public UAISenseEvent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	FAINoiseEvent Event;

public:
	AIMODULE_API UAISenseEvent_Hearing(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	AIMODULE_API virtual FAISenseID GetSenseID() const override;
	
	FORCEINLINE FAINoiseEvent GetNoiseEvent()
	{
		Event.Compile();
		return Event;
	}
};

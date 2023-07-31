// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Perception/AISense.h"
#include "AISense_Touch.generated.h"

class IAIPerceptionListenerInterface;
class UAISense_Touch;

USTRUCT()
struct AIMODULE_API FAITouchEvent
{	
	GENERATED_USTRUCT_BODY()

	typedef UAISense_Touch FSenseClass;

	FVector Location;
	
	UPROPERTY()
	TObjectPtr<AActor> TouchReceiver;

	UPROPERTY()
	TObjectPtr<AActor> OtherActor;
		
	FAITouchEvent() : TouchReceiver(nullptr), OtherActor(nullptr) {}
	
	FAITouchEvent(AActor* InTouchReceiver, AActor* InOtherActor, const FVector& EventLocation)
		: Location(EventLocation), TouchReceiver(InTouchReceiver), OtherActor(InOtherActor)
	{
	}

	IAIPerceptionListenerInterface* GetTouchedActorAsPerceptionListener() const;
};

UCLASS(ClassGroup=AI)
class AIMODULE_API UAISense_Touch : public UAISense
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FAITouchEvent> RegisteredEvents;

public:		
	void RegisterEvent(const FAITouchEvent& Event);	

	UFUNCTION(BlueprintCallable, Category = "AI|Perception", meta = (WorldContext = "WorldContextObject"))
	static void ReportTouchEvent(UObject* WorldContextObject, AActor* TouchReceiver, AActor* OtherActor, FVector Location);

protected:
	virtual float Update() override;
};

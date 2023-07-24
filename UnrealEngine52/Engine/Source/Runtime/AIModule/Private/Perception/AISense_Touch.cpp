// Copyright Epic Games, Inc. All Rights Reserved.

#include "Perception/AISense_Touch.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Perception/AIPerceptionListenerInterface.h"
#include "Perception/AIPerceptionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AISense_Touch)


IAIPerceptionListenerInterface* FAITouchEvent::GetTouchedActorAsPerceptionListener() const
{
	IAIPerceptionListenerInterface* Listener = nullptr;
	if (TouchReceiver)
	{
		Listener = Cast<IAIPerceptionListenerInterface>(TouchReceiver);
		if (Listener == nullptr)
		{
			APawn* ListenerAsPawn = Cast<APawn>(TouchReceiver);
			if (ListenerAsPawn)
			{
				Listener = Cast<IAIPerceptionListenerInterface>(ListenerAsPawn->GetController());
			}
		}
	}
	return Listener;
}

UAISense_Touch::UAISense_Touch(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
}

float UAISense_Touch::Update()
{
	AIPerception::FListenerMap& ListenersMap = *GetListeners();

	for (const FAITouchEvent& Event : RegisteredEvents)
	{
		if (Event.TouchReceiver != NULL && Event.OtherActor != NULL)
		{
			IAIPerceptionListenerInterface* PerceptionListener = Event.GetTouchedActorAsPerceptionListener();
			if (PerceptionListener != NULL)
			{
				UAIPerceptionComponent* PerceptionComponent = PerceptionListener->GetPerceptionComponent();
				if (PerceptionComponent != NULL && ListenersMap.Contains(PerceptionComponent->GetListenerId()))
				{
					// this has to succeed, will assert a failure
					FPerceptionListener& Listener = ListenersMap[PerceptionComponent->GetListenerId()];
					if (Listener.HasSense(GetSenseID()))
					{
						Listener.RegisterStimulus(Event.OtherActor, FAIStimulus(*this, 1.f, Event.Location, Event.Location));
					}
				}
			}
		}
	}

	RegisteredEvents.Reset();

	// return decides when next tick is going to happen
	return SuspendNextUpdate;
}

void UAISense_Touch::RegisterEvent(const FAITouchEvent& Event)
{
	RegisteredEvents.Add(Event);

	RequestImmediateUpdate();
}

void UAISense_Touch::ReportTouchEvent(UObject* WorldContextObject, AActor* TouchReceiver, AActor* OtherActor, FVector Location)
{
	UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(WorldContextObject);
	if (PerceptionSystem)
	{
		FAITouchEvent Event(TouchReceiver, OtherActor, Location);
		PerceptionSystem->OnEvent(Event);
	}
}


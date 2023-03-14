// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AISense.h"
#include "AISubsystem.h"
#include "AIPerceptionSystem.generated.h"

class UAIPerceptionComponent;
class UAISenseEvent;

AIMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogAIPerception, Warning, All);

class APawn;

/**
 *	By design checks perception between hostile teams
 */
UCLASS(ClassGroup=AI, config=Game, defaultconfig)
class AIMODULE_API UAIPerceptionSystem : public UAISubsystem
{
	GENERATED_BODY()
		
public:

	UAIPerceptionSystem(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	// FTickableGameObject begin
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// FTickableGameObject end

protected:	
	AIPerception::FListenerMap ListenerContainer;

	UPROPERTY()
	TArray<TObjectPtr<UAISense>> Senses;

	UPROPERTY(config, EditAnywhere, Category = Perception)
	float PerceptionAgingRate;

	FActorEndPlaySignature::FDelegate StimuliSourceEndPlayDelegate;

	// not a UPROPERTY on purpose so that we have a control over when stuff gets removed from the map
	TMap<const AActor*, FPerceptionStimuliSource> RegisteredStimuliSources;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(4.25, "This property will be removed in future versions. UnregisterSource is called by AActor.OnEndPlay delegate and will perform the cleanup.")
	uint32 bStimuliSourcesRefreshRequired : 1;
#endif

	uint32 bHandlePawnNotification : 1;

	struct FDelayedStimulus
	{
		float DeliveryTimestamp;
		FPerceptionListenerID ListenerId;
		TWeakObjectPtr<AActor> Instigator;
		FAIStimulus Stimulus;
	};

	TArray<FDelayedStimulus> DelayedStimuli;

	struct FPerceptionSourceRegistration
	{
		FAISenseID SenseID;
		TWeakObjectPtr<AActor> Source;

		FPerceptionSourceRegistration(FAISenseID InSenseID, AActor* SourceActor)
			: SenseID(InSenseID), Source(SourceActor)
		{}

		FORCEINLINE bool operator==(const FPerceptionSourceRegistration& Other) const
		{
			return SenseID == Other.SenseID && Source == Other.Source;
		}
	};
	TArray<FPerceptionSourceRegistration> SourcesToRegister;

public:

	FORCEINLINE bool IsSenseInstantiated(const FAISenseID& SenseID) const { return SenseID.IsValid() && Senses.IsValidIndex(SenseID) && Senses[SenseID] != nullptr; }

	/** Registers listener if not registered */
	void UpdateListener(UAIPerceptionComponent& Listener);
	void UnregisterListener(UAIPerceptionComponent& Listener);

	template<typename FEventClass, typename FSenseClass = typename FEventClass::FSenseClass>
	void OnEvent(const FEventClass& Event)
	{
		const FAISenseID SenseID = UAISense::GetSenseID<FSenseClass>();
		if (Senses.IsValidIndex(SenseID) && Senses[SenseID] != nullptr)
		{
			((FSenseClass*)Senses[SenseID])->RegisterEvent(Event);
		}
		// otherwise there's no one interested in this event, skip it.
	}

	template<typename FEventClass, typename FSenseClass = typename FEventClass::FSenseClass>
	void OnEventsBatch(const TArray<FEventClass>& Events)
	{
		if (Events.Num() > 0)
		{
			const FAISenseID SenseID = UAISense::GetSenseID<FSenseClass>();
			if (Senses.IsValidIndex(SenseID) && Senses[SenseID] != nullptr)
			{
				((FSenseClass*)Senses[SenseID])->RegisterEventsBatch(Events);
			}
		}
		// otherwise there's no one interested in this event, skip it.
	}

	template<typename FEventClass, typename FSenseClass = typename FEventClass::FSenseClass>
	static void OnEvent(UWorld* World, const FEventClass& Event)
	{
		UAIPerceptionSystem* PerceptionSys = GetCurrent(World);
		if (PerceptionSys != NULL)
		{
			PerceptionSys->OnEvent<FEventClass, FSenseClass>(Event);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void ReportEvent(UAISenseEvent* PerceptionEvent);

	UFUNCTION(BlueprintCallable, Category = "AI|Perception", meta = (WorldContext="WorldContextObject"))
	static void ReportPerceptionEvent(UObject* WorldContextObject, UAISenseEvent* PerceptionEvent);

	/** Registers a source of given sense's stimuli */
	template<typename FSenseClass>
	void RegisterSource(AActor& SourceActor);

	/** Registers given actor as a source for all registered senses */
	void RegisterSource(AActor& SourceActor);

	void RegisterSourceForSenseClass(TSubclassOf<UAISense> Sense, AActor& Target);

	/** 
	 *	unregisters given actor from the list of active stimuli sources
	 *	@param Sense if null will result in removing SourceActor from all the senses
	 */
	void UnregisterSource(AActor& SourceActor, const TSubclassOf<UAISense> Sense = nullptr);

	void OnListenerForgetsActor(const UAIPerceptionComponent& Listener, AActor& ActorToForget);
	void OnListenerForgetsAll(const UAIPerceptionComponent& Listener);
	void OnListenerConfigUpdated(FAISenseID SenseID, const UAIPerceptionComponent& Listener);

	void RegisterDelayedStimulus(FPerceptionListenerID ListenerId, float Delay, AActor* Instigator, const FAIStimulus& Stimulus);

	static UAIPerceptionSystem* GetCurrent(UObject* WorldContextObject);
	static UAIPerceptionSystem* GetCurrent(UWorld& World);

	static void MakeNoiseImpl(AActor* NoiseMaker, float Loudness, APawn* NoiseInstigator, const FVector& NoiseLocation, float MaxRange, FName Tag);

	UFUNCTION(BlueprintCallable, Category = "AI|Perception", meta = (WorldContext="WorldContextObject"))
	static bool RegisterPerceptionStimuliSource(UObject* WorldContextObject, TSubclassOf<UAISense> Sense, AActor* Target);

	FAISenseID RegisterSenseClass(TSubclassOf<UAISense> SenseClass);

	UFUNCTION(BlueprintCallable, Category = "AI|Perception", meta = (WorldContext="WorldContextObject"))
	static TSubclassOf<UAISense> GetSenseClassForStimulus(UObject* WorldContextObject, const FAIStimulus& Stimulus);
	
protected:
	
	UFUNCTION()
	void OnPerceptionStimuliSourceEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);
	
	/** requests registration of a given actor as a perception data source for specified sense */
	void RegisterSource(FAISenseID SenseID, AActor& SourceActor);

	/** iterates over all pawns and registeres them as a source for sense indicated by SenseID. Note that this will 
	 *  be performed only for senses that request that (see UAISense.bAutoRegisterAllPawnsAsSources).*/
	virtual void RegisterAllPawnsAsSourcesForSense(FAISenseID SenseID);

	enum EDelayedStimulusSorting 
	{
		RequiresSorting,
		NoNeedToSort,
	};
	/** sorts DelayedStimuli and delivers all the ones that are no longer "in the future"
	 *	@return true if any stimuli has become "current" stimuli (meaning being no longer in future) */
	bool DeliverDelayedStimuli(EDelayedStimulusSorting Sorting);
	void OnNewListener(const FPerceptionListener& NewListener);
	void OnListenerUpdate(const FPerceptionListener& UpdatedListener);
	void OnListenerRemoved(const FPerceptionListener& UpdatedListener);
	void PerformSourceRegistration();

	/** Returns true if aging resulted in tagging any of the listeners to process 
	 *	its stimuli (@see MarkForStimulusProcessing)*/
	bool AgeStimuli(const float Amount);

	UE_DEPRECATED(4.26, "Parameterless AgeStimuli has been deprecated, please use the other AgeStimuli flavor. To get behavior identical to the old one calls AgeStimuli(PerceptionAgingRate).")
	void AgeStimuli();

	friend class UAISense;
	FORCEINLINE AIPerception::FListenerMap& GetListenersMap() { return ListenerContainer; }

	friend class UAISystem;
	virtual void OnNewPawn(APawn& Pawn);
	virtual void StartPlay();

	/** Timestamp of the next stimuli aging */
	float NextStimuliAgingTick;
private:
	/** cached world's timestamp */
	float CurrentTime;
};

//////////////////////////////////////////////////////////////////////////
template<typename FSenseClass>
void UAIPerceptionSystem::RegisterSource(AActor& SourceActor)
{
	FAISenseID SenseID = UAISense::GetSenseID<FSenseClass>();
	if (IsSenseInstantiated(SenseID) == false)
	{
		RegisterSenseClass(FSenseClass::StaticClass());
		SenseID = UAISense::GetSenseID<FSenseClass>();
		check(SenseID.IsValid());
	}
	RegisterSource(SenseID, SourceActor);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectKey.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "GenericTeamAgentInterface.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AISense.h"
#include "Perception/AIPerceptionSystem.h"
#include "AIPerceptionComponent.generated.h"

class AAIController;
class FGameplayDebuggerCategory;
class UAISenseConfig;
struct FVisualLogEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPerceptionUpdatedDelegate, const TArray<AActor*>&, UpdatedActors);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FActorPerceptionUpdatedDelegate, AActor*, Actor, FAIStimulus, Stimulus);

USTRUCT(BlueprintType, meta = (DisplayName = "Sensed Actor's Update Data"))
struct FActorPerceptionUpdateInfo
{
	GENERATED_USTRUCT_BODY()

	/** Id of to the stimulus source */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	int32 TargetId = -1;

	/** Actor associated to the stimulus (can be null) */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	TWeakObjectPtr<AActor> Target;

	/** Updated stimulus */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	FAIStimulus Stimulus;

	FActorPerceptionUpdateInfo() = default;
	FActorPerceptionUpdateInfo(const int32 TargetId, const TWeakObjectPtr<AActor>& Target, const FAIStimulus& Stimulus);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActorPerceptionInfoUpdatedDelegate, const FActorPerceptionUpdateInfo&, UpdateInfo);

struct AIMODULE_API FActorPerceptionInfo
{
	TWeakObjectPtr<AActor> Target;

	TArray<FAIStimulus> LastSensedStimuli;

	/** if != MAX indicates the sense that takes precedense over other senses when it comes
		to determining last stimulus location */
	FAISenseID DominantSense;

	/** indicates whether this Actor is hostile to perception holder */
	uint32 bIsHostile : 1;
	
	FActorPerceptionInfo(AActor* InTarget = NULL)
		: Target(InTarget), DominantSense(FAISenseID::InvalidID())
	{
		LastSensedStimuli.AddDefaulted(FAISenseID::GetSize());
	}

	/** Retrieves last known location. Active (last reported as "successful")
	 *	stimuli are preferred. */
	FVector GetLastStimulusLocation(float* OptionalAge = NULL) const 
	{
		FVector Location(FAISystem::InvalidLocation);
		float BestAge = FLT_MAX;
		bool bBestWasSuccessfullySensed = false;
		for (int32 Sense = 0; Sense < LastSensedStimuli.Num(); ++Sense)
		{
			const float Age = LastSensedStimuli[Sense].GetAge();
			const bool bWasSuccessfullySensed = LastSensedStimuli[Sense].WasSuccessfullySensed();

			if (Age >= 0 && (Age < BestAge 
				|| (bBestWasSuccessfullySensed == false && bWasSuccessfullySensed)
				|| (Sense == DominantSense && bWasSuccessfullySensed)))
			{
				BestAge = Age;
				Location = LastSensedStimuli[Sense].StimulusLocation;
				bBestWasSuccessfullySensed = bWasSuccessfullySensed;

				if (Sense == DominantSense && bWasSuccessfullySensed)
				{
					// if dominant sense is active we don't want to look any further 
					break;
				}
			}
		}

		if (OptionalAge)
		{
			*OptionalAge = BestAge;
		}

		return Location;
	}

	/** it includes both currently live (visible) stimulus, as well as "remembered" ones */
	bool HasAnyKnownStimulus() const
	{
		for (const FAIStimulus& Stimulus : LastSensedStimuli)
		{
			// not that WasSuccessfullySensed will return 'false' for expired stimuli
			if (Stimulus.IsValid() && (Stimulus.WasSuccessfullySensed() == true || Stimulus.IsExpired() == false))
			{
				return true;
			}
		}

		return false;
	}

	/** Indicates currently live (visible) stimulus from any sense */
	bool HasAnyCurrentStimulus() const
	{
		for (const FAIStimulus& Stimulus : LastSensedStimuli)
		{
			// not that WasSuccessfullySensed will return 'false' for expired stimuli
			if (Stimulus.IsValid() && Stimulus.WasSuccessfullySensed() == true && Stimulus.IsExpired() == false)
			{
				return true;
			}
		}

		return false;
	}

	/** Retrieves location of the last sensed stimuli for a given sense
	* @param Sense	The AISenseID of the sense
	*
	* @return Location of the last sensed stimuli or FAISystem::InvalidLocation if given sense has never registered related Target actor or if last stimuli has expired.
	*/
	FORCEINLINE FVector GetStimulusLocation(FAISenseID Sense) const
	{
		return LastSensedStimuli.IsValidIndex(Sense) && (LastSensedStimuli[Sense].IsValid() && (LastSensedStimuli[Sense].IsExpired() == false)) ? LastSensedStimuli[Sense].StimulusLocation : FAISystem::InvalidLocation;
	}

	/** Retrieves receiver location of the last sense stimuli for a given sense
	* @param Sense	The AISenseID of the sense
	*
	* @return Location of the receiver for the last sensed stimuli or FAISystem::InvalidLocation if given sense has never registered related Target actor or last stimuli has expired.
	*/
	FORCEINLINE FVector GetReceiverLocation(FAISenseID Sense) const
	{
		return LastSensedStimuli.IsValidIndex(Sense) && (LastSensedStimuli[Sense].IsValid() && (LastSensedStimuli[Sense].IsExpired() == false)) ? LastSensedStimuli[Sense].ReceiverLocation : FAISystem::InvalidLocation;
	}

	UE_DEPRECATED(4.23, "This method is identical to IsSenseActive and will be removed in future versions. Please use IsSenseActive to check for a currently active stimuli or HasKnownStimulusOfSense for an active or remembered stimuli.")
	FORCEINLINE bool IsSenseRegistered(FAISenseID Sense) const
	{
		return LastSensedStimuli.IsValidIndex(Sense) && LastSensedStimuli[Sense].IsActive();
	}

	/** Indicates a currently active or "remembered" stimuli for a given sense
	* @param Sense	The AISenseID of the sense
	*
	* @return True if a target has been registered (even if not currently sensed) for the given sense and the stimuli is not expired.
	*/
	FORCEINLINE bool HasKnownStimulusOfSense(FAISenseID Sense) const
	{
		return LastSensedStimuli.IsValidIndex(Sense) && (LastSensedStimuli[Sense].IsValid() && (LastSensedStimuli[Sense].IsExpired() == false));
	}

	/** Indicates a currently active stimuli for a given sense
	* @param Sense	The AISenseID of the sense
	*
	* @return True if a target is still sensed for the given sense and the stimuli is not expired.
	*/
	FORCEINLINE bool IsSenseActive(FAISenseID Sense) const
	{
		return LastSensedStimuli.IsValidIndex(Sense) && LastSensedStimuli[Sense].IsActive();
	}
	
	/** takes all "newer" info from Other and absorbs it */
	void Merge(const FActorPerceptionInfo& Other);
};

USTRUCT(BlueprintType, meta = (DisplayName = "Sensed Actor's Data"))
struct FActorPerceptionBlueprintInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	TObjectPtr<AActor> Target;

	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	TArray<FAIStimulus> LastSensedStimuli;

	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	uint32 bIsHostile : 1;

	FActorPerceptionBlueprintInfo() : Target(NULL), bIsHostile(false)
	{}
	FActorPerceptionBlueprintInfo(const FActorPerceptionInfo& Info);
};

/**
 *	AIPerceptionComponent is used to register as stimuli listener in AIPerceptionSystem
 *	and gathers registered stimuli. UpdatePerception is called when component gets new stimuli (batched)
 */
UCLASS(ClassGroup=AI, HideCategories=(Activation, Collision), meta=(BlueprintSpawnableComponent), config=Game)
class AIMODULE_API UAIPerceptionComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()
	
	static const int32 InitialStimuliToProcessArraySize;

	typedef TMap<TObjectKey<AActor>, FActorPerceptionInfo> TActorPerceptionContainer;
	typedef TActorPerceptionContainer FActorPerceptionContainer;

protected:
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "AI Perception")
	TArray<TObjectPtr<UAISenseConfig>> SensesConfig;

	/** Indicated sense that takes precedence over other senses when determining sensed actor's location. 
	 *	Should be set to one of the senses configured in SensesConfig, or None. */
	UPROPERTY(EditDefaultsOnly, Category = "AI Perception")
	TSubclassOf<UAISense> DominantSense;
	
	FAISenseID DominantSenseID;

	UPROPERTY(Transient)
	TObjectPtr<AAIController> AIOwner;

	/** @todo this field is misnamed. It's an allow list. */
	FPerceptionChannelAllowList PerceptionFilter;

private:
	FPerceptionListenerID PerceptionListenerId;
	FActorPerceptionContainer PerceptualData;
		
protected:	
	struct FStimulusToProcess
	{
		TObjectKey<AActor> Source;
		FAIStimulus Stimulus;

		FStimulusToProcess(AActor* InSource, const FAIStimulus& InStimulus)
			: Source(InSource), Stimulus(InStimulus)
		{

		}
	};

	TArray<FStimulusToProcess> StimuliToProcess; 
	
	/** max age of stimulus to consider it "active" (e.g. target is visible) */
	TArray<float> MaxActiveAge;

private:

	/** Determines whether all knowledge of previously sensed actors will be removed or not when they become stale.
		That is, when they are no longer perceived and have exceeded the max age of the sense. */
	uint32 bForgetStaleActors : 1;

	uint32 bCleanedUp : 1;

public:

	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	UFUNCTION()
	void OnOwnerEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);
	
	void GetLocationAndDirection(FVector& Location, FVector& Direction) const;
	const AActor* GetBodyActor() const;
	AActor* GetMutableBodyActor();

	FORCEINLINE const FPerceptionChannelAllowList GetPerceptionFilter() const { return PerceptionFilter; }

	FGenericTeamId GetTeamIdentifier() const;
	FORCEINLINE FPerceptionListenerID GetListenerId() const { return PerceptionListenerId; }

	FVector GetActorLocation(const AActor& Actor) const;
	FORCEINLINE const FActorPerceptionInfo* GetActorInfo(const AActor& Actor) const { return PerceptualData.Find(&Actor); }
	FORCEINLINE FActorPerceptionContainer::TIterator GetPerceptualDataIterator() { return FActorPerceptionContainer::TIterator(PerceptualData); }
	FORCEINLINE FActorPerceptionContainer::TConstIterator GetPerceptualDataConstIterator() const { return FActorPerceptionContainer::TConstIterator(PerceptualData); }

	virtual void GetHostileActors(TArray<AActor*>& OutActors) const;
	
	void GetHostileActorsBySense(TSubclassOf<UAISense> SenseToFilterBy, TArray<AActor*>& OutActors) const;

	/**	Retrieves all actors in PerceptualData matching the predicate.
	 *	@return whether dead data (invalid actors) have been found while iterating over PerceptualData
	 */
	bool GetFilteredActors(TFunctionRef<bool(const FActorPerceptionInfo&)> Predicate, TArray<AActor*>& OutActors) const;

	// @note Will stop on first age 0 stimulus
	const FActorPerceptionInfo* GetFreshestTrace(const FAISenseID Sense) const;
	
	void SetDominantSense(TSubclassOf<UAISense> InDominantSense);
	FORCEINLINE FAISenseID GetDominantSenseID() const { return DominantSenseID; }
	FORCEINLINE TSubclassOf<UAISense> GetDominantSense() const { return DominantSense; }
	UAISenseConfig* GetSenseConfig(const FAISenseID& SenseID);
	const UAISenseConfig* GetSenseConfig(const FAISenseID& SenseID) const;
	void ConfigureSense(UAISenseConfig& SenseConfig);

	typedef TArray<UAISenseConfig*>::TConstIterator TAISenseConfigConstIterator;
	TAISenseConfigConstIterator GetSensesConfigIterator() const;

	/** Notifies AIPerceptionSystem to update properties for this "stimuli listener" */
	UFUNCTION(BlueprintCallable, Category="AI|Perception")
	void RequestStimuliListenerUpdate();

	/** Allows toggling senses on and off */
	void UpdatePerceptionAllowList(const FAISenseID Channel, const bool bNewValue);

	UE_DEPRECATED(5.0, "Use UpdatePerceptionAllowList instead")
	void UpdatePerceptionWhitelist(const FAISenseID Channel, const bool bNewValue)
	{
		UpdatePerceptionAllowList(Channel, bNewValue);
	}
	

	void RegisterStimulus(AActor* Source, const FAIStimulus& Stimulus);
	void ProcessStimuli();
	/** Returns true if, as result of stimuli aging, this listener needs an update (like if some stimuli expired) */
	bool AgeStimuli(const float ConstPerceptionAgingRate);
	void ForgetActor(AActor* ActorToForget);

	/** basically cleans up PerceptualData, resulting in loss of all previous perception */
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void ForgetAll();

	float GetYoungestStimulusAge(const AActor& Source) const;
	bool HasAnyActiveStimulus(const AActor& Source) const;
	bool HasAnyCurrentStimulus(const AActor& Source) const;
	bool HasActiveStimulus(const AActor& Source, FAISenseID Sense) const;

#if WITH_GAMEPLAY_DEBUGGER
	virtual void DescribeSelfToGameplayDebugger(FGameplayDebuggerCategory* DebuggerCategory) const;
#endif // WITH_GAMEPLAY_DEBUGGER

#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

	//----------------------------------------------------------------------//
	// blueprint interface
	//----------------------------------------------------------------------//
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void GetPerceivedHostileActors(TArray<AActor*>& OutActors) const;

	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void GetPerceivedHostileActorsBySense(const TSubclassOf<UAISense> SenseToUse, TArray<AActor*>& OutActors) const;

	/** If SenseToUse is none all actors currently perceived in any way will get fetched */
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void GetCurrentlyPerceivedActors(TSubclassOf<UAISense> SenseToUse, TArray<AActor*>& OutActors) const;

	/** If SenseToUse is none all actors ever perceived in any way (and not forgotten yet) will get fetched */
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void GetKnownPerceivedActors(TSubclassOf<UAISense> SenseToUse, TArray<AActor*>& OutActors) const;
	
	UE_DEPRECATED(4.13, "GetPerceivedActors is deprecated. Use GetCurrentlyPerceivedActors or GetKnownPerceivedActors")
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void GetPerceivedActors(TSubclassOf<UAISense> SenseToUse, TArray<AActor*>& OutActors) const;
	
	/** Retrieves whatever has been sensed about given actor */
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	bool GetActorsPerception(AActor* Actor, FActorPerceptionBlueprintInfo& Info);

	/** Note that this works only if given sense has been already configured for
	 *	this component instance */
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void SetSenseEnabled(TSubclassOf<UAISense> SenseClass, const bool bEnable);

	//////////////////////////////////////////////////////////////////////////
	// Might want to move these to special "BP_AIPerceptionComponent"
	//////////////////////////////////////////////////////////////////////////
	UPROPERTY(BlueprintAssignable)
	FPerceptionUpdatedDelegate OnPerceptionUpdated;

	/**
	 * Notifies all bound objects that perception info has been updated for a given target.
	 * The notification is broadcasted for any received stimulus or on change of state
	 * according to the stimulus configuration.
	 * 
	 * Note - This delegate will not be called if source actor is no longer valid 
	 * by the time a stimulus gets processed. 
	 * Use OnTargetPerceptionInfoUpdated providing a source id to handle those cases.
	 *
	 * @param	SourceActor	Actor associated to the stimulus (can not be null)
	 * @param	Stimulus	Updated stimulus
	 */
	UPROPERTY(BlueprintAssignable)
	FActorPerceptionUpdatedDelegate OnTargetPerceptionUpdated;

	/**
	 * Notifies all bound objects that perception info has been updated for a given target.
	 * The notification is broadcasted for any received stimulus or on change of state
	 * according to the stimulus configuration.
	 *
	 * Note - This delegate will be called even if source actor is no longer valid 
	 * by the time a stimulus gets processed so it is better to use source id for bookkeeping.
	 *
	 * @param	UpdateInfo	Data structure providing information related to the updated perceptual data
	 */
	UPROPERTY(BlueprintAssignable)
	FActorPerceptionInfoUpdatedDelegate OnTargetPerceptionInfoUpdated;

protected:
	FActorPerceptionContainer& GetPerceptualData() { return PerceptualData; }
	const FActorPerceptionContainer& GetPerceptualData() const { return PerceptualData; }

	/** called to clean up on owner's end play or destruction */
	virtual void CleanUp();

	void RemoveDeadData();

	/** Updates the stimulus entry in StimulusStore, if NewStimulus is more recent or stronger */
	virtual void RefreshStimulus(FAIStimulus& StimulusStore, const FAIStimulus& NewStimulus);

	/** @note no need to call super implementation, it's there just for some validity checking */
	virtual void HandleExpiredStimulus(FAIStimulus& StimulusStore);
	
private:
	friend UAIPerceptionSystem;

	void RegisterSenseConfig(UAISenseConfig& SenseConfig, UAIPerceptionSystem& AIPerceptionSys);
	void StoreListenerId(FPerceptionListenerID InListenerId) { PerceptionListenerId = InListenerId; }
	void SetMaxStimulusAge(FAISenseID SenseId, float MaxAge);
};


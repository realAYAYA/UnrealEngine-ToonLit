// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Perception/AIPerceptionTypes.h"
#include "AISense.generated.h"

class APawn;
class UAIPerceptionSystem;
class UAISenseEvent;

DECLARE_DELEGATE_OneParam(FOnPerceptionListenerUpdateDelegate, const FPerceptionListener&);

UCLASS(ClassGroup = AI, abstract, config = Engine)
class AIMODULE_API UAISense : public UObject
{
	GENERATED_UCLASS_BODY()

	static const float SuspendNextUpdate;

protected:
	UE_DEPRECATED(4.23, "This property will be removed in future versions. Use AISenseConfig::MaxAge instead.")
	/** age past which stimulus of this sense are "forgotten". (DEPRECATED: This property will be removed in future versions. Use AISenseConfig::MaxAge instead.)*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Perception", config)
	float DefaultExpirationAge;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Perception", config)
	EAISenseNotifyType NotifyType;

	/** whether this sense is interested in getting notified about new Pawns being spawned 
	 *	this can be used for example for automated sense sources registration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Perception", config)
	uint32 bWantsNewPawnNotification : 1;

	/** If true all newly spawned pawns will get auto registered as source for this sense. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Perception", config)
	uint32 bAutoRegisterAllPawnsAsSources : 1;

	/** this sense has some internal logic that requires it to be notified when 
	 *	a listener wants to forget an actor*/
	uint32 bNeedsForgettingNotification : 1;
	
private:
	UPROPERTY()
	TObjectPtr<UAIPerceptionSystem> PerceptionSystemInstance;

	/** then this count reaches 0 sense will be updated */
	float TimeUntilNextUpdate;

	FAISenseID SenseID;

protected:
	/**	If bound will be called when new FPerceptionListener gets registers with AIPerceptionSystem */
	FOnPerceptionListenerUpdateDelegate OnNewListenerDelegate;

	/**	If bound will be called when a FPerceptionListener's in AIPerceptionSystem change */
	FOnPerceptionListenerUpdateDelegate OnListenerUpdateDelegate;

	/**	If bound will be called when a FPerceptionListener's in removed from AIPerceptionSystem */
	FOnPerceptionListenerUpdateDelegate OnListenerRemovedDelegate;
				
public:

	virtual UWorld* GetWorld() const override;

	/** use with caution! Needs to be called before any senses get instantiated or listeners registered. DOES NOT update any perceptions system instances */
	static void HardcodeSenseID(TSubclassOf<UAISense> SenseClass, FAISenseID HardcodedID);

	static FAISenseID GetSenseID(const TSubclassOf<UAISense> SenseClass) { return SenseClass ? ((const UAISense*)SenseClass->GetDefaultObject())->SenseID : FAISenseID::InvalidID(); }
	template<typename TSense>
	static FAISenseID GetSenseID() 
	{ 
		return GetDefault<TSense>()->GetSenseID();
	}
	FORCEINLINE FAISenseID GetSenseID() const { return SenseID; }

	FORCEINLINE bool WantsUpdateOnlyOnPerceptionValueChange() const { return (NotifyType == EAISenseNotifyType::OnPerceptionChange); }

	virtual void PostInitProperties() override;

	/** 
	 *	@return should this sense be ticked now
	 */
	bool ProgressTime(float DeltaSeconds)
	{
		TimeUntilNextUpdate -= DeltaSeconds;
		return TimeUntilNextUpdate <= 0.f;
	}

	void Tick()
	{
		if (TimeUntilNextUpdate <= 0.f)
		{
			TimeUntilNextUpdate = Update();
		}
	}

	//virtual void RegisterSources(TArray<AActor&> SourceActors) {}
	virtual void RegisterSource(AActor& SourceActors){}
	virtual void UnregisterSource(AActor& SourceActors){}

	UE_DEPRECATED(4.25, "This method is no longer used and will be removed in future versions. UnregisterSource is called by AActor.OnEndPlay delegate and will perform the cleanup.")
	virtual void CleanseInvalidSources() {}

	virtual void RegisterWrappedEvent(UAISenseEvent& PerceptionEvent);
	virtual FAISenseID UpdateSenseID();

	bool NeedsNotificationOnForgetting() const { return bNeedsForgettingNotification; }
	virtual void OnListenerForgetsActor(const FPerceptionListener& Listener, AActor& ActorToForget) {}
	virtual void OnListenerForgetsAll(const FPerceptionListener& Listener) {}

	FORCEINLINE void OnNewListener(const FPerceptionListener& NewListener) { OnNewListenerDelegate.ExecuteIfBound(NewListener); }
	FORCEINLINE void OnListenerUpdate(const FPerceptionListener& UpdatedListener) { OnListenerUpdateDelegate.ExecuteIfBound(UpdatedListener); }
	FORCEINLINE void OnListenerRemoved(const FPerceptionListener& RemovedListener) { OnListenerRemovedDelegate.ExecuteIfBound(RemovedListener); }
	virtual void OnListenerConfigUpdated(const FPerceptionListener& UpdatedListener) { OnListenerUpdate(UpdatedListener); }

	UE_DEPRECATED(4.23, "This method will be removed in future versions. Perception relies on AISenseConfig::MaxAge so the value returned is no longer used by the perception system.")
	FORCEINLINE float GetDefaultExpirationAge() const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return DefaultExpirationAge; 
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	bool WantsNewPawnNotification() const { return bWantsNewPawnNotification; }
	bool ShouldAutoRegisterAllPawnsAsSources() const { return bAutoRegisterAllPawnsAsSources; }

protected:
	friend UAIPerceptionSystem;
	/** gets called when perception system gets notified about new spawned pawn. 
	 *	@Note: do not call super implementation. It's used to detect when subclasses don't override it */
	virtual void OnNewPawn(APawn& NewPawn);

	/** @return time until next update */
	virtual float Update() { return FLT_MAX; }

	/** will result in updating as soon as possible */
	FORCEINLINE void RequestImmediateUpdate() { TimeUntilNextUpdate = 0.f; }

	/** will result in updating in specified number of seconds */
	FORCEINLINE void RequestUpdateInSeconds(float UpdateInSeconds) { TimeUntilNextUpdate = UpdateInSeconds; }

	FORCEINLINE UAIPerceptionSystem* GetPerceptionSystem() { return PerceptionSystemInstance; }

	void SetSenseID(FAISenseID Index);

	/** returning pointer rather then a reference to prevent users from
	 *	accidentally creating copies by creating non-reference local vars */
	AIPerception::FListenerMap* GetListeners();

	/** To be called only for BP-generated classes */
	void ForceSenseID(FAISenseID SenseID);
};

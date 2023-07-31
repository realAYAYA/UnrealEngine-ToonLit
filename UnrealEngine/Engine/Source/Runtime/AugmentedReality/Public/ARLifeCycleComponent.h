// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ARTypes.h"
#include "UObject/ObjectMacros.h"
#include "ARComponent.h"
#include "ARActor.h"
#include "ARLifeCycleComponent.generated.h"

class USceneComponent;

UCLASS(BlueprintType, Experimental, meta = (BlueprintSpawnableComponent), ClassGroup = "AR Gameplay")
class AUGMENTEDREALITY_API UARLifeCycleComponent : public USceneComponent
{
	GENERATED_BODY()

	virtual void OnComponentCreated() override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FRequestSpawnARActorDelegate, UClass*, FGuid);
	static FRequestSpawnARActorDelegate RequestSpawnARActorDelegate;
	
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSpawnARActorDelegate, AARActor*, UARComponent*, FGuid);
	static FOnSpawnARActorDelegate OnSpawnARActorDelegate;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FRequestDestroyARActorDelegate, AARActor*);
	static FRequestDestroyARActorDelegate RequestDestroyARActorDelegate;
	
	/** Called when an AR actor is spawned on the server */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FInstanceARActorSpawnedDelegate, UClass*, ComponentClass, FGuid, NativeID, AARActor*, SpawnedActor);
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnARActorSpawned"))
	FInstanceARActorSpawnedDelegate OnARActorSpawnedDelegate;
	
	/** Called just before the AR actor is destroyed on the server */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInstanceARActorToBeDestroyedDelegate, AARActor*, Actor);
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnARActorToBeDestroyed"))
	FInstanceARActorToBeDestroyedDelegate OnARActorToBeDestroyedDelegate;
	
protected:
	virtual void OnUnregister() override;
	
private:
	void CallInstanceRequestSpawnARActorDelegate(UClass* Class, FGuid NativeID);
	void CallInstanceRequestDestroyARActorDelegate(AARActor* Actor);
	
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerSpawnARActor(UClass* ComponentClass, FGuid NativeID);
	
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerDestroyARActor(AARActor* Actor);
	
	FDelegateHandle SpawnDelegateHandle;
	FDelegateHandle DestroyDelegateHandle;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnhancedInputSubsystemInterface.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"

#include "EnhancedInputSubsystems.generated.h"

class FEnhancedInputWorldProcessor;
enum class ETickableTickType : uint8;
struct FInputKeyParams;

DECLARE_LOG_CATEGORY_EXTERN(LogWorldSubsystemInput, Log, All);

// Per local player input subsystem
UCLASS()
class ENHANCEDINPUT_API UEnhancedInputLocalPlayerSubsystem : public ULocalPlayerSubsystem, public IEnhancedInputSubsystemInterface
{
	GENERATED_BODY()

public:

	// Begin ULocalPlayerSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
	// End ULocalPlayerSubsystem
	
	// Begin IEnhancedInputSubsystemInterface
	virtual UEnhancedPlayerInput* GetPlayerInput() const override;
	virtual UEnhancedInputUserSettings* GetUserSettings() const override;
	virtual void InitalizeUserSettings() override;
	virtual void ControlMappingsRebuiltThisFrame() override;
protected:
	virtual TMap<TObjectPtr<const UInputAction>, FInjectedInput>& GetContinuouslyInjectedInputs() override { return ContinuouslyInjectedInputs; }
	// End IEnhancedInputSubsystemInterface
	
public:

	template<class UserSettingClass = UEnhancedInputUserSettings>
	inline UserSettingClass* GetUserSettings() const
	{
		return Cast<UserSettingClass>(GetUserSettings());
	}

	/** A delegate that will be called when control mappings have been rebuilt this frame. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControlMappingsRebuilt);

	/**
	 * Blueprint Event that is called at the end of any frame that Control Mappings have been rebuilt.
	 */
	UPROPERTY(BlueprintAssignable, DisplayName=OnControlMappingsRebuilt, Category = "Input")
	FOnControlMappingsRebuilt ControlMappingsRebuiltDelegate;

protected:
    	
	/** The user settings for this subsystem used to store each user's input related settings */
	UPROPERTY()
	TObjectPtr<UEnhancedInputUserSettings> UserSettings;

	// Map of inputs that should be injected every frame. These inputs will be injected when ForcedInput is ticked. 
	UPROPERTY(Transient) 
	TMap<TObjectPtr<const UInputAction>, FInjectedInput> ContinuouslyInjectedInputs;
	
};

/**
 * Per world input subsystem that allows you to bind input delegates to actors without an owning Player Controller. 
 * This should be used when an actor needs to receive input delegates but will never have an owning Player Controller.
 * For example, you can add input delegates to unlock a door when the user has a certain set of keys pressed.
 * Be sure to enable input on the actor, or else the input delegates won't fire!
 * 
 * Note: if you do have an actor with an owning Player Controller use the local player input subsystem instead.
 */
UCLASS(DisplayName="Enhanced Input World Subsystem (Experimental)")
class ENHANCEDINPUT_API UEnhancedInputWorldSubsystem : public UWorldSubsystem, public IEnhancedInputSubsystemInterface
{

// The Enhanced Input Module ticks the player input on this subsystem
friend class FEnhancedInputModule;
// The input processor tells us about what keys are pressed
friend class FEnhancedInputWorldProcessor;

	GENERATED_BODY()

public:

	//~ Begin UWorldSubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const;
	//~ End UWorldSubsystem interface

public:	
	//~ Begin IEnhancedInputSubsystemInterface
	virtual UEnhancedPlayerInput* GetPlayerInput() const override;
	virtual void ShowDebugInfo(UCanvas* Canvas) override;
protected:
	virtual TMap<TObjectPtr<const UInputAction>, FInjectedInput>& GetContinuouslyInjectedInputs() override { return ContinuouslyInjectedInputs; }
	//~ End IEnhancedInputSubsystemInterface

public:
	/** Adds this Actor's input component onto the stack to be processed by this subsystem's tick function */
	UFUNCTION(BlueprintCallable, Category = "Input|World", meta=(DefaultToSelf = "Actor"))
	void AddActorInputComponent(AActor* Actor);

	/** Removes this Actor's input component from the stack to be processed by this subsystem's tick function */
	UFUNCTION(BlueprintCallable, Category = "Input|World", meta = (DefaultToSelf = "Actor"))
	bool RemoveActorInputComponent(AActor* Actor);
	
protected:

	/** 
	* Inputs a key on this subsystem's player input which can then be processed as normal during Tick.
	* 
	* This should only be called by the FEnhancedInputWorldProcessor 
	*/
	bool InputKey(const FInputKeyParams& Params);

	/** 
	* Builds the current input stack and ticks the world subsystem's player input.
	* 
	* Called from the Enhanced Input Module Tick.
	* 
	* The Enhanced Input local player subsystem will have their Player Input's ticked by their owning 
	* Player Controller in APlayerController::TickPlayerInput, but because the world subsystem has no 
	* owning controller we need to tick it elsewhere.
	*/
	void TickPlayerInput(float DeltaTime);

	/** Adds all the default mapping contexts */
	void AddDefaultMappingContexts();

	/** Removes all the default mapping contexts */
	void RemoveDefaultMappingContexts();

	/** The player input that is processing the input within this subsystem */
	UPROPERTY()
	TObjectPtr<UEnhancedPlayerInput> PlayerInput = nullptr;

	/**
	 * Input processor that is created on Initalize.
	 */
	TSharedPtr<FEnhancedInputWorldProcessor> InputPreprocessor = nullptr;	
	
	/** Internal. This is the current stack of InputComponents that is being processed by the PlayerInput. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UInputComponent>> CurrentInputStack;

	// Map of inputs that should be injected every frame. These inputs will be injected when ForcedInput is ticked. 
	UPROPERTY(Transient) 
	TMap<TObjectPtr<const UInputAction>, FInjectedInput> ContinuouslyInjectedInputs;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "EnhancedInputWorldProcessor.h"
#include "Subsystems/EngineSubsystem.h"
#endif

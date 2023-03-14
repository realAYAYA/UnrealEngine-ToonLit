// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EditorSubsystem.h"
#include "Tickable.h"
#include "GameFramework/PlayerInput.h"
#include "EnhancedInputEditorSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEditorInput, Log, All);

class UInputComponent;
class UEnhancedPlayerInput;
class FEnhancedInputEditorProcessor;

/**
 * The Enhanced Input Editor Subsystem can be used to process input outside of PIE within the editor.
 * Calling StartConsumingInput will allow the input preprocessor to drive Input Action delegates
 * to be fired in the editor.
 *
 * This allows you to hook up Input Action delegates in Editor Utilities to make editor tools driven by
 * input.
 */
UCLASS()
class INPUTEDITOR_API UEnhancedInputEditorSubsystem : public UEditorSubsystem, public IEnhancedInputSubsystemInterface, public FTickableGameObject
{
	GENERATED_BODY()

public:

	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~ End USubsystem interface
	
	//~ Begin FTickableGameObject interface
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual bool IsTickableInEditor() const { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override;
	virtual void Tick(float DeltaTime) override;
	TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UEnhancedInputEditorSubsystem, STATGROUP_Tickables); }
	//~ End FTickableGameObject interface

	//~ UObject interface
	virtual UWorld* GetWorld() const override;
	//~ End UObject interface
	
	//~ Begin IEnhancedInputSubsystemInterface
	virtual UEnhancedPlayerInput* GetPlayerInput() const override;
	//~ End IEnhancedInputSubsystemInterface

	/** Pushes this input component onto the stack to be processed by this subsystem's tick function */
	UFUNCTION(BlueprintCallable, Category = "Input|Editor")
	void PushInputComponent(UInputComponent* InInputComponent);

	/** Removes this input component onto the stack to be processed by this subsystem's tick function */
	UFUNCTION(BlueprintCallable, Category = "Input|Editor")
	bool PopInputComponent(UInputComponent* InInputComponent);

	/** Start the consumption of input messages in this subsystem. This is required to have any Input Action delegates be fired. */
	UFUNCTION(BlueprintCallable, Category = "Input|Editor")
	void StartConsumingInput();

	/** Tells this subsystem to stop ticking and consuming any input. This will stop any Input Action Delegates from being called. */
	UFUNCTION(BlueprintCallable, Category = "Input|Editor")
	void StopConsumingInput();

	/** Returns true if this subsystem is currently consuming input */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Input|Editor")
	bool IsConsumingInput() const { return bIsCurrentlyConsumingInput; }

	/** Inputs a key on this subsystem's player input which can then be processed as normal during Tick. */
	bool InputKey(const FInputKeyParams& Params);
	
	/** Adds all the default mapping contexts from the UEnhancedInputEditorSettings */
	void AddDefaultMappingContexts();

	/** Removes all the default mapping contexts from the UEnhancedInputEditorSettings */
	void RemoveDefaultMappingContexts();
	
private:

	/** The player input that is processing the input within this subsystem */
	UPROPERTY()
	TObjectPtr<UEnhancedPlayerInput> PlayerInput = nullptr;

	/**
	 * Input processor that is created on Initalize. This will take input from the editor and pass it through
	 * to this subsystem via InputKey.
	 */
	TSharedPtr<FEnhancedInputEditorProcessor> InputPreprocessor = nullptr;
	
	/** If true, then this subsystem will Tick and process input delegates. */
	bool bIsCurrentlyConsumingInput = false;
	
	/** Internal. This is the current stack of InputComponents that is being processed by the PlayerInput. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UInputComponent>> CurrentInputStack;
};
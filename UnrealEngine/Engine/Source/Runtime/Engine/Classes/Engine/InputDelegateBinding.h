// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "InputDelegateBinding.generated.h"

class UInputComponent;

USTRUCT()
struct ENGINE_API FBlueprintInputDelegateBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint32 bConsumeInput:1;

	UPROPERTY()
	uint32 bExecuteWhenPaused:1;

	UPROPERTY()
	uint32 bOverrideParentBinding:1;

	FBlueprintInputDelegateBinding()
		: bConsumeInput(true)
		, bExecuteWhenPaused(false)
		, bOverrideParentBinding(true)
	{
	}
};

UCLASS(abstract)
class ENGINE_API UInputDelegateBinding : public UDynamicBlueprintBinding
{
	GENERATED_UCLASS_BODY()

	UE_DEPRECATED(5.0, "This version of BindToInputComponent is deprecated, please use the version that takes in an ObjectToBindTo instead.")
	virtual void BindToInputComponent(UInputComponent* InputComponent) const { };
	
	/**
	 * Override this function to bind a delegate to the given input component.
	 *
	 * @param InputComponent		The InputComponent to Bind a delegate to
	 * @param ObjectToBindTo		The UObject that the binding should use.
	 */
	virtual void BindToInputComponent(UInputComponent* InputComponent, UObject* ObjectToBindTo) const { };

	/** Returns true if the given class supports input binding delegates (i.e. it is a BP generated class) */
	static bool SupportsInputDelegate(const UClass* InClass);

	/**
	 * Calls BindToInputComponent for each dynamic binding object on the given Class if it supports input delegates.
	 *
	 * @param InClass				The class that will should be used to determine if Input Delegates are supported
	 * @param InputComponent		The InputComponent to Bind a delegate to
	 * @param ObjectToBindTo		The UObject that the binding should use. If this is null, the Owner of the input componet will be used.
	 */
	static void BindInputDelegates(const UClass* InClass, UInputComponent* InputComponent, UObject* ObjectToBindTo = nullptr);
	
	/**
	 * Will bind input delegates for the given Actor and traverse it's subobjects attempting to bind
	 * each of them
	 */
	static void BindInputDelegatesWithSubojects(AActor* InActor, UInputComponent* InputComponent);

protected:
	static TSet<UClass*> InputBindingClasses;
};
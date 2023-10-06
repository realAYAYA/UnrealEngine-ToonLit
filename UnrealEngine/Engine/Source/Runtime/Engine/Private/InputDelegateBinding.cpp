// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/InputDelegateBinding.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Components/InputComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputDelegateBinding)

TSet<UClass*> UInputDelegateBinding::InputBindingClasses;

UInputDelegateBinding::UInputDelegateBinding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (IsTemplate())
	{
		// Auto register the class
		InputBindingClasses.Emplace(GetClass());
	}
}

bool UInputDelegateBinding::SupportsInputDelegate(const UClass* InClass)
{
	return !!Cast<UBlueprintGeneratedClass>(InClass);
}

void UInputDelegateBinding::BindInputDelegates(const UClass* InClass, UInputComponent* InputComponent, UObject* InObjectToBindTo /* = nullptr */)
{
	if (InClass && InputComponent && SupportsInputDelegate(InClass))
	{
		ensureMsgf(InputComponent, TEXT("Attempting to bind input delegates to an invalid Input Component!"));
		
		// If there was an object given to bind to use that, otherwise fall back to the input component's owner
		// which will be an AActor.
		UObject* ObjectToBindTo = InObjectToBindTo ? InObjectToBindTo : InputComponent->GetOwner();
		
		BindInputDelegates(InClass->GetSuperClass(), InputComponent, ObjectToBindTo);

		for(UClass* BindingClass : InputBindingClasses)
		{
			UInputDelegateBinding* BindingObject = CastChecked<UInputDelegateBinding>(
				UBlueprintGeneratedClass::GetDynamicBindingObject(InClass, BindingClass)
				, ECastCheckedType::NullAllowed);
			if (BindingObject)
			{
				BindingObject->BindToInputComponent(InputComponent, ObjectToBindTo);
			}
		}
	}
}

void UInputDelegateBinding::BindInputDelegatesWithSubojects(AActor* InActor, UInputComponent* InputComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UInputDelegateBinding::BindInputDelegatesWithSubojects);
	
	ensureMsgf(InActor && InputComponent, TEXT("Attempting to bind input delegates to an invalid actor or input component!"));

	const UClass* ActorClass = InActor ? InActor->GetClass() : nullptr;

	if (ActorClass && InputComponent && SupportsInputDelegate(ActorClass))
	{
		// Bind any input delegates on the base actor class
		UInputDelegateBinding::BindInputDelegates(ActorClass, InputComponent, InputComponent->GetOwner());

		// Bind any input delegates on the actor's components
		TInlineComponentArray<UActorComponent*> ComponentArray;
		InActor->GetComponents(ComponentArray);
		for(UActorComponent* Comp : ComponentArray)
		{
			const UClass* CompClass = Comp ? Comp->GetClass() : nullptr;
			if(CompClass && Comp != InputComponent)
			{
				UInputDelegateBinding::BindInputDelegates(CompClass, InputComponent, Comp);	
			}			
		}
	}
}

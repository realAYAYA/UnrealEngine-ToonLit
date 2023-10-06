// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComponentReregisterContext.h"
#include "AI/NavigationSystemBase.h"

UWorld* FComponentReregisterContextBase::UnRegister(UActorComponent* InComponent)
{
	UWorld* World = nullptr;

	check(InComponent);
	checkf(!InComponent->IsUnreachable(), TEXT("%s"), *InComponent->GetFullName());

	if(InComponent->IsRegistered() && InComponent->GetWorld())
	{
		// Save the world and set the component's world to NULL to prevent a nested FComponentReregisterContext from reregistering this component.
		World = InComponent->GetWorld();
		FNavigationLockContext NavUpdateLock(World);

		// Will set bRegistered to false
		InComponent->ExecuteUnregisterEvents();

		InComponent->WorldPrivate = nullptr;
	}
	return World;	
}

void FComponentReregisterContextBase::ReRegister(UActorComponent* InComponent, UWorld* InWorld)
{
	check(InComponent);
		
	if( IsValid(InComponent) )
	{
		// Set scene pointer back
		check(InWorld != NULL); // If Component is set, World should be too (see logic in constructor)

		if( InComponent->IsRegistered() )
		{
			// The component has been registered already but external code is
			// going to expect the reregister to happen now. So unregister and
			// re-register.
			UE_LOG(LogActorComponent, Log, TEXT("~FComponentReregisterContext: (%s) Component already registered."), *InComponent->GetPathName());
			InComponent->ExecuteUnregisterEvents();
		}

		InComponent->WorldPrivate = InWorld;
		FNavigationLockContext NavUpdateLock(InWorld);

		// Will set bRegistered to true
		InComponent->ExecuteRegisterEvents();
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakInterfacePtr.h"
#include "NavigationObjectRepository.generated.h"

class INavLinkCustomInterface;
class INavRelevantInterface;

DECLARE_DELEGATE_OneParam(FOnNavRelevantObjectRegistrationEvent, INavRelevantInterface&);
DECLARE_DELEGATE_OneParam(FOnCustomNavLinkObjectRegistrationEvent, INavLinkCustomInterface&);

/**
 * World subsystem dedicated to store different types of navigation related objects that the
 * NavigationSystem needs to access.
 */
UCLASS()
class UNavigationObjectRepository : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/**
	 * Adds the provided object implementing INavRelevantInterface to the list of registered navigation relevant objects.
	 * @param NavRelevantObject INavRelevantInterface of the object to register.
	 * @note Method will also assert if same interface pointers is registered twice.
	 */
	void RegisterNavRelevantObject(INavRelevantInterface& NavRelevantObject);

	/**
	 * Removes the provided interface from the list of registered navigation relevant objects.
	 * @param NavRelevantObject INavRelevantInterface of the object to unregister.
	 * @note Method will also assert if interface can't be removed (i.e. not registered or already unregistered).
	 */
	void UnregisterNavRelevantObject(INavRelevantInterface& NavRelevantObject);

	/**
	 * Returns the list of registered navigation relevant objects.
	 * @return Const view on the list of all registered navigation relevant objects.
	 */
	TConstArrayView<TWeakInterfacePtr<INavRelevantInterface>> GetNavRelevantObjects() const
	{
		return NavRelevantObjects;
	}

	/**
	 * Adds the provided interface to the list of registered custom navigation links.
	 * @param CustomNavLinkObject INavLinkCustomInterface of the object to register.
	 * @note Method will also assert if same interface pointers is registered twice.
	 */
	void RegisterCustomNavLinkObject(INavLinkCustomInterface& CustomNavLinkObject);

	/**
	 * Removes the provided interface from the list of registered custom navigation links.
	 * @param CustomNavLinkObject INavLinkCustomInterface of the object to unregister.
	 * @note Method will also assert if interface can't be removed (i.e. not registered or already unregistered).
	 */
	void UnregisterCustomNavLinkObject(INavLinkCustomInterface& CustomNavLinkObject);

	/**
	 * Returns the list of registered custom navigation links.
	 * @return Const view on the list of all registered custom navigation links.
	 */
	TConstArrayView<TWeakInterfacePtr<INavLinkCustomInterface>> GetCustomLinks() const
	{
		return CustomLinkObjects;
	}

	/** Delegate executed when a navigation relevant object is registered to the repository. */
	FOnNavRelevantObjectRegistrationEvent OnNavRelevantObjectRegistered;

	/** Delegate executed when a navigation relevant object is unregistered from the repository. */
	FOnNavRelevantObjectRegistrationEvent OnNavRelevantObjectUnregistered;

	/** Delegate executed when a custom navigation link is registered to the repository. */
	FOnCustomNavLinkObjectRegistrationEvent OnCustomNavLinkObjectRegistered;

	/** Delegate executed when a custom navigation link is unregistered from the repository. */
	FOnCustomNavLinkObjectRegistrationEvent OnCustomNavLinkObjectUnregistered;

private:

	/** List of registered navigation relevant objects. */
	TArray<TWeakInterfacePtr<INavRelevantInterface>> NavRelevantObjects;

	/** List of registered custom navigation link objects. */
	TArray<TWeakInterfacePtr<INavLinkCustomInterface>> CustomLinkObjects;
};

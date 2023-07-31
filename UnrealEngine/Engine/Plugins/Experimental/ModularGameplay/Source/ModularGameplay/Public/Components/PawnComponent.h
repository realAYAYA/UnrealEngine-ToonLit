// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Pawn.h"
#include "GameFrameworkComponent.h"
#include "PawnComponent.generated.h"

/**
 * PawnComponent is an actor component made for APawn and receives pawn events.
 */
UCLASS()
class MODULARGAMEPLAY_API UPawnComponent : public UGameFrameworkComponent
{
	GENERATED_BODY()

public:
	
	UPawnComponent(const FObjectInitializer& ObjectInitializer);

	/** Gets the pawn that owns the component, this will always be valid during gameplay but can return null in the editor */
	template <class T>
	T* GetPawn() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, APawn>::Value, "'T' template parameter to GetPawn must be derived from APawn");
		return Cast<T>(GetOwner());
	}

	template <class T>
	T* GetPawnChecked() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, APawn>::Value, "'T' template parameter to GetPawnChecked must be derived from APawn");
		return CastChecked<T>(GetOwner());
	}

	//////////////////////////////////////////////////////////////////////////////
	// Pawn accessors, only valid if called during gameplay
	//////////////////////////////////////////////////////////////////////////////

	/** Gets the player state that owns the component, this can return null on clients for player pawns that are still being replicated */
	template <class T>
	T* GetPlayerState() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, APlayerState>::Value, "'T' template parameter to GetPlayerState must be derived from APlayerState");
		return GetPawnChecked<APawn>()->GetPlayerState<T>();
	}

	/** Gets the controller that owns the component, this will usually be null on clients */
	template <class T>
	T* GetController() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, AController>::Value, "'T' template parameter to GetController must be derived from AController");
		return GetPawnChecked<APawn>()->GetController<T>();
	}

};
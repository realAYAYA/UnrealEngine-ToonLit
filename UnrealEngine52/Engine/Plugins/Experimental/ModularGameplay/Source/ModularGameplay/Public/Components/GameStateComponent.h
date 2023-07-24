// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameStateBase.h"
#include "GameFrameworkComponent.h"
#include "GameStateComponent.generated.h"

/**
 * GameStateComponent is an actor component made for AGameStateBase and receives GameState events.
 */
UCLASS()
class MODULARGAMEPLAY_API UGameStateComponent : public UGameFrameworkComponent
{
	GENERATED_BODY()

public:
	
	UGameStateComponent(const FObjectInitializer& ObjectInitializer);

	/** Gets the game state that owns the component, this will always be valid during gameplay but can return null in the editor */
	template <class T>
	T* GetGameState() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, AGameStateBase>::Value, "'T' template parameter to GetGameState must be derived from AGameStateBase");
		return Cast<T>(GetOwner());
	}

	template <class T>
	T* GetGameStateChecked() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, AGameStateBase>::Value, "'T' template parameter to GetGameStateChecked must be derived from AGameStateBase");
		return CastChecked<T>(GetOwner());
	}

	//////////////////////////////////////////////////////////////////////////////
	// GameState accessors, only valid if called during gameplay
	//////////////////////////////////////////////////////////////////////////////

	/** Gets the game mode that owns this component, this will always return null on the client */
	template <class T>
	T* GetGameMode() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, AGameModeBase>::Value, "'T' template parameter to GetGameMode must be derived from AGameModeBase");
		return Cast<T>(GetGameStateChecked<AGameStateBase>()->AuthorityGameMode);
	}

public:

	//////////////////////////////////////////////////////////////////////////////
	// GameState events
	//////////////////////////////////////////////////////////////////////////////

	/** Called when gameplay has fully started */
	virtual void HandleMatchHasStarted() {}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "GameFramework/GameMode.h"
#include "GameFramework/GameState.h"
#endif

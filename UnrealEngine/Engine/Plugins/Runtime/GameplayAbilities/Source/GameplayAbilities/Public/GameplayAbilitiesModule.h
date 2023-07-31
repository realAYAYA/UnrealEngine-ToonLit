// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * The public interface to this module
 */
class IGameplayAbilitiesModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IGameplayAbilitiesModule& Get()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_IGameplayAbilitiesModule_Get);
		static IGameplayAbilitiesModule& Singleton = FModuleManager::LoadModuleChecked< IGameplayAbilitiesModule >("GameplayAbilities");
		return Singleton;
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_IGameplayAbilitiesModule_IsAvailable);
		return FModuleManager::Get().IsModuleLoaded( "GameplayAbilities" );
	}

	virtual class UAbilitySystemGlobals* GetAbilitySystemGlobals() = 0;

	virtual bool IsAbilitySystemGlobalsAvailable() = 0;

	virtual void CallOrRegister_OnAbilitySystemGlobalsReady(FSimpleMulticastDelegate::FDelegate Delegate) = 0;
};


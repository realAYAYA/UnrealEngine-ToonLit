// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DecoupledOutputProvider.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

template<typename T>
class TSubclassOf;

namespace UE::DecoupledOutputProvider
{
	class IOutputProviderLogic;
	
	struct FOutputProviderLogicCreationArgs
	{
		UDecoupledOutputProvider* Provider;
	};
	DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<IOutputProviderLogic>, FOutputProviderLogicFactoryDelegate, const FOutputProviderLogicCreationArgs&);
	
	/** Provides an interface for managing decoupled output provider logic objects. */
	class DECOUPLEDOUTPUTPROVIDER_API IDecoupledOutputProviderModule : public IModuleInterface
	{
	public:

		static IDecoupledOutputProviderModule& Get(){ return FModuleManager::Get().LoadModuleChecked<IDecoupledOutputProviderModule>(TEXT("DecoupledOutputProvider")); }

		/**
		 * Registers a factory delegate for Class and its subclasses. You can overwrite subclasses to use a different delegate.
		 * @param Class The base class for which to register the logic
		 * @param FactoryDelegate The delegate that will create the logic object
		 */
		virtual void RegisterLogicFactory(TSubclassOf<UDecoupledOutputProvider> Class, FOutputProviderLogicFactoryDelegate FactoryDelegate) = 0;
		/**
		 * Unregisters a previously registered delegate. Existing logic objects will continue to exist; if you want them to be removed: BeginDestroy will implicitly destroy the associated logic object.
		 * @param Class You must supply the exact class previously passed to RegisterLogicFactory. This does not remove subclass overrides.
		 */
		virtual void UnregisterLogicFactory(TSubclassOf<UDecoupledOutputProvider> Class) = 0;
		
		enum class EBreakBehavior
		{
			Continue,
			Break
		};
		/** Iterates all output providers with the given class for which a logic object has been created. Note this includes subclass overrides as well. */
		virtual void ForEachOutputProvidersWithLogicObject(TFunctionRef<EBreakBehavior(UDecoupledOutputProvider&)> Callback, TSubclassOf<UDecoupledOutputProvider> Class = UDecoupledOutputProvider::StaticClass()) const = 0;
		/** Gets all output providers for which a logic object has been created. Note this includes subclass overrides as well. */
		TArray<UDecoupledOutputProvider*> GetOutputProvidersWithLogicObjects(TSubclassOf<UDecoupledOutputProvider> Class = UDecoupledOutputProvider::StaticClass()) const
		{
			TArray<UDecoupledOutputProvider*> Result;
			ForEachOutputProvidersWithLogicObject([&Result](UDecoupledOutputProvider& OutputProvider){ Result.Emplace(&OutputProvider); return EBreakBehavior::Continue; }, Class);
			return Result;
		}
	};
}
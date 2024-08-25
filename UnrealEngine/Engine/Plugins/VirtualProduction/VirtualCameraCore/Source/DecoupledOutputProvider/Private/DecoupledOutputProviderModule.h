// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDecoupledOutputProviderModule.h"
#include "Misc/Optional.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UClass;

namespace UE::DecoupledOutputProvider
{
	class IOutputProviderEvent;
}

namespace UE::DecoupledOutputProvider::Private
{
	class FDecoupledOutputProviderModule
		: public IDecoupledOutputProviderModule
	{
	public:
		
		static FDecoupledOutputProviderModule& Get() { return FModuleManager::Get().LoadModuleChecked<FDecoupledOutputProviderModule>(TEXT("DecoupledOutputProvider")); }
		static bool IsAvailable() { return FModuleManager::Get().IsModuleLoaded(TEXT("DecoupledOutputProvider")); }

		//~ Begin IModuleInterface Interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IModuleInterface Interface

		//~ Begin IDecoupledOutputProviderModule Interface
		virtual void RegisterLogicFactory(TSubclassOf<UDecoupledOutputProvider> Class, FOutputProviderLogicFactoryDelegate FactoryDelegate) override;
		virtual void UnregisterLogicFactory(TSubclassOf<UDecoupledOutputProvider> Class) override;
		virtual void ForEachOutputProvidersWithLogicObject(TFunctionRef<EBreakBehavior(UDecoupledOutputProvider&)> Callback, TSubclassOf<UDecoupledOutputProvider> Class) const override;
		//~ End IDecoupledOutputProviderModule Interface

		void OnInitialize(IOutputProviderEvent& Args);
		void OnDeinitialize(IOutputProviderEvent& Args);
		void OnTick(IOutputProviderEvent& Args, const float DeltaTime);
		void OnActivate(IOutputProviderEvent& Args);
		void OnDeactivate(IOutputProviderEvent& Args);
		TOptional<VCamCore::EViewportChangeReply> PreReapplyViewport(IOutputProviderEvent& Args);
		void PostReapplyViewport(IOutputProviderEvent& Args);
		void OnAddReferencedObjects(IOutputProviderEvent& Args, FReferenceCollector& Collector);
		void OnBeginDestroy(IOutputProviderEvent& Args);
		void OnSerialize(IOutputProviderEvent& Args, FArchive& Ar);
		void OnPostLoad(IOutputProviderEvent& Args);
#if WITH_EDITOR
		void OnPostEditChangeProperty(IOutputProviderEvent& Args, FPropertyChangedEvent& PropertyChangedEvent);
#endif

	private:
		
		struct FOutputProviderClassMetaData
		{
			/** Used to create new logic objects. */
			FOutputProviderLogicFactoryDelegate Factory;
		};

		/** Keeps track of registered factory functions */
		TMap<UClass*, FOutputProviderClassMetaData> RegisteredFactories;
		
		/** Maps output providers to their logic. There can be at most one logic object per output provider. */
		TMap<TWeakObjectPtr<UDecoupledOutputProvider>, TSharedRef<IOutputProviderLogic>> ActiveLogic;

		/** Creates a new logic object that will be associated with OutputProviderBase, or if one was previously associated, returns the preexisting one. */
		TSharedPtr<IOutputProviderLogic> GetOrCreateLogicFor(UDecoupledOutputProvider& OutputProviderBase);
		
		// Data queries
		void ForEachRegisteredClassInClassHierarchy(UClass* Class, TFunctionRef<EBreakBehavior(UClass& Key, FOutputProviderClassMetaData&)> Callback);
		void ForEachRegisteredClassInClassHierarchy(UClass* Class, TFunctionRef<EBreakBehavior(UClass& Key, const FOutputProviderClassMetaData&)> Callback) const;
		TPair<UClass*, FOutputProviderClassMetaData*> FindRegisteredFactoryInClassHierarchy(UClass* Class);
		TPair<UClass*, const FOutputProviderClassMetaData*> FindRegisteredFactoryInClassHierarchy(UClass* Class) const;

		// Delegates
		void OnWorldCleanup(UWorld* World, bool bArg, bool bCond);
	};
}

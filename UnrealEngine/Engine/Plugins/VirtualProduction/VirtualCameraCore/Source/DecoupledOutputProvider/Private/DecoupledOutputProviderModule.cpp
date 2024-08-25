// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoupledOutputProviderModule.h"

#include "IOutputProviderLogic.h"

#include "Engine/World.h"

namespace UE::DecoupledOutputProvider::Private
{
	void FDecoupledOutputProviderModule::StartupModule()
	{
		FWorldDelegates::OnWorldCleanup.AddRaw(this, &FDecoupledOutputProviderModule::OnWorldCleanup);
	}

	void FDecoupledOutputProviderModule::ShutdownModule()
	{
		FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	}

	void FDecoupledOutputProviderModule::RegisterLogicFactory(TSubclassOf<UDecoupledOutputProvider> Class, FOutputProviderLogicFactoryDelegate FactoryDelegate)
	{
		check(FactoryDelegate.IsBound() && Class != UDecoupledOutputProvider::StaticClass());
		
		FOutputProviderClassMetaData& ClassMetaData = RegisteredFactories.FindOrAdd(Class);
		if (!ClassMetaData.Factory.IsBound())
		{
			ClassMetaData.Factory = FactoryDelegate;
		}
	}

	void FDecoupledOutputProviderModule::UnregisterLogicFactory(TSubclassOf<UDecoupledOutputProvider> Class)
	{
		RegisteredFactories.Remove(Class);
	}

	void FDecoupledOutputProviderModule::ForEachOutputProvidersWithLogicObject(TFunctionRef<EBreakBehavior(UDecoupledOutputProvider&)> Callback, TSubclassOf<UDecoupledOutputProvider> Class) const
	{
		for (const TPair<TWeakObjectPtr<UDecoupledOutputProvider>, TSharedRef<IOutputProviderLogic>>& Pair : ActiveLogic)
		{
			TWeakObjectPtr<UDecoupledOutputProvider> OutputProvider = Pair.Key;
			if (OutputProvider.IsValid() && Callback(*OutputProvider) == EBreakBehavior::Break)
			{
				return;
			}
		}
	}

	void FDecoupledOutputProviderModule::OnInitialize(IOutputProviderEvent& Args)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			Logic->OnInitialize(Args);
		}
	}

	void FDecoupledOutputProviderModule::OnDeinitialize(IOutputProviderEvent& Args)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			Logic->OnDeinitialize(Args);
		}
	}

	void FDecoupledOutputProviderModule::OnTick(IOutputProviderEvent& Args, const float DeltaTime)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			Logic->OnTick(Args, DeltaTime);
		}
	}

	void FDecoupledOutputProviderModule::OnActivate(IOutputProviderEvent& Args)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			Logic->OnActivate(Args);
		}
	}

	void FDecoupledOutputProviderModule::OnDeactivate(IOutputProviderEvent& Args)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			Logic->OnDeactivate(Args);
		}
	}

	TOptional<VCamCore::EViewportChangeReply> FDecoupledOutputProviderModule::PreReapplyViewport(IOutputProviderEvent& Args)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			return Logic->PreReapplyViewport(Args);
		}
		return {};
	}

	void FDecoupledOutputProviderModule::PostReapplyViewport(IOutputProviderEvent& Args)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			Logic->PostReapplyViewport(Args);
		}
	}

	void FDecoupledOutputProviderModule::OnAddReferencedObjects(IOutputProviderEvent& Args, FReferenceCollector& Collector)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			Logic->OnAddReferencedObjects(Args, Collector);
		}
	}

	void FDecoupledOutputProviderModule::OnBeginDestroy(IOutputProviderEvent& Args)
	{
		UDecoupledOutputProvider& OutputProvider = Args.GetOutputProvider();
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(OutputProvider))
		{
			Logic->OnBeginDestroy(Args);
			ActiveLogic.Remove(&OutputProvider);
		}
	}

	void FDecoupledOutputProviderModule::OnSerialize(IOutputProviderEvent& Args, FArchive& Ar)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			Logic->OnSerialize(Args, Ar);
		}
	}

	void FDecoupledOutputProviderModule::OnPostLoad(IOutputProviderEvent& Args)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			Logic->OnPostLoad(Args);
		}
	}

#if WITH_EDITOR
	void FDecoupledOutputProviderModule::OnPostEditChangeProperty(IOutputProviderEvent& Args, FPropertyChangedEvent& PropertyChangedEvent)
	{
		if (const TSharedPtr<IOutputProviderLogic> Logic = GetOrCreateLogicFor(Args.GetOutputProvider()))
		{
			Logic->OnPostEditChangeProperty(Args, PropertyChangedEvent);
		}
	}
#endif

	TSharedPtr<IOutputProviderLogic> FDecoupledOutputProviderModule::GetOrCreateLogicFor(UDecoupledOutputProvider& OutputProviderBase)
	{
		if (const TSharedRef<IOutputProviderLogic>* Logic = ActiveLogic.Find(&OutputProviderBase))
		{
			return *Logic;
		}

		TPair<UClass*, FOutputProviderClassMetaData*> FoundClassData = FindRegisteredFactoryInClassHierarchy(OutputProviderBase.GetClass());
		const bool bHasRegisteredLogicFactory = FoundClassData.Key && FoundClassData.Value;
		if (!bHasRegisteredLogicFactory
			// CDOs and archetypes do not perform any outputting so skip them!
			|| OutputProviderBase.HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject)
			// Users should not be able to register for the UDecoupledOutputProvider base class
			|| OutputProviderBase.GetClass() == UDecoupledOutputProvider::StaticClass())
		{
			return nullptr;
		}

		FOutputProviderClassMetaData& MetaData = *FoundClassData.Value;
		const TSharedPtr<IOutputProviderLogic> Result = MetaData.Factory.Execute({ &OutputProviderBase });
		if (!Result)
		{
			return nullptr;
		}

		ActiveLogic.Add(&OutputProviderBase, Result.ToSharedRef());
		return Result;
	}

	void FDecoupledOutputProviderModule::ForEachRegisteredClassInClassHierarchy(UClass* Class, TFunctionRef<EBreakBehavior(UClass& Class, FOutputProviderClassMetaData&)> Callback)
	{
		const_cast<const FDecoupledOutputProviderModule*>(this)->ForEachRegisteredClassInClassHierarchy(Class, [&Callback](UClass& Class, const FOutputProviderClassMetaData& Data)
		{
			return Callback(Class, const_cast<FOutputProviderClassMetaData&>(Data));
		});
	}

	void FDecoupledOutputProviderModule::ForEachRegisteredClassInClassHierarchy(UClass* Class, TFunctionRef<EBreakBehavior(UClass& Class, const FOutputProviderClassMetaData&)> Callback) const
	{
		// Recursion termination
		if (!Class
			|| !ensure(Class->IsChildOf(UDecoupledOutputProvider::StaticClass()))
			|| Class == UDecoupledOutputProvider::StaticClass())
		{
			return;
		}

		// Keep walking up the hierarchy until the callback says to stop or we reach UDecoupledOutputProvider
		const FOutputProviderClassMetaData* Data = RegisteredFactories.Find(Class);
		const EBreakBehavior BreakBehavior = Data
			? Callback(*Class, *Data)
			: EBreakBehavior::Continue;
		if (BreakBehavior == EBreakBehavior::Continue)
		{
			ForEachRegisteredClassInClassHierarchy(Class->GetSuperClass(), Callback);
		}
	}

	TPair<UClass*, FDecoupledOutputProviderModule::FOutputProviderClassMetaData*> FDecoupledOutputProviderModule::FindRegisteredFactoryInClassHierarchy(UClass* Class)
	{
		TPair<UClass*, const FOutputProviderClassMetaData*> Result = const_cast<const FDecoupledOutputProviderModule*>(this)->FindRegisteredFactoryInClassHierarchy(Class);
		return { Result.Key, const_cast<FOutputProviderClassMetaData*>(Result.Value) };
	}

	TPair<UClass*, const FDecoupledOutputProviderModule::FOutputProviderClassMetaData*> FDecoupledOutputProviderModule::FindRegisteredFactoryInClassHierarchy(UClass* Class) const
	{
		UClass* ResultClass = nullptr;
		const FOutputProviderClassMetaData* FoundData = nullptr;
		// Walk up the class hierarchy and return the first registered factory (the parent closest to the passed in Class)
		ForEachRegisteredClassInClassHierarchy(Class, [&ResultClass, &FoundData](UClass& Class, const FOutputProviderClassMetaData& Data)
		{
			ResultClass = &Class;
			FoundData = &Data;
			return EBreakBehavior::Break;
		});
		return { ResultClass, FoundData };
	}

	void FDecoupledOutputProviderModule::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
	{
		// It's incorrect to call ActiveLogic.Reset() because there are many worlds, e.g. PIE, editor, editor preview, etc.
		for (auto ActiveLogicIterator = ActiveLogic.CreateIterator(); ActiveLogicIterator; ++ActiveLogicIterator)
		{
			if (!ActiveLogicIterator.Key().IsValid() || ActiveLogicIterator.Key()->IsIn(World))
			{
				ActiveLogicIterator.RemoveCurrent();
			}
		}
	}
}

IMPLEMENT_MODULE(UE::DecoupledOutputProvider::Private::FDecoupledOutputProviderModule, DecoupledOutputProvider);
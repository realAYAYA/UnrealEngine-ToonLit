// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class LEVELSTREAMINGPERSISTENCE_API ILevelStreamingPersistenceModule : public IModuleInterface
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FShouldPersistProperty, const UObject*, const FProperty*);
	DECLARE_DELEGATE_RetVal_TwoParams(void, FPostRestorePersistedProperty, const UObject*, const FProperty*);

	template<typename ClassType>
	FShouldPersistProperty& OnShouldPersistProperty()
	{
		return OnShouldPersistProperty(ClassType::StaticClass());
	}

	FShouldPersistProperty& OnShouldPersistProperty(const UClass* InClass)
	{
		return ClassShouldPersistProperty.FindOrAdd(InClass);
	}

	template<typename ClassType>
	FPostRestorePersistedProperty& OnPostRestorePersistedProperty()
	{
		return OnPostRestorePersistedProperty(ClassType::StaticClass());
	}

	FPostRestorePersistedProperty& OnPostRestorePersistedProperty(const UClass* InClass)
	{
		return ClassPostRestorePersistedProperty.FindOrAdd(InClass);
	}

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline ILevelStreamingPersistenceModule& Get()
	{
		return FModuleManager::LoadModuleChecked<ILevelStreamingPersistenceModule>("LevelStreamingPersistence");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("LevelStreamingPersistence");
	}

private:

	bool ShouldPersistProperty(const UObject* InObject, const FProperty* InProperty) const
	{
		const UClass* Class = InObject->GetClass();
		while (Class)
		{
			const FShouldPersistProperty* Found = ClassShouldPersistProperty.Find(Class);
			if (Found && Found->IsBound() && !Found->Execute(InObject, InProperty))
			{
				return false;
			}
			Class = Class->GetSuperClass();
		};
		return true;
	}

	void PostRestorePersistedProperty(const UObject* InObject, const FProperty* InProperty) const
	{
		const UClass* Class = InObject->GetClass();
		while (Class)
		{
			if (const FPostRestorePersistedProperty* Found = ClassPostRestorePersistedProperty.Find(Class))
			{
				Found->ExecuteIfBound(InObject, InProperty);
			}
			Class = Class->GetSuperClass();
		};
	}
	
	TMap<TWeakObjectPtr<const UClass>, FShouldPersistProperty> ClassShouldPersistProperty;
	TMap<TWeakObjectPtr<const UClass>, FPostRestorePersistedProperty> ClassPostRestorePersistedProperty;

	friend class ULevelStreamingPersistenceManager;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#include "FakeObjectToPropertiesEditorModel.h"

#include "Replication/Editor/Model/Property/IPropertySelectionSourceModel.h"
#include "Replication/Editor/Model/Property/IPropertySourceModel.h"
#include "Replication/ObjectUtils.h"
#include "Replication/PropertyChainUtils.h"

namespace UE::ConcertSharedSlate
{
	FSoftClassPath FFakeObjectToPropertiesEditorModel::GetObjectClass(const FSoftObjectPath& Object) const
	{
		const FSoftClassPath ResolvedClass = RealModel->GetObjectClass(Object);
		if (ResolvedClass.IsValid())
		{
			return ResolvedClass;
		}

#if WITH_EDITOR
		// The object is not yet in the model.
		const UObject* LoadedObject = Object.ResolveObject();
		return LoadedObject ? LoadedObject->GetClass() : FSoftClassPath{};
#else
		return FSoftClassPath{};
#endif
	}

	bool FFakeObjectToPropertiesEditorModel::ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const
	{
		return IterateObjects(Delegate);
	}

	bool FFakeObjectToPropertiesEditorModel::ForEachProperty(const FSoftObjectPath& ObjectPath, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const
	{
		IterateDisplayedProperties(ObjectPath, Delegate);
		return true;
	}

	bool FFakeObjectToPropertiesEditorModel::IterateObjects(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const
	{
		// Case: RealModel contains only components but not the owning actor.
		// In that case, we want the UI to still show the owning actor.
		// We'll track this with these containers:
		TSet<FSoftObjectPath> Actors;
		TSet<FSoftObjectPath> NonActors;

		// Here we only exposes objects that are top level so we skip components, etc.
		EBreakBehavior BreakBehavior = EBreakBehavior::Continue;
		const bool bResult = RealModel->ForEachReplicatedObject([this, &Delegate, &Actors, &NonActors, &BreakBehavior](const FSoftObjectPath& Object)
		{
			if (ObjectUtils::IsActor(Object))
			{
				NonActors.Add(Object);
				BreakBehavior = Delegate(Object); 
				return BreakBehavior;
			}
			
			Actors.Add(Object);
			return EBreakBehavior::Continue;
		});

		// No additional objects to show
		if (BreakBehavior == EBreakBehavior::Break)
		{
			return bResult;
		}

		// Now determine the top level objects that are not in RealModel but that need to be shown for subobjects that are in RealModel
		for (const FSoftObjectPath& NonTopLevelObject : Actors)
		{
			const TOptional<FSoftObjectPath> TopLevelObject = ObjectUtils::GetActorOf(NonTopLevelObject);
			if (!TopLevelObject)
			{
				continue;
			}
			
			if (!NonActors.Contains(*TopLevelObject))
			{
				if (Delegate(*TopLevelObject) == EBreakBehavior::Break)
				{
					return true;
				}

				// Avoid calling Delegate again for TopLevelObject
				NonActors.Add(*TopLevelObject);
			}
		}

		return bResult;
	}

	void FFakeObjectToPropertiesEditorModel::IterateDisplayedProperties(const FSoftObjectPath& ObjectPath, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const
	{
		// GetObjectClass might return null because ObjectPath might not be contained in the model
		const FSoftClassPath ClassPath = GetObjectClass(ObjectPath);
		// This makes SObjectToPropertyView list all available properties
		PropertySelectionSource->GetPropertySource(ClassPath)
		   ->EnumerateSelectableItems([&Delegate](const FSelectablePropertyInfo& PropertyInfo)
		   {
			   return Delegate(PropertyInfo.Property);
		   });
	}
}

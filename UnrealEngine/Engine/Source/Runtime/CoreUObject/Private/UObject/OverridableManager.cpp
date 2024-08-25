// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/OverridableManager.h"
#include "InstancedReferenceSubobjectHelper.h"
#include "UObject/UObjectGlobals.h"

/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 */
 
FOverridableManager& FOverridableManager::Get()
{
	static FOverridableManager OverridableManager;
	return OverridableManager;
}

bool FOverridableManager::IsEnabled(const UObject& Object)
{
	return OverriddenObjectAnnotations.IsEnabled(Object);
}

void FOverridableManager::Enable(UObject& Object)
{
	OverriddenObjectAnnotations.FindOrAdd(Object);
}

void FOverridableManager::Disable(UObject& Object)
{
	OverriddenObjectAnnotations.RemoveAnnotation(&Object);
}

void FOverridableManager::InheritEnabledFrom(UObject& Object, const UObject* DefaultData)
{
	if (!OverriddenObjectAnnotations.IsEnabled(Object))
	{
		const UObject* Outer = Object.GetOuter();
		if ((Outer && IsEnabled(*Outer)) || (DefaultData && IsEnabled(*DefaultData)))
		{
			Enable(Object);
		}
	}
}

bool FOverridableManager::NeedSubObjectTemplateInstantiation(const UObject& Object)
{
	if( const FOverriddenPropertySet* OverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		return OverriddenProperties->bNeedsSubobjectTemplateInstantiation;
	}
	return false;
}

FOverriddenPropertySet* FOverridableManager::GetOverriddenProperties(UObject& Object)
{
	return OverriddenObjectAnnotations.Find(Object);
}

const FOverriddenPropertySet* FOverridableManager::GetOverriddenProperties(const UObject& Object)
{
	return OverriddenObjectAnnotations.Find(Object);
}

FOverriddenPropertySet& FOverridableManager::SetOverriddenProperties(UObject& Object, EOverriddenPropertyOperation Operation)
{
	FOverriddenPropertySet& ObjectOverriddenProperties = OverriddenObjectAnnotations.FindOrAdd(Object);
	ObjectOverriddenProperties.Reset();
	ObjectOverriddenProperties.SetOverriddenPropertyOperation(Operation, /*CurrentPropertyChain*/nullptr, /*Property*/nullptr);
	return ObjectOverriddenProperties;
}

EOverriddenState FOverridableManager::GetOverriddenState(UObject& Object)
{
	if(const FOverriddenPropertySet* OverriddenProperties = GetOverriddenProperties(Object))
	{
		// Consider any object that its template is a CDO as added.
		if (UObject* Archetype = Object.GetArchetype())
		{
			if (Archetype->HasAnyFlags(RF_ClassDefaultObject))
			{
				return EOverriddenState::Added;
			}
		}

		const EOverriddenPropertyOperation Operation = OverriddenProperties->GetOverriddenPropertyOperation((FArchiveSerializedPropertyChain*)nullptr, (FProperty*)nullptr);
		if (Operation != EOverriddenPropertyOperation::None)
		{
			return Operation == EOverriddenPropertyOperation::Replace ? EOverriddenState::AllOverridden : EOverriddenState::HasOverrides;
		}

		// Need to check subobjects to 
		TSet<UObject*> InstancedSubObjects;
		FFindInstancedReferenceSubobjectHelper::GetInstancedSubObjects(&Object, InstancedSubObjects);
		for (UObject* InstancedSubObject : InstancedSubObjects)
		{
			if (InstancedSubObject && InstancedSubObject->IsIn(&Object))
			{
				if (GetOverriddenState(*InstancedSubObject) != EOverriddenState::NoOverrides)
				{
					return EOverriddenState::HasOverrides;
				}
			}
		}
	}
	return EOverriddenState::NoOverrides;
}

void FOverridableManager::OverrideObject(UObject& Object)
{
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		// Passing no property node means we are overriding the object itself
		ThisObjectOverriddenProperties->OverrideProperty(FPropertyChangedEvent(nullptr), /*PropertyNode*/nullptr, &Object);
	}
}

void FOverridableManager::OverrideInstancedSubObject(UObject& Object, UObject& InstancedSubObject)
{
	if (InstancedSubObject.IsIn(&Object))
	{
		OverrideObject(InstancedSubObject);
	}
}

void FOverridableManager::PropagateOverrideToInstancedSubObjects(UObject& Object)
{
	TSet<UObject*> InstancedSubObjects;
	FFindInstancedReferenceSubobjectHelper::GetInstancedSubObjects(&Object, InstancedSubObjects);
	for (UObject* InstancedSubObject : InstancedSubObjects)
	{
		checkf(InstancedSubObject, TEXT("Expecting non null SubObjects"));
		OverrideInstancedSubObject(Object, *InstancedSubObject);
	}
}

void FOverridableManager::OverrideProperty(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain& PropertyChain)
{
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		ThisObjectOverriddenProperties->OverrideProperty(PropertyEvent, PropertyChain.GetActiveMemberNode() ? PropertyChain.GetActiveMemberNode() : PropertyChain.GetHead(), &Object);
	}
}

bool FOverridableManager::ClearOverriddenProperty(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode)
{
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		return ThisObjectOverriddenProperties->ClearOverriddenProperty(PropertyEvent, PropertyNode);
	}
	return false;
}

void FOverridableManager::PreOverrideProperty(UObject& Object, const FEditPropertyChain& PropertyChain)
{
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		ThisObjectOverriddenProperties->NotifyPropertyChange(EPropertyNotificationType::PreEdit, FPropertyChangedEvent(nullptr), PropertyChain.GetActiveMemberNode() ? PropertyChain.GetActiveMemberNode() : PropertyChain.GetHead(), &Object);
	}
}

void FOverridableManager::PostOverrideProperty(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain& PropertyChain)
{
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		ThisObjectOverriddenProperties->NotifyPropertyChange(EPropertyNotificationType::PostEdit, PropertyEvent, PropertyChain.GetActiveMemberNode() ? PropertyChain.GetActiveMemberNode() : PropertyChain.GetHead(), &Object);
	}
}

void FOverridableManager::NotifyPropertyChange(const EPropertyNotificationType Notification, UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode)
{
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		ThisObjectOverriddenProperties->NotifyPropertyChange(Notification, PropertyEvent, PropertyNode, &Object);
	}
}

EOverriddenPropertyOperation FOverridableManager::GetOverriddenPropertyOperation(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, bool* bOutInheritedOperation)
{
	if (const FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		return ThisObjectOverriddenProperties->GetOverriddenPropertyOperation(PropertyEvent, PropertyNode, bOutInheritedOperation);
	}
	return EOverriddenPropertyOperation::None;
}

void FOverridableManager::ClearOverrides(UObject& Object)
{
	if(FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		ThisObjectOverriddenProperties->Reset();
		PropagateClearOverridesToInstancedSubObjects(Object);
	}
}

void FOverridableManager::ClearInstancedSubObjectOverrides(UObject& Object, UObject& InstancedSubObject)
{
	if (InstancedSubObject.IsIn(&Object))
	{
		ClearOverrides(InstancedSubObject);
	}
}

void FOverridableManager::PropagateClearOverridesToInstancedSubObjects(UObject& Object)
{
	TSet<UObject*> InstancedSubObjects;
	FFindInstancedReferenceSubobjectHelper::GetInstancedSubObjects(&Object, InstancedSubObjects);
	for (UObject* InstancedSubObject : InstancedSubObjects)
	{
		checkf(InstancedSubObject, TEXT("Expecting non null SubObjects"));

		// There are some cases where the property has information about that should be an instanced subobject, but it is not owned by us.
		ClearInstancedSubObjectOverrides(Object, *InstancedSubObject);
	}
}

void FOverridableManager::SerializeOverriddenProperties(UObject& Object, FStructuredArchive::FRecord ObjectRecord)
{
	const FArchiveState& ArchiveState = ObjectRecord.GetArchiveState();
	FOverriddenPropertySet* OverriddenProperties = ArchiveState.IsSaving() ? GetOverriddenProperties(Object) : nullptr;
	TOptional<FStructuredArchiveSlot> OverridenPropertiesSlot = ObjectRecord.TryEnterField(TEXT("OverridenProperties"), OverriddenProperties != nullptr);
	if (OverridenPropertiesSlot.IsSet())
	{
		EOverriddenPropertyOperation Operation = OverriddenProperties ? OverriddenProperties->GetOverriddenPropertyOperation((FArchiveSerializedPropertyChain*)nullptr, (FProperty*)nullptr) : EOverriddenPropertyOperation::None;
		*OverridenPropertiesSlot << SA_ATTRIBUTE( TEXT("OverriddenOperation"), Operation);

		if (ArchiveState.IsLoading())
		{
			OverriddenProperties = &SetOverriddenProperties(Object, Operation);
		}

		if (Operation != EOverriddenPropertyOperation::None)
		{
			FOverriddenPropertySet::StaticStruct()->SerializeItem(*OverridenPropertiesSlot, OverriddenProperties, /* Defaults */ nullptr);
		}
	}
	else if (ArchiveState.IsLoading())
	{
		Disable(Object);
	}
}

FOverridableManager::FOverridableManager()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReinstanced.AddRaw(this, &FOverridableManager::HandleObjectsReInstantiated);
#endif
}

void FOverridableManager::HandleObjectsReInstantiated(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	const TMap<const UObjectBase *, FOverriddenPropertyAnnotation>& AnnotationMap = OverriddenObjectAnnotations.GetAnnotationMap();
	for (const auto& Pair : AnnotationMap)
	{
		if( FOverriddenPropertySet* OverridenProperties = Pair.Value.OverriddenProperties.Get())
		{
			OverridenProperties->HandleObjectsReInstantiated(OldToNewInstanceMap);
		}
	}
}

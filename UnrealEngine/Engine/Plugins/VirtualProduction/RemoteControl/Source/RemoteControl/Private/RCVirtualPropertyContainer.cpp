// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCVirtualPropertyContainer.h"

#include "UObject/StructOnScope.h"
#include "Templates/SubclassOf.h"

#include "RCVirtualProperty.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "URCVirtualPropertyInContainer"

void URCVirtualPropertyContainerBase::AddVirtualProperty(URCVirtualPropertyBase* InVirtualProperty)
{
	if(ensure(InVirtualProperty))
	{
		// Add property to Set
		VirtualProperties.Add(InVirtualProperty);
	}
}

URCVirtualPropertyInContainer* URCVirtualPropertyContainerBase::AddProperty(const FName& InPropertyName, TSubclassOf<URCVirtualPropertyInContainer> InPropertyClass, const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject, TArray<FPropertyBagPropertyDescMetaData> MetaData/* = TArray<FPropertyBagPropertyDescMetaData>()*/)
{
	const FName PropertyName = GenerateUniquePropertyName(InPropertyName, InValueType, InValueTypeObject, this);

	FPropertyBagPropertyDesc PropertyBagDesc = FPropertyBagPropertyDesc(PropertyName, InValueType, InValueTypeObject);

#if WITH_EDITORONLY_DATA
	PropertyBagDesc.MetaData = MetaData;
#endif

	Bag.AddProperties({ PropertyBagDesc });

	// Ensure that the property has been successfully added to the Bag
	const FPropertyBagPropertyDesc* BagPropertyDesc = Bag.FindPropertyDescByName(PropertyName);
	if (!ensure(BagPropertyDesc))
	{
		return nullptr;
	}
	
	// Create Property in Container
	URCVirtualPropertyInContainer* VirtualPropertyInContainer = NewObject<URCVirtualPropertyInContainer>(this, InPropertyClass.Get(), NAME_None, RF_Transactional);
	VirtualPropertyInContainer->PropertyName = PropertyName;
	VirtualPropertyInContainer->DisplayName = PropertyName;
	VirtualPropertyInContainer->PresetWeakPtr = PresetWeakPtr;
	VirtualPropertyInContainer->ContainerWeakPtr = this;
	VirtualPropertyInContainer->Id = FGuid::NewGuid();

	ControllerLabelToIdCache.Add(PropertyName, VirtualPropertyInContainer->Id);
	AddVirtualProperty(VirtualPropertyInContainer);

	return VirtualPropertyInContainer;
}

URCVirtualPropertyInContainer* URCVirtualPropertyContainerBase::DuplicateProperty(const FName& InPropertyName, const FProperty* InSourceProperty, TSubclassOf<URCVirtualPropertyInContainer> InPropertyClass)
{
	// Ensure that the property being duplicated is not already a part of the Bag (not to be confused with a similar looking ensure performed above in AddProperty, this is the exact inverse!)
	const FPropertyBagPropertyDesc* BagPropertyDesc = Bag.FindPropertyDescByName(InPropertyName);
	if (!ensure(!BagPropertyDesc))
	{
		return nullptr;
	}

	Bag.AddProperty(InPropertyName, InSourceProperty);
	
	URCVirtualPropertyInContainer* VirtualPropertyInContainer = NewObject<URCVirtualPropertyInContainer>(this, InPropertyClass.Get());
	VirtualPropertyInContainer->PropertyName = InPropertyName;
	VirtualPropertyInContainer->DisplayName = GenerateUniqueDisplayName(VirtualPropertyInContainer->DisplayName, this);;
	VirtualPropertyInContainer->PresetWeakPtr = PresetWeakPtr;
	VirtualPropertyInContainer->ContainerWeakPtr = this;
	VirtualPropertyInContainer->Id = FGuid::NewGuid();

	ControllerLabelToIdCache.Add(InPropertyName, VirtualPropertyInContainer->Id);
	AddVirtualProperty(VirtualPropertyInContainer);

	return VirtualPropertyInContainer;
}

URCVirtualPropertyInContainer* URCVirtualPropertyContainerBase::DuplicatePropertyWithCopy(const FName& InPropertyName, const FProperty* InSourceProperty, const uint8* InSourceContainerPtr, TSubclassOf<URCVirtualPropertyInContainer> InPropertyClass)
{
	if (InSourceContainerPtr == nullptr)
	{
		return nullptr;
	}
	
	URCVirtualPropertyInContainer* VirtualPropertyInContainer = DuplicateProperty(InPropertyName, InSourceProperty, InPropertyClass);
	if (VirtualPropertyInContainer == nullptr)
	{
		return nullptr;
	}

	const FPropertyBagPropertyDesc* BagPropertyDesc = Bag.FindPropertyDescByName(InPropertyName);
	check(BagPropertyDesc); // Property bag should be exists after DuplicateProperty()

	ensure(Bag.SetValue(InPropertyName, InSourceProperty, InSourceContainerPtr) == EPropertyBagResult::Success);

	return VirtualPropertyInContainer;
}

URCVirtualPropertyInContainer* URCVirtualPropertyContainerBase::DuplicateVirtualProperty(URCVirtualPropertyInContainer* InVirtualProperty)
{
	if (URCVirtualPropertyInContainer* NewVirtualProperty = DuplicateObject<URCVirtualPropertyInContainer>(InVirtualProperty, InVirtualProperty->GetOuter()))
	{
		NewVirtualProperty->PropertyName = GenerateUniquePropertyName(InVirtualProperty->PropertyName, this);
		NewVirtualProperty->DisplayName = GenerateUniqueDisplayName(InVirtualProperty->DisplayName, this);
		NewVirtualProperty->Id = FGuid::NewGuid();

		// Sync Property Bag
		Bag.AddProperty(NewVirtualProperty->PropertyName, InVirtualProperty->GetProperty());

		// Ensure that the property has been successfully added to the Bag
		const FPropertyBagPropertyDesc* BagPropertyDesc = Bag.FindPropertyDescByName(NewVirtualProperty->PropertyName);
		if (!ensure(BagPropertyDesc))
		{
			return nullptr;
		}

		// Sync Virtual Properties List
		AddVirtualProperty(NewVirtualProperty);

		// Sync Cache
		ControllerLabelToIdCache.Add(NewVirtualProperty->DisplayName, NewVirtualProperty->Id);

		return NewVirtualProperty;
	}

	return nullptr;
}

bool URCVirtualPropertyContainerBase::RemoveProperty(const FName& InPropertyName)
{
	Bag.RemovePropertyByName(InPropertyName);

	for (TSet<TObjectPtr<URCVirtualPropertyBase>>::TIterator PropertiesIt = VirtualProperties.CreateIterator(); PropertiesIt; ++PropertiesIt)
	{
		if (const URCVirtualPropertyBase* VirtualProperty = *PropertiesIt)
		{
			if (VirtualProperty->PropertyName == InPropertyName)
			{
				ControllerLabelToIdCache.Remove(VirtualProperty->DisplayName);
				PropertiesIt.RemoveCurrent();
				return true;
			}
		}
	}

	return false;
}

void URCVirtualPropertyContainerBase::Reset()
{
	VirtualProperties.Empty();
	ControllerLabelToIdCache.Reset();
	Bag.Reset();
}

URCVirtualPropertyBase* URCVirtualPropertyContainerBase::GetVirtualProperty(const FName InPropertyName) const
{
	for (URCVirtualPropertyBase* VirtualProperty : VirtualProperties)
	{
		if (!ensure(VirtualProperty))
		{
			continue;
		}

		if (VirtualProperty->PropertyName == InPropertyName)
		{
			return VirtualProperty;
		}
	}
	
	return nullptr;
}

URCVirtualPropertyBase* URCVirtualPropertyContainerBase::GetVirtualProperty(const FGuid& InId) const
{
	for (URCVirtualPropertyBase* VirtualProperty : VirtualProperties)
	{
		if (VirtualProperty->Id == InId)
		{
			return VirtualProperty;
		}
	}

	return nullptr;
}

URCVirtualPropertyBase* URCVirtualPropertyContainerBase::GetVirtualPropertyByDisplayName(const FName InDisplayName) const
{
	if (const FGuid* ControllerId = ControllerLabelToIdCache.Find(InDisplayName))
	{
		if (URCVirtualPropertyBase* Controller = GetVirtualProperty(*ControllerId))
		{
			return Controller;
		}
	}

	for (URCVirtualPropertyBase* VirtualProperty : VirtualProperties)
	{
		if (VirtualProperty->DisplayName == InDisplayName)
		{
			return VirtualProperty;
		}
	}

	return nullptr;
}

URCVirtualPropertyBase* URCVirtualPropertyContainerBase::GetVirtualPropertyByFieldId(const FName InFieldId) const
{
	for (URCVirtualPropertyBase* VirtualProperty : VirtualProperties)
	{
		if (VirtualProperty->FieldId == InFieldId)
		{
			return VirtualProperty;
		}
	}

	return nullptr;
}

URCVirtualPropertyBase* URCVirtualPropertyContainerBase::GetVirtualPropertyByFieldId(const FName InFieldId, const EPropertyBagPropertyType InType) const
{
	const TArray<URCVirtualPropertyBase*>& VirtualPropertiesByFieldId = GetVirtualPropertiesByFieldId(InFieldId);

	for (URCVirtualPropertyBase* VirtualProperty : VirtualPropertiesByFieldId)
	{
		if (VirtualProperty->GetValueType() == InType)
		{
			return VirtualProperty;
		}
	}

	return nullptr;
}

TArray<URCVirtualPropertyBase*> URCVirtualPropertyContainerBase::GetVirtualPropertiesByFieldId(const FName InFieldId) const
{
	TArray<URCVirtualPropertyBase*> VirtualPropertiesByFieldId;
	
	for (URCVirtualPropertyBase* VirtualProperty : VirtualProperties)
	{
		if (VirtualProperty->FieldId == InFieldId)
		{
			VirtualPropertiesByFieldId.Add(VirtualProperty);
		}
	}

	return VirtualPropertiesByFieldId;
}

int32 URCVirtualPropertyContainerBase::GetNumVirtualProperties() const
{
	const int32 NumPropertiesInBag = Bag.GetNumPropertiesInBag();
	const int32 NumVirtualProperties = VirtualProperties.Num();

	check(NumPropertiesInBag == NumVirtualProperties);

	return NumPropertiesInBag;
}

TSharedPtr<FStructOnScope> URCVirtualPropertyContainerBase::CreateStructOnScope()
{
	return MakeShared<FStructOnScope>(Bag.GetPropertyBagStruct(), Bag.GetMutableValue().GetMemory());
}

FName URCVirtualPropertyContainerBase::SetControllerDisplayName(FGuid InGuid, FName InNewName)
{
	if (URCVirtualPropertyBase* Controller = GetVirtualProperty(InGuid))
	{
		Controller->Modify();

		ControllerLabelToIdCache.Remove(Controller->DisplayName);
		Controller->DisplayName = GenerateUniqueDisplayName(InNewName, this);
		ControllerLabelToIdCache.Add(Controller->DisplayName, Controller->Id);

		return Controller->DisplayName;
	}

	return NAME_None;
}

FName URCVirtualPropertyContainerBase::GenerateUniquePropertyName(const FName& InPropertyName, const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject, const URCVirtualPropertyContainerBase* InContainer)
{
	FName BaseName = InPropertyName;
	if (BaseName.IsNone())
	{
		BaseName = URCVirtualPropertyBase::GetVirtualPropertyTypeDisplayName(InValueType, InValueTypeObject);
	}

	return GenerateUniquePropertyName(BaseName, InContainer);
}

FName URCVirtualPropertyContainerBase::GenerateUniquePropertyName(const FName& InPropertyName, const URCVirtualPropertyContainerBase* InContainer)
{
	auto GetFinalName = [](const FString& InPrefix, const int32 InIndex = 0)
	{
		FString FinalName = InPrefix;

		if (InIndex > 0)
		{
			FinalName += TEXT("_") + FString::FromInt(InIndex);
		}

		return FinalName;
	};

	int32 Index = 0;
	const FString InitialName = InPropertyName.ToString();
	FString FinalName = InitialName;

	// Recursively search for an available name by incrementing suffix till we find one.
	const FPropertyBagPropertyDesc* PropertyDesc = InContainer->Bag.FindPropertyDescByName(*FinalName);
	while (PropertyDesc)
	{
		++Index;
		FinalName = GetFinalName(InitialName, Index);
		PropertyDesc = InContainer->Bag.FindPropertyDescByName(*FinalName);
	}

	return *FinalName;
}

FName URCVirtualPropertyContainerBase::GenerateUniqueDisplayName(const FName& InPropertyName, const URCVirtualPropertyContainerBase* InContainer)
{
	auto GetFinalName = [](const FName& InPrefix, const int32 InIndex = 0)
	{
		FName FinalName = InPrefix;

		if (InIndex > 0)
		{
			FinalName = FName(FinalName.ToString() + TEXT("_") + FString::FromInt(InIndex));
		}

		return FinalName;
	};

	int32 Index = 0;
	const FName InitialName = InPropertyName;
	FName FinalName = InitialName;

	// Recursively search for an available name by incrementing suffix till we find one.
	bool bControllerExist = InContainer->ControllerLabelToIdCache.Contains(FinalName);
	while (bControllerExist)
	{
		++Index;
		FinalName = GetFinalName(InitialName, Index);
		bControllerExist = InContainer->ControllerLabelToIdCache.Contains(FinalName);
	}

	return FinalName;
}

void URCVirtualPropertyContainerBase::UpdateEntityIds(const TMap<FGuid, FGuid>& InEntityIdMap)
{
	for (const TObjectPtr<URCVirtualPropertyBase>& VirtualProperty : VirtualProperties)
	{
		if (VirtualProperty)
		{
			VirtualProperty->UpdateEntityIds(InEntityIdMap);
		}
	}
}

void URCVirtualPropertyContainerBase::CacheControllersLabels()
{
	ControllerLabelToIdCache.Reset();
	for (const URCVirtualPropertyBase* Controller : VirtualProperties)
	{
		if (Controller)
		{
			if (!ControllerLabelToIdCache.Contains(Controller->DisplayName))
			{
				ControllerLabelToIdCache.Add(Controller->DisplayName, Controller->Id);
			}
		}
	}
}

void URCVirtualPropertyContainerBase::FixAndCacheControllersLabels()
{
	ControllerLabelToIdCache.Reset();
	for (URCVirtualPropertyBase* Controller : VirtualProperties)
	{
		if (Controller)
		{
			if (!ControllerLabelToIdCache.Contains(Controller->DisplayName))
			{
				ControllerLabelToIdCache.Add(Controller->DisplayName, Controller->Id);
			}
			// Cases for older presets where you could have the same DisplayName, and if it is already saved then we need to change it now before caching it
			else
			{
				const FName NewControllerName = GenerateUniqueDisplayName(Controller->DisplayName, this);
				Controller->DisplayName = NewControllerName;
				ControllerLabelToIdCache.Add(NewControllerName, Controller->Id);
			}
		}
	}
}

#if WITH_EDITOR
void URCVirtualPropertyContainerBase::PostEditUndo()
{
	Super::PostEditUndo();

	OnVirtualPropertyContainerModifiedDelegate.Broadcast();
}

void URCVirtualPropertyContainerBase::OnModifyPropertyValue(const FPropertyChangedEvent& PropertyChangedEvent)
{
	const FScopedTransaction Transaction(LOCTEXT("OnModifyPropertyValue", "On Modify Property Value"));

	Modify();

	MarkPackageDirty();
}
#endif

#undef LOCTEXT_NAMESPACE

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
	VirtualPropertyInContainer->PresetWeakPtr = PresetWeakPtr;
	VirtualPropertyInContainer->ContainerWeakPtr = this;
	VirtualPropertyInContainer->Id = FGuid::NewGuid();

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
	VirtualPropertyInContainer->PresetWeakPtr = PresetWeakPtr;
	VirtualPropertyInContainer->ContainerWeakPtr = this;
	VirtualPropertyInContainer->Id = FGuid::NewGuid();

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

		return NewVirtualProperty;
	}

	return nullptr;
}

bool URCVirtualPropertyContainerBase::RemoveProperty(const FName& InPropertyName)
{
	Bag.RemovePropertyByName(InPropertyName);

	for (auto PropertiesIt = VirtualProperties.CreateIterator(); PropertiesIt; ++PropertiesIt)
	{
		if (const URCVirtualPropertyBase* VirtualProperty = *PropertiesIt)
		{
			if (VirtualProperty->PropertyName == InPropertyName)
			{
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
	for (URCVirtualPropertyBase* VirtualProperty : VirtualProperties)
	{
		if (VirtualProperty->DisplayName == InDisplayName)
		{
			return VirtualProperty;
		}
	}

	return nullptr;
}

int32 URCVirtualPropertyContainerBase::GetNumVirtualProperties() const
{
	const int32 NumPropertiesInBag = Bag.GetNumPropertiesInBag();
	const int32 NumVirtualProperties = VirtualProperties.Num();

	check(NumPropertiesInBag == NumVirtualProperties);

	return NumPropertiesInBag;
}

TSharedPtr<FStructOnScope> URCVirtualPropertyContainerBase::CreateStructOnScope() const
{
	return MakeShared<FStructOnScope>(Bag.GetPropertyBagStruct(), Bag.GetMutableValue().GetMutableMemory());
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
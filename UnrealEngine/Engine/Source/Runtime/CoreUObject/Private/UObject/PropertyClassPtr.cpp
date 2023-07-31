// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyHelper.h"
#include "UObject/UnrealType.h"
#include "UObject/LinkerPlaceholderExportObject.h"
#include "UObject/LinkerPlaceholderClass.h"

/*-----------------------------------------------------------------------------
	FClassPtrProperty.
-----------------------------------------------------------------------------*/
IMPLEMENT_FIELD(FClassPtrProperty)

FString FClassPtrProperty::GetCPPType(FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/) const
{
	return FString::Printf(TEXT("TObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}
FString FClassPtrProperty::GetCPPMacroType(FString& ExtendedTypeText) const
{
	ExtendedTypeText = FString::Printf(TEXT("TObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("OBJECTPTR");
}

void FClassPtrProperty::SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const
{
	// Share serialization code with FObjectPtrProperty
	FObjectPtrProperty::StaticSerializeItem(this, Slot, Value, Defaults);
}

bool FClassPtrProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	FObjectPtr ObjectA = A ? *((FObjectPtr*)A) : FObjectPtr();
	FObjectHandle ObjectAHandle = ObjectA.GetHandle();
	FObjectPtr ObjectB = B ? *((FObjectPtr*)B) : FObjectPtr();
	FObjectHandle ObjectBHandle = ObjectB.GetHandle();

	return ObjectAHandle == ObjectBHandle;
}

bool FClassPtrProperty::SameType(const FProperty* Other) const
{
	bool bPropertyTypeMatch = FObjectProperty::SameType(Other) ||
								(Other && Other->IsA<FClassProperty>() &&
									(PropertyClass == ((FObjectPropertyBase*)Other)->PropertyClass) );
	return bPropertyTypeMatch && (MetaClass == ((FClassProperty*)Other)->MetaClass);
}

UObject* FClassPtrProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return ((FObjectPtr&)GetPropertyValue(PropertyValueAddress)).Get();
}

UObject* FClassPtrProperty::GetObjectPropertyValue_InContainer(const void* ContainerAddress, int32 ArrayIndex) const
{
	return GetWrappedObjectPropertyValue_InContainer<FObjectPtr>(ContainerAddress, ArrayIndex);
}

void FClassPtrProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	if (Value || !HasAnyPropertyFlags(CPF_NonNullable))
	{
		SetPropertyValue(PropertyValueAddress, TCppType(Value));
	}
	else
	{
		UE_LOG(LogProperty, Verbose /*Warning*/, TEXT("Trying to assign null object value to non-nullable \"%s\""), *GetFullName());
	}
}

void FClassPtrProperty::SetObjectPropertyValue_InContainer(void* ContainerAddress, UObject* Value, int32 ArrayIndex) const
{
	if (Value || !HasAnyPropertyFlags(CPF_NonNullable))
	{
		SetWrappedObjectPropertyValue_InContainer<FObjectPtr>(ContainerAddress, Value, ArrayIndex);
	}
	else
	{
		UE_LOG(LogProperty, Verbose /*Warning*/, TEXT("Trying to assign null object value to non-nullable \"%s\""), *GetFullName());
	}
}

uint32 FClassPtrProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash((FObjectPtr&)GetPropertyValue(Src));
}


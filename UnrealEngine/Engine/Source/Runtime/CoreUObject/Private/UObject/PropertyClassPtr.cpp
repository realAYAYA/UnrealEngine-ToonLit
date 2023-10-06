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
	UObject* Result = nullptr;
	GetWrappedUObjectPtrValues<FObjectPtr>(&Result, ContainerAddress, EPropertyMemoryAccess::InContainer, ArrayIndex, 1);
	return Result;
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
		SetWrappedUObjectPtrValues<FObjectPtr>(ContainerAddress, EPropertyMemoryAccess::InContainer, &Value, ArrayIndex, 1);
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

void FClassPtrProperty::CopySingleValueToScriptVM( void* Dest, void const* Src ) const
{
	*(UObject**)Dest = ((const FObjectPtr*)Src)->Get();
}

void FClassPtrProperty::CopySingleValueFromScriptVM( void* Dest, void const* Src ) const
{
	*(FObjectPtr*)Dest = *(UObject**)Src;
}

void FClassPtrProperty::CopyCompleteValueToScriptVM( void* Dest, void const* Src ) const
{
	GetWrappedUObjectPtrValues<FObjectPtr>((UObject**)Dest, Src, EPropertyMemoryAccess::Direct, 0, ArrayDim);
}

void FClassPtrProperty::CopyCompleteValueFromScriptVM( void* Dest, void const* Src ) const
{
	SetWrappedUObjectPtrValues<FObjectPtr>(Dest, EPropertyMemoryAccess::Direct, (UObject**)Src, 0, ArrayDim);
}

void FClassPtrProperty::CopyCompleteValueToScriptVM_InContainer(void* OutValue, void const* InContainer) const
{
	GetWrappedUObjectPtrValues<FObjectPtr>((UObject**)OutValue, InContainer, EPropertyMemoryAccess::InContainer, 0, ArrayDim);
}

void FClassPtrProperty::CopyCompleteValueFromScriptVM_InContainer(void* OutContainer, void const* InValue) const
{
	SetWrappedUObjectPtrValues<FObjectPtr>(OutContainer, EPropertyMemoryAccess::InContainer, (UObject**)InValue, 0, ArrayDim);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "UObject/LinkerPlaceholderExportObject.h"
#include "UObject/LinkerPlaceholderClass.h"

/*-----------------------------------------------------------------------------
	FWeakObjectProperty.
-----------------------------------------------------------------------------*/
IMPLEMENT_FIELD(FWeakObjectProperty)

FWeakObjectProperty::FWeakObjectProperty(FFieldVariant InOwner, const UECodeGen_Private::FWeakObjectPropertyParams& Prop)
	: TFObjectPropertyBase(InOwner, Prop)
{
}

FString FWeakObjectProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	return GetCPPTypeCustom(ExtendedTypeText, CPPExportFlags,
		FString::Printf(TEXT("%s%s"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName()));
}

FString FWeakObjectProperty::GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName) const
{
	ensure(!InnerNativeTypeName.IsEmpty());
	if (PropertyFlags & CPF_AutoWeak)
	{
		return FString::Printf(TEXT("TAutoWeakObjectPtr<%s>"), *InnerNativeTypeName);
	}
	return FString::Printf(TEXT("TWeakObjectPtr<%s>"), *InnerNativeTypeName);
}

FString FWeakObjectProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("class %s%s;"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}

FString FWeakObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	if (PropertyFlags & CPF_AutoWeak)
	{
		ExtendedTypeText = FString::Printf(TEXT("TAutoWeakObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
		return TEXT("AUTOWEAKOBJECT");
	}
	ExtendedTypeText = FString::Printf(TEXT("TWeakObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("WEAKOBJECT");
}

void FWeakObjectProperty::LinkInternal(FArchive& Ar)
{
	checkf(!HasAnyPropertyFlags(CPF_NonNullable), TEXT("Weak Object Properties can't be non nullable but \"%s\" is marked as CPF_NonNullable"), *GetFullName());
	Super::LinkInternal(Ar);
}

void FWeakObjectProperty::SerializeItem( FStructuredArchive::FSlot Slot, void* Value, void const* Defaults ) const
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();

	UObject* OldObjectValue = GetObjectPropertyValue(Value);
	Slot << *(FWeakObjectPtr*)Value;

	if (UnderlyingArchive.IsLoading() || UnderlyingArchive.IsModifyingWeakAndStrongReferences())
	{
		UObject* NewObjectValue = GetObjectPropertyValue(Value);

		if (OldObjectValue != NewObjectValue)
		{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			if (UnderlyingArchive.IsLoading() && !UnderlyingArchive.IsObjectReferenceCollector())
			{
				if (ULinkerPlaceholderExportObject* PlaceholderVal = Cast<ULinkerPlaceholderExportObject>(NewObjectValue))
				{
					PlaceholderVal->AddReferencingPropertyValue(this, Value);
				}
				else if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(NewObjectValue))
				{
					PlaceholderClass->AddReferencingPropertyValue(this, Value);
				}
			}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

			CheckValidObject(Value, nullptr); // FWeakObjectProperty is never non-nullable at this point so it's ok to pass null as the current value
		}
	}
}

TObjectPtr<UObject> FWeakObjectProperty::GetObjectPtrPropertyValue(const void* PropertyValueAddress) const
{
	return TObjectPtr<UObject>(GetPropertyValue(PropertyValueAddress).Get());
}

UObject* FWeakObjectProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress).Get();
}

UObject* FWeakObjectProperty::GetObjectPropertyValue_InContainer(const void* ContainerAddress, int32 ArrayIndex) const
{
	return GetWrappedObjectPropertyValue_InContainer<FWeakObjectPtr>(ContainerAddress, ArrayIndex);
}

void FWeakObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	SetPropertyValue(PropertyValueAddress, TCppType(Value));
}

void FWeakObjectProperty::SetObjectPropertyValue_InContainer(void* ContainerAddress, UObject* Value, int32 ArrayIndex) const
{
	SetWrappedObjectPropertyValue_InContainer<FWeakObjectPtr>(ContainerAddress, Value, ArrayIndex);
}

uint32 FWeakObjectProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(*(FWeakObjectPtr*)Src);
}

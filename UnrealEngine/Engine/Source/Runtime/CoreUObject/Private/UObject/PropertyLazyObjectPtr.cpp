// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyHelper.h"

/*-----------------------------------------------------------------------------
	FLazyObjectProperty.
-----------------------------------------------------------------------------*/
IMPLEMENT_FIELD(FLazyObjectProperty)

FLazyObjectProperty::FLazyObjectProperty(FFieldVariant InOwner, const UECodeGen_Private::FLazyObjectPropertyParams& Prop)
	: TFObjectPropertyBase(InOwner, Prop)
{
}

FString FLazyObjectProperty::GetCPPType(FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/) const
{
	return GetCPPTypeCustom(ExtendedTypeText, CPPExportFlags,
		FString::Printf(TEXT("%s%s"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName()));
}

FString FLazyObjectProperty::GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName) const
{
	ensure(!InnerNativeTypeName.IsEmpty());
	return FString::Printf(TEXT("TLazyObjectPtr<%s>"), *InnerNativeTypeName);
}

FString FLazyObjectProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("class %s%s;"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}

FString FLazyObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("TLazyObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("LAZYOBJECT");
}

FName FLazyObjectProperty::GetID() const
{
	return NAME_LazyObjectProperty;
}

void FLazyObjectProperty::SerializeItem( FStructuredArchive::FSlot Slot, void* Value, void const* Defaults ) const
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();

	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want lazy pointers to keep objects from being garbage collected

	if( !UnderlyingArchive.IsObjectReferenceCollector() || UnderlyingArchive.IsModifyingWeakAndStrongReferences() )
	{
		UObject* ObjectValue = GetObjectPropertyValue(Value);

		Slot << *(FLazyObjectPtr*)Value;

		if ((UnderlyingArchive.IsLoading() || UnderlyingArchive.IsModifyingWeakAndStrongReferences()) && ObjectValue != GetObjectPropertyValue(Value))
		{
			CheckValidObject(Value, ObjectValue);
		}
	}
	else
	{
		// TODO: This isn't correct, but it keeps binary serialization happy. We should ALWAYS be serializing the pointer
		// to the archive in this function, and allowing the underlying archive to ignore it if necessary
		Slot.EnterStream();
	}
}

bool FLazyObjectProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	FLazyObjectPtr ObjectA = A ? *((FLazyObjectPtr*)A) : FLazyObjectPtr();
	FLazyObjectPtr ObjectB = B ? *((FLazyObjectPtr*)B) : FLazyObjectPtr();

	// Compare actual pointers. We don't do this during PIE because we want to be sure to serialize everything. An example is the LevelScriptActor being serialized against its CDO,
	// which contains actor references. We want to serialize those references so they are fixed up.
	const bool bDuplicatingForPIE = (PortFlags&PPF_DuplicateForPIE) != 0;
	bool bResult = !bDuplicatingForPIE ? (ObjectA == ObjectB) : false;
	// always serialize the cross level references, because they could be NULL
	// @todo: okay, this is pretty hacky overall - we should have a PortFlag or something
	// that is set during SavePackage. Other times, we don't want to immediately return false
	// (instead of just this ExportDefProps case)
	// instance testing
	if (!bResult && ObjectA.IsValid() && ObjectB.IsValid() && ObjectA->GetClass() == ObjectB->GetClass())
	{
		bool bPerformDeepComparison = (PortFlags&PPF_DeepComparison) != 0;
		if ((PortFlags&PPF_DeepCompareInstances) && !bPerformDeepComparison)
		{
			bPerformDeepComparison = ObjectA->IsTemplate() != ObjectB->IsTemplate();
		}

		if (!bResult && bPerformDeepComparison)
		{
			// In order for deep comparison to be match they both need to have the same name and that name needs to be included in the instancing table for the class
			if (ObjectA->GetFName() == ObjectB->GetFName() && ObjectA->GetClass()->GetDefaultSubobjectByName(ObjectA->GetFName()))
			{
				checkSlow(ObjectA->IsDefaultSubobject() && ObjectB->IsDefaultSubobject() && ObjectA->GetClass()->GetDefaultSubobjectByName(ObjectA->GetFName()) == ObjectB->GetClass()->GetDefaultSubobjectByName(ObjectB->GetFName())); // equivalent
				bResult = AreInstancedObjectsIdentical(ObjectA.Get(), ObjectB.Get(), PortFlags);
			}
		}
	}
	return bResult;
}

TObjectPtr<UObject> FLazyObjectProperty::GetObjectPtrPropertyValue(const void* PropertyValueAddress) const
{
	return TObjectPtr<UObject>(GetPropertyValue(PropertyValueAddress).Get());
}

UObject* FLazyObjectProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress).Get();
}

UObject* FLazyObjectProperty::GetObjectPropertyValue_InContainer(const void* ContainerAddress, int32 ArrayIndex) const
{
	UObject* Result = nullptr;
	GetWrappedUObjectPtrValues<FLazyObjectPtr>(&Result, ContainerAddress, EPropertyMemoryAccess::InContainer, ArrayIndex, 1);
	return Result;
}

void FLazyObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
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

void FLazyObjectProperty::SetObjectPropertyValue_InContainer(void* ContainerAddress, UObject* Value, int32 ArrayIndex) const
{
	if (Value || !HasAnyPropertyFlags(CPF_NonNullable))
	{
		SetWrappedUObjectPtrValues<FLazyObjectPtr>(ContainerAddress, EPropertyMemoryAccess::InContainer, &Value, ArrayIndex, 1);
	}
	else
	{
		UE_LOG(LogProperty, Verbose /*Warning*/, TEXT("Trying to assign null object value to non-nullable \"%s\""), *GetFullName());
	}
}

bool FLazyObjectProperty::AllowCrossLevel() const
{
	return true;
}

uint32 FLazyObjectProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(GetPropertyValue(Src));
}

void FLazyObjectProperty::CopySingleValueToScriptVM( void* Dest, void const* Src ) const
{
	*(FObjectPtr*)Dest = ((const FLazyObjectPtr*)Src)->Get();
}

void FLazyObjectProperty::CopySingleValueFromScriptVM( void* Dest, void const* Src ) const
{
	*(FLazyObjectPtr*)Dest = *(UObject**)Src;
}

void FLazyObjectProperty::CopyCompleteValueToScriptVM( void* Dest, void const* Src ) const
{
	GetWrappedUObjectPtrValues<FLazyObjectPtr>((UObject**)Dest, Src, EPropertyMemoryAccess::Direct, 0, ArrayDim);
}

void FLazyObjectProperty::CopyCompleteValueFromScriptVM( void* Dest, void const* Src ) const
{
	SetWrappedUObjectPtrValues<FLazyObjectPtr>(Dest, EPropertyMemoryAccess::Direct, (UObject**)Src, 0, ArrayDim);
}

void FLazyObjectProperty::CopyCompleteValueToScriptVM_InContainer(void* OutValue, void const* InContainer) const
{
	GetWrappedUObjectPtrValues<FLazyObjectPtr>((UObject**)OutValue, InContainer, EPropertyMemoryAccess::InContainer, 0, ArrayDim);
}

void FLazyObjectProperty::CopyCompleteValueFromScriptVM_InContainer(void* OutContainer, void const* InValue) const
{
	SetWrappedUObjectPtrValues<FLazyObjectPtr>(OutContainer, EPropertyMemoryAccess::InContainer, (UObject**)InValue, 0, ArrayDim);
}

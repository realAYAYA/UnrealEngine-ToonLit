// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyHelper.h"
#include "UObject/UnrealType.h"
#include "UObject/LinkerLoad.h"
#include "UObject/LinkerPlaceholderExportObject.h"
#include "UObject/LinkerPlaceholderClass.h"

/*-----------------------------------------------------------------------------
	FObjectPtrProperty.
-----------------------------------------------------------------------------*/
struct FObjectPropertyFieldNameOverride
{
	FObjectPropertyFieldNameOverride(FFieldClass* FieldClass)
	{
		FFieldClass::GetNameToFieldClassMap().FindOrAdd(TEXT("ObjectPtrProperty")) = FieldClass;
		if (!FLinkerLoad::IsImportLazyLoadEnabled())
		{
			FFieldClass::GetNameToFieldClassMap().FindOrAdd(TEXT("ObjectProperty")) = FObjectProperty::StaticClass();
		}
	}
};

FField* FObjectPtrProperty::Construct(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InFlags)
{
	IMPLEMENT_FIELD_CONSTRUCT_IMPLEMENTATION(FObjectPtrProperty)
}

FFieldClass* FObjectPtrProperty::StaticClass()
{
	// This property type shares the same field name and can supplant FObjectProperty in the NameToFieldClassMap in some configurations because it is serialization compatible with it
	static FFieldClass StaticFieldClass(TEXT("FObjectProperty"), FObjectPtrProperty::StaticClassCastFlagsPrivate(), FObjectPtrProperty::StaticClassCastFlags(), FObjectPtrProperty::Super::StaticClass(), &FObjectPtrProperty::Construct);
	static FObjectPropertyFieldNameOverride ObjectPropertyFieldNameOverride(&StaticFieldClass);
	return &StaticFieldClass;
}

FString FObjectPtrProperty::GetCPPType(FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/) const
{
	return FString::Printf(TEXT("TObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}
FString FObjectPtrProperty::GetCPPMacroType(FString& ExtendedTypeText) const
{
	ExtendedTypeText = FString::Printf(TEXT("TObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("OBJECTPTR");
}

void FObjectPtrProperty::SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const
{
	StaticSerializeItem(this, Slot, Value, Defaults);
}

void FObjectPtrProperty::StaticSerializeItem(const FObjectPropertyBase* ObjectProperty, FStructuredArchive::FSlot Slot, void* Value, void const* Defaults)
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	FObjectPtr* ObjectPtr = (FObjectPtr*)GetPropertyValuePtr(Value);
	const bool IsHandleResolved = IsObjectHandleResolved(ObjectPtr->GetHandle());
	UObject* CurrentValue = IsHandleResolved && *ObjectPtr ? UE::CoreUObject::Private::ReadObjectHandlePointerNoCheck(ObjectPtr->GetHandle()) : nullptr;

	if (UnderlyingArchive.IsObjectReferenceCollector())
	{
		Slot << *ObjectPtr;

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING) 
		if(!UnderlyingArchive.IsSaving() && IsObjectHandleResolved(ObjectPtr->GetHandle()))
		{
			ObjectProperty->CheckValidObject(ObjectPtr, CurrentValue);
		}
#endif
	}
	else
	{
		FObjectHandle OriginalHandle = ObjectPtr->GetHandle();
		Slot << *ObjectPtr;

		FObjectHandle CurrentHandle = ObjectPtr->GetHandle();
		if ((OriginalHandle != CurrentHandle) && IsObjectHandleResolved(CurrentHandle))
		{
	#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			if (ObjectPtr->IsA<ULinkerPlaceholderExportObject>())
			{
				//resolve the handle with no read to avoid trigger a handle read. 
				ULinkerPlaceholderExportObject* PlaceholderVal = static_cast<ULinkerPlaceholderExportObject*>(UE::CoreUObject::Private::ReadObjectHandlePointerNoCheck(CurrentHandle));
				PlaceholderVal->AddReferencingPropertyValue(ObjectProperty, Value);
			}
			else if (ObjectPtr->IsA<ULinkerPlaceholderClass>())
			{
				//resolve the handle with no read to avoid trigger a handle read. 
				ULinkerPlaceholderClass* PlaceholderClass = static_cast<ULinkerPlaceholderClass*>(UE::CoreUObject::Private::ReadObjectHandlePointerNoCheck(CurrentHandle));
				PlaceholderClass->AddReferencingPropertyValue(ObjectProperty, Value);
			}
			// NOTE: we don't remove this from CurrentValue if it is a 
			//       ULinkerPlaceholderExportObject; this is because this property 
			//       could be an array inner, and another member of that array (also 
			//       referenced through this property)... if this becomes a problem,
			//       then we could inc/decrement a ref count per referencing property 
			//
			// @TODO: if this becomes problematic (because ObjectValue doesn't match 
			//        this property's PropertyClass), then we could spawn another
			//        placeholder object (of PropertyClass's type), or use null; but
			//        we'd have to modify ULinkerPlaceholderExportObject::ReplaceReferencingObjectValues()
			//        to accommodate this (as it depends on finding itself as the set value)
	#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

			ObjectProperty->CheckValidObject(Value, CurrentValue);
		}
	}
}

bool FObjectPtrProperty::SameType(const FProperty* Other) const
{
	// @TODO: OBJPTR: Should this be done through a new, separate API on FProperty (eg: ImplicitConv)
	return Super::SameType(Other) ||
		(Other && Other->IsA<FObjectProperty>() &&
			(PropertyClass == ((FObjectPropertyBase*)Other)->PropertyClass) );
}

bool FObjectPtrProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	// We never return Identical when duplicating for PIE because we want to be sure to serialize everything. An example is the LevelScriptActor being serialized against its CDO,
	// which contains actor references. We want to serialize those references so they are fixed up.
	if ((PortFlags & PPF_DuplicateForPIE) != 0)
	{
		return false;
	}

	FObjectPtr ObjectA = A ? *((FObjectPtr*)A) : FObjectPtr();
	FObjectHandle ObjectAHandle = ObjectA.GetHandle();
	FObjectPtr ObjectB = B ? *((FObjectPtr*)B) : FObjectPtr();
	FObjectHandle ObjectBHandle = ObjectB.GetHandle();

	if (ObjectAHandle == ObjectBHandle)
	{
		return true;
	}

	if (IsObjectHandleNull(ObjectAHandle) || IsObjectHandleNull(ObjectBHandle))
	{
		return false;
	}

	// If a deep comparison is required, resolve the object handles and run the deep comparison logic
	// If a deep comparison is not required, avoid resolving the object handles because resolving declares
	// a cook dependency.
	if ((PortFlags & (PPF_DeepCompareInstances | PPF_DeepComparison)) != 0)
	{
		bool bPerformDeepComparison = true;
		if ((PortFlags & PPF_DeepCompareDSOsOnly) != 0)
		{
			UClass* ClassA = ObjectA.GetClass();
			UObject* DSO = ClassA ? ClassA->GetDefaultSubobjectByName(ObjectA.GetFName()) : nullptr;
			bPerformDeepComparison = DSO != nullptr;
		}
		if (bPerformDeepComparison && ObjectA.GetClass() == ObjectB.GetClass() && ObjectA.GetFName() == ObjectB.GetFName())
		{
			return FObjectPropertyBase::StaticIdentical(ObjectA.Get(), ObjectB.Get(), PortFlags);
		}
	}
	return false;
}

FObjectPtr& FObjectPtrProperty::GetObjectPropertyValueAsPtr(const void* PropertyValueAddress) const
{
	return (FObjectPtr&)GetPropertyValue(PropertyValueAddress);
}

TObjectPtr<UObject> FObjectPtrProperty::GetObjectPtrPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress);
}

UObject* FObjectPtrProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress).Get();
}

UObject* FObjectPtrProperty::GetObjectPropertyValue_InContainer(const void* ContainerAddress, int32 ArrayIndex) const
{
	UObject* Result = nullptr;
	GetWrappedUObjectPtrValues<FObjectPtr>(&Result, ContainerAddress, EPropertyMemoryAccess::InContainer, ArrayIndex, 1);
	return Result;
}

void FObjectPtrProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
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

void FObjectPtrProperty::SetObjectPropertyValue_InContainer(void* ContainerAddress, UObject* Value, int32 ArrayIndex) const
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

bool FObjectPtrProperty::AllowObjectTypeReinterpretationTo(const FObjectPropertyBase* Other) const
{
	return Other && Other->IsA<FObjectProperty>();
}

uint32 FObjectPtrProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash((FObjectPtr&)GetPropertyValue(Src));
}

void FObjectPtrProperty::CopySingleValueToScriptVM(void* Dest, const void* Src) const
{
	*(UObject**)Dest = ((const FObjectPtr*)Src)->Get();
}

void FObjectPtrProperty::CopySingleValueFromScriptVM(void* Dest, const void* Src) const
{
	*(FObjectPtr*)Dest = *(UObject**)Src;
}

void FObjectPtrProperty::CopyCompleteValueToScriptVM(void* Dest, const void* Src) const
{
	GetWrappedUObjectPtrValues<FObjectPtr>((UObject**)Dest, Src, EPropertyMemoryAccess::Direct, 0, ArrayDim);
}

void FObjectPtrProperty::CopyCompleteValueFromScriptVM(void* Dest, const void* Src) const
{
	SetWrappedUObjectPtrValues<FObjectPtr>(Dest, EPropertyMemoryAccess::Direct, (UObject**)Src, 0, ArrayDim);
}

void FObjectPtrProperty::CopyCompleteValueToScriptVM_InContainer(void* OutValue, void const* InContainer) const
{
	GetWrappedUObjectPtrValues<FObjectPtr>((UObject**)OutValue, InContainer, EPropertyMemoryAccess::InContainer, 0, ArrayDim);
}

void FObjectPtrProperty::CopyCompleteValueFromScriptVM_InContainer(void* OutContainer, void const* InValue) const
{
	SetWrappedUObjectPtrValues<FObjectPtr>(OutContainer, EPropertyMemoryAccess::InContainer, (UObject**)InValue, 0, ArrayDim);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Blueprint/BlueprintSupport.h"
#include "UObject/LinkerPlaceholderBase.h"
#include "UObject/LinkerPlaceholderExportObject.h"
#include "UObject/LinkerPlaceholderClass.h"
#include "UObject/PropertyHelper.h"

/*-----------------------------------------------------------------------------
	FObjectProperty.
-----------------------------------------------------------------------------*/
IMPLEMENT_FIELD(FObjectProperty)

FObjectProperty::FObjectProperty(FFieldVariant InOwner, const UECodeGen_Private::FObjectPropertyParams& Prop)
	: TFObjectPropertyBase(InOwner, Prop)
{
}

FString FObjectProperty::GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName)  const
{
	ensure(!InnerNativeTypeName.IsEmpty());
	if (HasAnyPropertyFlags(CPF_TObjectPtr) && !(CPPExportFlags & CPPF_NoTObjectPtr))
	{
		return FString::Printf(TEXT("TObjectPtr<%s>"), *InnerNativeTypeName);
	}
	return FString::Printf(TEXT("%s*"), *InnerNativeTypeName);
}

FString FObjectProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("class %s%s;"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}

FString FObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	if (HasAnyPropertyFlags(CPF_TObjectPtr))
	{
		ExtendedTypeText = FString::Printf(TEXT("TObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
		return TEXT("OBJECTPTR");
	}
	ExtendedTypeText = FString::Printf(TEXT("%s%s"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("OBJECT");
}

EConvertFromTypeResult FObjectProperty::ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct, const uint8* Defaults)
{
	static FName NAME_AssetObjectProperty = "AssetObjectProperty"; // old name of soft object property

	if (Tag.Type == NAME_SoftObjectProperty || Tag.Type == NAME_AssetObjectProperty)
	{
		// This property used to be a TSoftObjectPtr<Foo> but is now a raw FObjectProperty Foo*, we can convert without loss of data
		FSoftObjectPtr PreviousValue;
		Slot << PreviousValue;

		UObject* PreviousValueObj = nullptr;

		// If we're async loading, only zenloader supports synchronous loads from the async loading thread.
		// Other loaders are not safe to do a sync load because they may crash or fail to set the variable, so throw an error if it's not already in memory
		ELoaderType LoaderType = GetLoaderType();
		if (!IsInGameThread() && (LoaderType != ELoaderType::ZenLoader))
		{
			PreviousValueObj = PreviousValue.Get();

			if (!PreviousValueObj && !PreviousValue.IsNull())
			{
				UE_LOG(LogClass, Error, TEXT("Failed to convert soft path %s to unloaded object as this is not supported during async loading with the currently active loader '%s'. Load and resave %s in the editor to fix!"), *PreviousValue.ToString(), LexToString(LoaderType), *Slot.GetUnderlyingArchive().GetArchiveName());
			}
		}
		else
		{
			PreviousValueObj = PreviousValue.LoadSynchronous();
		}

		// Now copy the value into the object's address space
		SetPropertyValue_InContainer(Data, PreviousValueObj, Tag.ArrayIndex);

		// Validate the type is proper
		CheckValidObject(GetPropertyValuePtr_InContainer(Data, Tag.ArrayIndex), PreviousValueObj);

		return EConvertFromTypeResult::Converted;
	}
	else if (Tag.Type == NAME_InterfaceProperty)
	{
		UObject* ObjectValue;
		Slot << ObjectValue;

		if (ObjectValue && !ObjectValue->IsA(PropertyClass))
		{
			UE_LOG(LogClass, Warning, TEXT("Failed to convert interface property %s of %s from Interface to %s"), *this->GetName(), *Slot.GetUnderlyingArchive().GetArchiveName(), *PropertyClass->GetName());
			return EConvertFromTypeResult::CannotConvert;
		}

		SetPropertyValue_InContainer(Data, ObjectValue, Tag.ArrayIndex);
		CheckValidObject(GetPropertyValuePtr_InContainer(Data, Tag.ArrayIndex), ObjectValue);
		return EConvertFromTypeResult::Converted;
	}

	return EConvertFromTypeResult::UseSerializeItem;
}
bool FObjectProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	// We never return Identical when duplicating for PIE because we want to be sure to serialize everything. An example is the LevelScriptActor being serialized against its CDO,
	// which contains actor references. We want to serialize those references so they are fixed up.
	if ((PortFlags & PPF_DuplicateForPIE) != 0)
	{
		return false;
	}

	TObjectPtr<UObject> ObjectA = A ? GetPropertyValue(A) : nullptr;
	TObjectPtr<UObject> ObjectB = B ? GetPropertyValue(B) : nullptr;

	if (ObjectA == ObjectB)
	{
		return true;
	}

	if (!ObjectA || !ObjectB)
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

bool FObjectProperty::AllowCrossLevel() const
{
	return HasAnyPropertyFlags(CPF_InstancedReference);
}

void FObjectProperty::SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	TObjectPtr<UObject>* ObjectPtr = GetPropertyValuePtr(Value);
	if (UnderlyingArchive.IsObjectReferenceCollector())
	{
		TObjectPtr<UObject> CurrentValue = *ObjectPtr;
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		if (HasAnyPropertyFlags(CPF_TObjectPtr))
		{
			FObjectPtr* Ptr = (FObjectPtr*)ObjectPtr;
			Slot << *Ptr;
		}
		else
#endif
		{
			// Serialize in place
			Slot << (*ObjectPtr);
		}
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING) 
		if (!UnderlyingArchive.IsSaving())
		{
			CheckValidObject(Value, CurrentValue);
		}
#endif
	}
	else
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		if (HasAnyPropertyFlags(CPF_TObjectPtr))
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
					PlaceholderVal->AddReferencingPropertyValue(this, Value);
				}
				else if (ObjectPtr->IsA<ULinkerPlaceholderClass>())
				{
					//resolve the handle with no read to avoid trigger a handle read. 
					ULinkerPlaceholderClass* PlaceholderClass = static_cast<ULinkerPlaceholderClass*>(UE::CoreUObject::Private::ReadObjectHandlePointerNoCheck(CurrentHandle));
					PlaceholderClass->AddReferencingPropertyValue(this, Value);
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

				TObjectPtr<UObject> CurrentValue(*ObjectPtr);
				CheckValidObject(Value, CurrentValue);
			}
		}
		else
#endif
		{
			TObjectPtr<UObject> ObjectValuePtr = GetObjectPtrPropertyValue(Value);
			check(ObjectValuePtr.IsResolved());
			UObject* ObjectValue = UE::CoreUObject::Private::ReadObjectHandlePointerNoCheck(ObjectValuePtr.GetHandle());

			Slot << ObjectValue;

			TObjectPtr<UObject> CurrentValuePtr = GetObjectPtrPropertyValue(Value);
			check(CurrentValuePtr.IsResolved());
			UObject* CurrentValue = UE::CoreUObject::Private::ReadObjectHandlePointerNoCheck(CurrentValuePtr.GetHandle());
			PostSerializeObjectItem(UnderlyingArchive, Value, CurrentValue, ObjectValue, EObjectPropertyOptions::None);
		}
	}
}

void FObjectProperty::PostSerializeObjectItem(FArchive& SerializingArchive, void* Value, UObject* CurrentValue, UObject* ObjectValue, EObjectPropertyOptions Options /*= EObjectPropertyOptions::None*/) const
{
	// Make sure non-nullable properties don't end up with null values
	if (!(Options & EObjectPropertyOptions::AllowNullValuesOnNonNullableProperty) &&
		!ObjectValue && HasAnyPropertyFlags(CPF_NonNullable) &&
		!SerializingArchive.IsSerializingDefaults() && // null values when Serializing CDOs are allowed, they will be fixed up later
		!SerializingArchive.IsSaving() && // Constructing new objects when saving may confuse package saving code (new import/export created after import/export collection pass)
		!SerializingArchive.IsTransacting() && // Don't create new objects when loading from the transaction buffer
		!SerializingArchive.IsCountingMemory())
	{
		UObject* DefaultValue = ConstructDefaultObjectValueIfNecessary(CurrentValue);

		UE_LOG(LogProperty, Warning,
			TEXT("Failed to serialize value for non-nullable property %s. Reference will be defaulted to %s."),
			*GetFullName(),
			DefaultValue ? *DefaultValue->GetFullName() : TEXT("None")
		);

		SetObjectPropertyValue(Value, DefaultValue);
		ObjectValue = DefaultValue;
	}

	if (ObjectValue != CurrentValue)
	{
		SetObjectPropertyValue(Value, ObjectValue);

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
		if (ULinkerPlaceholderExportObject* PlaceholderVal = Cast<ULinkerPlaceholderExportObject>(ObjectValue))
		{
			PlaceholderVal->AddReferencingPropertyValue(this, Value);
		}
		else if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(ObjectValue))
		{
			PlaceholderClass->AddReferencingPropertyValue(this, Value);
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

		CheckValidObject(Value, CurrentValue);
	}
}

const TCHAR* FObjectProperty::ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const
{
	const TCHAR* Result = TFObjectPropertyBase<TObjectPtr<UObject>>::ImportText_Internal(Buffer, ContainerOrPropertyPtr, PropertyPointerType, OwnerObject, PortFlags, ErrorText);
	if (Result)
	{
		void* Data = PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
		TObjectPtr<UObject> ObjectValue = GetObjectPtrPropertyValue(Data);
		CheckValidObject(Data, ObjectValue);

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING		
		if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(ObjectValue))
		{
			// we use this tracker mechanism to help record the instance that is
			// referencing the placeholder (so we can replace it later on fixup)
			FScopedPlaceholderContainerTracker ImportingObjTracker(OwnerObject);

			PlaceholderClass->AddReferencingPropertyValue(this, Data);
		}
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		else
		{
			// as far as we know, ULinkerPlaceholderClass is the only type we have to handle through ImportText()
			check(!FBlueprintSupport::IsDeferredDependencyPlaceholder(ObjectValue));
		}
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	}
	return Result;
}

uint32 FObjectProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(GetPropertyValue(Src));
}

TObjectPtr<UObject> FObjectProperty::GetObjectPtrPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress);
}

UObject* FObjectProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress).Get();
}

UObject* FObjectProperty::GetObjectPropertyValue_InContainer(const void* ContainerAddress, int32 ArrayIndex) const
{
	UObject* Result = nullptr;
	GetWrappedUObjectPtrValues<FObjectPtr>(&Result, ContainerAddress, EPropertyMemoryAccess::InContainer, ArrayIndex, 1);
	return Result;
}

void FObjectProperty::SetObjectPtrPropertyValue(void* PropertyValueAddress, TObjectPtr<UObject> Value) const
{
	if (Value || !HasAnyPropertyFlags(CPF_NonNullable))
	{
		SetPropertyValue(PropertyValueAddress, Value);
	}
	else
	{
		UE_LOG(LogProperty, Verbose /*Warning*/, TEXT("Trying to assign null object value to non-nullable \"%s\""), *GetFullName());
	}
}

void FObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	if (Value || !HasAnyPropertyFlags(CPF_NonNullable))
	{
		SetPropertyValue(PropertyValueAddress, Value);
	}
	else
	{
		UE_LOG(LogProperty, Verbose /*Warning*/, TEXT("Trying to assign null object value to non-nullable \"%s\""), *GetFullName());
	}
}

void FObjectProperty::SetObjectPropertyValue_InContainer(void* ContainerAddress, UObject* Value, int32 ArrayIndex) const
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

void FObjectProperty::CopySingleValueToScriptVM(void* Dest, const void* Src) const
{
	*(FObjectPtr*)Dest = ((const FObjectPtr*)Src)->Get();
}

void FObjectProperty::CopySingleValueFromScriptVM(void* Dest, const void* Src) const
{
	*(FObjectPtr*)Dest = *(UObject**)Src;
}

void FObjectProperty::CopyCompleteValueToScriptVM(void* Dest, const void* Src) const
{
	GetWrappedUObjectPtrValues<FObjectPtr>((UObject**)Dest, Src, EPropertyMemoryAccess::Direct, 0, ArrayDim);
}

void FObjectProperty::CopyCompleteValueFromScriptVM(void* Dest, const void* Src) const
{
	SetWrappedUObjectPtrValues<FObjectPtr>(Dest, EPropertyMemoryAccess::Direct, (UObject**)Src, 0, ArrayDim);
}

void FObjectProperty::CopyCompleteValueToScriptVM_InContainer(void* OutValue, void const* InContainer) const
{
	GetWrappedUObjectPtrValues<FObjectPtr>((UObject**)OutValue, InContainer, EPropertyMemoryAccess::InContainer, 0, ArrayDim);
}

void FObjectProperty::CopyCompleteValueFromScriptVM_InContainer(void* OutContainer, void const* InValue) const
{
	SetWrappedUObjectPtrValues<FObjectPtr>(OutContainer, EPropertyMemoryAccess::InContainer, (UObject**)InValue, 0, ArrayDim);
}

void FObjectProperty::CopyValuesInternal(void* Dest, void const* Src, int32 Count) const
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	if (HasAllPropertyFlags(CPF_TObjectPtr))
	{
		for (int32 Index = 0; Index < Count; Index++)
		{
			GetPropertyValuePtr(Dest)[Index] = GetPropertyValuePtr(Src)[Index];
		}
	}
	else
#endif	
	{
		for (int32 Index = 0; Index < Count; Index++)
		{
			GetPropertyValuePtr(Dest)[Index] = GetPropertyValuePtr(Src)[Index].Get();
		}
	}
	
}

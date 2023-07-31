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
	return FString::Printf(TEXT("%s*"), *InnerNativeTypeName);
}

FString FObjectProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("class %s%s;"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}

FString FObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("%s%s"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("OBJECT");
}

EConvertFromTypeResult FObjectProperty::ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct)
{
	static FName NAME_AssetObjectProperty = "AssetObjectProperty"; // old name of soft object property

	if (Tag.Type == NAME_SoftObjectProperty || Tag.Type == NAME_AssetObjectProperty)
	{
		// This property used to be a TSoftObjectPtr<Foo> but is now a raw FObjectProperty Foo*, we can convert without loss of data
		FSoftObjectPtr PreviousValue;
		Slot << PreviousValue;

		UObject* PreviousValueObj = nullptr;

		// If we're async loading it's not safe to do a sync load because it may crash or fail to set the variable, so throw an error if it's not already in memory
		if (IsInAsyncLoadingThread())
		{
			PreviousValueObj = PreviousValue.Get();

			if (!PreviousValueObj && !PreviousValue.IsNull())
			{
				UE_LOG(LogClass, Error, TEXT("Failed to convert soft path %s to unloaded object as this is not safe during async loading. Load and resave %s in the editor to fix!"), *PreviousValue.ToString(), *Slot.GetUnderlyingArchive().GetArchiveName());
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

void FObjectProperty::SerializeItem( FStructuredArchive::FSlot Slot, void* Value, void const* Defaults ) const
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();

	if (UnderlyingArchive.IsObjectReferenceCollector())
	{
		// Serialize in place
		TObjectPtr<UObject>* ObjectPtr = GetPropertyValuePtr(Value);
		Slot << (*ObjectPtr);

		if(!UnderlyingArchive.IsSaving())
		{
			CheckValidObject(ObjectPtr, *ObjectPtr);
		}
	}
	else
	{
		UObject* ObjectValue = GetObjectPropertyValue(Value);
		Slot << ObjectValue;

		UObject* CurrentValue = GetObjectPropertyValue(Value);
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
}

const TCHAR* FObjectProperty::ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const
{
	const TCHAR* Result = TFObjectPropertyBase<TObjectPtr<UObject>>::ImportText_Internal(Buffer, ContainerOrPropertyPtr, PropertyPointerType, OwnerObject, PortFlags, ErrorText);
	if (Result)
	{
		void* Data = PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
		UObject* ObjectValue = GetObjectPropertyValue(Data);
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
	// Dummy struct to satisfy GetWrappedObjectPropertyValue_InContainer API requirements (Get() function)
	struct FWrappedObjectPtr
	{
		UObject* Obj = nullptr;
		UObject* Get() const
		{
			return Obj;
		}
	};
	return GetWrappedObjectPropertyValue_InContainer<FWrappedObjectPtr>(ContainerAddress, ArrayIndex);
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
	// Dummy struct to satisfy SetWrappedObjectPropertyValue_InContainer API requirements
	struct FWrappedObjectPtr
	{
		FWrappedObjectPtr() = default;
		FWrappedObjectPtr(UObject* InObj)
			: Obj(InObj)
		{
		}
		UObject* Obj = nullptr;
	};
	if (Value || !HasAnyPropertyFlags(CPF_NonNullable))
	{
		SetWrappedObjectPropertyValue_InContainer<FWrappedObjectPtr>(ContainerAddress, Value, ArrayIndex);
	}
	else
	{
		UE_LOG(LogProperty, Verbose /*Warning*/, TEXT("Trying to assign null object value to non-nullable \"%s\""), *GetFullName());
	}
}

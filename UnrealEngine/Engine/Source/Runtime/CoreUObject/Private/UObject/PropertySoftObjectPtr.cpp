// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "UObject/LinkerLoad.h"

/*-----------------------------------------------------------------------------
	FSoftObjectProperty.
-----------------------------------------------------------------------------*/
IMPLEMENT_FIELD(FSoftObjectProperty)

FSoftObjectProperty::FSoftObjectProperty(FFieldVariant InOwner, const UECodeGen_Private::FSoftObjectPropertyParams& Prop)
	: TFObjectPropertyBase(InOwner, Prop)
{
}

FSoftObjectProperty::FSoftObjectProperty(FFieldVariant InOwner, const UECodeGen_Private::FObjectPropertyParamsWithoutClass& Prop, UClass* InClass)
	: TFObjectPropertyBase(InOwner, Prop, InClass)
{
}

FString FSoftObjectProperty::GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName) const
{
	ensure(!InnerNativeTypeName.IsEmpty());
	return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *InnerNativeTypeName);
}
FString FSoftObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("TSoftObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("SOFTOBJECT");
}

FString FSoftObjectProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("class %s%s;"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}

FName FSoftObjectProperty::GetID() const
{
	// SoftClass shares the same tag, they are binary compatible
	return NAME_SoftObjectProperty;
}

// this is always shallow, can't see that we would want it any other way
bool FSoftObjectProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	FSoftObjectPtr ObjectA = A ? *((FSoftObjectPtr*)A) : FSoftObjectPtr();
	FSoftObjectPtr ObjectB = B ? *((FSoftObjectPtr*)B) : FSoftObjectPtr();

	return ObjectA.GetUniqueID() == ObjectB.GetUniqueID();
}

void FSoftObjectProperty::LinkInternal(FArchive& Ar)
{
	checkf(!HasAnyPropertyFlags(CPF_NonNullable), TEXT("Soft Object Properties can't be non nullable but \"%s\" is marked as CPF_NonNullable"), *GetFullName());
	Super::LinkInternal(Ar);
}

void FSoftObjectProperty::SerializeItem( FStructuredArchive::FSlot Slot, void* Value, void const* Defaults ) const
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();

	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want soft object pointers to keep objects from being garbage collected
	// Allow persistent archives so they can keep track of string references. (e.g. FArchiveSaveTagImports)
	if( !UnderlyingArchive.IsObjectReferenceCollector() || UnderlyingArchive.IsModifyingWeakAndStrongReferences() || UnderlyingArchive.IsPersistent() )
	{
		FSoftObjectPtr OldValue = *(FSoftObjectPtr*)Value;
		Slot << *(FSoftObjectPtr*)Value;

		// Check for references to instances of wrong types and null them out 
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING) 
		if (UnderlyingArchive.IsLoading() || UnderlyingArchive.IsModifyingWeakAndStrongReferences())
		{
			if (OldValue.GetUniqueID() != ((FSoftObjectPtr*)Value)->GetUniqueID())
			{
				CheckValidObject(Value, nullptr); // FSoftObjectProperty is never non-nullable at this point so it's ok to pass null as the current value
			}
		}
#endif
	}
	else
	{
		// TODO: This isn't correct, but it keeps binary serialization happy. We should ALWAYS be serializing the pointer
		// to the archive in this function, and allowing the underlying archive to ignore it if necessary
		Slot.EnterStream();
	}
}

bool FSoftObjectProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8>* MetaData) const
{
	// Serialize directly, will use FBitWriter/Reader
	Ar << *(FSoftObjectPtr*)Data;

	return true;
}

void FSoftObjectProperty::ExportText_Internal( FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	FSoftObjectPtr SoftObjectPtr;
	
	if (PropertyPointerType == EPropertyPointerType::Container && HasGetter())
	{
		GetValue_InContainer(PropertyValueOrContainer, &SoftObjectPtr);
	}
	else
	{
		SoftObjectPtr = *(FSoftObjectPtr*)PointerToValuePtr(PropertyValueOrContainer, PropertyPointerType);
	}

	FSoftObjectPath SoftObjectPath;
	UObject *Object = SoftObjectPtr.Get();

	if (Object)
	{
		// Use object in case name has changed.
		SoftObjectPath = FSoftObjectPath(Object);
	}
	else
	{
		SoftObjectPath = SoftObjectPtr.GetUniqueID();
	}

	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += FString::Printf(TEXT("FSoftObjectPath(TEXT(\"%s\"))"), *SoftObjectPath.ToString().ReplaceCharWithEscapedChar());
		return;
	}

	SoftObjectPath.ExportTextItem(ValueStr, SoftObjectPath, Parent, PortFlags, ExportRootScope);
}

const TCHAR* FSoftObjectProperty::ImportText_Internal( const TCHAR* InBuffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* Parent, int32 PortFlags, FOutputDevice* ErrorText ) const
{
	FSoftObjectPath SoftObjectPath;

	bool bImportTextSuccess = false;
#if WITH_EDITOR
	if (HasAnyPropertyFlags(CPF_EditorOnly))
	{
		FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, ESoftObjectPathCollectType::EditorOnlyCollect, ESoftObjectPathSerializeType::AlwaysSerialize);
		bImportTextSuccess = SoftObjectPath.ImportTextItem(InBuffer, PortFlags, Parent, ErrorText, GetLinker());
	}
	else
#endif // WITH_EDITOR
	{
		bImportTextSuccess = SoftObjectPath.ImportTextItem(InBuffer, PortFlags, Parent, ErrorText, GetLinker());
	}

	if (bImportTextSuccess)
	{
		if (PropertyPointerType == EPropertyPointerType::Container && HasSetter())
		{
			FSoftObjectPtr SoftObjectPtr(SoftObjectPath);
			SetValue_InContainer(ContainerOrPropertyPtr, SoftObjectPtr);
		}
		else
		{
			FSoftObjectPtr& SoftObjectPtr = *(FSoftObjectPtr*)PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
			SoftObjectPtr = SoftObjectPath;
		}
		return InBuffer;
	}
	else
	{
		if (PropertyPointerType == EPropertyPointerType::Container && HasSetter())
		{
			FSoftObjectPtr NullPtr(nullptr);
			SetValue_InContainer(ContainerOrPropertyPtr, NullPtr);
		}
		else
		{
			FSoftObjectPtr& SoftObjectPtr = *(FSoftObjectPtr*)PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
			SoftObjectPtr = nullptr;
		}
		return nullptr;
	}
}

EConvertFromTypeResult FSoftObjectProperty::ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct)
{
	static FName NAME_AssetObjectProperty = "AssetObjectProperty";
	static FName NAME_SoftObjectPath = "SoftObjectPath";
	static FName NAME_SoftClassPath = "SoftClassPath";
	static FName NAME_StringAssetReference = "StringAssetReference";
	static FName NAME_StringClassReference = "StringClassReference";

	FArchive& Archive = Slot.GetUnderlyingArchive();

	if (Tag.Type == NAME_AssetObjectProperty)
	{
		// Old name of soft object property, serialize normally
		uint8* DestAddress = ContainerPtrToValuePtr<uint8>(Data, Tag.ArrayIndex);

		Tag.SerializeTaggedProperty(Slot, this, DestAddress, nullptr);

		if (Archive.IsCriticalError())
		{
			return EConvertFromTypeResult::CannotConvert;
		}

		return EConvertFromTypeResult::Converted;
	}
	else if (Tag.Type == NAME_ObjectProperty)
	{
		// This property used to be a raw FObjectProperty Foo* but is now a TSoftObjectPtr<Foo>;
		// Serialize from mismatched tag directly into the FSoftObjectPtr's soft object path to ensure that the delegates needed for cooking
		// are fired
		FSoftObjectPtr* PropertyValue = GetPropertyValuePtr_InContainer(Data, Tag.ArrayIndex);
		check(PropertyValue);

		return PropertyValue->GetUniqueID().SerializeFromMismatchedTag(Tag, Slot) ? EConvertFromTypeResult::Converted : EConvertFromTypeResult::UseSerializeItem;
	}
	else if (Tag.Type == NAME_StructProperty && (Tag.StructName == NAME_SoftObjectPath || Tag.StructName == NAME_SoftClassPath || Tag.StructName == NAME_StringAssetReference || Tag.StructName == NAME_StringClassReference))
	{
		// This property used to be a FSoftObjectPath but is now a TSoftObjectPtr<Foo>
		FSoftObjectPath PreviousValue;
		// explicitly call Serialize to ensure that the various delegates needed for cooking are fired
		PreviousValue.Serialize(Slot);

		// now copy the value into the object's address space
		FSoftObjectPtr PreviousValueSoftObjectPtr;
		PreviousValueSoftObjectPtr = PreviousValue;
		SetPropertyValue_InContainer(Data, PreviousValueSoftObjectPtr, Tag.ArrayIndex);

		return EConvertFromTypeResult::Converted;
	}

	return EConvertFromTypeResult::UseSerializeItem;
}

UObject* FSoftObjectProperty::LoadObjectPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress).LoadSynchronous();
}

TObjectPtr<UObject> FSoftObjectProperty::GetObjectPtrPropertyValue(const void* PropertyValueAddress) const
{
	return TObjectPtr<UObject>(GetPropertyValue(PropertyValueAddress).Get());
}

UObject* FSoftObjectProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress).Get();
}

UObject* FSoftObjectProperty::GetObjectPropertyValue_InContainer(const void* ContainerAddress, int32 ArrayIndex) const
{
	return GetWrappedObjectPropertyValue_InContainer<FSoftObjectPtr>(ContainerAddress, ArrayIndex);
}

void FSoftObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	SetPropertyValue(PropertyValueAddress, TCppType(Value));
}

void FSoftObjectProperty::SetObjectPropertyValue_InContainer(void* ContainerAddress, UObject* Value, int32 ArrayIndex) const
{
	SetWrappedObjectPropertyValue_InContainer<FSoftObjectPtr>(ContainerAddress, Value, ArrayIndex);
}

bool FSoftObjectProperty::AllowCrossLevel() const
{
	return true;
}

uint32 FSoftObjectProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(GetPropertyValue(Src));
}

void FSoftObjectProperty::CopySingleValueToScriptVM(void* Dest, void const* Src) const
{
	CopySingleValue(Dest, Src);
}

void FSoftObjectProperty::CopyCompleteValueToScriptVM(void* Dest, void const* Src) const
{
	CopyCompleteValue(Dest, Src);
}

void FSoftObjectProperty::CopySingleValueFromScriptVM(void* Dest, void const* Src) const
{
	CopySingleValue(Dest, Src);
}

void FSoftObjectProperty::CopyCompleteValueFromScriptVM(void* Dest, void const* Src) const
{
	CopyCompleteValue(Dest, Src);
}


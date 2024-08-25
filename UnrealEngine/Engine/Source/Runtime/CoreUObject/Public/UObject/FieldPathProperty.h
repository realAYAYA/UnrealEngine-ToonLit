// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Serialization/StructuredArchive.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

class FArchive;
class FOutputDevice;
class UClass;
class UField;
class UObject;
class UStruct;
namespace UECodeGen_Private { struct FFieldPathPropertyParams; }
struct FPropertyTag;

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FFieldPath, FProperty> FFieldPathProperty_Super;

class FFieldPathProperty : public FFieldPathProperty_Super
{
	DECLARE_FIELD_API(FFieldPathProperty, FFieldPathProperty_Super, CASTCLASS_FFieldPathProperty, COREUOBJECT_API)

public:

	typedef FFieldPathProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	FFieldPathProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
		: FFieldPathProperty_Super(InOwner, InName, InObjectFlags)
		, PropertyClass(nullptr)
	{
	}

	UE_DEPRECATED(5.1, "Compiled-in property constructor is deprecated, use other constructors instead.")
	COREUOBJECT_API FFieldPathProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags, FFieldClass* InPropertyClass)
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		: FFieldPathProperty_Super(InOwner, InName, InObjectFlags, InOffset, InFlags)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		, PropertyClass(InPropertyClass)
	{
	}

	/**
	 * Constructor used for constructing compiled in properties
	 * @param InOwner Owner of the property
	 * @param PropBase Pointer to the compiled in structure describing the property
	 **/
	FFieldPathProperty(FFieldVariant InOwner, const UECodeGen_Private::FFieldPathPropertyParams& Prop);

#if WITH_EDITORONLY_DATA
	COREUOBJECT_API explicit FFieldPathProperty(UField* InField);
#endif // WITH_EDITORONLY_DATA

	FFieldClass* PropertyClass;

	// UHT interface
	COREUOBJECT_API virtual FString GetCPPMacroType(FString& ExtendedTypeText) const  override;
	UE_DEPRECATED(5.4, "UnrealHeaderTool only API.  No replacement available.")
	COREUOBJECT_API virtual FString GetCPPTypeForwardDeclaration() const override;
	COREUOBJECT_API virtual FString GetCPPType(FString* ExtendedTypeText = nullptr, uint32 CPPExportFlags = 0) const override;
	// End of UHT interface

	// FProperty interface
	COREUOBJECT_API virtual EConvertFromTypeResult ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct, const uint8* Defaults) override;
	COREUOBJECT_API virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	COREUOBJECT_API virtual void SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const override;
	COREUOBJECT_API virtual void ExportText_Internal( FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	COREUOBJECT_API virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const override;
	COREUOBJECT_API virtual void Serialize(FArchive& Ar) override;
	COREUOBJECT_API virtual bool ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType = EPropertyObjectReferenceType::Strong) const override;
	COREUOBJECT_API virtual void EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath) override;
	COREUOBJECT_API virtual bool SupportsNetSharedSerialization() const override;
	// End of FProperty interface

	static COREUOBJECT_API FString RedirectFieldPathName(const FString& InPathName);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Internationalization/Text.h"
#include "Serialization/StructuredArchive.h"
#include "UObject/Field.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

class FOutputDevice;
class UField;
class UObject;
class UStruct;
struct FPropertyTag;

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FText, FProperty> FTextProperty_Super;

class COREUOBJECT_API FTextProperty : public FTextProperty_Super
{
	DECLARE_FIELD(FTextProperty, FTextProperty_Super, CASTCLASS_FTextProperty)

public:

	typedef FTextProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	FTextProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
		: FTextProperty_Super(InOwner, InName, InObjectFlags)
	{
	}

	UE_DEPRECATED(5.1, "Compiled-in property constructor is deprecated, use other constructors instead.")
	FTextProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		: FTextProperty_Super(InOwner, InName, InObjectFlags, InOffset, InFlags)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
	}

	/**
	 * Constructor used for constructing compiled in properties
	 * @param InOwner Owner of the property
	 * @param PropBase Pointer to the compiled in structure describing the property
	 **/
	FTextProperty(FFieldVariant InOwner, const UECodeGen_Private::FTextPropertyParams& Prop);

#if WITH_EDITORONLY_DATA
	explicit FTextProperty(UField* InField)
		: FTextProperty_Super(InField)
	{
	}
#endif // WITH_EDITORONLY_DATA

	// FProperty interface
	virtual EConvertFromTypeResult ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct) override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual void SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const override;
protected:
	virtual void ExportText_Internal( FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const override;
public:
	virtual FString GetCPPTypeForwardDeclaration() const override;
	// End of FProperty interface

	static bool Identical_Implementation(const FText& A, const FText& B, uint32 PortFlags);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "UObject/UnrealType.h"

namespace Verse
{
	struct VRestValue;
}

//
// Metadata for a property of FVerseValueProperty type.
//
class FVerseValueProperty : public FProperty
{
	DECLARE_FIELD(FVerseValueProperty, FProperty, CASTCLASS_FVerseValueProperty)

public:

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	using TCppType = Verse::VRestValue;
#endif

	COREUOBJECT_API FVerseValueProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);

	/**
	 * Constructor used for constructing compiled in properties
	 * @param InOwner Owner of the property
	 * @param Prop Pointer to the compiled in structure describing the property
	 **/
	COREUOBJECT_API FVerseValueProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop);


	// UHT interface
	virtual FString GetCPPType(FString* ExtendedTypeText = NULL, uint32 CPPExportFlags = 0) const override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	virtual FString GetCPPMacroType(FString& ExtendedTypeText) const override;
	// End of UHT interface

	// FProperty interface
	virtual void LinkInternal(FArchive& Ar) override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags) const override;
	virtual void SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const override;
	virtual void ExportText_Internal(FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const override;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const override;
	virtual void ClearValueInternal(void* Data) const override;
	virtual void InitializeValueInternal(void* Data) const override;
	virtual void DestroyValueInternal(void* Data) const override;
	virtual int32 GetMinAlignment() const override;
	virtual bool ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType = EPropertyObjectReferenceType::Strong) const override;
	virtual void EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath) override;
	// End of FProperty interface
};

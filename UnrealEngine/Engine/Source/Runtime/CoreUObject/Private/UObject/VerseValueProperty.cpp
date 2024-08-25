// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/VerseValueProperty.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/VerseTypes.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/Inline/VVMValueInline.h"

IMPLEMENT_FIELD(FVerseValueProperty)

FVerseValueProperty::FVerseValueProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FProperty(InOwner, InName, InObjectFlags)	
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	ElementSize = sizeof(TCppType);
#endif
}

FVerseValueProperty::FVerseValueProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop)
	: FProperty(InOwner, (const UECodeGen_Private::FPropertyParamsBaseWithOffset&)Prop, CPF_HasGetValueTypeHash)
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	ElementSize = sizeof(TCppType);
#endif
}

void FVerseValueProperty::SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const
{
	uint8 ScratchValue = 0;
	Slot << ScratchValue;
}

FString FVerseValueProperty::GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags) const
{
	return TEXT("TCppType");
}

FString FVerseValueProperty::GetCPPMacroType(FString& ExtendedTypeText) const
{
	ExtendedTypeText = FString();
	return FString();
}

FString FVerseValueProperty::GetCPPTypeForwardDeclaration() const
{
	return FString();
}

void FVerseValueProperty::ExportText_Internal(FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	check(false);
	return;
}

const TCHAR* FVerseValueProperty::ImportText_Internal(const TCHAR* InBuffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* Parent, int32 PortFlags, FOutputDevice* ErrorText) const
{
	check(false);
	return TEXT("");
}

void FVerseValueProperty::LinkInternal(FArchive& Ar)
{
#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
	PropertyFlags |= CPF_NoDestructor | CPF_ZeroConstructor;
#endif
}

bool FVerseValueProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	const TCppType* Lhs = reinterpret_cast<const TCppType*>(A);
	const TCppType* Rhs = reinterpret_cast<const TCppType*>(B);
	return *Lhs == *Rhs;
#else
	return true;
#endif
}

void FVerseValueProperty::CopyValuesInternal(void* Dest, void const* Source, int32 Count) const
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	TCppType* Lhs = reinterpret_cast<TCppType*>(Dest);
	const TCppType* Rhs = reinterpret_cast<const TCppType*>(Source);
	for (; Count-- > 0; ++Lhs, ++Rhs)
	{
		*Lhs = *Rhs;
	}
#endif
}

void FVerseValueProperty::ClearValueInternal(void* Data) const
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	TCppType* Value = reinterpret_cast<TCppType*>(Data);
	Value->Reset(0);
#endif
}

void FVerseValueProperty::InitializeValueInternal(void* Data) const
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	new (Data) TCppType(0);
#endif
}

void FVerseValueProperty::DestroyValueInternal(void* Data) const
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	TCppType* Value = reinterpret_cast<TCppType*>(Data);
	Value->~TCppType();
#endif
}

int32 FVerseValueProperty::GetMinAlignment() const
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	static_assert(alignof(TCppType) == alignof(uintptr_t));
	return alignof(TCppType);
#else
	return alignof(uintptr_t);
#endif
}

bool FVerseValueProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType) const
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	return true;
#else
	return false;
#endif
}

void FVerseValueProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	for (int32 Idx = 0, Num = ArrayDim; Idx < Num; ++Idx)
	{
		Schema.Add(UE::GC::DeclareMember(DebugPath, BaseOffset + GetOffset_ForGC() + Idx * sizeof(TCppType), UE::GC::EMemberType::VerseValue));
	}
#endif
}

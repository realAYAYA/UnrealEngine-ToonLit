// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/Casts.h"
#include "UObject/PropertyTag.h"
#include "UObject/UnrealType.h"
#include "UObject/UnrealTypePrivate.h"
#include "UObject/LinkerLoad.h"
#include "UObject/PropertyHelper.h"
#include "Misc/ScopeExit.h"
#include "Serialization/ArchiveUObjectFromStructuredArchive.h"
#include "UObject/UObjectThreadContext.h"

namespace UEMapProperty_Private
{
	/**
	 * Checks if any of the pairs in the map compare equal to the one passed.
	 *
	 * @param  MapHelper  The map to search through.
	 * @param  Index      The index in the map to start searching from.
	 * @param  Num        The number of elements to compare.
	 */
	bool AnyEqual(const FScriptMapHelper& MapHelper, int32 Index, int32 Num, const uint8* PairToCompare, uint32 PortFlags)
	{
		FProperty* KeyProp   = MapHelper.GetKeyProperty();
		FProperty* ValueProp = MapHelper.GetValueProperty();

		int32 ValueOffset = MapHelper.MapLayout.ValueOffset;

		for (; Num; --Num)
		{
			while (!MapHelper.IsValidIndex(Index))
			{
				++Index;
			}

			if (KeyProp->Identical(MapHelper.GetPairPtr(Index), PairToCompare, PortFlags) && ValueProp->Identical(MapHelper.GetPairPtr(Index) + ValueOffset, PairToCompare + ValueOffset, PortFlags))
			{
				return true;
			}

			++Index;
		}

		return false;
	}

	bool RangesContainSameAmountsOfVal(const FScriptMapHelper& MapHelperA, int32 IndexA, const FScriptMapHelper& MapHelperB, int32 IndexB, int32 Num, const uint8* PairToCompare, uint32 PortFlags)
	{
		FProperty* KeyProp   = MapHelperA.GetKeyProperty();
		FProperty* ValueProp = MapHelperA.GetValueProperty();

		// Ensure that both maps are the same type
		check(KeyProp   == MapHelperB.GetKeyProperty());
		check(ValueProp == MapHelperB.GetValueProperty());

		int32 ValueOffset = MapHelperA.MapLayout.ValueOffset;

		int32 CountA = 0;
		int32 CountB = 0;
		for (;;)
		{
			if (Num == 0)
			{
				return CountA == CountB;
			}

			while (!MapHelperA.IsValidIndex(IndexA))
			{
				++IndexA;
			}

			while (!MapHelperB.IsValidIndex(IndexB))
			{
				++IndexB;
			}

			const uint8* PairA = MapHelperA.GetPairPtr(IndexA);
			const uint8* PairB = MapHelperB.GetPairPtr(IndexB);
			if (PairA == PairToCompare || (KeyProp->Identical(PairA, PairToCompare, PortFlags) && ValueProp->Identical(PairA + ValueOffset, PairToCompare + ValueOffset, PortFlags)))
			{
				++CountA;
			}

			if (PairB == PairToCompare || (KeyProp->Identical(PairB, PairToCompare, PortFlags) && ValueProp->Identical(PairB + ValueOffset, PairToCompare + ValueOffset, PortFlags)))
			{
				++CountB;
			}

			++IndexA;
			++IndexB;
			--Num;
		}
	}

	bool IsPermutation(const FScriptMapHelper& MapHelperA, const FScriptMapHelper& MapHelperB, uint32 PortFlags)
	{
		FProperty* KeyProp   = MapHelperA.GetKeyProperty();
		FProperty* ValueProp = MapHelperA.GetValueProperty();

		// Ensure that both maps are the same type
		check(KeyProp   == MapHelperB.GetKeyProperty());
		check(ValueProp == MapHelperB.GetValueProperty());

		int32 Num = MapHelperA.Num();
		if (Num != MapHelperB.Num())
		{
			return false;
		}

		int32 ValueOffset = MapHelperA.MapLayout.ValueOffset;

		// Skip over common initial sequence
		int32 IndexA = 0;
		int32 IndexB = 0;
		for (;;)
		{
			if (Num == 0)
			{
				return true;
			}

			while (!MapHelperA.IsValidIndex(IndexA))
			{
				++IndexA;
			}

			while (!MapHelperB.IsValidIndex(IndexB))
			{
				++IndexB;
			}

			const uint8* PairA = MapHelperA.GetPairPtr(IndexA);
			const uint8* PairB = MapHelperB.GetPairPtr(IndexB);
			if (!KeyProp->Identical(PairA, PairB, PortFlags))
			{
				break;
			}

			if (!ValueProp->Identical(PairA + ValueOffset, PairB + ValueOffset, PortFlags))
			{
				break;
			}

			++IndexA;
			++IndexB;
			--Num;
		}

		int32 FirstIndexA = IndexA;
		int32 FirstIndexB = IndexB;
		int32 FirstNum    = Num;
		for (;;)
		{
			const uint8* PairA = MapHelperA.GetPairPtr(IndexA);
			if (!AnyEqual(MapHelperA, FirstIndexA, FirstNum - Num, PairA, PortFlags) && !RangesContainSameAmountsOfVal(MapHelperA, FirstIndexA, MapHelperB, FirstIndexB, FirstNum, PairA, PortFlags))
			{
				return false;
			}

			--Num;
			if (Num == 0)
			{
				return true;
			}

			++IndexA;
			while (!MapHelperA.IsValidIndex(IndexA))
			{
				++IndexA;
			}
		}
	}
}

IMPLEMENT_FIELD(FMapProperty)

FMapProperty::FMapProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, EMapPropertyFlags InMapFlags)
	: FMapProperty_Super(InOwner, InName, InObjectFlags)
{
	// These are expected to be set post-construction by AddCppProperty
	KeyProp = nullptr;
	ValueProp = nullptr;

	MapFlags = InMapFlags;
}

FMapProperty::FMapProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags, EMapPropertyFlags InMapFlags)
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	: FMapProperty_Super(InOwner, InName, InObjectFlags, InOffset, InFlags)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
{
	// These are expected to be set post-construction by AddCppProperty
	KeyProp   = nullptr;
	ValueProp = nullptr;

	MapFlags = InMapFlags;
}

FMapProperty::FMapProperty(FFieldVariant InOwner, const UECodeGen_Private::FMapPropertyParams& Prop)
	: FMapProperty_Super(InOwner, (const UECodeGen_Private::FPropertyParamsBaseWithOffset&)Prop)
{
	// These are expected to be set post-construction by AddCppProperty
	KeyProp = nullptr;
	ValueProp = nullptr;

	MapFlags = Prop.MapFlags;
}

#if WITH_EDITORONLY_DATA
FMapProperty::FMapProperty(UField* InField)
	: FMapProperty_Super(InField)
	, MapFlags(EMapPropertyFlags::None)
{
	UMapProperty* SourceProperty = CastChecked<UMapProperty>(InField);
	MapLayout = SourceProperty->MapLayout;

	KeyProp = CastField<FProperty>(SourceProperty->KeyProp->GetAssociatedFField());
	if (!KeyProp)
	{
		KeyProp = CastField<FProperty>(CreateFromUField(SourceProperty->KeyProp));
		SourceProperty->KeyProp->SetAssociatedFField(KeyProp);
	}

	ValueProp = CastField<FProperty>(SourceProperty->ValueProp->GetAssociatedFField());
	if (!ValueProp)
	{
		ValueProp = CastField<FProperty>(CreateFromUField(SourceProperty->ValueProp));
		SourceProperty->ValueProp->SetAssociatedFField(ValueProp);
	}
}
#endif // WITH_EDITORONLY_DATA

FMapProperty::~FMapProperty()
{
	delete KeyProp;
	KeyProp = nullptr;
	delete ValueProp;
	ValueProp = nullptr;
}

void FMapProperty::PostDuplicate(const FField& InField)
{
	const FMapProperty& Source = static_cast<const FMapProperty&>(InField);
	KeyProp = CastFieldChecked<FProperty>(FField::Duplicate(Source.KeyProp, this));
	ValueProp = CastFieldChecked<FProperty>(FField::Duplicate(Source.ValueProp, this));
	MapLayout = Source.MapLayout;
	Super::PostDuplicate(InField);
}

void FMapProperty::LinkInternal(FArchive& Ar)
{
	check(KeyProp && ValueProp);

	KeyProp  ->Link(Ar);
	ValueProp->Link(Ar);

	int32 KeySize        = KeyProp  ->GetSize();
	int32 ValueSize      = ValueProp->GetSize();
	int32 KeyAlignment   = KeyProp  ->GetMinAlignment();
	int32 ValueAlignment = ValueProp->GetMinAlignment();

	MapLayout = FScriptMap::GetScriptLayout(KeySize, KeyAlignment, ValueSize, ValueAlignment);

	ValueProp->SetOffset_Internal(MapLayout.ValueOffset);

	Super::LinkInternal(Ar);
}

bool FMapProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	checkSlow(KeyProp);
	checkSlow(ValueProp);

	FScriptMapHelper MapHelperA(this, A);

	int32 ANum = MapHelperA.Num();

	if (!B)
	{
		return ANum == 0;
	}

	FScriptMapHelper MapHelperB(this, B);
	if (ANum != MapHelperB.Num())
	{
		return false;
	}

	return UEMapProperty_Private::IsPermutation(MapHelperA, MapHelperB, PortFlags);
}

void FMapProperty::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	if (KeyProp)
	{
		KeyProp->GetPreloadDependencies(OutDeps);
	}
	if (ValueProp)
	{
		ValueProp->GetPreloadDependencies(OutDeps);
	}
}

void FMapProperty::SerializeItem(FStructuredArchive::FSlot Slot, void* Value, const void* Defaults) const
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	// Map containers must be serialized as a "whole" value, which means that we need to serialize every field for struct-typed entries.
	// When using a custom property list, we need to temporarily bypass this logic to ensure that all map elements are fully serialized.
	const bool bIsUsingCustomPropertyList = !!UnderlyingArchive.ArUseCustomPropertyList;
	UnderlyingArchive.ArUseCustomPropertyList = false;
	ON_SCOPE_EXIT
	{
		UnderlyingArchive.ArUseCustomPropertyList = bIsUsingCustomPropertyList;
	};

	// If we're doing delta serialization within this property, act as if there are no defaults
	if (!UnderlyingArchive.DoIntraPropertyDelta())
	{
		Defaults = nullptr;
	}

	// Ar related calls in this function must be mirrored in FMapProperty::ConvertFromType
	checkSlow(KeyProp);
	checkSlow(ValueProp);

	FScriptMapHelper MapHelper(this, Value);

	if (UnderlyingArchive.IsLoading())
	{
		if (Defaults)
		{
			CopyValuesInternal(Value, Defaults, 1);
		}

		// Delete any explicitly-removed elements
		int32 NumKeysToRemove = 0;
		FStructuredArchive::FArray KeysToRemoveArray = Record.EnterArray(TEXT("KeysToRemove"), NumKeysToRemove);

		if (!Defaults || MapHelper.Num() == 0) // Faster loading path when loading into an empty map
		{
			if (NumKeysToRemove)
			{
				// Load and discard keys to remove, map is empty
				void* TempKeyValueStorage = FMemory::Malloc(MapLayout.SetLayout.Size);
				KeyProp->InitializeValue(TempKeyValueStorage);

				FSerializedPropertyScope SerializedProperty(UnderlyingArchive, KeyProp, this);
				for (; NumKeysToRemove; --NumKeysToRemove)
				{
					KeyProp->SerializeItem(KeysToRemoveArray.EnterElement(), TempKeyValueStorage);
				}

				KeyProp->DestroyValue(TempKeyValueStorage);
				FMemory::Free(TempKeyValueStorage);
			}

			int32 NumEntries = 0;
			FStructuredArchive::FArray EntriesArray = Record.EnterArray(TEXT("Entries"), NumEntries);

			// Empty and reserve then deserialize pairs directly into map memory
			MapHelper.EmptyValues(NumEntries);
			for (; NumEntries; --NumEntries)
			{
				FStructuredArchive::FRecord EntryRecord = EntriesArray.EnterElement().EnterRecord();
				int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();

				{
					FSerializedPropertyScope SerializedProperty(UnderlyingArchive, KeyProp, this);
					KeyProp->SerializeItem(EntryRecord.EnterField(TEXT("Key")), MapHelper.GetKeyPtr(Index));
				}
				{
					FSerializedPropertyScope SerializedProperty(UnderlyingArchive, ValueProp, this);
					ValueProp->SerializeItem(EntryRecord.EnterField(TEXT("Value")), MapHelper.GetValuePtr(Index));
				}
			}

			MapHelper.Rehash();
		}
		else // Slower loading path that mutates non-empty map
		{
			uint8* TempKeyValueStorage = nullptr;
			ON_SCOPE_EXIT
			{
				if (TempKeyValueStorage)
				{
					KeyProp->DestroyValue(TempKeyValueStorage);
					FMemory::Free(TempKeyValueStorage);
				}
			};

			if (NumKeysToRemove)
			{
				TempKeyValueStorage = (uint8*)FMemory::Malloc(MapLayout.SetLayout.Size);
				KeyProp->InitializeValue(TempKeyValueStorage);

				FSerializedPropertyScope SerializedProperty(UnderlyingArchive, KeyProp, this);
				for (; NumKeysToRemove; --NumKeysToRemove)
				{
					// Read key into temporary storage
					KeyProp->SerializeItem(KeysToRemoveArray.EnterElement(), TempKeyValueStorage);

					// If the key is in the map, remove it
					if (uint8* PairPtr = MapHelper.FindMapPairPtrFromHash(TempKeyValueStorage))
					{
						MapHelper.RemovePair(PairPtr);
					}
				}
			}

			int32 NumEntries = 0;
			FStructuredArchive::FArray EntriesArray = Record.EnterArray(TEXT("Entries"), NumEntries);

			// Allocate temporary key space if we haven't allocated it already above
			if (NumEntries != 0 && !TempKeyValueStorage)
			{
				TempKeyValueStorage = (uint8*)FMemory::Malloc(MapLayout.SetLayout.Size);
				KeyProp->InitializeValue(TempKeyValueStorage);
			}

			// Read remaining items into container
			for (; NumEntries; --NumEntries)
			{
				FStructuredArchive::FRecord EntryRecord = EntriesArray.EnterElement().EnterRecord();

				// Read key into temporary storage
				{
					FSerializedPropertyScope SerializedProperty(UnderlyingArchive, KeyProp, this);
					KeyProp->SerializeItem(EntryRecord.EnterField(TEXT("Key")), TempKeyValueStorage);
				}

				void* ValuePtr = MapHelper.FindOrAdd(TempKeyValueStorage);

				// Deserialize value into hash map-owned memory
				{
					FSerializedPropertyScope SerializedProperty(UnderlyingArchive, ValueProp, this);
					ValueProp->SerializeItem(EntryRecord.EnterField(TEXT("Value")), ValuePtr);
				}
			}
		}
	}
	else
	{
		FScriptMapHelper DefaultsHelper(this, Defaults);

		// Container for temporarily tracking some indices
		TSet<int32> Indices;

		// Determine how many keys are missing from the object
		if (Defaults)
		{
			for (int32 Index = 0, Count = DefaultsHelper.Num(); Count; ++Index)
			{
				uint8* DefaultPairPtr = DefaultsHelper.GetPairPtrWithoutCheck(Index);

				if (DefaultsHelper.IsValidIndex(Index))
				{
					if (!MapHelper.FindMapPairPtrWithKey(DefaultPairPtr))
					{
						Indices.Add(Index);
					}

					--Count;
				}
			}
		}

		// Write out the missing keys
		int32 MissingKeysNum = Indices.Num();
		FStructuredArchive::FArray KeysToRemoveArray = Record.EnterArray(TEXT("KeysToRemove"), MissingKeysNum);
		{
			FSerializedPropertyScope SerializedProperty(UnderlyingArchive, KeyProp, this);
			for (int32 Index : Indices)
			{
				KeyProp->SerializeItem(KeysToRemoveArray.EnterElement(), DefaultsHelper.GetPairPtr(Index));
			}
		}

		// Write out differences from defaults
		if (Defaults)
		{
			Indices.Empty(Indices.Num());
			for (int32 Index = 0, Count = MapHelper.Num(); Count; ++Index)
			{
				if (MapHelper.IsValidIndex(Index))
				{
					uint8* ValuePairPtr   = MapHelper.GetPairPtrWithoutCheck(Index);
					uint8* DefaultPairPtr = DefaultsHelper.FindMapPairPtrWithKey(ValuePairPtr);

					if (!DefaultPairPtr || !ValueProp->Identical(ValuePairPtr + MapLayout.ValueOffset, DefaultPairPtr + MapLayout.ValueOffset))
					{
						Indices.Add(Index);
					}

					--Count;
				}
			}

			// Write out differences from defaults
			int32 Num = Indices.Num();
			FStructuredArchive::FArray EntriesArray = Record.EnterArray(TEXT("Entries"), Num);
			for (int32 Index : Indices)
			{
				uint8* ValuePairPtr = MapHelper.GetPairPtrWithoutCheck(Index);
				FStructuredArchive::FRecord EntryRecord = EntriesArray.EnterElement().EnterRecord();

				{
					FSerializedPropertyScope SerializedProperty(UnderlyingArchive, KeyProp, this);
					KeyProp->SerializeItem(EntryRecord.EnterField(TEXT("Key")), ValuePairPtr);
				}
				{
					FSerializedPropertyScope SerializedProperty(UnderlyingArchive, ValueProp, this);
					ValueProp->SerializeItem(EntryRecord.EnterField(TEXT("Value")), ValuePairPtr + MapLayout.ValueOffset);
				}
			}
		}
		else
		{
			int32 Num = MapHelper.Num();
			FStructuredArchive::FArray EntriesArray = Record.EnterArray(TEXT("Entries"), Num);

			for (int32 Index = 0; Num; ++Index)
			{
				if (MapHelper.IsValidIndex(Index))
				{
					FStructuredArchive::FRecord EntryRecord = EntriesArray.EnterElement().EnterRecord();

					uint8* ValuePairPtr = MapHelper.GetPairPtrWithoutCheck(Index);

					{
						FSerializedPropertyScope SerializedProperty(UnderlyingArchive, KeyProp, this);
						KeyProp->SerializeItem(EntryRecord.EnterField(TEXT("Key")), ValuePairPtr);
					}
					{
						FSerializedPropertyScope SerializedProperty(UnderlyingArchive, ValueProp, this);
						ValueProp->SerializeItem(EntryRecord.EnterField(TEXT("Value")), ValuePairPtr + MapLayout.ValueOffset);
					}

					--Num;
				}
			}
		}
	}
}

bool FMapProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	UE_LOG( LogProperty, Error, TEXT( "Replicated TMaps are not supported." ) );
	return 1;
}

void FMapProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	SerializeSingleField(Ar, KeyProp, this);
	SerializeSingleField(Ar, ValueProp, this);
}

void FMapProperty::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	if (KeyProp)
	{
		KeyProp->AddReferencedObjects(Collector);
	}
	if (ValueProp)
	{
		ValueProp->AddReferencedObjects(Collector);
	}
}

FString FMapProperty::GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& KeyTypeText, const FString& InKeyExtendedTypeText, const FString& ValueTypeText, const FString& InValueExtendedTypeText) const
{
	if (ExtendedTypeText)
	{
		// if property type is a template class, add a space between the closing brackets
		FString KeyExtendedTypeText = InKeyExtendedTypeText;
		if ((KeyExtendedTypeText.Len() && KeyExtendedTypeText.Right(1) == TEXT(">"))
			|| (!KeyExtendedTypeText.Len() && KeyTypeText.Len() && KeyTypeText.Right(1) == TEXT(">")))
		{
			KeyExtendedTypeText += TEXT(" ");
		}

		// if property type is a template class, add a space between the closing brackets
		FString ValueExtendedTypeText = InValueExtendedTypeText;
		if ((ValueExtendedTypeText.Len() && ValueExtendedTypeText.Right(1) == TEXT(">"))
			|| (!ValueExtendedTypeText.Len() && ValueTypeText.Len() && ValueTypeText.Right(1) == TEXT(">")))
		{
			ValueExtendedTypeText += TEXT(" ");
		}

		*ExtendedTypeText = FString::Printf(TEXT("<%s%s,%s%s>"), *KeyTypeText, *KeyExtendedTypeText, *ValueTypeText, *ValueExtendedTypeText);
	}

	return TEXT("TMap");
}

FString FMapProperty::GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags) const
{
	checkSlow(KeyProp);
	checkSlow(ValueProp);

	FString KeyTypeText, KeyExtendedTypeText;
	FString ValueTypeText, ValueExtendedTypeText;

	if (ExtendedTypeText)
	{
		KeyTypeText = KeyProp->GetCPPType(&KeyExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider map keys to be "arguments or return values"
		ValueTypeText = ValueProp->GetCPPType(&ValueExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider map values to be "arguments or return values"
	}

	return GetCPPTypeCustom(ExtendedTypeText, CPPExportFlags, KeyTypeText, KeyExtendedTypeText, ValueTypeText, ValueExtendedTypeText);
}

FString FMapProperty::GetCPPTypeForwardDeclaration() const
{
	checkSlow(KeyProp);
	checkSlow(ValueProp);
	// Generates a single ' ' when no forward declaration is needed. Purely an aesthetic concern at this time:
	return FString::Printf( TEXT("%s %s"), *KeyProp->GetCPPTypeForwardDeclaration(), *ValueProp->GetCPPTypeForwardDeclaration());
}

FString FMapProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	checkSlow(KeyProp);
	checkSlow(ValueProp);
	ExtendedTypeText = FString::Printf(TEXT("%s,%s"), *KeyProp->GetCPPType(), *ValueProp->GetCPPType());
	return TEXT("TMAP");
}

void FMapProperty::ExportText_Internal(FString& ValueStr, const void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += TEXT("{}");
		return;
	}

	checkSlow(KeyProp);
	checkSlow(ValueProp);

	uint8* TempMapStorage = nullptr;
	void* PropertyValuePtr = nullptr;
	if (PropertyPointerType == EPropertyPointerType::Container && HasGetter())
	{
		// Allocate temporary map as we first need to initialize it with the value provided by the getter function and then export it
		TempMapStorage = (uint8*)AllocateAndInitializeValue();
		PropertyValuePtr = TempMapStorage;
		FProperty::GetValue_InContainer(ContainerOrPropertyPtr, PropertyValuePtr);
	}
	else
	{
		PropertyValuePtr = PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
	}

	ON_SCOPE_EXIT
	{
		DestroyAndFreeValue(TempMapStorage);
	};

	FScriptMapHelper MapHelper(this, PropertyValuePtr);

	if (MapHelper.Num() == 0)
	{
		ValueStr += TEXT("()");
		return;
	}

	const bool bExternalEditor = (0 != (PPF_ExternalEditor & PortFlags));

	uint8* StructDefaults = nullptr;
	if (FStructProperty* StructValueProp = CastField<FStructProperty>(ValueProp))
	{
		checkSlow(StructValueProp->Struct);

		if (!bExternalEditor)
		{
			// For external editor, we always export all fields
			StructDefaults = (uint8*)FMemory::Malloc(MapLayout.SetLayout.Size);
			ValueProp->InitializeValue(StructDefaults + MapLayout.ValueOffset);
		}
	}
	ON_SCOPE_EXIT
	{
		if (StructDefaults)
		{
			ValueProp->DestroyValue(StructDefaults + MapLayout.ValueOffset);
			FMemory::Free(StructDefaults);
		}
	};

	FScriptMapHelper DefaultMapHelper(this, DefaultValue);

	uint8* PropData = MapHelper.GetPairPtrWithoutCheck(0);
	if (PortFlags & PPF_BlueprintDebugView)
	{
		int32 Index  = 0;
		bool  bFirst = true;
		for (int32 Count = MapHelper.Num(); Count; PropData += MapLayout.SetLayout.Size, ++Index)
		{
			if (MapHelper.IsValidIndex(Index))
			{
				if (bFirst)
				{
					bFirst = false;
				}
				else
				{
					ValueStr += TCHAR('\n');
				}

				ValueStr += TEXT("[");
				KeyProp->ExportText_Internal(ValueStr, PropData, EPropertyPointerType::Direct, nullptr, Parent, PortFlags | PPF_Delimited, ExportRootScope);
				ValueStr += TEXT("] ");

				// Always use struct defaults if the inner is a struct, for symmetry with the import of array inner struct defaults
				uint8* PropDefault = StructDefaults ? StructDefaults : DefaultValue ? DefaultMapHelper.FindMapPairPtrWithKey(PropData) : nullptr;

				if (bExternalEditor)
				{
					// For external editor, always write
					PropDefault = PropData;
				}

				ValueProp->ExportText_Internal(ValueStr, PropData + MapLayout.ValueOffset, EPropertyPointerType::Direct, PropDefault + MapLayout.ValueOffset, Parent, PortFlags | PPF_Delimited, ExportRootScope);

				--Count;
			}
		}
	}
	else
	{
		int32 Index  = 0;
		bool  bFirst = true;
		for (int32 Count = MapHelper.Num(); Count; PropData += MapLayout.SetLayout.Size, ++Index)
		{
			if (MapHelper.IsValidIndex(Index))
			{
				if (bFirst)
				{
					ValueStr += TCHAR('(');
					bFirst = false;
				}
				else
				{
					ValueStr += TCHAR(',');
				}

				ValueStr += TEXT("(");

				KeyProp->ExportText_Internal(ValueStr, PropData, EPropertyPointerType::Direct, nullptr, Parent, PortFlags | PPF_Delimited, ExportRootScope);

				ValueStr += TEXT(", ");

				// Always use struct defaults if the inner is a struct, for symmetry with the import of array inner struct defaults
				uint8* PropDefault = StructDefaults ? StructDefaults : DefaultValue ? DefaultMapHelper.FindMapPairPtrWithKey(PropData) : nullptr;

				if (bExternalEditor)
				{
					// For external editor, always write
					PropDefault = PropData;
				}

				ValueProp->ExportText_Internal(ValueStr, PropData + MapLayout.ValueOffset, EPropertyPointerType::Direct, PropDefault + MapLayout.ValueOffset, Parent, PortFlags | PPF_Delimited, ExportRootScope);

				ValueStr += TEXT(")");

				--Count;
			}
		}

		ValueStr += TEXT(")");
	}
}

const TCHAR* FMapProperty::ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* Parent, int32 PortFlags, FOutputDevice* ErrorText) const
{
	checkSlow(KeyProp);
	checkSlow(ValueProp);

	FScriptMapHelper MapHelper(this, PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType));
	uint8* TempMapStorage = nullptr;

	ON_SCOPE_EXIT
	{
		if (TempMapStorage)
		{
			// TempMap is used by property setter so if it was allocated call the setter now
			FProperty::SetValue_InContainer(ContainerOrPropertyPtr, TempMapStorage);

			// Destroy and free the temp map used by property setter
			DestroyAndFreeValue(TempMapStorage);
		}
	};

	if (PropertyPointerType == EPropertyPointerType::Container && HasSetter())
	{
		// Allocate temporary map as we first need to initialize it with the parsed items and then use the setter to update the property
		TempMapStorage = (uint8*)AllocateAndInitializeValue();
		// Reinitialize the map helper with the temp value
		MapHelper = FScriptMapHelper(this, TempMapStorage);
	}

	MapHelper.EmptyValues();

	// If we export an empty array we export an empty string, so ensure that if we're passed an empty string
	// we interpret it as an empty array.
	if (*Buffer++ != TCHAR('('))
	{
		return nullptr;
	}

	SkipWhitespace(Buffer);
	if (*Buffer == TCHAR(')'))
	{
		return Buffer + 1;
	}

	uint8* TempPairStorage   = (uint8*)FMemory::Malloc(MapLayout.ValueOffset + ValueProp->ElementSize);

	bool bSuccess = false;
	ON_SCOPE_EXIT
	{
		FMemory::Free(TempPairStorage);

		// If we are returning because of an error, remove any already-added elements from the map before returning
		// to ensure we're not left with a partial state.
		if (!bSuccess)
		{
			MapHelper.EmptyValues();
		}
	};

	for (;;)
	{
		KeyProp->InitializeValue(TempPairStorage);
		ValueProp->InitializeValue(TempPairStorage + MapLayout.ValueOffset);
		ON_SCOPE_EXIT
		{
			ValueProp->DestroyValue(TempPairStorage + MapLayout.ValueOffset);
			KeyProp->DestroyValue(TempPairStorage);
		};

		if (*Buffer++ != TCHAR('('))
		{
			return nullptr;
		}

		// Parse the key
		SkipWhitespace(Buffer);
		Buffer = KeyProp->ImportText_Internal(Buffer, TempPairStorage, EPropertyPointerType::Direct, Parent, PortFlags | PPF_Delimited, ErrorText);
		if (!Buffer)
		{
			return nullptr;
		}

		// Skip this element if it's already in the map
		bool bSkip = MapHelper.FindMapIndexWithKey(TempPairStorage) != INDEX_NONE;

		SkipWhitespace(Buffer);
		if (*Buffer++ != TCHAR(','))
		{
			return nullptr;
		}

		// Parse the value
		SkipWhitespace(Buffer);
		Buffer = ValueProp->ImportText_Internal(Buffer, TempPairStorage + MapLayout.ValueOffset, EPropertyPointerType::Direct, Parent, PortFlags | PPF_Delimited, ErrorText);
		if (!Buffer)
		{
			return nullptr;
		}

		SkipWhitespace(Buffer);
		if (*Buffer++ != TCHAR(')'))
		{
			return nullptr;
		}

		if (!bSkip)
		{
			int32  Index   = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
			uint8* PairPtr = MapHelper.GetPairPtrWithoutCheck(Index);

			// Copy over imported key and value from temporary storage
			KeyProp  ->CopyCompleteValue_InContainer(PairPtr, TempPairStorage);
			ValueProp->CopyCompleteValue_InContainer(PairPtr, TempPairStorage);
		}

		SkipWhitespace(Buffer);
		switch (*Buffer++)
		{
			case TCHAR(')'):
				MapHelper.Rehash();
				bSuccess = true;
				return Buffer;

			case TCHAR(','):
				SkipWhitespace(Buffer);
				break;

			default:
				return nullptr;
		}
	}
}

void FMapProperty::AddCppProperty(FProperty* Property)
{
	check(Property);

	if (!KeyProp)
	{
		// If the key is unset, assume it's the key
		check(!KeyProp);
		ensureAlwaysMsgf(Property->HasAllPropertyFlags(CPF_HasGetValueTypeHash), TEXT("Attempting to create Map Property with unhashable key type: %s - Provide a GetTypeHash function!"), *Property->GetName());
		KeyProp = Property;
	}
	else
	{
		// Otherwise assume it's the value
		check(!ValueProp);
		ValueProp = Property;
	}
}

void FMapProperty::CopyValuesInternal(void* Dest, void const* Src, int32 Count) const
{
	check(Count == 1);

	FScriptMapHelper SrcMapHelper (this, Src);
	FScriptMapHelper DestMapHelper(this, Dest);

	int32 Num = SrcMapHelper.Num();
	DestMapHelper.EmptyValues(Num);

	if (Num == 0)
	{
		return;
	}

	for (int32 SrcIndex = 0; Num; ++SrcIndex)
	{
		if (SrcMapHelper.IsValidIndex(SrcIndex))
		{
			int32 DestIndex = DestMapHelper.AddDefaultValue_Invalid_NeedsRehash();

			uint8* SrcData  = SrcMapHelper .GetPairPtrWithoutCheck(SrcIndex);
			uint8* DestData = DestMapHelper.GetPairPtrWithoutCheck(DestIndex);

			KeyProp  ->CopyCompleteValue_InContainer(DestData, SrcData);
			ValueProp->CopyCompleteValue_InContainer(DestData, SrcData);

			--Num;
		}
	}

	DestMapHelper.Rehash();
}

void FMapProperty::ClearValueInternal(void* Data) const
{
	FScriptMapHelper MapHelper(this, Data);
	MapHelper.EmptyValues();
}

void FMapProperty::DestroyValueInternal(void* Data) const
{
	FScriptMapHelper MapHelper(this, Data);
	MapHelper.EmptyValues();

	//@todo UE potential double destroy later from this...would be ok for a script map, but still
	((FScriptMap*)Data)->~FScriptMap();
}

bool FMapProperty::PassCPPArgsByRef() const
{
	return true;
}

/**
 * Creates new copies of components
 * 
 * @param	Data				pointer to the address of the instanced object referenced by this UComponentProperty
 * @param	DefaultData			pointer to the address of the default value of the instanced object referenced by this UComponentProperty
 * @param	Owner				the object that contains this property's data
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void FMapProperty::InstanceSubobjects(void* Data, void const* DefaultData, UObject* InOwner, FObjectInstancingGraph* InstanceGraph)
{
	if (!Data)
	{
		return;
	}

	bool bInstancedKey   = KeyProp  ->ContainsInstancedObjectProperty();
	bool bInstancedValue = ValueProp->ContainsInstancedObjectProperty();

	if (!bInstancedKey && !bInstancedValue)
	{
		return;
	}

	FScriptMapHelper MapHelper(this, Data);

	if (DefaultData)
	{
		FScriptMapHelper DefaultMapHelper(this, DefaultData);
		int32            DefaultNum = DefaultMapHelper.Num();

		for (int32 Index = 0, Num = MapHelper.Num(); Num; ++Index)
		{
			if (MapHelper.IsValidIndex(Index))
			{
				uint8* PairPtr        = MapHelper.GetPairPtr(Index);
				uint8* DefaultPairPtr = DefaultMapHelper.FindMapPairPtrWithKey(PairPtr, Index);

				if (bInstancedKey)
				{
					KeyProp->InstanceSubobjects(PairPtr, DefaultPairPtr, InOwner, InstanceGraph);
				}

				if (bInstancedValue)
				{
					ValueProp->InstanceSubobjects(PairPtr + MapLayout.ValueOffset, DefaultPairPtr ? DefaultPairPtr + MapLayout.ValueOffset : nullptr, InOwner, InstanceGraph);
				}

				--Num;
			}
		}
	}
	else
	{
		for (int32 Index = 0, Num = MapHelper.Num(); Num; ++Index)
		{
			if (MapHelper.IsValidIndex(Index))
			{
				uint8* PairPtr = MapHelper.GetPairPtr(Index);

				if (bInstancedKey)
				{
					KeyProp->InstanceSubobjects(PairPtr, nullptr, InOwner, InstanceGraph);
				}

				if (bInstancedValue)
				{
					ValueProp->InstanceSubobjects(PairPtr + MapLayout.ValueOffset, nullptr, InOwner, InstanceGraph);
				}

				--Num;
			}
		}
	}
}

bool FMapProperty::SameType(const FProperty* Other) const
{
	FMapProperty* MapProp = (FMapProperty*)Other;
	return Super::SameType(Other) && KeyProp && ValueProp && KeyProp->SameType(MapProp->KeyProp) && ValueProp->SameType(MapProp->ValueProp);
}

EConvertFromTypeResult FMapProperty::ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct)
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();

	// Ar related calls in this function must be mirrored in FMapProperty::SerializeItem
	checkSlow(KeyProp);
	checkSlow(ValueProp);

	const auto SerializeOrConvert = [](FProperty* CurrentType, const FPropertyTag& InTag, FStructuredArchive::FSlot InnerSlot, uint8* InData, UStruct* InDefaultsStruct) -> bool
	{
		// Serialize wants the property address, while convert wants the container address. InData is the container address
		if(CurrentType->GetID() == InTag.Type)
		{
			uint8* DestAddress = CurrentType->ContainerPtrToValuePtr<uint8>(InData, InTag.ArrayIndex);
			CurrentType->SerializeItem(InnerSlot, DestAddress, nullptr);
			return true;
		}
		else if( CurrentType->ConvertFromType(InTag, InnerSlot, InData, InDefaultsStruct) == EConvertFromTypeResult::Converted )
		{
			return true;
		}
		return false;
	};

	if (Tag.Type == NAME_MapProperty)
	{
		if( (Tag.InnerType != NAME_None && Tag.InnerType != KeyProp->GetID()) || (Tag.ValueType != NAME_None && Tag.ValueType != ValueProp->GetID()) )
		{
			FScriptMapHelper MapHelper(this, ContainerPtrToValuePtr<void>(Data));

			uint8* TempKeyValueStorage = nullptr;
			ON_SCOPE_EXIT
			{
				if (TempKeyValueStorage)
				{
					KeyProp->DestroyValue(TempKeyValueStorage);
					FMemory::Free(TempKeyValueStorage);
				}
			};

			FPropertyTag KeyPropertyTag;
			KeyPropertyTag.Type = Tag.InnerType;
			KeyPropertyTag.ArrayIndex = 0;

			FPropertyTag ValuePropertyTag;
			ValuePropertyTag.Type = Tag.ValueType;
			ValuePropertyTag.ArrayIndex = 0;

			bool bConversionSucceeded = true;

			FStructuredArchive::FRecord ValueRecord = Slot.EnterRecord();

			// When we saved this instance we wrote out any elements that were in the 'Default' instance but not in the 
			// instance that was being written. Presumably we were constructed from our defaults and must now remove 
			// any of the elements that were not present when we saved this Map:
			int32 NumKeysToRemove = 0;
			FStructuredArchive::FArray KeysToRemoveArray = ValueRecord.EnterArray(TEXT("KeysToRemove"), NumKeysToRemove);

			if( NumKeysToRemove != 0 )
			{
				TempKeyValueStorage = (uint8*)FMemory::Malloc(MapLayout.SetLayout.Size);
				KeyProp->InitializeValue(TempKeyValueStorage);

				if (SerializeOrConvert( KeyProp, KeyPropertyTag, KeysToRemoveArray.EnterElement(), TempKeyValueStorage, DefaultsStruct))
				{
					// If the key is in the map, remove it
					int32 Found = MapHelper.FindMapIndexWithKey(TempKeyValueStorage);
					if (Found != INDEX_NONE)
					{
						MapHelper.RemoveAt(Found);
					}

					// things are going fine, remove the rest of the keys:
					for(int32 I = 1; I < NumKeysToRemove; ++I)
					{
						verify(SerializeOrConvert( KeyProp, KeyPropertyTag, KeysToRemoveArray.EnterElement(), TempKeyValueStorage, DefaultsStruct));
						Found = MapHelper.FindMapIndexWithKey(TempKeyValueStorage);
						if (Found != INDEX_NONE)
						{
							MapHelper.RemoveAt(Found);
						}
					}
				}
				else
				{
					bConversionSucceeded = false;
				}
			}

			int32 NumEntries = 0;
			FStructuredArchive::FArray EntriesArray = ValueRecord.EnterArray(TEXT("Entries"), NumEntries);

			if( bConversionSucceeded )
			{
				if( NumEntries != 0 )
				{
					if(TempKeyValueStorage == nullptr )
					{
						TempKeyValueStorage = (uint8*)FMemory::Malloc(MapLayout.SetLayout.Size);
						KeyProp->InitializeValue(TempKeyValueStorage);
					}

					FStructuredArchive::FRecord FirstPropertyRecord = EntriesArray.EnterElement().EnterRecord();

					if( SerializeOrConvert( KeyProp, KeyPropertyTag, FirstPropertyRecord.EnterField(TEXT("Key")), TempKeyValueStorage, DefaultsStruct ) )
					{
						// Add a new default value if the key doesn't currently exist in the map
						bool bKeyAlreadyPresent = true;
						int32 NextPairIndex = MapHelper.FindMapIndexWithKey(TempKeyValueStorage);
						if (NextPairIndex == INDEX_NONE)
						{
							bKeyAlreadyPresent = false;
							NextPairIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
						}

						uint8* NextPairPtr = MapHelper.GetPairPtrWithoutCheck(NextPairIndex);
						// This copy is unnecessary when the key was already in the map:
						KeyProp->CopyCompleteValue_InContainer(NextPairPtr, TempKeyValueStorage);

						// Deserialize value
						if( SerializeOrConvert( ValueProp, ValuePropertyTag, FirstPropertyRecord.EnterField(TEXT("Value")), NextPairPtr, DefaultsStruct ) )
						{
							// first entry went fine, convert the rest:
							for(int32 I = 1; I < NumEntries; ++I)
							{
								FStructuredArchive::FRecord PropertyRecord = EntriesArray.EnterElement().EnterRecord();

								verify( SerializeOrConvert( KeyProp, KeyPropertyTag, PropertyRecord.EnterField(TEXT("Key")), TempKeyValueStorage, DefaultsStruct ) );
								NextPairIndex = MapHelper.FindMapIndexWithKey(TempKeyValueStorage);
								if (NextPairIndex == INDEX_NONE)
								{
									NextPairIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
								}

								NextPairPtr = MapHelper.GetPairPtrWithoutCheck(NextPairIndex);
								// This copy is unnecessary when the key was already in the map:
								KeyProp->CopyCompleteValue_InContainer(NextPairPtr, TempKeyValueStorage);
								verify( SerializeOrConvert( ValueProp, ValuePropertyTag, PropertyRecord.EnterField(TEXT("Value")), NextPairPtr, DefaultsStruct ) );
							}
						}
						else
						{
							if(!bKeyAlreadyPresent)
							{
								MapHelper.EmptyValues();
							}

							bConversionSucceeded = false;
						}
					}
					else
					{
						bConversionSucceeded = false;
					}

					MapHelper.Rehash();
				}
			}

			// if we could not convert the property ourself, then indicate that calling code needs to advance the property
			if(!bConversionSucceeded)
			{
				UE_LOG(
					LogClass,
					Warning,
					TEXT("Map Element Type mismatch in %s of %s - Previous (%s to %s) Current (%s to %s) for package: %s"),
					*Tag.Name.ToString(),
					*GetName(),
					*Tag.InnerType.ToString(),
					*Tag.ValueType.ToString(),
					*KeyProp->GetID().ToString(),
					*ValueProp->GetID().ToString(),
					*UnderlyingArchive.GetArchiveName()
				);
			}

			return bConversionSucceeded ? EConvertFromTypeResult::Converted : EConvertFromTypeResult::CannotConvert;
		}

		if (FStructProperty* KeyPropAsStruct = CastField<FStructProperty>(KeyProp))
		{
			if(!KeyPropAsStruct->Struct || (KeyPropAsStruct->Struct->GetCppStructOps() && !KeyPropAsStruct->Struct->GetCppStructOps()->HasGetTypeHash() ) )
			{
				// If the type we contain is no longer hashable, we're going to drop the saved data here. This can
				// happen if the native GetTypeHash function is removed.
				ensureMsgf(false, TEXT("FMapProperty %s with tag %s has an unhashable key type %s and will lose its saved data"), *GetName(), *Tag.Name.ToString(), *KeyProp->GetID().ToString());

				FScriptMapHelper ScriptMapHelper(this, ContainerPtrToValuePtr<void>(Data));
				ScriptMapHelper.EmptyValues();

				return EConvertFromTypeResult::CannotConvert;
			}
		}
	}

	return EConvertFromTypeResult::UseSerializeItem;
}

#if WITH_EDITORONLY_DATA
void FMapProperty::AppendSchemaHash(FBlake3& Builder, bool bSkipEditorOnly) const
{
	Super::AppendSchemaHash(Builder, bSkipEditorOnly);
	if (KeyProp)
	{
		KeyProp->AppendSchemaHash(Builder, bSkipEditorOnly);
	}
	if (ValueProp)
	{
		ValueProp->AppendSchemaHash(Builder, bSkipEditorOnly);
	}
}
#endif

void FScriptMapHelper::Rehash()
{
	WithScriptMap([this](auto* Map)
	{
		// Moved out-of-line to maybe fix a weird link error
		Map->Rehash(MapLayout, [=](const void* Src) {
			return KeyProp->GetValueTypeHash(Src);
		});
	});
}

FField* FMapProperty::GetInnerFieldByName(const FName& InName)
{
	if (KeyProp && KeyProp->GetFName() == InName)
	{
		return KeyProp;
	}
	else if (ValueProp && ValueProp->GetFName() == InName)
	{
		return ValueProp;
	}
	return nullptr;
}

void FMapProperty::GetInnerFields(TArray<FField*>& OutFields)
{
	if (KeyProp)
	{
		OutFields.Add(KeyProp);
		KeyProp->GetInnerFields(OutFields);
	}
	if (ValueProp)
	{
		OutFields.Add(ValueProp);
		ValueProp->GetInnerFields(OutFields);
	}
}

void* FMapProperty::GetValueAddressAtIndex_Direct(const FProperty* Inner, void* InValueAddress, int32 Index) const
{
	FScriptMapHelper MapHelper(this, InValueAddress);
	checkf(Inner == KeyProp || Inner == ValueProp, TEXT("Inner property must be either KeyProp or ValueProp"));

	for (int32 MapIndex = 0, Num = MapHelper.Num(), LocalIndex = Index; LocalIndex >= 0 && Num > 0; ++MapIndex)
	{
		if (MapHelper.IsValidIndex(MapIndex))
		{
			if (LocalIndex == 0)
			{
				if (Inner == KeyProp)
				{
					return MapHelper.GetKeyPtr(MapIndex);
				}
				else
				{
					return MapHelper.GetValuePtr(MapIndex);
				}
			}
			LocalIndex--;
			Num--;
		}
	}
	checkf(false, TEXT("Map element index (%d) out of range"), Index);
	return nullptr;
}

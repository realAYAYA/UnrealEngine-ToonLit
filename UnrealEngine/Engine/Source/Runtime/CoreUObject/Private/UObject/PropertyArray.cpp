// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/Casts.h"
#include "UObject/PropertyTag.h"
#include "UObject/UnrealType.h"
#include "UObject/UnrealTypePrivate.h"
#include "UObject/LinkerLoad.h"
#include "UObject/PropertyHelper.h"

/*-----------------------------------------------------------------------------
	FArrayProperty.
-----------------------------------------------------------------------------*/
IMPLEMENT_FIELD(FArrayProperty)

FArrayProperty::FArrayProperty(FFieldVariant InOwner, const UECodeGen_Private::FArrayPropertyParams& Prop)
	: Super(InOwner, (const UECodeGen_Private::FPropertyParamsBaseWithOffset&)Prop)
	, Inner(nullptr)
{
	ArrayFlags = Prop.ArrayFlags;
	SetElementSize();
}

#if WITH_EDITORONLY_DATA
FArrayProperty::FArrayProperty(UField* InField)
	: FArrayProperty_Super(InField)
	, ArrayFlags(EArrayPropertyFlags::None)
{
	UArrayProperty* SourceProperty = CastChecked<UArrayProperty>(InField);
	Inner = CastField<FProperty>(SourceProperty->Inner->GetAssociatedFField());
	if (!Inner)
	{
		Inner = CastField<FProperty>(CreateFromUField(SourceProperty->Inner));
		SourceProperty->Inner->SetAssociatedFField(Inner);
	}
}
#endif // WITH_EDITORONLY_DATA

FArrayProperty::~FArrayProperty()
{
	delete Inner;
	Inner = nullptr;
}

void FArrayProperty::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	if (Inner)
	{
		Inner->GetPreloadDependencies(OutDeps);
	}
}

void FArrayProperty::PostDuplicate(const FField& InField)
{
	const FArrayProperty& Source = static_cast<const FArrayProperty&>(InField);
	Inner = CastFieldChecked<FProperty>(FField::Duplicate(Source.Inner, this));
	Super::PostDuplicate(InField);
}

void FArrayProperty::LinkInternal(FArchive& Ar)
{
	//FLinkerLoad* MyLinker = GetLinker();
	//if( MyLinker )
	//{
	//	MyLinker->Preload(this);
	//}
	//Ar.Preload(Inner);
	Inner->Link(Ar);

	SetElementSize();
}
bool FArrayProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	checkSlow(Inner);

	FScriptArrayHelper ArrayHelperA(this, A);

	const int32 ArrayNum = ArrayHelperA.Num();
	if ( B == NULL )
	{
		return ArrayNum == 0;
	}

	FScriptArrayHelper ArrayHelperB(this, B);
	if ( ArrayNum != ArrayHelperB.Num() )
	{
		return false;
	}

	for ( int32 ArrayIndex = 0; ArrayIndex < ArrayNum; ArrayIndex++ )
	{
		if ( !Inner->Identical( ArrayHelperA.GetRawPtr(ArrayIndex), ArrayHelperB.GetRawPtr(ArrayIndex), PortFlags) )
		{
			return false;
		}
	}

	return true;
}

static bool CanBulkSerialize(FProperty* Property)
{
#if PLATFORM_LITTLE_ENDIAN
	// All numeric properties except TEnumAsByte
	uint64 CastFlags = Property->GetClass()->GetCastFlags();
	if (!!(CastFlags & CASTCLASS_FNumericProperty))
	{
		bool bEnumAsByte = (CastFlags & CASTCLASS_FByteProperty) != 0 && static_cast<FByteProperty*>(Property)->Enum;
		return !bEnumAsByte;
	}
#endif

	return false;
}

void FArrayProperty::SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const
{
	check(Inner);
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	const bool bIsTextFormat = UnderlyingArchive.IsTextFormat();
	const bool bUPS = Slot.GetArchiveState().UseUnversionedPropertySerialization();
	TOptional<FPropertyTag> MaybeInnerTag;

	// Ensure that the Inner itself has been loaded before calling SerializeItem() on it
	//UnderlyingArchive.Preload(Inner);

	FScriptArrayHelper ArrayHelper(this, Value);
	int32		n		= ArrayHelper.Num();

	// Custom branch for UPS to try and take advantage of bulk serialization
	if (bUPS)
	{
		checkf(!UnderlyingArchive.ArUseCustomPropertyList, TEXT("Custom property lists are not supported with UPS"));
		checkf(!bIsTextFormat, TEXT("Text-based archives are not supported with UPS"));

		if (CanBulkSerialize(Inner))
		{
			// We need to enter the slot as *something* to keep the structured archive system happy,
			// but which maps down to straight writes to the underlying archive.
			FStructuredArchiveStream Stream = Slot.EnterStream();

			Stream.EnterElement() << n;

			if (UnderlyingArchive.IsLoading())
			{
				ArrayHelper.EmptyAndAddUninitializedValues(n);
			}

			Stream.EnterElement().Serialize(ArrayHelper.GetRawPtr(), n * Inner->ElementSize);
		}
		else
		{
			FStructuredArchiveArray Array = Slot.EnterArray(n);

			if (UnderlyingArchive.IsLoading())
			{
				ArrayHelper.EmptyAndAddValues(n);
			}

			FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Inner, this);
			for (int32 i = 0; i < n; ++i)
			{
#if WITH_EDITOR
				static const FName NAME_UArraySerialize = FName(TEXT("FArrayProperty::Serialize"));
				FName NAME_UArraySerializeCount = FName(NAME_UArraySerialize);
				NAME_UArraySerializeCount.SetNumber(i);
				FArchive::FScopeAddDebugData P(UnderlyingArchive, NAME_UArraySerializeCount);
#endif
				Inner->SerializeItem(Array.EnterElement(), ArrayHelper.GetRawPtr(i));
			}
		}

		return;
	}

	if (bIsTextFormat && Inner->IsA<FStructProperty>())
	{
		MaybeInnerTag.Emplace(UnderlyingArchive, Inner, 0, (uint8*)Value, (uint8*)Defaults);	
		Slot << SA_ATTRIBUTE(TEXT("InnerStructName"), MaybeInnerTag.GetValue().StructName);
		Slot << SA_OPTIONAL_ATTRIBUTE(TEXT("InnerStructGuid"), MaybeInnerTag.GetValue().StructGuid, FGuid());
	}

	FStructuredArchiveArray Array = Slot.EnterArray(n);

	if( UnderlyingArchive.IsLoading() )
	{
		// If using a custom property list, don't empty the array on load. Not all indices may have been serialized, so we need to preserve existing values at those slots.
		if (UnderlyingArchive.ArUseCustomPropertyList)
		{
			const int32 OldNum = ArrayHelper.Num();
			if (n > OldNum)
			{
				ArrayHelper.AddValues(n - OldNum);
			}
			else if (n < OldNum)
			{
				ArrayHelper.RemoveValues(n, OldNum - n);
			}
		}
		else
		{
			ArrayHelper.EmptyAndAddValues(n);
		}
	}
	ArrayHelper.CountBytes( UnderlyingArchive );


	TOptional<FPropertyTag> SerializeFromMismatchedTag;

	// TODO: Should work for maps + sets too.
	auto SerializeContainerItem = [this, &SerializeFromMismatchedTag](FStructuredArchiveSlot Slot, uint8* Item)
	{
		if(SerializeFromMismatchedTag.IsSet())
		{
			const int64 StartOfProperty = Slot.GetUnderlyingArchive().Tell();
			FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Inner);
			switch(StructProperty->ConvertFromType(SerializeFromMismatchedTag.GetValue(), Slot, Item, nullptr))
			{
				case EConvertFromTypeResult::Converted:
					return;
				case EConvertFromTypeResult::CannotConvert:
					// FStructProperty::ConvertFromType doesn't handle setting the default, so do it here.
					StructProperty->Struct->InitializeDefaultValue(Item);
					Slot.GetUnderlyingArchive().Seek(StartOfProperty + SerializeFromMismatchedTag.GetValue().Size);	// Skip this item
					return;
				case EConvertFromTypeResult::UseSerializeItem:
					// Fall through to default serialize
					break;
			}
		}
			
		Inner->SerializeItem(Slot, Item);
	};
	
	// Serialize a PropertyTag for the inner property of this array, allows us to validate the inner struct to see if it has changed
	if (UnderlyingArchive.UEVer() >= VER_UE4_INNER_ARRAY_TAG_INFO && Inner->IsA<FStructProperty>())
	{
		if (!MaybeInnerTag)
		{
			MaybeInnerTag.Emplace(UnderlyingArchive, Inner, 0, (uint8*)Value, (uint8*)Defaults);
			UnderlyingArchive << MaybeInnerTag.GetValue();
		}

		FPropertyTag& InnerTag = MaybeInnerTag.GetValue();

		if (UnderlyingArchive.IsLoading())
		{
			auto CanSerializeFromStructWithDifferentName = [](const FPropertyTag& PropertyTag, const FStructProperty* StructProperty)
			{
				return StructProperty
					&& StructProperty->Struct
					&& PropertyTag.StructGuid.IsValid()
					&& PropertyTag.StructGuid == StructProperty->Struct->GetCustomGuid();
			};

			// Check if the Inner property can successfully serialize, the type may have changed
			FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Inner);
			// if check redirector to make sure if the name has changed
			FName NewName = FLinkerLoad::FindNewNameForStruct(InnerTag.StructName);
			FName StructName = StructProperty->Struct->GetFName();
			if (NewName != NAME_None && NewName == StructName)
			{
				InnerTag.StructName = NewName;
			}

			if (InnerTag.StructName != StructProperty->Struct->GetFName()
				&& !CanSerializeFromStructWithDifferentName(InnerTag, StructProperty))
			{
				// Attempt mismatched tag serialization if available
				if ((StructProperty->Struct->StructFlags & STRUCT_SerializeFromMismatchedTag) && (InnerTag.Type != NAME_StructProperty || (InnerTag.StructName != StructProperty->Struct->GetFName())))
				{
					SerializeFromMismatchedTag = InnerTag;
				}
				else
				{
					UE_LOG(LogClass, Warning, TEXT("Array Property %s of %s contains a struct type mismatch (tag %s != prop %s) in package:  %s. If that struct got renamed, add an entry to ActiveStructRedirects."),
					*InnerTag.Name.ToString(), *GetName(), *InnerTag.StructName.ToString(), *CastFieldChecked<FStructProperty>(Inner)->Struct->GetName(), *UnderlyingArchive.GetArchiveName());

#if WITH_EDITOR
					// Ensure the structure is initialized
					for (int32 i = 0; i < n; i++)
					{
						StructProperty->Struct->InitializeDefaultValue(ArrayHelper.GetRawPtr(i));
					}
#endif // WITH_EDITOR

					if (!bIsTextFormat)
					{
						// Skip the property
						const int64 StartOfProperty = UnderlyingArchive.Tell();
						const int64 RemainingSize = InnerTag.Size - (UnderlyingArchive.Tell() - StartOfProperty);
						uint8 B;
						for (int64 i = 0; i < RemainingSize; i++)
						{
							UnderlyingArchive << B;
						}
					}
					return;
				}
			}
		}
	}

	// need to know how much data this call to SerializeItem consumes, so mark where we are
	int64 DataOffset = UnderlyingArchive.Tell();

	// If we're using a custom property list, first serialize any explicit indices
	int32 i = 0;
	bool bSerializeRemainingItems = true;
	bool bUsingCustomPropertyList = UnderlyingArchive.ArUseCustomPropertyList;
	if (bUsingCustomPropertyList && UnderlyingArchive.ArCustomPropertyList != nullptr)
	{
		// Initially we only serialize indices that are explicitly specified (in order)
		bSerializeRemainingItems = false;

		const FCustomPropertyListNode* CustomPropertyList = UnderlyingArchive.ArCustomPropertyList;
		const FCustomPropertyListNode* PropertyNode = CustomPropertyList;
		FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Inner, this);
		while (PropertyNode && i < n && !bSerializeRemainingItems)
		{
			if (PropertyNode->Property != Inner)
			{
				// A null property value signals that we should serialize the remaining array values in full starting at this index
				if (PropertyNode->Property == nullptr)
				{
					i = PropertyNode->ArrayIndex;
				}

				bSerializeRemainingItems = true;
			}
			else
			{
				// Set a temporary node to represent the item
				FCustomPropertyListNode ItemNode = *PropertyNode;
				ItemNode.ArrayIndex = 0;
				ItemNode.PropertyListNext = nullptr;
				UnderlyingArchive.ArCustomPropertyList = &ItemNode;

				// Serialize the item at this array index
				i = PropertyNode->ArrayIndex;
				SerializeContainerItem(Array.EnterElement(), ArrayHelper.GetRawPtr(i));
				PropertyNode = PropertyNode->PropertyListNext;

				// Restore the current property list
				UnderlyingArchive.ArCustomPropertyList = CustomPropertyList;
			}
		}
	}

	if (bSerializeRemainingItems)
	{
		// Temporarily suspend the custom property list (as we need these items to be serialized in full)
		UnderlyingArchive.ArUseCustomPropertyList = false;

		// Serialize each item until we get to the end of the array
		FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Inner, this);
		while (i < n)
		{
#if WITH_EDITOR
			static const FName NAME_UArraySerialize = FName(TEXT("FArrayProperty::Serialize"));
			FName NAME_UArraySerializeCount = FName(NAME_UArraySerialize);
			NAME_UArraySerializeCount.SetNumber(i);
			FArchive::FScopeAddDebugData P(UnderlyingArchive, NAME_UArraySerializeCount);
#endif
			SerializeContainerItem(Array.EnterElement(), ArrayHelper.GetRawPtr(i++));
		}

		// Restore use of the custom property list (if it was previously enabled)
		UnderlyingArchive.ArUseCustomPropertyList = bUsingCustomPropertyList;
	}

	if (MaybeInnerTag.IsSet() && UnderlyingArchive.IsSaving() && !bIsTextFormat)
	{
		FPropertyTag& InnerTag = MaybeInnerTag.GetValue();

		// set the tag's size
		InnerTag.Size = UnderlyingArchive.Tell() - DataOffset;

		if (InnerTag.Size > 0)
		{
			// mark our current location
			DataOffset = UnderlyingArchive.Tell();

			// go back and re-serialize the size now that we know it
			UnderlyingArchive.Seek(InnerTag.SizeOffset);
			UnderlyingArchive << InnerTag.Size;

			// return to the current location
			UnderlyingArchive.Seek(DataOffset);
		}
	}
}

bool FArrayProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	UE_LOG( LogProperty, Fatal, TEXT( "Deprecated code path" ) );
	return 1;
}

void FArrayProperty::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	
	SerializeSingleField(Ar, Inner, this);
	checkSlow(Inner);
}
void FArrayProperty::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	if (Inner)
	{
		Inner->AddReferencedObjects(Collector);
	}
}

FString FArrayProperty::GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerTypeText, const FString& InInnerExtendedTypeText) const
{
	if (ExtendedTypeText != NULL)
	{
		FString InnerExtendedTypeText = InInnerExtendedTypeText;
		if (InnerExtendedTypeText.Len() && InnerExtendedTypeText.Right(1) == TEXT(">"))
		{
			// if our internal property type is a template class, add a space between the closing brackets b/c VS.NET cannot parse this correctly
			InnerExtendedTypeText += TEXT(" ");
		}
		else if (!InnerExtendedTypeText.Len() && InnerTypeText.Len() && InnerTypeText.Right(1) == TEXT(">"))
		{
			// if our internal property type is a template class, add a space between the closing brackets b/c VS.NET cannot parse this correctly
			InnerExtendedTypeText += TEXT(" ");
		}
		*ExtendedTypeText = FString::Printf(TEXT("<%s%s>"), *InnerTypeText, *InnerExtendedTypeText);
	}
	return TEXT("TArray");
}

FString FArrayProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	checkSlow(Inner);
	FString InnerExtendedTypeText;
	FString InnerTypeText;
	if ( ExtendedTypeText != NULL )
	{
		InnerTypeText = Inner->GetCPPType(&InnerExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider array inners to be "arguments or return values"
	}
	return GetCPPTypeCustom(ExtendedTypeText, CPPExportFlags, InnerTypeText, InnerExtendedTypeText);
}

FString FArrayProperty::GetCPPTypeForwardDeclaration() const
{
	checkSlow(Inner);
	return Inner->GetCPPTypeForwardDeclaration();
}
FString FArrayProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	checkSlow(Inner);
	ExtendedTypeText = Inner->GetCPPType();
	return TEXT("TARRAY");
}
void FArrayProperty::ExportText_Internal( FString& ValueStr, const void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	checkSlow(Inner);

	if (0 != (PortFlags & PPF_ExportCpp))
	{
		FString ExtendedTypeText;
		FString TypeText = GetCPPType(&ExtendedTypeText, EPropertyExportCPPFlags::CPPF_BlueprintCppBackend);
		ValueStr += FString::Printf(TEXT("%s%s()"), *TypeText, *ExtendedTypeText);
		return;
	}

	uint8* TempArrayStorage = nullptr;
	void* PropertyValuePtr = nullptr;
	if (PropertyPointerType == EPropertyPointerType::Container && HasGetter())
	{
		// Allocate temporary map as we first need to initialize it with the value provided by the getter function and then export it
		TempArrayStorage = (uint8*)AllocateAndInitializeValue();
		PropertyValuePtr = TempArrayStorage;
		FProperty::GetValue_InContainer(ContainerOrPropertyPtr, PropertyValuePtr);
	}
	else
	{
		PropertyValuePtr = PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
	}

	ON_SCOPE_EXIT
	{
		DestroyAndFreeValue(TempArrayStorage);
	};

	FScriptArrayHelper ArrayHelper(this, PropertyValuePtr);

	int32 DefaultSize = 0;
	if (DefaultValue)
	{
		FScriptArrayHelper DefaultArrayHelper(this, DefaultValue);
		DefaultSize = DefaultArrayHelper.Num();
		DefaultValue = DefaultArrayHelper.GetRawPtr(0);
	}

	ExportTextInnerItem(ValueStr, Inner, ArrayHelper.GetRawPtr(0), ArrayHelper.Num(), DefaultValue, DefaultSize, Parent, PortFlags, ExportRootScope);
}

void FArrayProperty::ExportTextInnerItem(FString& ValueStr, const FProperty* Inner, const void* PropertyValue, int32 PropertySize, const void* DefaultValue, int32 DefaultSize, UObject* Parent, int32 PortFlags, UObject* ExportRootScope)
{
	checkSlow(Inner);

	uint8* StructDefaults = NULL;
	const FStructProperty* StructProperty = CastField<FStructProperty>(Inner);

	const bool bReadableForm = (0 != (PPF_BlueprintDebugView & PortFlags));
	const bool bExternalEditor = (0 != (PPF_ExternalEditor & PortFlags));

	// ArrayProperties only export a diff because array entries are cleared and recreated upon import. Static arrays are overwritten when importing,
	// so we export the entire struct to ensure all data is copied over correctly. Behavior is currently inconsistent when copy/pasting between the two types.
	// In the future, static arrays could export diffs if the property being imported to is reset to default before the import.
	// When exporting to an external editor, we want to save defaults so all information is available for editing
	if ( StructProperty != NULL && Inner->ArrayDim == 1 && !bExternalEditor )
	{
		checkSlow(StructProperty->Struct);
		StructDefaults = (uint8*)FMemory::Malloc(StructProperty->Struct->GetStructureSize() * Inner->ArrayDim);
		StructProperty->InitializeValue(StructDefaults);
	}

	int32 Count = 0;
	for( int32 i=0; i<PropertySize; i++ )
	{
		++Count;
		if(!bReadableForm)
		{
			if ( Count == 1 )
			{
				ValueStr += TCHAR('(');
			}
			else
			{
				ValueStr += TCHAR(',');
			}
		}
		else
		{
			if(Count > 1)
			{
				ValueStr += TCHAR('\n');
			}
			ValueStr += FString::Printf(TEXT("[%i] "), i);
		}

		uint8* PropData = (uint8*)PropertyValue + i * Inner->ElementSize;

		// Always use struct defaults if the inner is a struct, for symmetry with the import of array inner struct defaults
		uint8* PropDefault = nullptr;
		if (bExternalEditor)
		{
			PropDefault = PropData;
		}
		else if (StructProperty)
		{
			PropDefault = StructDefaults;
		}
		else
		{
			if (DefaultValue && DefaultSize > i)
			{
				PropDefault = (uint8*)DefaultValue + i * Inner->ElementSize;
			}
		}

		Inner->ExportTextItem_Direct( ValueStr, PropData, PropDefault, Parent, PortFlags|PPF_Delimited, ExportRootScope );
	}

	if ((Count > 0) && !bReadableForm)
	{
		ValueStr += TEXT(")");
	}
	if (StructDefaults)
	{
		StructProperty->DestroyValue(StructDefaults);
		FMemory::Free(StructDefaults);
	}
}

const TCHAR* FArrayProperty::ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const
{
	uint8* TempArrayStorage = nullptr;
	ON_SCOPE_EXIT
	{
		if (TempArrayStorage)
		{
			// TempArrayStorage is used by property setter so if it was allocated call the setter now
			FProperty::SetValue_InContainer(ContainerOrPropertyPtr, TempArrayStorage);

			// Destroy and free the temp array used by property setter
			DestroyAndFreeValue(TempArrayStorage);
		}
	};

	void* ArrayPtr = nullptr;
	if (PropertyPointerType == EPropertyPointerType::Container && HasSetter())
	{
		// Allocate temporary map as we first need to initialize it with the parsed items and then use the setter to update the property
		TempArrayStorage = (uint8*)AllocateAndInitializeValue();
		ArrayPtr = TempArrayStorage;
	}
	else
	{
		ArrayPtr = PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
	}

	FScriptArrayHelper ArrayHelper(this, ArrayPtr);
	return ImportTextInnerItem(Buffer, Inner, ArrayPtr, PortFlags, OwnerObject, &ArrayHelper, ErrorText);
}

const TCHAR* FArrayProperty::ImportTextInnerItem( const TCHAR* Buffer, const FProperty* Inner, void* Data, int32 PortFlags, UObject* Parent, FScriptArrayHelper* ArrayHelper, FOutputDevice* ErrorText )
{
	checkSlow(Inner);

	// If we export an empty array we export an empty string, so ensure that if we're passed an empty string
	// we interpret it as an empty array.
	if (*Buffer == TCHAR('\0') || *Buffer == TCHAR(')') || *Buffer == TCHAR(','))
	{
		if (ArrayHelper)
		{
			ArrayHelper->EmptyValues();
		}
		return Buffer;
	}

	if ( *Buffer++ != TCHAR('(') )
	{
		return NULL;
	}

	if (ArrayHelper)
	{
		ArrayHelper->EmptyValues();
		ArrayHelper->ExpandForIndex(0);
	}

	SkipWhitespace(Buffer);

	int32 Index = 0;
	while (*Buffer != TCHAR(')'))
	{
		SkipWhitespace(Buffer);

		if (*Buffer != TCHAR(','))
		{
			uint8* Address = ArrayHelper ? ArrayHelper->GetRawPtr(Index) : ((uint8*)Data + Inner->ElementSize * Index);
			// Parse the item
			checkf(ArrayHelper == nullptr || Inner->GetOffset_ForInternal() == 0, TEXT("Expected the Inner property of the FArrayProperty."));
			Buffer = Inner->ImportText_Direct(Buffer, Address, Parent, PortFlags | PPF_Delimited, ErrorText);

			if(!Buffer)
			{
				return NULL;
			}

			SkipWhitespace(Buffer);
		}


		if (*Buffer == TCHAR(','))
		{
			Buffer++;
			Index++;
			if (ArrayHelper)
			{
				ArrayHelper->ExpandForIndex(Index);
			}
			else if (Index >= Inner->ArrayDim)
			{
				ErrorText->Logf(ELogVerbosity::Warning, TEXT("%s is a fixed-sized array of %i values. Additional data after %i has been ignored during import."), *Inner->GetName(), Inner->ArrayDim, Inner->ArrayDim);
				break;
			}
		}
		else
		{
			break;
		}
	}

	// Make sure we ended on a )
	if (*Buffer++ != TCHAR(')'))
	{
		return NULL;
	}

	return Buffer;
}

#if WITH_EDITORONLY_DATA
void FArrayProperty::AppendSchemaHash(FBlake3& Builder, bool bSkipEditorOnly) const
{
	Super::AppendSchemaHash(Builder, bSkipEditorOnly);
	if (Inner)
	{
		Inner->AppendSchemaHash(Builder, bSkipEditorOnly);
	}
}
#endif

void FArrayProperty::AddCppProperty(FProperty* Property)
{
	check(!Inner);
	check(Property);

	Inner = Property;
}

void FArrayProperty::CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const
{
	check(Count==1); // this was never supported, apparently
	FScriptArrayHelper SrcArrayHelper(this, Src);
	FScriptArrayHelper DestArrayHelper(this, Dest);

	int32 Num = SrcArrayHelper.Num();
	if ( !(Inner->PropertyFlags & CPF_IsPlainOldData) )
	{
		DestArrayHelper.EmptyAndAddValues(Num);
	}
	else
	{
		DestArrayHelper.EmptyAndAddUninitializedValues(Num);
	}
	if (Num)
	{
		int32 Size = Inner->ElementSize;
		uint8* SrcData = (uint8*)SrcArrayHelper.GetRawPtr();
		uint8* DestData = (uint8*)DestArrayHelper.GetRawPtr();
		if( !(Inner->PropertyFlags & CPF_IsPlainOldData) )
		{
			for( int32 i=0; i<Num; i++ )
			{
				Inner->CopyCompleteValue( DestData + i * Size, SrcData + i * Size );
			}
		}
		else
		{
			FMemory::Memcpy( DestData, SrcData, Num*Size );
		}
	}
}
void FArrayProperty::ClearValueInternal( void* Data ) const
{
	FScriptArrayHelper ArrayHelper(this, Data);
	ArrayHelper.EmptyValues();
}
void FArrayProperty::DestroyValueInternal( void* Dest ) const
{
	FScriptArrayHelper ArrayHelper(this, Dest);
	ArrayHelper.EmptyValues();

	//@todo UE potential double destroy later from this...would be ok for a script array, but still
	ArrayHelper.DestroyContainer_Unsafe();
}
bool FArrayProperty::PassCPPArgsByRef() const
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
void FArrayProperty::InstanceSubobjects( void* Data, void const* DefaultData, UObject* InOwner, FObjectInstancingGraph* InstanceGraph )
{
	if( Data && Inner->ContainsInstancedObjectProperty())
	{
		FScriptArrayHelper ArrayHelper(this, Data);
		FScriptArrayHelper DefaultArrayHelper(this, DefaultData);

		int32 InnerElementSize = Inner->ElementSize;
		void* TempElement = FMemory_Alloca(InnerElementSize);

		for( int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ElementIndex++ )
		{
			uint8* DefaultValue = (DefaultData && ElementIndex < DefaultArrayHelper.Num()) ? DefaultArrayHelper.GetRawPtr(ElementIndex) : nullptr;
			FMemory::Memmove(TempElement, ArrayHelper.GetRawPtr(ElementIndex), InnerElementSize);
			Inner->InstanceSubobjects( TempElement, DefaultValue, InOwner, InstanceGraph );
			if (ElementIndex < ArrayHelper.Num())
			{
				FMemory::Memmove(ArrayHelper.GetRawPtr(ElementIndex), TempElement, InnerElementSize);
			}
			else
			{
				Inner->DestroyValue(TempElement);
			}
		}
	}
}

bool FArrayProperty::SameType(const FProperty* Other) const
{
	return Super::SameType(Other) && Inner && Inner->SameType(((FArrayProperty*)Other)->Inner);
}

EConvertFromTypeResult FArrayProperty::ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct)
{
	// TODO: The ArrayProperty Tag really doesn't have adequate information for
	// many types. This should probably all be moved in to ::SerializeItem

	if (Tag.Type == NAME_ArrayProperty && Tag.InnerType != NAME_None && Tag.InnerType != Inner->GetID())
	{
		void* ArrayPropertyData = ContainerPtrToValuePtr<void>(Data);

		int32 ElementCount = 0;

		if (Slot.GetUnderlyingArchive().IsTextFormat())
		{
			Slot.EnterArray(ElementCount);
		}
		else
		{
			Slot.GetUnderlyingArchive() << ElementCount;
		}

		FScriptArrayHelper ScriptArrayHelper(this, ArrayPropertyData);
		ScriptArrayHelper.EmptyAndAddValues(ElementCount);

		FPropertyTag InnerPropertyTag;
		InnerPropertyTag.Type = Tag.InnerType;
		InnerPropertyTag.ArrayIndex = 0;

		if (Slot.GetArchiveState().UEVer() >= VER_UE4_INNER_ARRAY_TAG_INFO && Tag.InnerType == NAME_StructProperty)
		{
			Slot.GetUnderlyingArchive() << InnerPropertyTag;
		}

		// Convert properties from old type to new type automatically if types are compatible (array case)
		if (ElementCount > 0)
		{
			FStructuredArchive::FStream ValueStream = Slot.EnterStream();

			if (Inner->ConvertFromType(InnerPropertyTag, ValueStream.EnterElement(), ScriptArrayHelper.GetRawPtr(0), DefaultsStruct) == EConvertFromTypeResult::Converted)
			{
				for (int32 i = 1; i < ElementCount; ++i)
				{
					verify(Inner->ConvertFromType(InnerPropertyTag, ValueStream.EnterElement(), ScriptArrayHelper.GetRawPtr(i), DefaultsStruct) == EConvertFromTypeResult::Converted);
				}

				return EConvertFromTypeResult::Converted;
			}
			// TODO: Implement SerializeFromMismatchedTag handling for arrays of structs
			else
			{
				UE_LOG(LogClass, Warning, TEXT("Array Inner Type mismatch in %s of %s - Previous (%s) Current(%s) for package:  %s"), *Tag.Name.ToString(), *GetName(), *Tag.InnerType.ToString(), *Inner->GetID().ToString(), *Slot.GetUnderlyingArchive().GetArchiveName() );
				return EConvertFromTypeResult::CannotConvert;
			}
		}
		else
		{
			return EConvertFromTypeResult::Converted;
		}
	}

	return EConvertFromTypeResult::UseSerializeItem;
}

FField* FArrayProperty::GetInnerFieldByName(const FName& InName)
{
	if (Inner && Inner->GetFName() == InName)
	{
		return Inner;
	}
	return nullptr;
}

void FArrayProperty::GetInnerFields(TArray<FField*>& OutFields)
{
	if (Inner)
	{
		OutFields.Add(Inner);
		Inner->GetInnerFields(OutFields);
	}
}

void* FArrayProperty::GetValueAddressAtIndex_Direct(const FProperty* InInner, void* InValueAddress, int32 Index) const
{
	FScriptArrayHelper ArrayHelper(this, InValueAddress);
	checkf(Inner == InInner, TEXT("Passed in inner must be identical to the array property inner property"));
	checkf(Index < ArrayHelper.Num() && Index >= 0, TEXT("Array index (%d) out of range"), Index);

	return ArrayHelper.GetRawPtr(Index);
}

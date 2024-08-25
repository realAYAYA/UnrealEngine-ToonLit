// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/Casts.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UnrealTypePrivate.h"
#include "UObject/UObjectHash.h"
#include "Blueprint/BlueprintSupport.h"
#include "UObject/CoreNet.h"
#include "UObject/PropertyHelper.h"
#include "UObject/LinkerPlaceholderClass.h"
#include "UObject/LinkerPlaceholderExportObject.h"
#include "UObject/LinkerLoadImportBehavior.h"
#include "UObject/UObjectThreadContext.h"
#include "Misc/StringBuilder.h"

/*-----------------------------------------------------------------------------
	FObjectPropertyBase.
-----------------------------------------------------------------------------*/
IMPLEMENT_FIELD(FObjectPropertyBase)

FObjectPropertyBase::FObjectPropertyBase(FFieldVariant InOwner, const UECodeGen_Private::FObjectPropertyParams& Prop, EPropertyFlags AdditionalPropertyFlags /*= CPF_None*/)
	: FProperty(InOwner, (const UECodeGen_Private::FPropertyParamsBaseWithOffset&)Prop, AdditionalPropertyFlags)
{
	PropertyClass = Prop.ClassFunc ? Prop.ClassFunc() : nullptr;
}
FObjectPropertyBase::FObjectPropertyBase(FFieldVariant InOwner, const UECodeGen_Private::FObjectPropertyParamsWithoutClass& Prop, EPropertyFlags AdditionalPropertyFlags /*= CPF_None*/)
	: FProperty(InOwner, (const UECodeGen_Private::FPropertyParamsBaseWithOffset&)Prop, AdditionalPropertyFlags)
	, PropertyClass(nullptr)
{
}

#if WITH_EDITORONLY_DATA
FObjectPropertyBase::FObjectPropertyBase(UField* InField)
	: FProperty(InField)
{
	UObjectPropertyBase* SourceProperty = CastChecked<UObjectPropertyBase>(InField);
	PropertyClass = SourceProperty->PropertyClass;
}
#endif // WITH_EDITORONLY_DATA

void FObjectPropertyBase::BeginDestroy()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(PropertyClass))
	{
		PlaceholderClass->RemoveReferencingProperty(this);
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	Super::BeginDestroy();
}

void FObjectPropertyBase::InstanceSubobjects(void* Data, void const* DefaultData, UObject* InOwner, FObjectInstancingGraph* InstanceGraph )
{
	for ( int32 ArrayIndex = 0; ArrayIndex < ArrayDim; ArrayIndex++ )
	{
		UObject* CurrentValue = GetObjectPropertyValue((uint8*)Data + ArrayIndex * ElementSize);
		if ( CurrentValue )
		{
			UObject *SubobjectTemplate = DefaultData ? GetObjectPropertyValue((uint8*)DefaultData + ArrayIndex * ElementSize): nullptr;
			UObject* NewValue = InstanceGraph->InstancePropertyValue(SubobjectTemplate, CurrentValue, InOwner, HasAnyPropertyFlags(CPF_InstancedReference) ? EInstancePropertyValueFlags::CausesInstancing : EInstancePropertyValueFlags::None);
			SetObjectPropertyValue((uint8*)Data + ArrayIndex * ElementSize, NewValue);
		}
	}
}

bool FObjectPropertyBase::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	// We never return Identical when duplicating for PIE because we want to be sure to serialize everything. An example is the LevelScriptActor being serialized against its CDO,
	// which contains actor references. We want to serialize those references so they are fixed up.
	if ((PortFlags & PPF_DuplicateForPIE) != 0)
	{
		return false;
	}

	UObject* ObjectA = A ? GetObjectPropertyValue(A) : nullptr;
	UObject* ObjectB = B ? GetObjectPropertyValue(B) : nullptr;

	return StaticIdentical(ObjectA, ObjectB, PortFlags);
}

bool FObjectPropertyBase::StaticIdentical(UObject* ObjectA, UObject* ObjectB, uint32 PortFlags)
{
	if (ObjectA == ObjectB)
	{
		return true;
	}
	if (!ObjectA || !ObjectB)
	{
		return false;
	}

	bool bResult = false;

	// In order for a deep comparison of instanced objects to match both objects must have the same class and name
	if (ObjectA->GetClass() == ObjectB->GetClass() && ObjectA->GetFName() == ObjectB->GetFName())
	{
		bool bPerformDeepComparison = (PortFlags&PPF_DeepComparison) != 0;
		if (((PortFlags&PPF_DeepCompareInstances) != 0) && !bPerformDeepComparison)
		{
			bPerformDeepComparison = !(ObjectA->IsTemplate() && ObjectB->IsTemplate());
		}

		if (bPerformDeepComparison)
		{
			if ((PortFlags&PPF_DeepCompareDSOsOnly) != 0)
			{
				if (UObject* DSO = ObjectA->GetClass()->GetDefaultSubobjectByName(ObjectA->GetFName()))
				{
					checkSlow(ObjectA->IsDefaultSubobject() && ObjectB->IsDefaultSubobject() && DSO == ObjectB->GetClass()->GetDefaultSubobjectByName(ObjectB->GetFName()));
				}
				else
				{
					bPerformDeepComparison = false;
				}
			}

			if (bPerformDeepComparison)
			{
				bResult = AreInstancedObjectsIdentical(ObjectA, ObjectB, PortFlags);
			}
		}
	}
	return bResult;
}

bool FObjectPropertyBase::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	UObject* Object = GetObjectPropertyValue(Data);
	bool Result = Map->SerializeObject( Ar, PropertyClass, Object );
	// Prevent serializing invalid objects through network
	if (!Object || IsValidChecked(Object))
	{
		SetObjectPropertyValue(Data, Object);
	}
	return Result;
}
void FObjectPropertyBase::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << PropertyClass;

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (Ar.IsLoading() || Ar.IsObjectReferenceCollector())
	{
		if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(PropertyClass))
		{
			PlaceholderClass->AddReferencingProperty(this);
		}
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

void FObjectPropertyBase::PostDuplicate(const FField& InField)
{
	const FObjectPropertyBase& Source = static_cast<const FObjectPropertyBase&>(InField);
	PropertyClass = Source.PropertyClass;
	Super::PostDuplicate(InField);
}

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
void FObjectPropertyBase::SetPropertyClass(UClass* NewPropertyClass)
{
	if (ULinkerPlaceholderClass* NewPlaceholderClass = Cast<ULinkerPlaceholderClass>(NewPropertyClass))
	{
		NewPlaceholderClass->AddReferencingProperty(this);
	}
	
	if (ULinkerPlaceholderClass* OldPlaceholderClass = Cast<ULinkerPlaceholderClass>(PropertyClass))
	{
		OldPlaceholderClass->RemoveReferencingProperty(this);
	}
	PropertyClass = NewPropertyClass;
}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

void FObjectPropertyBase::AddReferencedObjects(FReferenceCollector& Collector)
{	
	Collector.AddReferencedObject( PropertyClass );
	Super::AddReferencedObjects( Collector );
}

FString FObjectPropertyBase::GetExportPath(FTopLevelAssetPath ClassPathName, const FString& ObjectPathName)
{
	TStringBuilder<256> StringBuilder;
	StringBuilder << ClassPathName.GetPackageName() << "." << ClassPathName.GetAssetName() << "'" << ObjectPathName << "'";
	return FString(StringBuilder);
}

FString FObjectPropertyBase::GetExportPath(const TObjectPtr<const UObject>& Object, const UObject* Parent /*= nullptr*/, const UObject* ExportRootScope /*= nullptr*/, const uint32 PortFlags /*= PPF_None*/)
{
	bool bExportFullyQualified = true;

	// When exporting from one package or graph to another package or graph, we don't want to fully qualify the name, as it may refer
	// to a level or graph that doesn't exist or cause a linkage to a node in a different graph
	const UObject* StopOuter = nullptr;
	if (PortFlags & PPF_ExportsNotFullyQualified)
	{
		StopOuter = (ExportRootScope || (Parent == nullptr)) ? ExportRootScope : Parent->GetOutermost();
		bExportFullyQualified = StopOuter && !Object->IsIn(StopOuter);

		// Also don't fully qualify the name if it's a sibling of the root scope, since it may be included in the exported set of objects
		if (bExportFullyQualified)
		{
			StopOuter = StopOuter->GetOuter();
			bExportFullyQualified = (StopOuter == nullptr) || (!Object->IsIn(StopOuter));
		}
	}

	// if we want a full qualified object reference, use the pathname, otherwise, use just the object name
	if (bExportFullyQualified)
	{
		StopOuter = nullptr;
		if ( (PortFlags&PPF_SimpleObjectText) != 0 && Parent != nullptr )
		{
			StopOuter = Parent->GetOutermost();
		}
	}
	else if (Parent != nullptr && Object->IsIn(Parent))
	{
		StopOuter = Parent;
	}

	// Take the path name relative to the stopping point outermost ptr.
	// This is so that cases like a component referencing a component in another actor work correctly when pasted
	FString PathName = StopOuter ? Object->GetPathName(StopOuter) : Object.GetPathName();
	const FTopLevelAssetPath ClassPathName = Object.GetClass()->GetClassPathName();
	FString ExportPath = GetExportPath(ClassPathName, *PathName);
	// Object names that contain invalid characters and paths that contain spaces must be put into quotes to be handled correctly
	if (PortFlags & PPF_Delimited)
	{
		ExportPath = FString::Printf(TEXT("\"%s\""), *ExportPath.ReplaceQuotesWithEscapedQuotes());
	}
	return ExportPath;
}

void FObjectPropertyBase::ExportText_Internal( FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	TObjectPtr<UObject> Temp;

	if (PropertyPointerType == EPropertyPointerType::Container && HasGetter())
	{
		GetValue_InContainer(PropertyValueOrContainer, &Temp);
	}
	else
	{
		Temp = GetObjectPtrPropertyValue(PointerToValuePtr(PropertyValueOrContainer, PropertyPointerType));
	}

	if (!Temp)
	{
		ValueStr += TEXT("None");
	}
	else
	{
		if (PortFlags & PPF_DebugDump)
		{
			ValueStr += Temp ? Temp->GetFullName() : TEXT("None");
		}
		else if (
		         Parent && !Parent->HasAnyFlags(RF_ClassDefaultObject)
				 // @NOTE: OBJPTR: In the event that we're trying to handle a default subobject, the requirement would 
				 // be that it's inside the package we are currently in, which means the Temp pointer should be resolved 
				 // already.  So don't move forward with the check unless it's resolved, we don't want to force a deferred
				 // loaded asset if we don't have to with this check.
		         // We also want to make sure the object is actually inside the package we are currently in
				 && Temp.IsResolved() && Temp && Temp->IsDefaultSubobject() && Temp->IsIn(Parent->GetOutermostObject())
				)
		{
			if (PortFlags & PPF_Delimited)
			{
				ValueStr += Temp ? FString::Printf(TEXT("\"%s\""), *Temp->GetName().ReplaceQuotesWithEscapedQuotes()) : TEXT("None");
			}
			else
			{
				ValueStr += Temp.GetName();
			}
		}
		else
		{
			ValueStr += GetExportPath(Temp, Parent, ExportRootScope, PortFlags);
		}
	}
}

/**
 * Parses a text buffer into an object reference.
 *
 * @param	Property			the property that the value is being importing to
 * @param	OwnerObject			the object that is importing the value; used for determining search scope.
 * @param	RequiredMetaClass	the meta-class for the object to find; if the object that is resolved is not of this class type, the result is nullptr.
 * @param	PortFlags			bitmask of EPropertyPortFlags that can modify the behavior of the search
 * @param	Buffer				the text to parse; should point to a textual representation of an object reference.  Can be just the object name (either fully 
 *								fully qualified or not), or can be formatted as a const object reference (i.e. SomeClass'SomePackage.TheObject')
 *								When the function returns, Buffer will be pointing to the first character after the object value text in the input stream.
 * @param	ResolvedValue		receives the object that is resolved from the input text.
 *
 * @return	true if the text is successfully resolved into a valid object reference of the correct type, false otherwise.
 */
bool FObjectPropertyBase::ParseObjectPropertyValue(const FProperty* Property, UObject* OwnerObject, UClass* RequiredMetaClass, uint32 PortFlags, const TCHAR*& Buffer, TObjectPtr<UObject>& out_ResolvedValue, FUObjectSerializeContext* InSerializeContext /*= nullptr*/, bool bAllowAnyPackage /*= true*/)
{
	check(Property);
	if (!RequiredMetaClass)
	{
		UE_LOG(LogProperty, Error, TEXT("ParseObjectPropertyValue Error: RequiredMetaClass is null, for property: %s "), *Property->GetFullName());
		out_ResolvedValue = nullptr;
		return false;
	}

 	const TCHAR* InBuffer = Buffer;

	TStringBuilder<256> Temp;
	Buffer = FPropertyHelpers::ReadToken(Buffer, /* out */ Temp, true);
	if ( Buffer == nullptr )
	{
		return false;
	}

	if ( Temp == TEXTVIEW("None") )
	{
		out_ResolvedValue = nullptr;
	}
	else
	{
		UClass*	ObjectClass = RequiredMetaClass;

		SkipWhitespace(Buffer);

		bool bWarnOnnullptr = (PortFlags&PPF_CheckReferences)!=0;

		if( *Buffer == TCHAR('\'') )
		{
			Temp.Reset();
			Buffer = FPropertyHelpers::ReadToken( ++Buffer, /* out */ Temp, true);
			if( Buffer == nullptr )
			{
				return false;
			}

			if( *Buffer++ != TCHAR('\'') )
			{
				return false;
			}

			// ignore the object class, it isn't fully qualified, and searching globally might get the wrong one!
			// Try the find the object.
			out_ResolvedValue = FObjectPropertyBase::FindImportedObject(Property, OwnerObject, ObjectClass, RequiredMetaClass, Temp.ToString(), PortFlags, InSerializeContext, bAllowAnyPackage);
		}
		else
		{
			// Try the find the object.
			out_ResolvedValue = FObjectPropertyBase::FindImportedObject(Property, OwnerObject, ObjectClass, RequiredMetaClass, Temp.ToString(), PortFlags, InSerializeContext, bAllowAnyPackage);
		}

		if ( out_ResolvedValue != nullptr && !out_ResolvedValue.GetClass()->IsChildOf(RequiredMetaClass) )
		{
			if (bWarnOnnullptr )
			{
				UE_LOG(LogProperty, Error, TEXT("%s: bad cast in '%s'"), *Property->GetFullName(), InBuffer );
			}

			out_ResolvedValue = nullptr;
			return false;
		}

		// If we couldn't find it or load it, we'll have to do without it.
		if ( out_ResolvedValue == nullptr )
		{
			if( bWarnOnnullptr )
			{
				UE_LOG(LogProperty, Warning, TEXT("%s: unresolved reference to '%s'"), *Property->GetFullName(), InBuffer );
			}
			return false;
		}
	}

	return true;
}

const TCHAR* FObjectPropertyBase::ImportText_Internal( const TCHAR* InBuffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* Parent, int32 PortFlags, FOutputDevice* ErrorText ) const
{
	const TCHAR* Buffer = InBuffer;
	TObjectPtr<UObject> Result = nullptr;

	bool bOk = ParseObjectPropertyValue(this, Parent, PropertyClass, PortFlags, Buffer, Result, FUObjectThreadContext::Get().GetSerializeContext());

	if (Result && (PortFlags & PPF_InstanceSubobjects) != 0 && HasAnyPropertyFlags(CPF_InstancedReference))
	{
		FName DesiredName = Result->GetFName();

		// If an object currently exists with the same name as the imported object that is to be instanced
		// 
		if (UObject* ExistingObject = static_cast<UObject*>(FindObjectWithOuter(Parent, nullptr, DesiredName)))
		{
			ExistingObject->Rename(nullptr, nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
		}

		FObjectDuplicationParameters ObjectDuplicationParams = InitStaticDuplicateObjectParams(Result, Parent, DesiredName);
		EnumRemoveFlags(ObjectDuplicationParams.FlagMask, RF_ArchetypeObject);
		if (Parent->IsTemplate())
		{
			EnumAddFlags(ObjectDuplicationParams.ApplyFlags, RF_ArchetypeObject);
		}
		else
		{
			EnumRemoveFlags(ObjectDuplicationParams.ApplyFlags, RF_ArchetypeObject);
		}
		Result = StaticDuplicateObjectEx(ObjectDuplicationParams);
	}

	if (PropertyPointerType == EPropertyPointerType::Container && HasSetter())
	{
		SetObjectPropertyValue_InContainer(ContainerOrPropertyPtr, Result); //TODO change this to not resolve TObjectPtr's
	}
	else
	{
		SetObjectPtrPropertyValue(PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType), Result);
	}
	return Buffer;
}

TObjectPtr<UObject> FObjectPropertyBase::FindImportedObject( const FProperty* Property, UObject* OwnerObject, UClass* ObjectClass, UClass* RequiredMetaClass, const TCHAR* Text, uint32 PortFlags/*=0*/, FUObjectSerializeContext* InSerializeContext /*= nullptr*/, bool bAllowAnyPackage /*= true*/)
{
	TObjectPtr<UObject>	Result = nullptr;
	check( ObjectClass->IsChildOf(RequiredMetaClass) );

	bool AttemptNonQualifiedSearch = (PortFlags & PPF_AttemptNonQualifiedSearch) != 0; 

	// if we are importing default properties, first look for a matching subobject by
	// looking through the archetype chain at each outer and stop once the outer chain reaches the owning class's default object
	if (PortFlags & PPF_ParsingDefaultProperties)
	{
		for (UObject* SearchStart = OwnerObject; Result == nullptr && SearchStart != nullptr; SearchStart = SearchStart->GetOuter())
		{
			UObject* ScopedSearchRoot = SearchStart;
			while (Result == nullptr && ScopedSearchRoot != nullptr)
			{
				Result = StaticFindObjectSafe(ObjectClass, ScopedSearchRoot, Text);
				// don't think it's possible to get a non-subobject here, but it doesn't hurt to check
				if (Result != nullptr && !Result->IsTemplate(RF_ClassDefaultObject))
				{
					Result = nullptr;
				}

				ScopedSearchRoot = ScopedSearchRoot->GetArchetype();
			}
			if (SearchStart->HasAnyFlags(RF_ClassDefaultObject))
			{
				break;
			}
		}
	}
	
	// if we have a parent, look in the parent, then it's outer, then it's outer, ... 
	// this is because exported object properties that point to objects in the level aren't
	// fully qualified, and this will step up the nested object chain to solve any name
	// collisions within a nested object tree
	UObject* ScopedSearchRoot = OwnerObject;
	while (Result == nullptr && ScopedSearchRoot != nullptr)
	{
		Result = StaticFindObjectSafe(ObjectClass, ScopedSearchRoot, Text);
		// disallow class default subobjects here while importing defaults
		// this prevents the use of a subobject name that doesn't exist in the scope of the default object being imported
		// from grabbing some other subobject with the same name and class in some other arbitrary default object
		if (Result != nullptr && (PortFlags & PPF_ParsingDefaultProperties) && Result->IsTemplate(RF_ClassDefaultObject))
		{
			Result = nullptr;
		}

		ScopedSearchRoot = ScopedSearchRoot->GetOuter();
	}

	if (Result == nullptr)
	{
		// attempt to find a fully qualified object
		Result = StaticFindObjectSafe(ObjectClass, nullptr, Text);

		if (Result == nullptr && (PortFlags & PPF_SerializedAsImportText))
		{
			// Check string asset redirectors
			FSoftObjectPath Path(Text);
			if (Path.PreSavePath())
			{
				Result = StaticFindObjectSafe(ObjectClass, nullptr, *Path.ToString());
			}
		}

		if (Result == nullptr && bAllowAnyPackage)
		{
			// RobM: We should delete this path
			// match any object of the correct class who shares the same name regardless of package path
			Result = StaticFindFirstObject(ObjectClass, Text, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("FindImportedObject"));
			// disallow class default subobjects here while importing defaults
			if (Result != nullptr && (PortFlags & PPF_ParsingDefaultProperties) && Result->IsTemplate(RF_ClassDefaultObject))
			{
				Result = nullptr;
			}
		}
	}

	// if we haven;t found it yet, then try to find it without a qualified name
	if (!Result)
	{
		const TCHAR* Dot = FCString::Strrchr(Text, '.');
		if (Dot && AttemptNonQualifiedSearch)
		{
			// search with just the object name
			Result = FindImportedObject(Property, OwnerObject, ObjectClass, RequiredMetaClass, Dot + 1, 0);
		}
		FString NewText(Text);
		// if it didn't have a dot, then maybe they just gave a uasset package name
		if (!Dot && !Result)
		{
			int32 LastSlash = NewText.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (LastSlash >= 0)
			{
				NewText += TEXT(".");
				NewText += (Text + LastSlash + 1);
				Dot = FCString::Strrchr(*NewText, '.');
			}
		}
		// If we still can't find it, try to load it. (Only try to load fully qualified names)
		if(!Result && Dot && !GIsSavingPackage)
		{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE 
			FSoftObjectPath Path(Text);
			if (UE::LinkerLoad::TryLazyLoad(*RequiredMetaClass, Path, Result))
			{
				return Result;
			}
#endif

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			FLinkerLoad* Linker = (OwnerObject != nullptr) ? OwnerObject->GetClass()->GetLinker() : nullptr;
			if (Linker == nullptr)
			{
				// Fall back on the Properties owner. That is probably the thing that has triggered this load:
				Linker = Property->GetLinker();
			}
			const bool bDeferAssetImports = (Linker != nullptr) && (Linker->LoadFlags & LOAD_DeferDependencyLoads);

			if (bDeferAssetImports)
			{
				Result = Linker->RequestPlaceholderValue(Property, ObjectClass, Text);
			}
			
			if (Result == nullptr)
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			{
				const uint32 LoadFlags = LOAD_NoWarn | LOAD_FindIfFail;		

				UE_LOG(LogProperty, Verbose, TEXT("FindImportedObject is attempting to import [%s] (class = %s) with StaticLoadObject"), Text, *GetFullNameSafe(ObjectClass));
				Result = StaticLoadObject(ObjectClass, nullptr, Text, nullptr, LoadFlags, nullptr, true);

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
				check(!bDeferAssetImports || !Result || !FBlueprintSupport::IsInBlueprintPackage(Result));
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			}
		}
	}

	// if we found an object, and we have a parent, make sure we are in the same package or share an outer if the found object is private, unless it's a cross level property
	if (Result && !Result->HasAnyFlags(RF_Public) && OwnerObject && !OwnerObject->HasAnyFlags(RF_Transient)
		&& Result->GetOutermostObject() != OwnerObject->GetOutermostObject()
		&& Result->GetPackage() != OwnerObject->GetPackage())
	{
		const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Property);
		if ( !ObjectProperty || !ObjectProperty->AllowCrossLevel())
		{
			UE_LOG(LogProperty, Warning, TEXT("Illegal TEXT reference to a private object in external package (%s) from referencer (%s).  Import failed..."), *Result->GetFullName(), *OwnerObject->GetFullName());
			Result = nullptr;
		}
	}

	check(!Result || Result->IsA(RequiredMetaClass));
	return Result;
}

FName FObjectPropertyBase::GetID() const
{
	return NAME_ObjectProperty;
}

TObjectPtr<UObject> FObjectPropertyBase::GetObjectPtrPropertyValue(const void* PropertyValueAddress) const
{
	checkf(false, TEXT("%s is missing implementation of GetObjectPtrPropertyValue"), *GetFullName());
	return TObjectPtr<UObject>();
}

void FObjectPropertyBase::SetObjectPtrPropertyValue(void* PropertyValueAddress, TObjectPtr<UObject> Ptr) const
{
	SetObjectPropertyValue(PropertyValueAddress, Ptr.Get());
}

UObject* FObjectPropertyBase::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	checkf(false, TEXT("%s is missing implementation of GetObjectPropertyValue"), *GetFullName());
	return nullptr;
}

UObject* FObjectPropertyBase::GetObjectPropertyValue_InContainer(const void* ContainerAddress, int32 ArrayIndex) const
{
	checkf(false, TEXT("%s is missing implementation of GetObjectPropertyValue_InContainer"), *GetFullName());
	return nullptr;
}

void FObjectPropertyBase::SetObjectPropertyValue_InContainer(void* ContainerAddress, UObject* Value, int32 ArrayIndex) const
{
	checkf(false, TEXT("%s is missing implementation of SetObjectPropertyValue_InContainer"), *GetFullName());
}

void FObjectPropertyBase::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	check(0);
}

bool FObjectPropertyBase::AllowCrossLevel() const
{
	return false;
}

bool FObjectPropertyBase::AllowObjectTypeReinterpretationTo(const FObjectPropertyBase* Other) const
{
	return false;
}

UObject* FObjectPropertyBase::ConstructDefaultObjectValueIfNecessary(UObject* ExistingValue) const
{
	UObject* NewDefaultObjectValue = nullptr;

	FUObjectSerializeContext* SerializeContext = FUObjectThreadContext::Get().GetSerializeContext();
	UObject* Outer = SerializeContext ? SerializeContext->SerializedObject : nullptr;
	if (!Outer)
	{
		Outer = GetTransientPackage();
	}

	if (ExistingValue)
	{
		UClass* ExistingValueClass = ExistingValue->GetClass();
		// Sanity check to make sure the existing value class matches the property class
		if (ExistingValueClass && (ExistingValueClass->IsChildOf(PropertyClass) || ExistingValueClass->GetAuthoritativeClass()->IsChildOf(PropertyClass)))
		{
			if (ExistingValue->IsTemplate() && 	// Existing value is a template so we can construct a new value with it as the archetype
				ExistingValue->GetOuter() != Outer) // Unless the template's Outer is the same as the new Outer in which case the template (ExistingValue) IS the object we can reuse
			{
				// We probably got here because an object value failed to load (missing import class) and the property is left with a template of default subobject
				NewDefaultObjectValue = NewObject<UObject>(Outer, ExistingValue->GetClass(), ExistingValue->GetFName(), RF_NoFlags, ExistingValue);
			}
			else
			{
				// Existing value is not a template or a template is what this property was pointing to so we can use it directly
				// Similar to the above condition but the property was not referencing an instanced value in which case it's ok to leave the CDO default here
				NewDefaultObjectValue = ExistingValue;
			}
		}
	}

	if (!NewDefaultObjectValue && !PropertyClass->HasAnyClassFlags(CLASS_Abstract))
	{
		// Existing value did not exist or it could not be used as a template
		// Existing value may be null in case we were serializing an array of UObjects that failed to load (missing import class). Since the array is first pre-allocated with null values
		// it will not have any existing objects to instantiate
		NewDefaultObjectValue = NewObject<UObject>(Outer, PropertyClass);
	}

	// Final sanity check. We still may end up with a null object if the property class is abstract and the previous object value was missing or was not compatible
	UE_CLOG(!NewDefaultObjectValue, LogProperty, Fatal, TEXT("Failed to create default object value for property %s. Previous value: %s"), 
		*GetFullName(), 
		ExistingValue ? *ExistingValue->GetFullName() : TEXT("None"));

	return NewDefaultObjectValue;
}

void FObjectPropertyBase::CheckValidObject(void* ValueAddress, TObjectPtr<UObject> OldValue) const
{
	const TObjectPtr<UObject> Object = GetObjectPtrPropertyValue(ValueAddress);
	if (!Object)
	{
		return;
	}
	//
	// here we want to make sure the the object value still matches the 
	// object type expected by the property...

	UClass* ObjectClass = Object.GetClass();
	UE_CLOG(!ObjectClass, LogProperty, Fatal, TEXT("Object without class referenced by %s, object: 0x%016llx %s"), *GetPathName(), (int64)(PTRINT)ValueAddress, *Object.GetPathName());

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	auto IsDeferringValueLoad = [&]()
	{
		FLinkerLoad* PropertyLinker = GetLinker();
		return ((PropertyLinker == nullptr) || (PropertyLinker->LoadFlags & LOAD_DeferDependencyLoads)) &&
			(ObjectClass->IsChildOf<ULinkerPlaceholderExportObject>() || ObjectClass->IsChildOf<ULinkerPlaceholderClass>());
	};

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	check( IsDeferringValueLoad() || (!Object->IsA<ULinkerPlaceholderExportObject>() && !Object->IsA<ULinkerPlaceholderClass>()) );
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

#else  // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING 
	auto IsDeferringValueLoad = [&]() { return false; };
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	if ((PropertyClass != nullptr) && !ObjectClass->IsChildOf(PropertyClass) && !ObjectClass->GetAuthoritativeClass()->IsChildOf(PropertyClass))
	{
			
		// we could be in the middle of replacing references to the 
		// PropertyClass itself (in the middle of an FArchiveReplaceObjectRef 
		// pass)... if this is the case, then we might have already replaced 
		// the object's class, but not the PropertyClass yet (or vise-versa)... 
		// so we use this to ensure, in that situation, that we don't clear the 
		// object value (if CLASS_NewerVersionExists is set, then we are likely 
		// in the middle of an FArchiveReplaceObjectRef pass)
		bool bIsReplacingClassRefs = PropertyClass && PropertyClass->HasAnyClassFlags(CLASS_NewerVersionExists) != ObjectClass->HasAnyClassFlags(CLASS_NewerVersionExists);
		if (!bIsReplacingClassRefs && !IsDeferringValueLoad())
		{
			if (!HasAnyPropertyFlags(CPF_NonNullable))
			{
				UE_LOG(LogProperty, Warning,
					TEXT("Serialized %s for a property of %s. Reference will be nullptred.\n    Property = %s\n    Item = %s"),
					*ObjectClass->GetFullName(),
					*PropertyClass->GetFullName(),
					*GetFullName(),
					*Object.GetFullName()
				);
				SetObjectPropertyValue(ValueAddress, nullptr);
			}
			else
			{
				UObject* DefaultValue = ConstructDefaultObjectValueIfNecessary(OldValue);

				UE_LOG(LogProperty, Warning,
					TEXT("Serialized %s for a non-nullable property of %s. Reference will be defaulted to %s.\n    Property = %s\n    Item = %s"),
					*ObjectClass->GetFullName(),
					*PropertyClass->GetFullName(),
					DefaultValue ? *DefaultValue->GetFullName() : TEXT("None"),
					*GetFullName(),
					*Object.GetFullName()
				);

				SetObjectPropertyValue(ValueAddress, DefaultValue);
			}
		}
	}
}

bool FObjectPropertyBase::SameType(const FProperty* Other) const
{
	return (Super::SameType(Other) || (Other && Other->IsA<FObjectPropertyBase>() && ((FObjectPropertyBase*)Other)->AllowObjectTypeReinterpretationTo(this))) && 
			 (PropertyClass == ((FObjectPropertyBase*)Other)->PropertyClass);
}

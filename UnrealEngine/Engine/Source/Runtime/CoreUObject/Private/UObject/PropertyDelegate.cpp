// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UnrealTypePrivate.h"
#include "UObject/PropertyHelper.h"
#include "UObject/LinkerPlaceholderFunction.h"
#include "Serialization/ArchiveUObjectFromStructuredArchive.h"
#include "Hash/Blake3.h"

/*-----------------------------------------------------------------------------
	FDelegateProperty.
-----------------------------------------------------------------------------*/
IMPLEMENT_FIELD(FDelegateProperty)

FDelegateProperty::FDelegateProperty(FFieldVariant InOwner, const UECodeGen_Private::FDelegatePropertyParams& Prop)
	: FDelegateProperty_Super(InOwner, (const UECodeGen_Private::FPropertyParamsBaseWithOffset&)Prop)
{
	SignatureFunction = Prop.SignatureFunctionFunc ? Prop.SignatureFunctionFunc() : nullptr;
}

#if WITH_EDITORONLY_DATA
FDelegateProperty::FDelegateProperty(UField* InField)
	: FDelegateProperty_Super(InField)
{
	UDelegateProperty* SourceProperty = CastChecked<UDelegateProperty>(InField);
	SignatureFunction = SourceProperty->SignatureFunction;
}
#endif // WITH_EDITORONLY_DATA

void FDelegateProperty::PostDuplicate(const FField& InField)
{
	const FDelegateProperty& Source = static_cast<const FDelegateProperty&>(InField);
	SignatureFunction = Source.SignatureFunction;
	Super::PostDuplicate(InField);
}

void FDelegateProperty::InstanceSubobjects(void* Data, void const* DefaultData, UObject* InOwner, FObjectInstancingGraph* InstanceGraph )
{
	for( int32 i=0; i<ArrayDim; i++ )
	{
		FScriptDelegate& DestDelegate = ((FScriptDelegate*)Data)[i];
		UObject* CurrentUObject = DestDelegate.GetUObject();

		if (CurrentUObject)
		{
			UObject *Template = NULL;

			if (DefaultData)
			{
				FScriptDelegate& DefaultDelegate = ((FScriptDelegate*)DefaultData)[i];
				Template = DefaultDelegate.GetUObject();
			}

			UObject* NewUObject = InstanceGraph->InstancePropertyValue(Template, CurrentUObject, InOwner, EInstancePropertyValueFlags::AllowSelfReference | EInstancePropertyValueFlags::DoNotCreateNewInstance);
			DestDelegate.BindUFunction(NewUObject, DestDelegate.GetFunctionName());
		}
	}
}

bool FDelegateProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	check(A);

	const FScriptDelegate* DA = (const FScriptDelegate*)A;
	const FScriptDelegate* DB = (const FScriptDelegate*)B;

	if (!DB)
	{
		return !DA->IsBound();
	}
	
	return *DA == *DB;
}


void FDelegateProperty::SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const
{
	Slot << *GetPropertyValuePtr(Value);
}


bool FDelegateProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	// Do not allow replication of delegates, as there is no way to make this secure (it allows the execution of any function in any object, on the remote client/server)
	return 1;
}


FString FDelegateProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	check(SignatureFunction);

	FString UnmangledFunctionName = SignatureFunction->GetName().LeftChop( FString( HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX ).Len() );
	const bool bBlueprintCppBackend = (0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_BlueprintCppBackend));
	const bool bNative = SignatureFunction->IsNative();
	if (bBlueprintCppBackend && bNative)
	{
		UStruct* StructOwner = Cast<UStruct>(SignatureFunction->GetOuter());
		if (StructOwner)
		{
			return FString::Printf(TEXT("%s%s::F%s"), StructOwner->GetPrefixCPP(), *StructOwner->GetName(), *UnmangledFunctionName);
		}
	}
	else
	{
		const bool NonNativeClassOwner = (SignatureFunction->GetOwnerClass() && !SignatureFunction->GetOwnerClass()->HasAnyClassFlags(CLASS_Native));
		if (bBlueprintCppBackend && NonNativeClassOwner)
		{
			// The name must be valid, this removes spaces, ?, etc from the user's function name. It could
			// be slightly shorter because the postfix ("__pf") is not needed here because we further post-
			// pend to the string. Normally the postfix is needed to make sure we don't mangle to a valid
			// identifier and collide:
			UnmangledFunctionName = UnicodeToCPPIdentifier(UnmangledFunctionName, false, TEXT(""));
			// the name must be unique
			const FString OwnerName = UnicodeToCPPIdentifier(SignatureFunction->GetOwnerClass()->GetName(), false, TEXT(""));
			const FString NewUnmangledFunctionName = FString::Printf(TEXT("%s__%s"), *UnmangledFunctionName, *OwnerName);
			UnmangledFunctionName = NewUnmangledFunctionName;
		}
		if (0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_CustomTypeName))
		{
			UnmangledFunctionName += TEXT("__SinglecastDelegate");
		}
	}
	return FString(TEXT("F")) + UnmangledFunctionName;
}

FString FDelegateProperty::GetCPPTypeForwardDeclaration() const
{
	return FString();
}

void FDelegateProperty::ExportText_Internal( FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	auto ExportDelegateAsText = [&ValueStr](FScriptDelegate* ScriptDelegate)
	{
		check(ScriptDelegate != NULL);
		bool bDelegateHasValue = ScriptDelegate->GetFunctionName() != NAME_None;
		ValueStr += FString::Printf(TEXT("%s.%s"),
			ScriptDelegate->GetUObject() != NULL ? *ScriptDelegate->GetUObject()->GetName() : TEXT("(null)"),
			*ScriptDelegate->GetFunctionName().ToString());
	};
	
	if (PropertyPointerType == EPropertyPointerType::Container && HasGetter())
	{
		FScriptDelegate LocalScriptDelegate;
		GetValue_InContainer(PropertyValueOrContainer, &LocalScriptDelegate);
		ExportDelegateAsText(&LocalScriptDelegate);
	}
	else
	{
		FScriptDelegate* ScriptDelegate = (FScriptDelegate*)PointerToValuePtr(PropertyValueOrContainer, PropertyPointerType);
		ExportDelegateAsText(ScriptDelegate);
	}
}


const TCHAR* FDelegateProperty::ImportText_Internal( const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* Parent, int32 PortFlags, FOutputDevice* ErrorText ) const
{
	const TCHAR* Result = nullptr;
	if (PropertyPointerType == EPropertyPointerType::Container && HasSetter())
	{
		FScriptDelegate LocalScriptDelegate;
		Result = DelegatePropertyTools::ImportDelegateFromText(*(FScriptDelegate*)&LocalScriptDelegate, SignatureFunction, Buffer, Parent, ErrorText);
		SetValue_InContainer(ContainerOrPropertyPtr, LocalScriptDelegate);
	}
	else
	{
		void* PropertyValue = PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
		Result = DelegatePropertyTools::ImportDelegateFromText(*(FScriptDelegate*)PropertyValue, SignatureFunction, Buffer, Parent, ErrorText);
	}

	return Result;
}

void FDelegateProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << SignatureFunction;

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (Ar.IsLoading() || Ar.IsObjectReferenceCollector())
	{
		if (auto PlaceholderFunc = Cast<ULinkerPlaceholderFunction>(SignatureFunction))
		{
			PlaceholderFunc->AddReferencingProperty(this);
		}
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

void FDelegateProperty::AddReferencedObjects(FReferenceCollector& Collector)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (SignatureFunction && !SignatureFunction->IsA<ULinkerPlaceholderFunction>())
#endif//USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	{
		Collector.AddReferencedObject(SignatureFunction);
	}
	Super::AddReferencedObjects(Collector);
}

bool FDelegateProperty::SameType(const FProperty* Other) const
{
	return Super::SameType(Other) && (SignatureFunction == ((FDelegateProperty*)Other)->SignatureFunction);
}

#if WITH_EDITORONLY_DATA
void FDelegateProperty::AppendSchemaHash(FBlake3& Builder, bool bSkipEditorOnly) const
{
	Super::AppendSchemaHash(Builder, bSkipEditorOnly);
	if (SignatureFunction)
	{
		// Hash the function's name instead of recursively hashing the function; the function's schema does not impact how we serialize our pointer to it
		FNameBuilder ObjectPath;
		SignatureFunction->GetPathName(nullptr, ObjectPath);
		Builder.Update(ObjectPath.GetData(), ObjectPath.Len() * sizeof(ObjectPath.GetData()[0]));
	}
}
#endif


void FDelegateProperty::BeginDestroy()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (auto PlaceholderFunc = Cast<ULinkerPlaceholderFunction>(SignatureFunction))
	{
		PlaceholderFunc->RemoveReferencingProperty(this);
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	Super::BeginDestroy();
}

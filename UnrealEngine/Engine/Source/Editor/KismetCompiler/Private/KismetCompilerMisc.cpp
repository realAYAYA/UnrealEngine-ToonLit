// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	KismetCompilerMisc.cpp
=============================================================================*/

#include "KismetCompilerMisc.h"

#include "BlueprintCompilationManager.h"
#include "Misc/CoreMisc.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/FieldPathProperty.h"
#include "UObject/ObjectRedirector.h"
#include "Engine/Blueprint.h"
#include "UObject/UObjectHash.h"
#include "Engine/MemberReference.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedStruct.h"
#include "FieldNotification/FieldNotificationLibrary.h"
#include "INotifyFieldValueChanged.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "FieldNotificationId.h"
#include "K2Node.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_Self.h"
#include "K2Node_TemporaryVariable.h"
#include "K2Node_Timeline.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "KismetCastingUtils.h"
#include "KismetCompiledFunctionContext.h"
#include "KismetCompiler.h"
#include "K2Node_EnumLiteral.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "ObjectTools.h"
#include "BlueprintEditorSettings.h"
#include "Components/ActorComponent.h"

#define LOCTEXT_NAMESPACE "KismetCompiler"

DECLARE_CYCLE_STAT(TEXT("Choose Terminal Scope"), EKismetCompilerStats_ChooseTerminalScope, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Resolve compiled statements"), EKismetCompilerStats_ResolveCompiledStatements, STATGROUP_KismetCompiler );

//////////////////////////////////////////////////////////////////////////
// FKismetCompilerUtilities

static bool DoesTypeNotMatchProperty(UEdGraphPin* SourcePin, const FEdGraphPinType& OwningType, const FEdGraphTerminalType& TerminalType, FProperty* TestProperty, FCompilerResultsLog& MessageLog, UClass* SelfClass)
{
	check(SourcePin);
	const EEdGraphPinDirection Direction = SourcePin->Direction; 
	const FName PinCategory = TerminalType.TerminalCategory;
	const FName PinSubCategory = TerminalType.TerminalSubCategory;
	const UObject* PinSubCategoryObject = TerminalType.TerminalSubCategoryObject.Get();
	
	const UFunction* OwningFunction = TestProperty->GetOwner<UFunction>();

	bool bTypeMismatch = false;
	bool bSubtypeMismatch = false;

	if (PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		FBoolProperty* SpecificProperty = CastField<FBoolProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == nullptr);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		FByteProperty* ByteProperty = CastField<FByteProperty>(TestProperty);
		FEnumProperty* EnumProperty = CastField<FEnumProperty>(TestProperty);
		bTypeMismatch = (ByteProperty == nullptr) && (EnumProperty == nullptr || !EnumProperty->GetUnderlyingProperty()->IsA<FByteProperty>());
	}
	else if ((PinCategory == UEdGraphSchema_K2::PC_Class) || (PinCategory == UEdGraphSchema_K2::PC_SoftClass))
	{
		const UClass* ClassType = (PinSubCategory == UEdGraphSchema_K2::PSC_Self) ? SelfClass : Cast<const UClass>(PinSubCategoryObject);
		if (ClassType)
		{
			ClassType = ClassType->GetAuthoritativeClass();
		}

		if (ClassType == NULL)
		{
			MessageLog.Error(*LOCTEXT("FindClassForPin_Error", "Failed to find class for pin @@").ToString(), SourcePin);
		}
		else
		{
			const UClass* MetaClass = NULL;
			if (FClassProperty* ClassProperty = CastField<FClassProperty>(TestProperty))
			{
				MetaClass = ClassProperty->MetaClass;
			}
			else if (FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(TestProperty))
			{
				MetaClass = SoftClassProperty->MetaClass;
			}

			if (MetaClass != NULL)
			{
				const UClass* OutputClass = (Direction == EGPD_Output) ? ClassType :  MetaClass;
				const UClass* InputClass = (Direction == EGPD_Output) ? MetaClass : ClassType;

				OutputClass = OutputClass->GetAuthoritativeClass();
				InputClass = InputClass->GetAuthoritativeClass();

				// It matches if it's an exact match or if the output class is more derived than the input class
				bTypeMismatch = bSubtypeMismatch = !((OutputClass == InputClass) || (OutputClass && OutputClass->IsChildOf(InputClass)));

				if ((PinCategory == UEdGraphSchema_K2::PC_SoftClass) && (!TestProperty->IsA<FSoftClassProperty>()))
				{
					bTypeMismatch = true;
				}
			}
			else
			{
				bTypeMismatch = true;
			}
		}
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (PinSubCategory == UEdGraphSchema_K2::PC_Float)
		{
			FFloatProperty* SpecificProperty = CastField<FFloatProperty>(TestProperty);
			bTypeMismatch = (SpecificProperty == nullptr);
		}
		else if (PinSubCategory == UEdGraphSchema_K2::PC_Double)
		{
			FDoubleProperty* SpecificProperty = CastField<FDoubleProperty>(TestProperty);
			bTypeMismatch = (SpecificProperty == nullptr);
		}
		else
		{
			checkf(false, TEXT("Erroneous pin subcategory for PC_Real: %s"), *PinSubCategory.ToString());
			bTypeMismatch = true;
		}
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		FIntProperty* SpecificProperty = CastField<FIntProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == nullptr);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		FInt64Property* SpecificProperty = CastField<FInt64Property>(TestProperty);
		bTypeMismatch = (SpecificProperty == nullptr);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		FNameProperty* SpecificProperty = CastField<FNameProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == nullptr);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Delegate)
	{
		const UFunction* SignatureFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(OwningType.PinSubCategoryMemberReference);
		const FDelegateProperty* PropertyDelegate = CastField<const FDelegateProperty>(TestProperty);
		bTypeMismatch = !(SignatureFunction 
			&& PropertyDelegate 
			&& PropertyDelegate->SignatureFunction 
			&& PropertyDelegate->SignatureFunction->IsSignatureCompatibleWith(SignatureFunction));
	}
	else if ((PinCategory == UEdGraphSchema_K2::PC_Object) || (PinCategory == UEdGraphSchema_K2::PC_Interface) || (PinCategory == UEdGraphSchema_K2::PC_SoftObject))
	{
		const UClass* ObjectType = (PinSubCategory == UEdGraphSchema_K2::PSC_Self) ? SelfClass : Cast<const UClass>(PinSubCategoryObject);


		if (!ObjectType)
		{
			MessageLog.Error(*LOCTEXT("FindClassForPin_Error", "Failed to find class for pin @@").ToString(), SourcePin);
		}
		// If the object type has been marked as transient and is no longer rooted in the GUObjectArray,
		// then then it has been "consigned to oblvion". This can be the case if a BP asset has been force 
		// deleted and references to it are still laying around
		else if(
			ObjectType->HasAnyFlags(RF_Transient) &&
			ObjectType->HasAnyClassFlags(CLASS_NewerVersionExists) &&
			!ObjectType->IsRooted() && 
			ObjectType->ClassDefaultObject &&
			!ObjectType->ClassDefaultObject->IsRooted())
		{
			MessageLog.Error(*LOCTEXT("InvalidClassForPin_Error", "Class for pin @@ is invalid! It has likely been deleted.").ToString(), SourcePin);
		}
		else
		{
			FObjectPropertyBase* ObjProperty = CastField<FObjectPropertyBase>(TestProperty);
			if (ObjProperty != nullptr && ObjProperty->PropertyClass)
			{
				const UClass* OutputClass = (Direction == EGPD_Output) ? ObjectType : ObjProperty->PropertyClass;
				const UClass* InputClass  = (Direction == EGPD_Output) ? ObjProperty->PropertyClass : ObjectType;

				// Fixup stale types to avoid unwanted mismatches during the reinstancing process
				if (OutputClass->HasAnyClassFlags(CLASS_NewerVersionExists))
				{
					UBlueprint* GeneratedByBP = Cast<UBlueprint>(OutputClass->ClassGeneratedBy);
					if (GeneratedByBP != nullptr)
					{
						const UClass* NewOutputClass = GeneratedByBP->GeneratedClass;
						if (NewOutputClass && !NewOutputClass->HasAnyClassFlags(CLASS_NewerVersionExists))
						{
							OutputClass = NewOutputClass;
						}
					}
				}
				if (InputClass->HasAnyClassFlags(CLASS_NewerVersionExists))
				{
					UBlueprint* GeneratedByBP = Cast<UBlueprint>(InputClass->ClassGeneratedBy);
					if (GeneratedByBP != nullptr)
					{
						const UClass* NewInputClass = GeneratedByBP->GeneratedClass;
						if (NewInputClass && !NewInputClass->HasAnyClassFlags(CLASS_NewerVersionExists))
						{
							InputClass = NewInputClass;
						}
					}
				}

				InputClass = InputClass->GetAuthoritativeClass();
				OutputClass = OutputClass->GetAuthoritativeClass();

				// It matches if it's an exact match or if the output class is more derived than the input class
				bTypeMismatch = bSubtypeMismatch = !((OutputClass == InputClass) || (OutputClass && OutputClass->IsChildOf(InputClass)));

				if ((PinCategory == UEdGraphSchema_K2::PC_SoftObject) && (!TestProperty->IsA<FSoftObjectProperty>()))
				{
					bTypeMismatch = true;
				}
			}
			else if (FInterfaceProperty* IntefaceProperty = CastField<FInterfaceProperty>(TestProperty))
			{
				UClass const* InterfaceClass = IntefaceProperty->InterfaceClass;
				if (InterfaceClass == nullptr)
				{
					bTypeMismatch = true;
				}
				else 
				{
					bTypeMismatch = ObjectType->ImplementsInterface(InterfaceClass);
				}
			}
			else
			{
				bTypeMismatch = true;
			}
		}
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_String)
	{
		FStrProperty* SpecificProperty = CastField<FStrProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == nullptr);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		FTextProperty* SpecificProperty = CastField<FTextProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == nullptr);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		const UScriptStruct* StructType = Cast<const UScriptStruct>(PinSubCategoryObject);
		if (StructType == NULL)
		{
			MessageLog.Error(*LOCTEXT("FindStructForPin_Error", "Failed to find struct for pin @@").ToString(), SourcePin);
		}
		else
		{
			FStructProperty* StructProperty = CastField<FStructProperty>(TestProperty);
			if (StructProperty != NULL)
			{
				bool bMatchingStructs = (StructType == StructProperty->Struct);
				if (const UUserDefinedStruct* UserDefinedStructFromProperty = Cast<const UUserDefinedStruct>(StructProperty->Struct))
				{
					bMatchingStructs |= (UserDefinedStructFromProperty->PrimaryStruct.Get() == StructType);
				}
				bSubtypeMismatch = bTypeMismatch = !bMatchingStructs;
			}
			else
			{
				bTypeMismatch = true;
			}

			if (OwningFunction && bTypeMismatch)
			{
				if (UK2Node_CallFunction::IsStructureWildcardProperty(OwningFunction, SourcePin->PinName))
				{
					bSubtypeMismatch = bTypeMismatch = false;
				}
			}
		}
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_FieldPath)
	{
		FFieldPathProperty* SpecificProperty = CastField<FFieldPathProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == nullptr);
	}
	else
	{
		MessageLog.Error(*FText::Format(LOCTEXT("UnsupportedTypeForPinFmt", "Unsupported type ({0}) on @@"), UEdGraphSchema_K2::TypeToText(OwningType)).ToString(), SourcePin);
	}

	return bTypeMismatch || bSubtypeMismatch;
}

/** Tests to see if a pin is schema compatible with a property */
bool FKismetCompilerUtilities::IsTypeCompatibleWithProperty(UEdGraphPin* SourcePin, FProperty* Property, FCompilerResultsLog& MessageLog, const UEdGraphSchema_K2* Schema, UClass* SelfClass)
{
	check(SourcePin != NULL);
	const FEdGraphPinType& Type = SourcePin->PinType;
	const EEdGraphPinDirection Direction = SourcePin->Direction; 

	const FName PinCategory = Type.PinCategory;
	const FName PinSubCategory = Type.PinSubCategory;
	const UObject* PinSubCategoryObject = Type.PinSubCategoryObject.Get();

	FProperty* TestProperty = NULL;
	const UFunction* OwningFunction = Property->GetOwner<UFunction>();
	
	int32 NumErrorsAtStart = MessageLog.NumErrors;
	bool bTypeMismatch = false;

	if( Type.IsArray() )
	{
		// For arrays, the property we want to test against is the inner property
		if( FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property) )
		{
			if(OwningFunction)
			{
				// Check for the magic ArrayParm property, which always matches array types
				const FString& ArrayPointerMetaData = OwningFunction->GetMetaData(FBlueprintMetadata::MD_ArrayParam);
				TArray<FString> ArrayPinComboNames;
				ArrayPointerMetaData.ParseIntoArray(ArrayPinComboNames, TEXT(","), true);

				if (ArrayPinComboNames.Num() > 0)
				{
					TArray<FString> ArrayPinNames;
					const FString SourcePinName = SourcePin->PinName.ToString();

					for (const FString& ArrayPinComboName : ArrayPinComboNames)
					{
						ArrayPinNames.Reset();
						ArrayPinComboName.ParseIntoArray(ArrayPinNames, TEXT("|"), true);

						if (ArrayPinNames[0] == SourcePinName)
						{
							return true;
						}
					}
				}
			}
			
			bTypeMismatch = ::DoesTypeNotMatchProperty(SourcePin, Type, SourcePin->GetPrimaryTerminalType(), ArrayProp->Inner, MessageLog, SelfClass);
		}
		else
		{
			MessageLog.Error(*LOCTEXT("PinSpecifiedAsArray_Error", "Pin @@ is specified as an array, but does not have a valid array property.").ToString(), SourcePin);
			return false;
		}
	}
	else if (Type.IsSet())
	{
		if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
		{
			if (OwningFunction && FEdGraphUtilities::IsSetParam(OwningFunction, SourcePin->PinName))
			{
				return true;
			}

			bTypeMismatch = ::DoesTypeNotMatchProperty(SourcePin, Type, SourcePin->GetPrimaryTerminalType(), SetProperty->ElementProp, MessageLog, SelfClass);
		}
		else
		{
			MessageLog.Error(*LOCTEXT("PinSpecifiedAsSet_Error", "Pin @@ is specified as a set, but does not have a valid set property.").ToString(), SourcePin);
			return false;
		}
	}
	else if (Type.IsMap())
	{
		if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			if (OwningFunction && FEdGraphUtilities::IsMapParam(OwningFunction, SourcePin->PinName))
			{
				return true;
			}

			bTypeMismatch = ::DoesTypeNotMatchProperty(SourcePin, Type, SourcePin->GetPrimaryTerminalType(), MapProperty->KeyProp, MessageLog, SelfClass);
			bTypeMismatch = bTypeMismatch || ::DoesTypeNotMatchProperty(SourcePin, Type, Type.PinValueType, MapProperty->ValueProp, MessageLog, SelfClass);
		}
		else
		{
			MessageLog.Error(*LOCTEXT("PinSpecifiedAsSet_Error", "Pin @@ is specified as a set, but does not have a valid set property.").ToString(), SourcePin);
			return false;
		}
	}
	else
	{
		// For scalars, we just take the passed in property
		bTypeMismatch = ::DoesTypeNotMatchProperty(SourcePin, Type, SourcePin->GetPrimaryTerminalType(), Property, MessageLog, SelfClass);
	}

	// Check for the early out...if this is a type dependent parameter in an array function
	if( OwningFunction )
	{
		if ( OwningFunction->HasMetaData(FBlueprintMetadata::MD_ArrayParam) )
		{
			// Check to see if this param is type dependent on an array parameter
			const FString& DependentParams = OwningFunction->GetMetaData(FBlueprintMetadata::MD_ArrayDependentParam);
			TArray<FString>	DependentParamNames;
			DependentParams.ParseIntoArray(DependentParamNames, TEXT(","), true);
			if (DependentParamNames.Find(SourcePin->PinName.ToString()) != INDEX_NONE)
			{
				//@todo:  This assumes that the wildcard coercion has done its job...I'd feel better if there was some easier way of accessing the target array type
				return true;
			}
		}
		else if (OwningFunction->HasMetaData(FBlueprintMetadata::MD_SetParam))
		{
			// If the pin in question is part of a Set (inferred) parameter, then ignore pin matching:
			// @todo:  This assumes that the wildcard coercion has done its job...I'd feel better if 
			// there was some easier way of accessing the target set type
			if (FEdGraphUtilities::IsSetParam(OwningFunction, SourcePin->PinName))
			{
				return true;
			}
		}
		else if(OwningFunction->HasMetaData(FBlueprintMetadata::MD_MapParam))
		{
			// If the pin in question is part of a Set (inferred) parameter, then ignore pin matching:
			// @todo:  This assumes that the wildcard coercion has done its job...I'd feel better if 
			// there was some easier way of accessing the target container type
			if(FEdGraphUtilities::IsMapParam(OwningFunction, SourcePin->PinName))
			{
				return true;
			}
		}
	}

	if (bTypeMismatch)
	{
		MessageLog.Error(
			*FText::Format(
				LOCTEXT("TypeDoesNotMatchPropertyOfType_ErrorFmt", "@@ of type {0} doesn't match the property {1} of type {2}"),
				UEdGraphSchema_K2::TypeToText(Type),
				FText::FromString(Property->GetName()),
				UEdGraphSchema_K2::TypeToText(Property)
			).ToString(),
			SourcePin
		);
	}

	// Now check the direction if it is parameter coming in or out of a function call style node (variable nodes are excluded since they maybe local parameters)
	if (Property->HasAnyPropertyFlags(CPF_Parm) && !SourcePin->GetOwningNode()->IsA(UK2Node_Variable::StaticClass()))
	{
		// Parameters are directional
		const bool bOutParam = Property->HasAllPropertyFlags(CPF_ReturnParm) || (Property->HasAllPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ReferenceParm));

		if ( ((SourcePin->Direction == EGPD_Input) && bOutParam) || ((SourcePin->Direction == EGPD_Output) && !bOutParam))
		{
			MessageLog.Error(
				*FText::Format(
					LOCTEXT("DirectionMismatchParameter_ErrorFmt", "The direction of @@ doesn't match the direction of parameter {0}"),
					FText::FromString(Property->GetName())
				).ToString(),
				SourcePin
			);
		}

		if (Property->HasAnyPropertyFlags(CPF_ReferenceParm)
			&& (SourcePin->LinkedTo.Num() == 0)
			&& (SourcePin->PinType.PinSubCategoryObject.Get() != TBaseStructure<FTransform>::Get())
			&& (SourcePin->Direction == EGPD_Input))
		{
			TArray<FString> AutoEmittedTerms;
			Schema->GetAutoEmitTermParameters(OwningFunction, AutoEmittedTerms);
			const bool bIsAutoEmittedTerm = AutoEmittedTerms.Contains(SourcePin->PinName.ToString());

			// Make sure reference parameters are linked, except for FTransforms, which have a special node handler that adds an internal constant term
			if (!bIsAutoEmittedTerm)
			{
				MessageLog.Error(*LOCTEXT("PassLiteral_Error", "Cannot pass a literal to @@.  Connect a variable to it instead.").ToString(), SourcePin);
			}
		}
	}

	return NumErrorsAtStart == MessageLog.NumErrors;
}

uint32 FKismetCompilerUtilities::ConsignToOblivionCounter = 0;

// Rename a class and it's CDO into the transient package, and clear RF_Public on both of them
void FKismetCompilerUtilities::ConsignToOblivion(UClass* OldClass, bool bForceNoResetLoaders)
{
	if (OldClass != NULL)
	{
		// Use the Kismet class reinstancer to ensure that the CDO and any existing instances of this class are cleaned up!
		TSharedPtr<FBlueprintCompileReinstancer> CTOResinstancer = FBlueprintCompileReinstancer::Create(OldClass);

		UPackage* OwnerOutermost = OldClass->GetOutermost();
		if( OldClass->ClassDefaultObject )
		{
			// rename to a temp name, move into transient package
			OldClass->ClassDefaultObject->ClearFlags(RF_Public);
			OldClass->ClassDefaultObject->SetFlags(RF_Transient);
			OldClass->ClassDefaultObject->RemoveFromRoot(); // make sure no longer in root set
		}
		
		OldClass->SetMetaData(FBlueprintMetadata::MD_IsBlueprintBase, TEXT("false"));
		OldClass->ClearFlags(RF_Public);
		OldClass->SetFlags(RF_Transient);
		OldClass->ClassFlags |= CLASS_Deprecated|CLASS_NewerVersionExists;
		OldClass->RemoveFromRoot(); // make sure no longer in root set

		for( TFieldIterator<UFunction> ItFunc(OldClass,EFieldIteratorFlags::ExcludeSuper); ItFunc; ++ItFunc )
		{
			UFunction* CurrentFunc = *ItFunc;
			FLinkerLoad::InvalidateExport(CurrentFunc);
		}

		const FString BaseName = FString::Printf(TEXT("DEADCLASS_%s_C_%d"), *OldClass->ClassGeneratedBy->GetName(), ConsignToOblivionCounter++);
		OldClass->Rename(*BaseName, GetTransientPackage(), (REN_DontCreateRedirectors|REN_NonTransactional|(bForceNoResetLoaders ? REN_ForceNoResetLoaders : 0)));

		// Make sure MetaData doesn't have any entries to the class we just renamed out of package
		OwnerOutermost->GetMetaData()->RemoveMetaDataOutsidePackage();
	}
}

void FKismetCompilerUtilities::RemoveObjectRedirectorIfPresent(UObject* Package, const FString& NewName, UObject* ObjectBeingMovedIn)
{
	// We can rename on top of an object redirection (basically destroy the redirection and put us in its place).
	if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(StaticFindObject(UObjectRedirector::StaticClass(), Package, *NewName)))
	{
		ObjectTools::DeleteRedirector(Redirector);
		Redirector = NULL;
	}
}

/** Finds a property by name, starting in the specified scope; Validates property type and returns NULL along with emitting an error if there is a mismatch. */
FProperty* FKismetCompilerUtilities::FindPropertyInScope(UStruct* Scope, UEdGraphPin* Pin, FCompilerResultsLog& MessageLog, const UEdGraphSchema_K2* Schema, UClass* SelfClass, bool& bIsSparseProperty)
{
	if (FProperty* Property = FKismetCompilerUtilities::FindNamedPropertyInScope(Scope, Pin->PinName, bIsSparseProperty, /*bAllowDeprecated*/true))
	{
		if (FKismetCompilerUtilities::IsTypeCompatibleWithProperty(Pin, Property, MessageLog, Schema, SelfClass))
		{
			return Property;
		}
	}
	else if (!FKismetCompilerUtilities::IsMissingMemberPotentiallyLoading(Cast<UBlueprint>(SelfClass->ClassGeneratedBy), Scope))
	{
		UObject* MessageScope = Scope ? Scope : SelfClass;
		MessageLog.Error(*FText::Format(LOCTEXT("PropertyNotFound_Error", "The property associated with @@ could not be found in '{0}'"), FText::FromString(MessageScope->GetPathName())).ToString(), Pin);
	}

	return nullptr;
}

// Finds a property by name, starting in the specified scope, returning NULL if it's not found
FProperty* FKismetCompilerUtilities::FindNamedPropertyInScope(UStruct* Scope, FName PropertyName, bool& bIsSparseProperty, const bool bAllowDeprecated)
{
	auto FindProperty = [PropertyName, bAllowDeprecated](UStruct* CurrentScope) -> FProperty*
	{
		for (TFieldIterator<FProperty> It(CurrentScope); It; ++It)
		{
			FProperty* Property = *It;

			if (Property->GetFName() == PropertyName)
			{
				if (bAllowDeprecated || !Property->HasAllPropertyFlags(CPF_Deprecated))
				{
					return Property;
				}
				break;
			}
		}

		return nullptr;
	};

	auto FindSparseClassDataProperty = [&FindProperty](UStruct* CurrentScope) -> FProperty*
	{
		if (UClass* Class = Cast<UClass>(CurrentScope))
		{
			if (UStruct* SparseData = Class->GetSparseClassDataStruct())
			{
				return FindProperty(SparseData);
			}
		}
		return nullptr;
	};

	bIsSparseProperty = false;
	while (Scope)
	{
		// Check the given scope first
		if (FProperty* Property = FindProperty(Scope))
		{
			if (Property->HasAllPropertyFlags(CPF_Deprecated))
			{
				// If this property is deprecated, check to see if the sparse data has a property that 
				// we should use instead (eg, when migrating data from an object into the sparse data)
				if (FProperty* SparseProperty = FindSparseClassDataProperty(Scope))
				{
					bIsSparseProperty = true;
					return SparseProperty;
				}
			}
			return Property;
		}

		// Check the sparse data for the property
		if (FProperty* SparseProperty = FindSparseClassDataProperty(Scope))
		{
			bIsSparseProperty = true;
			return SparseProperty;
		}

		// Functions don't automatically check their class when using a field iterator
		UFunction* Function = Cast<UFunction>(Scope);
		Scope = Function ? Cast<UStruct>(Function->GetOuter()) : nullptr;
	}

	return nullptr;
}

void FKismetCompilerUtilities::CompileDefaultProperties(UClass* Class)
{
	UObject* DefaultObject = Class->GetDefaultObject(); // Force the default object to be constructed if it isn't already
	check(DefaultObject);
}

void FKismetCompilerUtilities::LinkAddedProperty(UStruct* Structure, FProperty* NewProperty)
{
	check(NewProperty->Next == NULL);
	check(Structure->ChildProperties != NewProperty);

	NewProperty->Next = Structure->ChildProperties;
	Structure->ChildProperties = NewProperty;
}

const UFunction* FKismetCompilerUtilities::FindOverriddenImplementableEvent(const FName& EventName, const UClass* Class)
{
	const uint32 RequiredFlagMask = FUNC_Event | FUNC_BlueprintEvent | FUNC_Native;
	const uint32 RequiredFlagResult = FUNC_Event | FUNC_BlueprintEvent;

	const UFunction* FoundEvent = Class ? Class->FindFunctionByName(EventName, EIncludeSuperFlag::ExcludeSuper) : NULL;

	const bool bFlagsMatch = (NULL != FoundEvent) && (RequiredFlagResult == ( FoundEvent->FunctionFlags & RequiredFlagMask ));

	return bFlagsMatch ? FoundEvent : NULL;
}

void FKismetCompilerUtilities::ValidateEnumProperties(const UObject* DefaultObject, FCompilerResultsLog& MessageLog)
{
	check(DefaultObject);
	for (TFieldIterator<FProperty> It(DefaultObject->GetClass()); It; ++It)
	{
		FProperty* Property = *It;

		if(!Property->HasAnyPropertyFlags(CPF_Transient))
		{
			const UEnum* Enum = nullptr;
			const FNumericProperty* UnderlyingProp = nullptr;
			if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
			{
				Enum = EnumProperty->GetEnum();
				UnderlyingProp = EnumProperty->GetUnderlyingProperty();
			}
			else if(const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
			{
				Enum = ByteProperty->GetIntPropertyEnum();
				UnderlyingProp = ByteProperty;
			}

			if(Enum)
			{
				const int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(DefaultObject));
				if(!Enum->IsValidEnumValue(EnumValue))
				{
					MessageLog.Warning(
						*FText::Format(
							LOCTEXT("InvalidEnumDefaultValue_ErrorFmt", "Default Enum value '{0}' for class '{1}' is invalid in object '{2}'. EnumVal: {3}. EnumAcceptableMax: {4} "),
							FText::FromString(Property->GetName()),
							FText::FromString(DefaultObject->GetClass()->GetName()),
							FText::FromString(DefaultObject->GetName()),
							EnumValue,
							Enum->GetMaxEnumValue()
						).ToString()
					);
				}
			}
		}
	}
}

bool FKismetCompilerUtilities::ValidateSelfCompatibility(const UEdGraphPin* Pin, FKismetFunctionContext& Context)
{
	const UBlueprint* Blueprint = Context.Blueprint;
	const UEdGraph* SourceGraph = Context.SourceGraph;
	const UEdGraphSchema_K2* K2Schema = Context.Schema;
	const UBlueprintGeneratedClass* BPClass = Context.NewClass;

	FString ErrorMsg;
	if (Blueprint->BlueprintType != BPTYPE_FunctionLibrary && K2Schema->IsStaticFunctionGraph(SourceGraph))
	{
		ErrorMsg = FText::Format(
			LOCTEXT("PinMustHaveConnection_Static_ErrorFmt", "'@@' must have a connection, because {0} is a static function and will not be bound to instances of this blueprint."),
			FText::FromString(SourceGraph->GetName())
		).ToString();
	}
	else
	{
		FEdGraphPinType SelfType;
		SelfType.PinCategory = UEdGraphSchema_K2::PC_Object;
		SelfType.PinSubCategory = UEdGraphSchema_K2::PSC_Self;

		if (!K2Schema->ArePinTypesCompatible(SelfType, Pin->PinType, BPClass))
		{
			FString PinType;
			if ((Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object) ||
				(Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface) ||
				(Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class))
			{
				if (Pin->PinType.PinSubCategoryObject.IsValid())
				{
					PinType = Pin->PinType.PinSubCategoryObject->GetName();
				}
			}
			else
			{
				PinType = Pin->PinType.PinCategory.ToString();
			}

			if (PinType.IsEmpty())
			{
				ErrorMsg = *LOCTEXT("PinMustHaveConnection_NoType_Error", "This blueprint (self) is not compatible with '@@', therefore that pin must have a connection.").ToString();
			}
			else
			{
				ErrorMsg = FText::Format(
					LOCTEXT("PinMustHaveConnection_WrongClass_ErrorFmt", "This blueprint (self) is not a {0}, therefore '@@' must have a connection."),
					FText::FromString(PinType)
				).ToString();
			}
		}
	}

	if (!ErrorMsg.IsEmpty())
	{
		Context.MessageLog.Error(*ErrorMsg, Pin);
		return false;
	}

	return true;
}

UEdGraphPin* FKismetCompilerUtilities::GenerateAssignmentNodes(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* CallBeginSpawnNode, UEdGraphNode* SpawnNode, UEdGraphPin* CallBeginResult, const UClass* ForClass )
{
	static const FName ObjectParamName(TEXT("Object"));
	static const FName ValueParamName(TEXT("Value"));
	static const FName PropertyNameParamName(TEXT("PropertyName"));

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	UEdGraphPin* LastThen = CallBeginSpawnNode->GetThenPin();

	// Create 'set var by name' nodes and hook them up
	for (int32 PinIdx = 0; PinIdx < SpawnNode->Pins.Num(); PinIdx++)
	{
		// Only create 'set param by name' node if this pin is linked to something
		UEdGraphPin* OrgPin = SpawnNode->Pins[PinIdx];
		const bool bHasDefaultValue = !OrgPin->DefaultValue.IsEmpty() || !OrgPin->DefaultTextValue.IsEmpty() || OrgPin->DefaultObject;
		if (NULL == CallBeginSpawnNode->FindPin(OrgPin->PinName) &&
			(OrgPin->LinkedTo.Num() > 0 || bHasDefaultValue))
		{
			FProperty* Property = FindFProperty<FProperty>(ForClass, OrgPin->PinName);
			// NULL property indicates that this pin was part of the original node, not the 
			// class we're assigning to:
			if (!Property)
			{
				continue;
			}

			if( OrgPin->LinkedTo.Num() == 0 )
			{
				// We don't want to generate an assignment node unless the default value 
				// differs from the value in the CDO:
				FString DefaultValueAsString;
					
				if (FBlueprintCompilationManager::GetDefaultValue(ForClass, Property, DefaultValueAsString))
				{
					if (Schema->DoesDefaultValueMatch(*OrgPin, DefaultValueAsString))
					{
						continue;
					}
				}
				else if(ForClass->ClassDefaultObject)
				{
					FBlueprintEditorUtils::PropertyValueToString(Property, (uint8*)ForClass->ClassDefaultObject.Get(), DefaultValueAsString);

					if (DefaultValueAsString == OrgPin->GetDefaultAsString())
					{
						continue;
					}
				}
			}

			const FString& SetFunctionName = Property->GetMetaData(FBlueprintMetadata::MD_PropertySetFunction);
			if (!SetFunctionName.IsEmpty())
			{
				UFunction* SetFunction = ForClass->FindFunctionByName(*SetFunctionName);
				check(SetFunction);

				// Add a cast node so we can call the Setter function with a pin of the right class
				UK2Node_DynamicCast* CastNode = CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(SpawnNode, SourceGraph);
				CastNode->TargetType = const_cast<UClass*>(ForClass);
				CastNode->SetPurity(true);
				CastNode->AllocateDefaultPins();
				CastNode->GetCastSourcePin()->MakeLinkTo(CallBeginResult);
				CastNode->NotifyPinConnectionListChanged(CastNode->GetCastSourcePin());

				UK2Node_CallFunction* CallFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
				CallFuncNode->SetFromFunction(SetFunction);
				CallFuncNode->AllocateDefaultPins();

				// Connect this node into the exec chain
				Schema->TryCreateConnection(LastThen, CallFuncNode->GetExecPin());
				LastThen = CallFuncNode->GetThenPin();

				// Connect the new object to the 'object' pin
				UEdGraphPin* ObjectPin = Schema->FindSelfPin(*CallFuncNode, EGPD_Input);
				CastNode->GetCastResultPin()->MakeLinkTo(ObjectPin);

				// Move Value pin connections
				UEdGraphPin* SetFunctionValuePin = nullptr;
				for (UEdGraphPin* CallFuncPin : CallFuncNode->Pins)
				{
					if (!Schema->IsMetaPin(*CallFuncPin))
					{
						check(CallFuncPin->Direction == EGPD_Input);
						SetFunctionValuePin = CallFuncPin;
						break;
					}
				}
				check(SetFunctionValuePin);

				CompilerContext.MovePinLinksToIntermediate(*OrgPin, *SetFunctionValuePin);
			}
			else if (FKismetCompilerUtilities::IsPropertyUsesFieldNotificationSetValueAndBroadcast(Property))
			{
				// Add a cast node so we can call the Setter function with a pin of the right class
				//UK2Node_DynamicCast* CastNode = CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(SpawnNode, SourceGraph);
				//CastNode->TargetType = const_cast<UClass*>(ForClass);
				//CastNode->SetPurity(true);
				//CastNode->AllocateDefaultPins();
				//CastNode->GetCastSourcePin()->MakeLinkTo(CallBeginResult);
				//CastNode->NotifyPinConnectionListChanged(CastNode->GetCastSourcePin());

				FMemberReference MemberReference;
				MemberReference.SetFromField<FProperty>(Property, false);
				TTuple<UEdGraphPin*, UEdGraphPin*> ExecThenPins = GenerateFieldNotificationSetNode(CompilerContext, SourceGraph, SpawnNode, CallBeginResult, Property, MemberReference, false, false, Property->HasAllPropertyFlags(CPF_Net));

				// Connect this node into the exec chain
				Schema->TryCreateConnection(LastThen, ExecThenPins.Get<0>());
				LastThen = ExecThenPins.Get<1>();
			}
			else if (UFunction* SetByNameFunction = Schema->FindSetVariableByNameFunction(OrgPin->PinType))
			{
				UK2Node_CallFunction* SetVarNode = nullptr;
				if (OrgPin->PinType.IsArray())
				{
					SetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(SpawnNode, SourceGraph);
				}
				else
				{
					SetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
				}
				SetVarNode->SetFromFunction(SetByNameFunction);
				SetVarNode->AllocateDefaultPins();

				// Connect this node into the exec chain
				Schema->TryCreateConnection(LastThen, SetVarNode->GetExecPin());
				LastThen = SetVarNode->GetThenPin();

				// Connect the new object to the 'object' pin
				UEdGraphPin* ObjectPin = SetVarNode->FindPinChecked(ObjectParamName);
				CallBeginResult->MakeLinkTo(ObjectPin);

				// Fill in literal for 'property name' pin - name of pin is property name
				UEdGraphPin* PropertyNamePin = SetVarNode->FindPinChecked(PropertyNameParamName);
				PropertyNamePin->DefaultValue = OrgPin->PinName.ToString();

				UEdGraphPin* ValuePin = SetVarNode->FindPinChecked(ValueParamName);
				if (OrgPin->LinkedTo.Num() == 0 &&
					OrgPin->DefaultValue != FString() &&
					OrgPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Byte &&
					OrgPin->PinType.PinSubCategoryObject.IsValid() &&
					OrgPin->PinType.PinSubCategoryObject->IsA<UEnum>())
				{
					// Pin is an enum, we need to alias the enum value to an int:
					UK2Node_EnumLiteral* EnumLiteralNode = CompilerContext.SpawnIntermediateNode<UK2Node_EnumLiteral>(SpawnNode, SourceGraph);
					EnumLiteralNode->Enum = CastChecked<UEnum>(OrgPin->PinType.PinSubCategoryObject.Get());
					EnumLiteralNode->AllocateDefaultPins();
					EnumLiteralNode->FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue)->MakeLinkTo(ValuePin);

					UEdGraphPin* InPin = EnumLiteralNode->FindPinChecked(UK2Node_EnumLiteral::GetEnumInputPinName());
					check( InPin );
					InPin->DefaultValue = OrgPin->DefaultValue;
				}
				else
				{
					// For non-array struct pins that are not linked, transfer the pin type so that the node will expand an auto-ref that will assign the value by-ref.
					if (OrgPin->PinType.IsArray() == false && OrgPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && OrgPin->LinkedTo.Num() == 0)
					{
						ValuePin->PinType.PinCategory = OrgPin->PinType.PinCategory;
						ValuePin->PinType.PinSubCategory = OrgPin->PinType.PinSubCategory;
						ValuePin->PinType.PinSubCategoryObject = OrgPin->PinType.PinSubCategoryObject;
						CompilerContext.MovePinLinksToIntermediate(*OrgPin, *ValuePin);
					}
					else
					{
						// For interface pins we need to copy over the subcategory
						if (OrgPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
						{
							ValuePin->PinType.PinSubCategoryObject = OrgPin->PinType.PinSubCategoryObject;
						}

						CompilerContext.MovePinLinksToIntermediate(*OrgPin, *ValuePin);
						SetVarNode->PinConnectionListChanged(ValuePin);
					}
				}
			}
		}
	}

	return LastThen;
}

namespace UE::KismetCompiler::Private
{
	UK2Node_MakeArray* MakeArrayNodeForFieldNotificationId(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphNode* SourceNode, UEdGraphPin* LinkTo)
	{
		UK2Node_MakeArray* MakeArrayNode = CompilerContext.SpawnIntermediateNode<UK2Node_MakeArray>(SourceNode, SourceGraph);
		MakeArrayNode->AllocateDefaultPins();
		CompilerContext.MessageLog.NotifyIntermediateObjectCreation(MakeArrayNode, SourceNode);

		// Link the array to the other input pin
		{
			UEdGraphPin* ArrayOut = MakeArrayNode->GetOutputPin();
			ArrayOut->MakeLinkTo(LinkTo);
			MakeArrayNode->PinConnectionListChanged(ArrayOut);
		}

		return MakeArrayNode;
	}

	UK2Node_MakeStruct* MakeFieldNotificationIdStruct(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphNode* SourceNode, const FString& FieldId)
	{
		UK2Node_MakeStruct* MakeStruct = CompilerContext.SpawnIntermediateNode<UK2Node_MakeStruct>(SourceNode, SourceGraph);
		MakeStruct->StructType = FFieldNotificationId::StaticStruct();;
		MakeStruct->AllocateDefaultPins();
		MakeStruct->bMadeAfterOverridePinRemoval = true;
		CompilerContext.MessageLog.NotifyIntermediateObjectCreation(MakeStruct, SourceNode);

		CompilerContext.GetSchema()->TrySetDefaultValue(*MakeStruct->FindPinChecked(GET_MEMBER_NAME_STRING_CHECKED(FFieldNotificationId, FieldName)), FieldId);

		return MakeStruct;
	}

	void MakeLinkFromMakeStructTo(UK2Node_MakeStruct* MakeStructNode, UEdGraphPin* InputPin)
	{
		UEdGraphPin** MakeStructOutPin = MakeStructNode->Pins.FindByPredicate([](UEdGraphPin* OtherPin) { return OtherPin->Direction == EGPD_Output; });
		check(MakeStructOutPin);
		(*MakeStructOutPin)->MakeLinkTo(InputPin);
	}

	void AddMakeStructToMakeArrayNode(UK2Node_MakeArray* MakeArrayNode, UK2Node_MakeStruct* MakeStructNode, int32 Index)
	{
		// Find the input pin on the "Make Array" node by index.
		if (Index > 0)
		{
			MakeArrayNode->AddInputPin();
		}

		const FString PinName = FString::Printf(TEXT("[%d]"), Index);
		MakeLinkFromMakeStructTo(MakeStructNode, MakeArrayNode->FindPinChecked(PinName));
	}
}

TTuple<UEdGraphPin*, UEdGraphPin*> FKismetCompilerUtilities::GenerateFieldNotificationSetNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphNode* SourceNode, UEdGraphPin* SelfPin, FProperty* VariableProperty, const FMemberReference& VariableReference, bool bHasLocalRepNotify, bool bShouldFlushDormancyOnSet, bool bIsNetProperty)
{
	UClass* OwnerClass = VariableProperty->GetOwnerClass();
	const FString& FieldNotifyMetaData = VariableProperty->GetMetaData(FBlueprintMetadata::MD_FieldNotify);
	TArray<FString> OtherFieldNotifyToTrigger;
	FieldNotifyMetaData.ParseIntoArray(OtherFieldNotifyToTrigger, TEXT("|"), true);

	// Set With Broadcast K2 function
	UK2Node_CallFunction* CallFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SourceNode, SourceGraph);
	if (OtherFieldNotifyToTrigger.Num() == 0)
	{
		CallFuncNode->SetFromFunction(UFieldNotificationLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UFieldNotificationLibrary, SetPropertyValueAndBroadcast)));
	}
	else
	{
		CallFuncNode->SetFromFunction(UFieldNotificationLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UFieldNotificationLibrary, SetPropertyValueAndBroadcastFields)));
	}
	CallFuncNode->AllocateDefaultPins();

	const UEdGraphSchema_K2* K2Schema = CompilerContext.GetSchema();

	// Set Self pin connections
	{
		if (ensure(SelfPin) && SelfPin->LinkedTo.Num() > 0 && SelfPin->Direction == EEdGraphPinDirection::EGPD_Input)
		{
			CompilerContext.CopyPinLinksToIntermediate(*SelfPin, *CallFuncNode->FindPinChecked(FName("Object")));
			CompilerContext.CopyPinLinksToIntermediate(*SelfPin, *CallFuncNode->FindPinChecked(FName("NetOwner")));
		}
		else if (SelfPin && SelfPin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			K2Schema->TryCreateConnection(SelfPin, CallFuncNode->FindPinChecked(FName("Object"), EGPD_Input));
			K2Schema->TryCreateConnection(SelfPin, CallFuncNode->FindPinChecked(FName("NetOwner"), EGPD_Input));
		}
		else
		{
			UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(SourceNode, SourceGraph);
			SelfNode->AllocateDefaultPins();

			SelfPin = SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self);
			K2Schema->TryCreateConnection(SelfPin, CallFuncNode->FindPinChecked(FName("Object"), EGPD_Input));
			K2Schema->TryCreateConnection(SelfPin, CallFuncNode->FindPinChecked(FName("NetOwner"), EGPD_Input));
		}
	}

	// OldValue and NewValue pin connections
	{
		UK2Node_VariableGet* VariableGetNode = CompilerContext.SpawnIntermediateNode<UK2Node_VariableGet>(SourceNode, SourceGraph);
		VariableGetNode->VariableReference = VariableReference;
		VariableGetNode->AllocateDefaultPins();
		CompilerContext.MessageLog.NotifyIntermediateObjectCreation(VariableGetNode, SourceNode);

		{
			UEdGraphPin* GetSelfPin = K2Schema->FindSelfPin(*VariableGetNode, EGPD_Input);
			check(SelfPin && GetSelfPin);
			if (SelfPin->Direction == EEdGraphPinDirection::EGPD_Output)
			{
				K2Schema->TryCreateConnection(SelfPin, GetSelfPin);
			}
			else
			{
				CompilerContext.CopyPinLinksToIntermediate(*SelfPin, *GetSelfPin);
			}
		}

		UEdGraphPin* VariableGetPin = VariableGetNode->FindPinChecked(VariableGetNode->GetVarName(), EGPD_Output);
		UEdGraphPin* OldValuePin = CallFuncNode->FindPinChecked(FName("OldValue"), EGPD_Input);
		K2Schema->TryCreateConnection(VariableGetPin, OldValuePin);
		CallFuncNode->NotifyPinConnectionListChanged(OldValuePin);

		// Force the pin to be the same type (see CustomStructureParam) 
		UEdGraphPin* NewValuePin = CallFuncNode->FindPinChecked(FName("NewValue"), EGPD_Input);
		NewValuePin->PinType = OldValuePin->PinType;
		CompilerContext.CopyPinLinksToIntermediate(*SourceNode->FindPinChecked(VariableReference.GetMemberName(), EGPD_Input), *NewValuePin);

		bool bUseReferenceByRef = NewValuePin->LinkedTo.Num() != 0;
		K2Schema->TrySetDefaultValue(*CallFuncNode->FindPinChecked(FName("NewValueByRef")), bUseReferenceByRef ? TEXT("True") : TEXT("False"));
	}

	// Set Net args pin
	{
		K2Schema->TrySetDefaultValue(*CallFuncNode->FindPinChecked(FName("bHasLocalRepNotify")), bHasLocalRepNotify ? TEXT("True") : TEXT("False"));
		K2Schema->TrySetDefaultValue(*CallFuncNode->FindPinChecked(FName("bShouldFlushDormancyOnSet")), bHasLocalRepNotify ? TEXT("True") : TEXT("False"));
		K2Schema->TrySetDefaultValue(*CallFuncNode->FindPinChecked(FName("bIsNetProperty")), bHasLocalRepNotify ? TEXT("True") : TEXT("False"));
	}

	TTuple<UEdGraphPin*, UEdGraphPin*> Result = {CallFuncNode->GetExecPin(), CallFuncNode->GetThenPin()};

	// Assign the other broadcast to generate
	if (OtherFieldNotifyToTrigger.Num() > 0)
	{
		UK2Node_MakeArray*MakeArrayNode = UE::KismetCompiler::Private::MakeArrayNodeForFieldNotificationId(CompilerContext, SourceGraph, SourceNode, CallFuncNode->FindPinChecked(TEXT("ExtraFieldIds")));
		for (int32 ArgIndex = 0; ArgIndex < OtherFieldNotifyToTrigger.Num(); ++ArgIndex)
		{
			const FString& OtherFieldId = OtherFieldNotifyToTrigger[ArgIndex];
			if (!OtherFieldId.IsEmpty())
			{
				// Spawn a "Make Struct" node to create the struct FFieldNotificationId
				UK2Node_MakeStruct* MakeStuctNode = UE::KismetCompiler::Private::MakeFieldNotificationIdStruct(CompilerContext, SourceGraph, SourceNode, OtherFieldId);
				UE::KismetCompiler::Private::AddMakeStructToMakeArrayNode(MakeArrayNode, MakeStuctNode, ArgIndex);
			}
		}
	}

	return Result;
}

TTuple<UEdGraphPin*, UEdGraphPin*> FKismetCompilerUtilities::GenerateBroadcastFieldNotificationNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphNode* SourceNode, FProperty* Property)
{
	UClass* OwnerClass = Property->GetOwnerClass();
	const FString& FieldNotifyMetaData = Property->GetMetaData(FBlueprintMetadata::MD_FieldNotify);
	TArray<FString> OtherFieldNotifyToTrigger;
	FieldNotifyMetaData.ParseIntoArray(OtherFieldNotifyToTrigger, TEXT("|"), true);

	// Broadcast function
	UK2Node_CallFunction* CallFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SourceNode, SourceGraph);
	if (OtherFieldNotifyToTrigger.Num() == 0)
	{
		CallFuncNode->SetFromFunction(UFieldNotificationLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UFieldNotificationLibrary, BroadcastFieldValueChanged)));
	}
	else
	{
		CallFuncNode->SetFromFunction(UFieldNotificationLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UFieldNotificationLibrary, BroadcastFieldsValueChanged)));
	}
	CallFuncNode->AllocateDefaultPins();

	const UEdGraphSchema_K2* K2Schema = CompilerContext.GetSchema();

	// Set Self pin connections
	{
		UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(SourceNode, SourceGraph);
		SelfNode->AllocateDefaultPins();

		UEdGraphPin* SelfNodePin = SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self);
		K2Schema->TryCreateConnection(SelfNodePin, CallFuncNode->FindPinChecked(FName("Object"), EGPD_Input));
	}

	TTuple<UEdGraphPin*, UEdGraphPin*> Result = { CallFuncNode->GetExecPin(), CallFuncNode->GetThenPin() };

	// Assign the FieldId
	if (OtherFieldNotifyToTrigger.Num() == 0)
	{
		UK2Node_MakeStruct* MakeStruct = UE::KismetCompiler::Private::MakeFieldNotificationIdStruct(CompilerContext, SourceGraph, SourceNode, Property->GetName());
		UE::KismetCompiler::Private::MakeLinkFromMakeStructTo(MakeStruct, CallFuncNode->FindPinChecked(TEXT("FieldId")));
	}
	else
	{
		OtherFieldNotifyToTrigger.Add(Property->GetName());
		UK2Node_MakeArray* MakeArrayNode = UE::KismetCompiler::Private::MakeArrayNodeForFieldNotificationId(CompilerContext, SourceGraph, SourceNode, CallFuncNode->FindPinChecked(TEXT("FieldIds")));
		for (int32 ArgIndex = 0; ArgIndex < OtherFieldNotifyToTrigger.Num(); ++ArgIndex)
		{
			const FString& OtherFieldId = OtherFieldNotifyToTrigger[ArgIndex];
			if (!OtherFieldId.IsEmpty())
			{
				// Spawn a "Make Struct" node to create the struct FFieldNotificationId
				UK2Node_MakeStruct* MakeStuctNode = UE::KismetCompiler::Private::MakeFieldNotificationIdStruct(CompilerContext, SourceGraph, SourceNode, OtherFieldId);
				UE::KismetCompiler::Private::AddMakeStructToMakeArrayNode(MakeArrayNode, MakeStuctNode, ArgIndex);
			}
		}
	}

	return Result;
}

void FKismetCompilerUtilities::CreateObjectAssignmentStatement(FKismetFunctionContext& Context, UEdGraphNode* Node, FBPTerminal* SrcTerm, FBPTerminal* DstTerm, UEdGraphPin* DstPin)
{
	UClass* InputObjClass = Cast<UClass>(SrcTerm->Type.PinSubCategoryObject.Get());
	UClass* OutputObjClass = Cast<UClass>(DstTerm->Type.PinSubCategoryObject.Get());

	const bool bIsOutputInterface = ((OutputObjClass != NULL) && OutputObjClass->HasAnyClassFlags(CLASS_Interface));
	const bool bIsInputInterface = ((InputObjClass != NULL) && InputObjClass->HasAnyClassFlags(CLASS_Interface));

	if (bIsOutputInterface != bIsInputInterface)
	{
		// Create a literal term from the class specified in the node
		FBPTerminal* ClassTerm = Context.CreateLocalTerminal(ETerminalSpecification::TS_Literal);
		ClassTerm->Name = GetNameSafe(OutputObjClass);
		ClassTerm->bIsLiteral = true;
		ClassTerm->Source = DstTerm->Source;
		ClassTerm->ObjectLiteral = OutputObjClass;
		ClassTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Class;

		EKismetCompiledStatementType CastOpType = bIsOutputInterface ? KCST_CastObjToInterface : KCST_CastInterfaceToObj;
		FBlueprintCompiledStatement& CastStatement = Context.AppendStatementForNode(Node);
		CastStatement.Type = CastOpType;
		CastStatement.LHS = DstTerm;
		CastStatement.RHS.Add(ClassTerm);
		CastStatement.RHS.Add(SrcTerm);
	}
	else
	{
		FBPTerminal* RHSTerm = SrcTerm;

		using namespace UE::KismetCompiler;

		FBPTerminal* ImplicitCastTerm = nullptr;

		// Some pins can share a single terminal (eg: those in UK2Node_FunctionResult)
		// In those cases, it's preferable to use a specific pin instead of relying on what DstTerm points to.
		UEdGraphPin* DstPinSearchKey = DstPin ? DstPin : DstTerm->SourcePin;

		// Some terms don't necessarily have a valid SourcePin (eg: FKCHandler_FunctionEntry)
		if (DstPinSearchKey)
		{
			ImplicitCastTerm = CastingUtils::InsertImplicitCastStatement(Context, DstPinSearchKey, RHSTerm);
		}

		if (ImplicitCastTerm != nullptr)
		{
			RHSTerm = ImplicitCastTerm;
		}

		FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
		Statement.Type = KCST_Assignment;
		Statement.LHS = DstTerm;
		Statement.RHS.Add(RHSTerm);
	}
}

FProperty* FKismetCompilerUtilities::CreatePrimitiveProperty(FFieldVariant PropertyScope, const FName& ValidatedPropertyName, const FName& PinCategory, const FName& PinSubCategory, UObject* PinSubCategoryObject, UClass* SelfClass, bool bIsWeakPointer, const class UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog)
{
	const EObjectFlags ObjectFlags = RF_Public;

	FProperty* NewProperty = nullptr;
	if ((PinCategory == UEdGraphSchema_K2::PC_Object) || (PinCategory == UEdGraphSchema_K2::PC_Interface) || (PinCategory == UEdGraphSchema_K2::PC_SoftObject))
	{
		UClass* SubType = (PinSubCategory == UEdGraphSchema_K2::PSC_Self) ? SelfClass : Cast<UClass>(PinSubCategoryObject);

		if (SubType == nullptr)
		{
			// If this is from a degenerate pin, because the object type has been removed, default this to a UObject subtype so we can make a dummy term for it to allow the compiler to continue
			SubType = UObject::StaticClass();
		}

		if (SubType)
		{
			const bool bIsInterface = SubType->HasAnyClassFlags(CLASS_Interface)
				|| ((SubType == SelfClass) && ensure(SelfClass->ClassGeneratedBy) && FBlueprintEditorUtils::IsInterfaceBlueprint(CastChecked<UBlueprint>(SelfClass->ClassGeneratedBy)));

			if (bIsInterface)
			{
				FInterfaceProperty* NewPropertyObj = new FInterfaceProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
				// we want to use this setter function instead of setting the 
				// InterfaceClass member directly, because it properly handles  
				// placeholder classes (classes that are stubbed in during load)
				NewPropertyObj->SetInterfaceClass(SubType);
				NewProperty = NewPropertyObj;
			}
			else
			{
				FObjectPropertyBase* NewPropertyObj = nullptr;

				if (PinCategory == UEdGraphSchema_K2::PC_SoftObject)
				{
					NewPropertyObj = new FSoftObjectProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
				}
				else if (bIsWeakPointer)
				{
					NewPropertyObj = new FWeakObjectProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
				}
				else
				{
					NewPropertyObj = new FObjectProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
					// If lazy load is enabled make the object property a TObjectPtr property
					// to allow for unresolved UObjects
					if (FLinkerLoad::IsImportLazyLoadEnabled())
					{
						NewPropertyObj->SetPropertyFlags(CPF_TObjectPtrWrapper);
					}
				}

				// Is the property a reference to something that should default to instanced?
				if (SubType->HasAnyClassFlags(CLASS_DefaultToInstanced))
				{
					NewPropertyObj->SetPropertyFlags(CPF_InstancedReference);

					// Actor components should only be instanced by the SCS editor.
					// 
					// Default actor components are outered to the generated BP class instead of the CDO.
					// If we set "EditInline" on actor components, we would actually outer them to the CDO.
					// This would lead to various serialization and instancing issues.
					if (!SubType->IsChildOf<UActorComponent>())
					{
						NewPropertyObj->SetMetaData(TEXT("EditInline"), TEXT("true"));
					}
				}

				// we want to use this setter function instead of setting the 
				// PropertyClass member directly, because it properly handles  
				// placeholder classes (classes that are stubbed in during load)
				NewPropertyObj->SetPropertyClass(SubType);
				NewPropertyObj->SetPropertyFlags(CPF_HasGetValueTypeHash);
				NewProperty = NewPropertyObj;
			}
		}
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (UScriptStruct* SubType = Cast<UScriptStruct>(PinSubCategoryObject))
		{
			FString StructureError;
			if (FStructureEditorUtils::EStructureError::Ok == FStructureEditorUtils::IsStructureValid(SubType, nullptr, &StructureError))
			{
				FStructProperty* NewPropertyStruct = new FStructProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
				NewPropertyStruct->Struct = SubType;
				NewProperty = NewPropertyStruct;

				if (SubType->StructFlags & STRUCT_HasInstancedReference)
				{
					NewProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
				}

				if (FBlueprintEditorUtils::StructHasGetTypeHash(SubType))
				{
					// tag the type as hashable to avoid crashes in core:
					NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
				}
			}
			else
			{
				MessageLog.Error(
					*FText::Format(
						LOCTEXT("InvalidStructForField_ErrorFmt", "Invalid property '{0}' structure '{1}' error: {2}"),
						FText::FromName(ValidatedPropertyName),
						FText::FromString(SubType->GetName()),
						FText::FromString(StructureError)
					).ToString()
				);
			}
		}
	}
	else if ((PinCategory == UEdGraphSchema_K2::PC_Class) || (PinCategory == UEdGraphSchema_K2::PC_SoftClass))
	{
		UClass* SubType = Cast<UClass>(PinSubCategoryObject);

		if (SubType == nullptr)
		{
			// If this is from a degenerate pin, because the object type has been removed, default this to a UObject subtype so we can make a dummy term for it to allow the compiler to continue
			SubType = UObject::StaticClass();

			MessageLog.Warning(
				*FText::Format(
					LOCTEXT("InvalidClassForField_ErrorFmt", "Invalid property '{0}' class, replaced with Object.  Please fix or remove."),
					FText::FromName(ValidatedPropertyName)
				).ToString()
			);
		}

		if (SubType)
		{
			if (PinCategory == UEdGraphSchema_K2::PC_SoftClass)
			{
				FSoftClassProperty* SoftClassProperty = new FSoftClassProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
				// we want to use this setter function instead of setting the 
				// MetaClass member directly, because it properly handles  
				// placeholder classes (classes that are stubbed in during load)
				SoftClassProperty->SetMetaClass(SubType);
				SoftClassProperty->PropertyClass = UClass::StaticClass();
				SoftClassProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
				NewProperty = SoftClassProperty;
			}
			else
			{
				FClassProperty* NewPropertyClass = new FClassProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
				// we want to use this setter function instead of setting the 
				// MetaClass member directly, because it properly handles  
				// placeholder classes (classes that are stubbed in during load)
				NewPropertyClass->SetMetaClass(SubType);
				NewPropertyClass->PropertyClass = UClass::StaticClass();
				NewPropertyClass->SetPropertyFlags(CPF_HasGetValueTypeHash);
				NewProperty = NewPropertyClass;
			}
		}
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		NewProperty = new FIntProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		NewProperty = new FInt64Property(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (PinSubCategory == UEdGraphSchema_K2::PC_Float)
		{
			NewProperty = new FFloatProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
		}
		else if (PinSubCategory == UEdGraphSchema_K2::PC_Double)
		{
			NewProperty = new FDoubleProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
		}
		else
		{
			checkf(false, TEXT("Erroneous pin subcategory for PC_Real: %s"), *PinSubCategory.ToString());
		}
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		FBoolProperty* BoolProperty = new FBoolProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
		BoolProperty->SetBoolSize(sizeof(bool), true);
		NewProperty = BoolProperty;
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_String)
	{
		NewProperty = new FStrProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		NewProperty = new FTextProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		UEnum* Enum = Cast<UEnum>(PinSubCategoryObject);

		if (Enum && Enum->GetCppForm() == UEnum::ECppForm::EnumClass)
		{
			FEnumProperty* EnumProp = new FEnumProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			FNumericProperty* UnderlyingProp = new FByteProperty(EnumProp, TEXT("UnderlyingType"), ObjectFlags);

			EnumProp->SetEnum(Enum);
			EnumProp->AddCppProperty(UnderlyingProp);

			NewProperty = EnumProp;
		}
		else
		{
			FByteProperty* ByteProp = new FByteProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			ByteProp->Enum = Cast<UEnum>(PinSubCategoryObject);

			NewProperty = ByteProp;
		}

		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		NewProperty = new FNameProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_FieldPath)
	{
		FFieldPathProperty* NewFieldPathProperty = new FFieldPathProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewFieldPathProperty->PropertyClass = FProperty::StaticClass();
		NewFieldPathProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
		NewProperty = NewFieldPathProperty;
	}
	else
	{
		// Failed to resolve the type-subtype, create a generic property to survive VM bytecode emission
		NewProperty = new FIntProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}

	return NewProperty;
}

/** Creates a property named PropertyName of type PropertyType in the Scope or returns NULL if the type is unknown, but does *not* link that property in */
FProperty* FKismetCompilerUtilities::CreatePropertyOnScope(UStruct* Scope, const FName& PropertyName, const FEdGraphPinType& Type, UClass* SelfClass, EPropertyFlags PropertyFlags, const UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog, UEdGraphPin* SourcePin)
{
	// When creating properties that depend on other properties (e.g. FDelegateProperty/FMulticastDelegateProperty::SignatureFunction)
	// you may need to update fixup logic in the compilation manager.
		
	const EObjectFlags ObjectFlags = RF_Public;

	FName ValidatedPropertyName = PropertyName;

	// Check to see if there's already a object on this scope with the same name, and throw an internal compiler error if so
	// If this happens, it breaks the property link, which causes stack corruption and hard-to-track errors, so better to fail at this point
	{
		FFieldVariant ExistingObject = CheckPropertyNameOnScope(Scope, PropertyName);
		if (ExistingObject.IsValid())
		{
			const FString ScopeName((Scope != nullptr) ? Scope->GetName() : FString(TEXT("None")));
			const FString ExistingTypeAndPath(ExistingObject.GetFullName());
			MessageLog.Error(*FString::Printf(TEXT("Internal Compiler Error: Tried to create a property %s in scope %s, but another object (%s) already exists there."), *PropertyName.ToString(), *ScopeName, *ExistingTypeAndPath));

			// Find a free name, so we can still create the property to make it easier to spot the duplicates, and avoid crashing
			uint32 Counter = 0;
			FName TestName;
			do 
			{
				FString TestNameString = PropertyName.ToString() + FString::Printf(TEXT("_ERROR_DUPLICATE_%d"), Counter++);
				TestName = FName(*TestNameString);

			} while (CheckPropertyNameOnScope(Scope, TestName).IsValid());

			ValidatedPropertyName = TestName;
		}
	}

	FProperty* NewProperty = nullptr;
	FFieldVariant PropertyScope;

	// Handle creating a container property, if necessary
	const bool bIsMapProperty = Type.IsMap();
	const bool bIsSetProperty = Type.IsSet();
	const bool bIsArrayProperty = Type.IsArray();
	FMapProperty* NewMapProperty = nullptr;
	FSetProperty* NewSetProperty = nullptr;
	FArrayProperty* NewArrayProperty = nullptr;
	FProperty* NewContainerProperty = nullptr;
	if (bIsMapProperty)
	{
		NewMapProperty = new FMapProperty(Scope, ValidatedPropertyName, ObjectFlags);
		PropertyScope = NewMapProperty;
		NewContainerProperty = NewMapProperty;
	}
	else if (bIsSetProperty)
	{
		NewSetProperty = new FSetProperty(Scope, ValidatedPropertyName, ObjectFlags);
		PropertyScope = NewSetProperty;
		NewContainerProperty = NewSetProperty;
	}
	else if( bIsArrayProperty )
	{
		NewArrayProperty = new FArrayProperty(Scope, ValidatedPropertyName, ObjectFlags);
		PropertyScope = NewArrayProperty;
		NewContainerProperty = NewArrayProperty;
	}
	else
	{
		PropertyScope = Scope;
	}

	if (Type.PinCategory == UEdGraphSchema_K2::PC_Delegate)
	{
		if (UFunction* SignatureFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(Type.PinSubCategoryMemberReference))
		{
			FDelegateProperty* NewPropertyDelegate = new FDelegateProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewPropertyDelegate->SignatureFunction = SignatureFunction;
			NewProperty = NewPropertyDelegate;
		}
	}
	else if (Type.PinCategory == UEdGraphSchema_K2::PC_MCDelegate)
	{
		UFunction* const SignatureFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(Type.PinSubCategoryMemberReference);
		FMulticastDelegateProperty* NewPropertyDelegate = new FMulticastInlineDelegateProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewPropertyDelegate->SignatureFunction = SignatureFunction;
		NewProperty = NewPropertyDelegate;
	}
	else
	{
		NewProperty = CreatePrimitiveProperty(PropertyScope, ValidatedPropertyName, Type.PinCategory, Type.PinSubCategory, Type.PinSubCategoryObject.Get(), SelfClass, Type.bIsWeakPointer, Schema, MessageLog);
	}

	if (NewProperty && Type.bIsUObjectWrapper)
	{
		NewProperty->SetPropertyFlags(CPF_UObjectWrapper);
	}

	if (NewContainerProperty && NewProperty && NewProperty->HasAnyPropertyFlags(CPF_ContainsInstancedReference | CPF_InstancedReference))
	{
		NewContainerProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
	}

	if (bIsMapProperty)
	{
		if (NewProperty)
		{
			if (!NewProperty->HasAnyPropertyFlags(CPF_HasGetValueTypeHash))
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("BadType"), Schema->GetCategoryText(Type.PinCategory));

				if (SourcePin && SourcePin->GetOwningNode())
				{
					MessageLog.Error(*FText::Format(LOCTEXT("MapKeyTypeUnhashable_Node_ErrorFmt", "@@ has key type of {BadType} which cannot be hashed and is therefore invalid"), Arguments).ToString(), SourcePin->GetOwningNode());
				}
				else
				{
					MessageLog.Error(*FText::Format(LOCTEXT("MapKeyTypeUnhashable_ErrorFmt", "Map Property @@ has key type of {BadType} which cannot be hashed and is therefore invalid"), Arguments).ToString(), NewMapProperty);
				}
			}

			// make the value property:
			// not feeling good about myself..
			// Fix up the array property to have the new type-specific property as its inner, and return the new FArrayProperty
			NewMapProperty->KeyProp = NewProperty;
			// make sure the value property does not collide with the key property:
			FName ValueName = FName( *(ValidatedPropertyName.GetPlainNameString() + FString(TEXT("_Value") )) );
			NewMapProperty->ValueProp = CreatePrimitiveProperty(PropertyScope, ValueName, Type.PinValueType.TerminalCategory, Type.PinValueType.TerminalSubCategory, Type.PinValueType.TerminalSubCategoryObject.Get(), SelfClass, Type.bIsWeakPointer, Schema, MessageLog);;
			if (!NewMapProperty->ValueProp)
			{
				delete NewMapProperty;
				NewMapProperty = nullptr;
				NewProperty = nullptr;
			}
			else
			{
				if (NewMapProperty->ValueProp->HasAnyPropertyFlags(CPF_ContainsInstancedReference | CPF_InstancedReference))
				{
					NewContainerProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
				}

				if (Type.PinValueType.bTerminalIsUObjectWrapper)
				{
					NewMapProperty->ValueProp->SetPropertyFlags(CPF_UObjectWrapper);
				}

				NewProperty = NewMapProperty;
			}
		}
		else
		{
			delete NewMapProperty;
			NewMapProperty = nullptr;
		}
	}
	else if (bIsSetProperty)
	{
		if (NewProperty)
		{
			if (!NewProperty->HasAnyPropertyFlags(CPF_HasGetValueTypeHash))
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("BadType"), Schema->GetCategoryText(Type.PinCategory));

				if(SourcePin && SourcePin->GetOwningNode())
				{
					MessageLog.Error(*FText::Format(LOCTEXT("SetKeyTypeUnhashable_Node_ErrorFmt", "@@ has container type of {BadType} which cannot be hashed and is therefore invalid"), Arguments).ToString(), SourcePin->GetOwningNode());
				}
				else
				{
					MessageLog.Error(*FText::Format(LOCTEXT("SetKeyTypeUnhashable_ErrorFmt", "Set Property @@ has container type of {BadType} which cannot be hashed and is therefore invalid"), Arguments).ToString(), NewSetProperty);
				}

				// We need to be able to serialize (for CPFUO to migrate data), so force the 
				// property to hash:
				NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
			}
			NewSetProperty->ElementProp = NewProperty;
			NewProperty = NewSetProperty;
		}
		else
		{
			delete NewSetProperty;
			NewSetProperty = nullptr;
		}
	}
	else if (bIsArrayProperty)
	{
		if (NewProperty)
		{
			// Fix up the array property to have the new type-specific property as its inner, and return the new FArrayProperty
			NewArrayProperty->Inner = NewProperty;
			NewProperty = NewArrayProperty;
		}
		else
		{
			delete NewArrayProperty;
			NewArrayProperty = nullptr;
		}
	}

	if (NewProperty)
	{
		NewProperty->SetPropertyFlags(PropertyFlags);
	}

	return NewProperty;
}

FFieldVariant FKismetCompilerUtilities::CheckPropertyNameOnScope(UStruct* Scope, const FName& PropertyName)
{
	FString NameStr = PropertyName.ToString();

	if (UObject* ExistingObject = FindObject<UObject>(Scope, *NameStr, false))
	{
		return ExistingObject;
	}

	if (Scope && !Scope->IsA<UFunction>() && (UBlueprintGeneratedClass::GetUberGraphFrameName() != PropertyName))
	{
		if (FProperty* Field = FindFProperty<FProperty>(Scope->GetSuperStruct(), *NameStr))
		{
			return Field;
		}
	}

	return FFieldVariant();
}

void FKismetCompilerUtilities::ValidateProperEndExecutionPath(FKismetFunctionContext& Context)
{
	ensureMsgf(false, TEXT("ValidateProperEndExecutionPath has been deprecated"));
}

void FKismetCompilerUtilities::DetectValuesReturnedByRef(const UFunction* Func, const UK2Node * Node, FCompilerResultsLog& MessageLog)
{
	for (TFieldIterator<FProperty> PropIt(Func); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		FProperty* FuncParam = *PropIt;
		if (FuncParam->HasAllPropertyFlags(CPF_OutParm) && !FuncParam->HasAllPropertyFlags(CPF_ConstParm))
		{
			const FString MessageStr = FText::Format(
				LOCTEXT("WrongRefOutputFmt", "No value will be returned by reference. Parameter '{0}'. Node: @@"),
				FText::FromString(FuncParam->GetName())
			).ToString();
			if (FuncParam->IsA<FArrayProperty>()) // array is always passed by reference, see FKismetCompilerContext::CreatePropertiesFromList
			{
				MessageLog.Note(*MessageStr, Node);
			}
			else
			{
				MessageLog.Warning(*MessageStr, Node);
			}
		}
	}
}

bool FKismetCompilerUtilities::IsPropertyUsesFieldNotificationSetValueAndBroadcast(const FProperty* Property)
{
	return Property->HasMetaData(FBlueprintMetadata::MD_FieldNotify)
		&& !Property->HasMetaData(FBlueprintMetadata::MD_PropertySetFunction)
		&& !Property->HasSetter()
		&& Cast<UBlueprintGeneratedClass>(Property->GetOwnerClass()) != nullptr
		&& Property->GetOwnerClass()->ImplementsInterface(UNotifyFieldValueChanged::StaticClass());
}

bool FKismetCompilerUtilities::IsStatementReducible(EKismetCompiledStatementType StatementType)
{
	switch (StatementType)
	{
	case EKismetCompiledStatementType::KCST_Nop:
	case EKismetCompiledStatementType::KCST_UnconditionalGoto:
	case EKismetCompiledStatementType::KCST_ComputedGoto:
	case EKismetCompiledStatementType::KCST_Return:
	case EKismetCompiledStatementType::KCST_EndOfThread:
	case EKismetCompiledStatementType::KCST_Comment:
	case EKismetCompiledStatementType::KCST_DebugSite:
	case EKismetCompiledStatementType::KCST_WireTraceSite:
	case EKismetCompiledStatementType::KCST_GotoReturn:
	case EKismetCompiledStatementType::KCST_AssignmentOnPersistentFrame:
		return true;
	}
	return false;
}

bool FKismetCompilerUtilities::IsMissingMemberPotentiallyLoading(const UBlueprint* SelfBlueprint, const UStruct* MemberOwner)
{
	bool bCouldBeCompiledInOnLoad = false;
	if (SelfBlueprint && SelfBlueprint->bIsRegeneratingOnLoad)
	{
		if (const UClass* OwnerClass = Cast<UClass>(MemberOwner))
		{
			UBlueprint* OwnerBlueprint = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy);
			bCouldBeCompiledInOnLoad = OwnerBlueprint && !OwnerBlueprint->bHasBeenRegenerated;
		}
	}
	return bCouldBeCompiledInOnLoad;
}

bool FKismetCompilerUtilities::IsIntermediateFunctionGraphTrivial(FName FunctionName, const UEdGraph* FunctionGraph)
{
	const auto HasFunctionEntry = [](const UEdGraph* InFunctionGraph ) -> bool
	{
		return InFunctionGraph->Nodes.FindByPredicate(
			[](const UEdGraphNode* Node) { return Cast<UK2Node_FunctionEntry>(Node); }
		) != nullptr;
	};

	const auto HasCallToParent = [](const UEdGraph* InFunctionGraph) -> bool
	{
		return InFunctionGraph->Nodes.FindByPredicate(
			[](const UEdGraphNode* Node) { return Cast<UK2Node_CallParentFunction>(Node); }
		) != nullptr;
	};

	if(FunctionGraph->Nodes.Num() <= 2)
	{
		if(const UBlueprint* OwningBP = FBlueprintEditorUtils::FindBlueprintForGraph(FunctionGraph))
		{
			if(UFunction* Fn = OwningBP->ParentClass->FindFunctionByName(FunctionName))
			{
				// this is an override, we consider this implementation trivial iff it contains
				// an entry node and a call to the parent or it contains only an entry node
				// and the parent is native and the FN is a Blueprint Event:
				if(FunctionGraph->Nodes.Num() == 2)
				{
					return HasFunctionEntry(FunctionGraph) && HasCallToParent(FunctionGraph);
				}
				else if(Fn->HasAnyFunctionFlags(FUNC_BlueprintEvent))
				{
					return FunctionGraph->Nodes.Num() == 1 &&
						HasFunctionEntry(FunctionGraph);
				}
			}
			else
			{
				return FunctionGraph->Nodes.Num() == 1&&
					HasFunctionEntry(FunctionGraph);
			}
		}
	}

	return false;
}

void FKismetCompilerUtilities::UpdateDependentBlueprints(UBlueprint* ForBP)
{
	for(TWeakObjectPtr<UBlueprint> Dependency : ForBP->CachedDependencies)
	{
		if(UBlueprint* BP = Dependency.Get())
		{
			if(BP != ForBP) // avoid tautology
			{
				BP->CachedDependents.Add(ForBP);
			}
		}
	}

	// CachedDependencies may not include all function calls, e.g. because we're 
	// calling a blueprint function library function via a macro (such a dependency
	// cannot be detected until after graph expansion):
	if(UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ForBP->GeneratedClass))
	{
		for(UFunction* Fn : BPGC->CalledFunctions)
		{
			if(UBlueprintGeneratedClass* OwningBPGC = Cast<UBlueprintGeneratedClass>(Fn->GetOwnerClass()))
			{
				if(UBlueprint* OwningBP = Cast<UBlueprint>(OwningBPGC->ClassGeneratedBy))
				{
					if(OwningBP != ForBP) // avoid tautology
					{
						OwningBP->CachedDependents.Add(ForBP);
					}
				}
			}
		}
	}

	// Clear out any stale references to invalid/deleted objects.
	TSet<TWeakObjectPtr<UBlueprint>> InvalidReferences;
	for (const TWeakObjectPtr<UBlueprint>& Reference : ForBP->CachedDependents)
	{
		if (!Reference.IsValid() || Reference.IsStale())
		{
			InvalidReferences.Add(Reference);
		}
	}

	for (const TWeakObjectPtr<UBlueprint>& InvalidReference : InvalidReferences)
	{
		ForBP->CachedDependents.Remove(InvalidReference);
	}
}

bool FKismetCompilerUtilities::CheckFunctionThreadSafety(const FKismetFunctionContext& InContext, FCompilerResultsLog& InMessageLog, bool InbEmitErrors)
{
	bool bIsThreadSafe = true;

	// 1st pass: Build set of 'thread safe' object terms
	TSet<const FBPTerminal*> ThreadSafeObjectTerms;
	
	// Input params to functions (is is assumed that this function is marked thread-safe)
	if(FBlueprintEditorUtils::HasFunctionBlueprintThreadSafeMetaData(InContext.Function))
	{
		for(const FBPTerminal& Parameter : InContext.Parameters)
		{
			if(Parameter.IsObjectContextType() && Parameter.IsLocalVarTerm() && Parameter.AssociatedVarProperty && Parameter.AssociatedVarProperty->IsA<FObjectProperty>())
			{
				ThreadSafeObjectTerms.Add(&Parameter);
			}
		}
	}

	for(const TPair<UEdGraphNode*, TArray<FBlueprintCompiledStatement*>>& StatementPair : InContext.StatementsPerNode)
	{
		for(const FBlueprintCompiledStatement* Statement : StatementPair.Value)
		{
			// Return values from thread-safe functions
			if(Statement->Type == KCST_CallFunction && Statement->FunctionToCall != nullptr)
			{
				if(FBlueprintEditorUtils::HasFunctionBlueprintThreadSafeMetaData(Statement->FunctionToCall))
				{
					if(Statement->LHS)
					{
						ThreadSafeObjectTerms.Add(Statement->LHS);
					}
				}
			}


		}
	}

	// 2nd pass, multiple times: Propagate thread safe terms down the statement lists via supported links
	// @TODO: we can probably reduce the order of this algorithm by keeping a working set of unchecked terms and only checking them each loop
	bool bPropagated = false;
	do
	{
		bPropagated = false;
		
		for(const TPair<UEdGraphNode*, TArray<FBlueprintCompiledStatement*>>& StatementPair : InContext.StatementsPerNode)
		{
			for(const FBlueprintCompiledStatement* Statement : StatementPair.Value)
			{
				switch(Statement->Type)
				{
				case KCST_CastObjToInterface:
				case KCST_DynamicCast:
				case KCST_MetaCast:
				case KCST_CastInterfaceToObj:
					if(Statement->LHS)
					{
						for(const FBPTerminal* RHSTerm : Statement->RHS)
						{
							if(ThreadSafeObjectTerms.Contains(RHSTerm) && !ThreadSafeObjectTerms.Contains(Statement->LHS))
							{
								check(Statement->LHS->AssociatedVarProperty && (Statement->LHS->AssociatedVarProperty->IsA<FObjectProperty>() || Statement->LHS->AssociatedVarProperty->IsA<FInterfaceProperty>()));
								ThreadSafeObjectTerms.Add(Statement->LHS); 
								bPropagated = true;
							}
						}
					}
					break;
				default:
					break;
				}
			}
		}	
	}
	while (bPropagated);

	// 3rd pass: Check statement lists
	for(const TPair<UEdGraphNode*, TArray<FBlueprintCompiledStatement*>>& StatementPair : InContext.StatementsPerNode)
	{
		bIsThreadSafe &= CheckFunctionCompiledStatementsThreadSafety(StatementPair.Key, InContext.SourceGraph, StatementPair.Value, InMessageLog, InbEmitErrors, &ThreadSafeObjectTerms);
	}

	return bIsThreadSafe;
}

// Helper used to emit to log as errors/warnings 
struct FLogThreadSafetyHelper
{
	FLogThreadSafetyHelper(FCompilerResultsLog& InLog, bool bInEmitErrors)
		: Log(InLog)
		, bEmitErrors(bInEmitErrors)
	{}
		
	FCompilerResultsLog& Log;
	bool bEmitErrors;
		
	template<typename... Args>
	void Message(const TCHAR* Format, Args... args)
	{
		if(bEmitErrors)
		{
			Log.Error(Format, args...);
		}
		else
		{
			Log.Warning(Format, args...);
		}
	}
};

#define LOG_THREADSAFETY_HELPER(EmitErrors, CategoryName, Format, ...) \
	if(EmitErrors) \
	{ \
		UE_LOG(CategoryName, Error, Format, ##__VA_ARGS__); \
	} \
	else \
	{ \
		UE_LOG(CategoryName, Warning, Format, ##__VA_ARGS__); \
	}

bool FKismetCompilerUtilities::CheckFunctionCompiledStatementsThreadSafety(const UEdGraphNode* InNode, const UEdGraph* InSourceGraph, const TArray<FBlueprintCompiledStatement*>& InStatements, FCompilerResultsLog& InMessageLog, bool InbEmitErrors, TSet<const FBPTerminal*>* InThreadSafeObjectTerms)
{
	bool bIsThreadSafe = true;

	const FText GenericThreadSafetyErrorOneParam = LOCTEXT("ThreadSafety_Error_Generic", "This is not thread safe when compiled. See the output log for more details.");

	FLogThreadSafetyHelper LogHelper(InMessageLog, InbEmitErrors);
	
	for(const FBlueprintCompiledStatement* Statement : InStatements)
	{
		auto LogDelegateUsage = [&bIsThreadSafe, &LogHelper, InNode, InbEmitErrors]()
		{
			LogHelper.Message(*LOCTEXT("ThreadSafety_Error_Delegate", "@@ Delegate usage is not thread-safe").ToString(), InNode);
			bIsThreadSafe = false;
		};

		auto CheckForInvalidInstancedObjectContext = [&bIsThreadSafe, &LogHelper, InNode, &GenericThreadSafetyErrorOneParam, InbEmitErrors, InThreadSafeObjectTerms](const FBPTerminal* InTerm)
		{
			const FBPTerminal* Context = InTerm;
			while(Context)
			{
				if(Context != nullptr)
				{
					if(InThreadSafeObjectTerms == nullptr || !InThreadSafeObjectTerms->Contains(Context))
					{
						if(Context->IsObjectContextType() && Context->Type.PinSubCategoryObject.IsValid() && (Context->IsInstancedVarTerm() || Context->IsLocalVarTerm()))
						{
							if(Context->SourcePin && Context->SourcePin->GetOwningNode())
							{
								// @TODO: we could possibly make exceptions for 'assets' here
								LogHelper.Message(*LOCTEXT("ThreadSafety_Error_InstancedObjectWithPin", "@@ Accessing an object reference is not thread-safe").ToString(), Context->SourcePin->GetOwningNode());
							}
							else
							{
								LogHelper.Message(*GenericThreadSafetyErrorOneParam.ToString(), InNode);
								LOG_THREADSAFETY_HELPER(InbEmitErrors, LogBlueprint, TEXT("Expression that accesses an instanced object context is not thread-safe"));
							}
							bIsThreadSafe = false;
						}
					}

					Context = Context->Context;
				}
			}
		};

		auto CheckForPrivateMemberUsage = [&bIsThreadSafe, &LogHelper, InNode, &GenericThreadSafetyErrorOneParam, InbEmitErrors](FBPTerminal* InTerm)
		{
			static const FBoolConfigValueHelper ThreadSafetyStrictPrivateMemberChecks(TEXT("Kismet"), TEXT("bThreadSafetyStrictPrivateMemberChecks"), GEngineIni);
			if (ThreadSafetyStrictPrivateMemberChecks)
			{
				const FBPTerminal* Context = InTerm;
				while(Context)
				{
					// Check for assignment only to private object variables
					if(Context->AssociatedVarProperty && !FBlueprintEditorUtils::IsPropertyPrivate(InTerm->AssociatedVarProperty))
					{
						if(Context->Context == nullptr && Context->IsInstancedVarTerm() && Context->IsObjectContextType())
						{
							UEdGraphNode* OwningNode = Context->SourcePin ? Context->SourcePin->GetOwningNode() : nullptr;
							if(OwningNode)
							{
								LogHelper.Message(*LOCTEXT("ThreadSafety_Error_NonPrivateMemberAccess", "@@ Accessing non-private member variables is not thread-safe. Make the variable private or use a local variable.").ToString(), OwningNode);
								UE_LOG(LogBlueprint, Display, TEXT("Expression that accesses non-private property '%s' is not thread-safe. This message can be disabled using bThreadSafetyStrictPrivateMemberChecks in Engine.ini"), *Context->AssociatedVarProperty->GetName())
							}
							else 
							{
								LogHelper.Message(*GenericThreadSafetyErrorOneParam.ToString(), InNode);
								LOG_THREADSAFETY_HELPER(InbEmitErrors, LogBlueprint, TEXT("Expression that accesses non-private property '%s' is not thread-safe. This message can be disabled using bThreadSafetyStrictPrivateMemberChecks in Engine.ini"), *Context->AssociatedVarProperty->GetName());
							}
						}

						bIsThreadSafe = false;
					}

					Context = Context->Context;
				}
			}
		};
		
		switch (Statement->Type)
		{
			case KCST_Nop:
				break;
			case KCST_CallFunction:
			{
				check(Statement->FunctionToCall);

				if(Statement->FunctionContext)
				{
					CheckForInvalidInstancedObjectContext(Statement->FunctionContext);
				}

				// Check RHS (function inputs) for invalid object access 
				for(FBPTerminal* RHSTerm : Statement->RHS)
				{
					if(RHSTerm->Context)
					{
						CheckForInvalidInstancedObjectContext(RHSTerm->Context);
					}

					CheckForPrivateMemberUsage(RHSTerm);
				}

				UFunction* SkeletonClassFunction = FBlueprintEditorUtils::GetMostUpToDateFunction(Statement->FunctionToCall);
				if(SkeletonClassFunction && !FBlueprintEditorUtils::HasFunctionBlueprintThreadSafeMetaData(SkeletonClassFunction))
				{
					// Check LHS (function return value) for invalid object access.
					// Note we only do this for BP functions and those that are not declared thread safe. This is to
					// allow already-useful cases where native code is able to make assumptions about multi-threaded
					// object access.
					// A good example of this is returning a 'hosting' anim instance in the context of a linked anim
					// instance.
					if(Statement->LHS)
					{
						CheckForInvalidInstancedObjectContext(Statement->LHS);
					}
					
					LogHelper.Message(*LOCTEXT("ThreadSafety_Error_NonThreadSafeFunction", "@@ Non-thread safe function @@ called from thread-safe graph @@").ToString(), InNode, Statement->FunctionToCall, InSourceGraph);
					bIsThreadSafe = false;
				}
				break;
			}
			case KCST_Assignment:
			{
				// Check LHS/RHS for invalid object access.
				if(Statement->LHS)
				{
					if(Statement->LHS->Context)
					{
						CheckForInvalidInstancedObjectContext(Statement->LHS->Context);
					}
					
					CheckForPrivateMemberUsage(Statement->LHS);
				}
					
				for(FBPTerminal* RHSTerm : Statement->RHS)
				{
					if(RHSTerm->Context)
					{
						CheckForInvalidInstancedObjectContext(RHSTerm->Context);
					}

					CheckForPrivateMemberUsage(RHSTerm);
				}
				break;
			}
			case KCST_CompileError:
		    case KCST_UnconditionalGoto:
			case KCST_PushState:
			case KCST_GotoIfNot:
			case KCST_Return:
			case KCST_EndOfThread:
			case KCST_Comment:
			case KCST_ComputedGoto:
			case KCST_EndOfThreadIfNot:
			case KCST_DebugSite:
			case KCST_CastObjToInterface:
			case KCST_DynamicCast:
			case KCST_DoubleToFloatCast:
			case KCST_FloatToDoubleCast:
			case KCST_ObjectToBool:
				break;
			case KCST_AddMulticastDelegate:
			case KCST_ClearMulticastDelegate:
				LogDelegateUsage();
				break;
			case KCST_WireTraceSite:
				break;
			case KCST_BindDelegate:
			case KCST_RemoveMulticastDelegate:
			case KCST_CallDelegate:
				LogDelegateUsage();
				break;
			case KCST_CreateArray:
			case KCST_CrossInterfaceCast:
			case KCST_MetaCast:
				break;
			case KCST_AssignmentOnPersistentFrame:
				LogHelper.Message(*GenericThreadSafetyErrorOneParam.ToString(), InSourceGraph);
				LOG_THREADSAFETY_HELPER(InbEmitErrors, LogBlueprint, TEXT("Persistent frame assignment is not supported in thread-safe function"));
				bIsThreadSafe = false;
				break;
			case KCST_CastInterfaceToObj:
			case KCST_GotoReturn:
			case KCST_GotoReturnIfNot:
			case KCST_SwitchValue:
				break;
			case KCST_InstrumentedEvent:
			case KCST_InstrumentedEventStop:
			case KCST_InstrumentedPureNodeEntry:
			case KCST_InstrumentedWireEntry:
			case KCST_InstrumentedWireExit:
			case KCST_InstrumentedStatePush:
			case KCST_InstrumentedStateRestore:
			case KCST_InstrumentedStateReset:
			case KCST_InstrumentedStateSuspend:
			case KCST_InstrumentedStatePop:
			case KCST_InstrumentedTunnelEndOfThread:
				// No way to test instrumentation right now, so this can potentially be removed
				LogHelper.Message(*GenericThreadSafetyErrorOneParam.ToString(), InSourceGraph);
				LOG_THREADSAFETY_HELPER(InbEmitErrors, LogBlueprint, TEXT("Instrumentation not supported in thread-safe function"));
				bIsThreadSafe = false;
				break;
			case KCST_ArrayGetByRef:
			case KCST_CreateSet:
			case KCST_CreateMap:
				break;
			default:
				LogHelper.Message(*GenericThreadSafetyErrorOneParam.ToString(), InSourceGraph);
				LOG_THREADSAFETY_HELPER(InbEmitErrors, LogBlueprint, TEXT("Non-thread safe unknown statement type %d (from %s) used in thread-safe context %s"), (int32)Statement->Type, *InNode->GetName(), *InSourceGraph->GetName());
				bIsThreadSafe = false;
				break;
		}
	}

	return bIsThreadSafe;
}

ConvertibleSignatureMatchResult FKismetCompilerUtilities::DoSignaturesHaveConvertibleFloatTypes(const UFunction* A, const UFunction* B)
{
	check(A);
	check(B);

	if (!A->IsSignatureCompatibleWith(B))
	{
		TFieldIterator<FProperty> PropAIt(A);
		TFieldIterator<FProperty> PropBIt(B);

		while (PropAIt)
		{
			if (PropBIt)
			{
				if (!FStructUtils::ArePropertiesTheSame(*PropAIt, *PropBIt, false))
				{
					bool bHasConvertibleProperties =
						(PropAIt->IsA<FFloatProperty>() && PropBIt->IsA<FDoubleProperty>()) ||
						(PropAIt->IsA<FDoubleProperty>() && PropBIt->IsA<FFloatProperty>());

					if (!bHasConvertibleProperties)
					{
						return ConvertibleSignatureMatchResult::Different;
					}
				}
			}
			else
			{
				// Mismatched parameter count
				return ConvertibleSignatureMatchResult::Different;
			}

			++PropAIt;
			++PropBIt;
		}

		// If PropBIt still has parameters, then there was a mismatch
		return PropBIt ? ConvertibleSignatureMatchResult::Different : ConvertibleSignatureMatchResult::HasConvertibleFloatParams;
	}

	return ConvertibleSignatureMatchResult::ExactMatch;
}

//////////////////////////////////////////////////////////////////////////
// FNodeHandlingFunctor

void FNodeHandlingFunctor::ResolveAndRegisterScopedTerm(FKismetFunctionContext& Context, UEdGraphPin* Net, TIndirectArray<FBPTerminal>& NetArray)
{
	// Determine the scope this takes place in
	UStruct* SearchScope = Context.Function;

	UEdGraphPin* SelfPin = CompilerContext.GetSchema()->FindSelfPin(*(Net->GetOwningNode()), EGPD_Input);
	if (SelfPin != NULL)
	{
		SearchScope = Context.GetScopeFromPinType(SelfPin->PinType, Context.NewClass);
	}

	// Find the variable in the search scope
	bool bIsSparseProperty;
	FProperty* BoundProperty = FKismetCompilerUtilities::FindPropertyInScope(SearchScope, Net, CompilerContext.MessageLog, CompilerContext.GetSchema(), Context.NewClass, bIsSparseProperty);
	if (BoundProperty != NULL)
	{
		// Create the term in the list
		FBPTerminal* Term = new FBPTerminal();
		NetArray.Add(Term);
		Term->CopyFromPin(Net, Net->PinName);
		Term->AssociatedVarProperty = BoundProperty;
		Term->bPassedByReference = true;
		Context.NetMap.Add(Net, Term);

		// Check if the property is a local variable and mark it so
		if( SearchScope == Context.Function && BoundProperty->GetOwner<UFunction>() == Context.Function)
		{
			Term->SetVarTypeLocal(true);
		}
		else if (BoundProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly) || (Context.IsConstFunction() && Context.NewClass && Context.NewClass->IsChildOf(SearchScope)))
		{
			// Read-only variables and variables in const classes are both const
			Term->bIsConst = true;
		}

		if (bIsSparseProperty)
		{
			Term->SetVarTypeSparseClassData();
		}

		// Resolve the context term
		if (SelfPin != NULL)
		{
			FBPTerminal** pContextTerm = Context.NetMap.Find(FEdGraphUtilities::GetNetFromPin(SelfPin));
			Term->Context = (pContextTerm != NULL) ? *pContextTerm : NULL;
		}
	}
}

FBlueprintCompiledStatement& FNodeHandlingFunctor::GenerateSimpleThenGoto(FKismetFunctionContext& Context, UEdGraphNode& Node, UEdGraphPin* ThenExecPin)
{
	UEdGraphNode* TargetNode = NULL;
	if ((ThenExecPin != NULL) && (ThenExecPin->LinkedTo.Num() > 0))
	{
		TargetNode = ThenExecPin->LinkedTo[0]->GetOwningNode();
	}

	if (Context.bCreateDebugData)
	{
		FBlueprintCompiledStatement& TraceStatement = Context.AppendStatementForNode(&Node);
		TraceStatement.Type = Context.GetWireTraceType();
		TraceStatement.Comment = Node.NodeComment.IsEmpty() ? Node.GetName() : Node.NodeComment;
	}

	FBlueprintCompiledStatement& GotoStatement = Context.AppendStatementForNode(&Node);
	GotoStatement.Type = KCST_UnconditionalGoto;
	Context.GotoFixupRequestMap.Add(&GotoStatement, ThenExecPin);

	return GotoStatement;
}

FBlueprintCompiledStatement& FNodeHandlingFunctor::GenerateSimpleThenGoto(FKismetFunctionContext& Context, UEdGraphNode& Node)
{
	UEdGraphPin* ThenExecPin = CompilerContext.GetSchema()->FindExecutionPin(Node, EGPD_Output);
	return GenerateSimpleThenGoto(Context, Node, ThenExecPin);
}

bool FNodeHandlingFunctor::ValidateAndRegisterNetIfLiteral(FKismetFunctionContext& Context, UEdGraphPin* Net)
{
	if (Net->LinkedTo.Num() == 0)
	{
		// Make sure the default value is valid
		FString DefaultAllowedResult = CompilerContext.GetSchema()->IsCurrentPinDefaultValid(Net);
		if (DefaultAllowedResult != TEXT(""))
		{
			CompilerContext.MessageLog.Error(
				*FText::Format(
					LOCTEXT("InvalidDefaultValue_ErrorFmt", "Default value '{0}' for @@ is invalid: '{1}'"),
					Net->GetDefaultAsText(),
					FText::FromString(DefaultAllowedResult)
				).ToString(),
				Net
			);
			return false;
		}

		FBPTerminal* LiteralTerm = Context.RegisterLiteral(Net);
		Context.LiteralHackMap.Add(Net, LiteralTerm);
	}

	return true;
}

void FNodeHandlingFunctor::SanitizeName(FString& Name)
{
	// Sanitize the name
	for (int32 i = 0; i < Name.Len(); ++i)
	{
		TCHAR& C = Name[i];

		const bool bGoodChar =
			((C >= 'A') && (C <= 'Z')) || ((C >= 'a') && (C <= 'z')) ||		// A-Z (upper and lowercase) anytime
			(C == '_') ||													// _ anytime
			((i > 0) && (C >= '0') && (C <= '9'));							// 0-9 after the first character

		if (!bGoodChar)
		{
			C = '_';
		}
	}
}

FBPTerminal* FNodeHandlingFunctor::RegisterLiteral(FKismetFunctionContext& Context, UEdGraphPin* Net)
{
	FBPTerminal* Term = nullptr;
	// Make sure the default value is valid
	FString DefaultAllowedResult = CompilerContext.GetSchema()->IsCurrentPinDefaultValid(Net);
	if (!DefaultAllowedResult.IsEmpty())
	{
		FText ErrorFormat = LOCTEXT("InvalidDefault_Error", "The current value of the '@@' pin is invalid: {0}");
		const FText InvalidReasonText = FText::FromString(DefaultAllowedResult);

		FText DefaultValue = FText::FromString(Net->GetDefaultAsString());
		if (!DefaultValue.IsEmpty())
		{
			ErrorFormat = LOCTEXT("InvalidDefaultVal_Error", "The current value ({1}) of the '@@' pin is invalid: {0}");
		}

		FString ErrorString = FText::Format(ErrorFormat, InvalidReasonText, DefaultValue).ToString();
		CompilerContext.MessageLog.Error(*ErrorString, Net);

		// Skip over these properties if they are container or ref properties, because the backend can't emit valid code for them
		if (Net->PinType.IsContainer() || Net->PinType.bIsReference)
		{
			return nullptr;
		}
	}

	Term = Context.RegisterLiteral(Net);
	Context.NetMap.Add(Net, Term);

	return Term;
}

void FNodeHandlingFunctor::RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin->bOrphanedPin)
		{
			if (Pin->bNotConnectable && Pin->LinkedTo.Num() > 0)
			{
				// If it is not connectible due to being orphaned no need to warn as we have other messaging for that
				CompilerContext.MessageLog.Warning(*LOCTEXT("NotConnectablePinLinked", "@@ is linked to another pin but is marked as not connectable. This pin connection will not be compiled.").ToString(), Pin);
			}
			else if (!CompilerContext.GetSchema()->IsMetaPin(*Pin)
				|| (Pin->LinkedTo.Num() == 0 && Pin->DefaultObject && CompilerContext.GetSchema()->IsSelfPin(*Pin) ))
			{
				UEdGraphPin* Net = FEdGraphUtilities::GetNetFromPin(Pin);

				if (Context.NetMap.Find(Net) == nullptr)
				{
					if ((Net->Direction == EGPD_Input) && (Net->LinkedTo.Num() == 0))
					{
						RegisterLiteral(Context, Net);
					}
					else
					{
						RegisterNet(Context, Pin);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FNetNameMapping
FString FNetNameMapping::MakeBaseName(const UEdGraphPin* Net)
{
	UEdGraphNode* Owner = Net->GetOwningNode();
	FString Part1 = Owner->GetDescriptiveCompiledName();

	return FString::Printf(TEXT("%s_%s"), *Part1, *Net->PinName.ToString());
}

FString FNetNameMapping::MakeBaseName(const UEdGraphNode* Net)
{
	return FString::Printf(TEXT("%s"), *Net->GetDescriptiveCompiledName());
}

FString FNetNameMapping::MakeBaseName(const UObject* Net)
{
	return FString::Printf(TEXT("%s"), *Net->GetFName().GetPlainNameString());
}

//////////////////////////////////////////////////////////////////////////
// FKismetFunctionContext

FKismetFunctionContext::FKismetFunctionContext(FCompilerResultsLog& InMessageLog, const UEdGraphSchema_K2* InSchema, UBlueprintGeneratedClass* InNewClass, UBlueprint* InBlueprint)
	: Blueprint(InBlueprint)
	, SourceGraph(nullptr)
	, EntryPoint(nullptr)
	, Function(nullptr)
	, NewClass(InNewClass)
	, LastFunctionPropertyStorageLocation(nullptr)
	, MessageLog(InMessageLog)
	, Schema(InSchema)
	, bIsUbergraph(false)
	, bCannotBeCalledFromOtherKismet(false)
	, bIsInterfaceStub(false)
	, bIsConstFunction(false)
	, bEnforceConstCorrectness(false)
	// only need debug-data when running in the editor app:
	, bCreateDebugData(GIsEditor && !IsRunningCommandlet())
	, bIsSimpleStubGraphWithNoParams(false)
	, NetFlags(0)
	, SourceEventFromStubGraph(nullptr)
	, bUseFlowStack(true)
{
	NetNameMap = new FNetNameMapping();
	bAllocatedNetNameMap = true;

	// Prevent debug generation when cooking or running other commandlets
	// Compile-on-load will recreate it if the editor is run
	if (IsRunningCommandlet())
	{
		bCreateDebugData = false;
	}
}

FKismetFunctionContext::~FKismetFunctionContext()
{
	if (bAllocatedNetNameMap)
	{
		delete NetNameMap;
		NetNameMap = nullptr;
	}

	for (int32 i = 0; i < AllGeneratedStatements.Num(); ++i)
	{
		delete AllGeneratedStatements[i];
	}
}

void FKismetFunctionContext::SetExternalNetNameMap(FNetNameMapping* NewMap)
{
	if (bAllocatedNetNameMap)
	{
		delete NetNameMap;
		NetNameMap = NULL;
	}

	bAllocatedNetNameMap = false;

	NetNameMap = NewMap;
}

void FKismetFunctionContext::MergeAdjacentStates()
{
	for (int32 ExecIndex = 0; ExecIndex < LinearExecutionList.Num(); ++ExecIndex)
	{
		// if the last statement in current node jumps to the first statement in next node, then it's redundant
		const UEdGraphNode* CurrentNode = LinearExecutionList[ExecIndex];
		TArray<FBlueprintCompiledStatement*>* CurStatementList = StatementsPerNode.Find(CurrentNode);
		const bool CurrentNodeIsValid = CurrentNode && CurStatementList && CurStatementList->Num();
		const FBlueprintCompiledStatement* LastStatementInCurrentNode = CurrentNodeIsValid ? CurStatementList->Last() : nullptr;

		if (LastStatementInCurrentNode
			&& LastStatementInCurrentNode->TargetLabel
			&& (LastStatementInCurrentNode->Type == KCST_UnconditionalGoto)
			&& !LastStatementInCurrentNode->bIsJumpTarget)
		{
			const int32 NextNodeIndex = ExecIndex + 1;
			const UEdGraphNode* NextNode = LinearExecutionList.IsValidIndex(NextNodeIndex) ? LinearExecutionList[NextNodeIndex] : nullptr;
			const TArray<FBlueprintCompiledStatement*>* NextNodeStatements = StatementsPerNode.Find(NextNode);
			const bool bNextNodeValid = NextNode && NextNodeStatements && NextNodeStatements->Num();
			const FBlueprintCompiledStatement* FirstStatementInNextNode = bNextNodeValid ? (*NextNodeStatements)[0] : nullptr;
			if (FirstStatementInNextNode == LastStatementInCurrentNode->TargetLabel)
			{
				CurStatementList->RemoveAt(CurStatementList->Num() - 1);
			}
		}
	}

	// Remove unnecessary GotoReturn statements
	// if it's last statement generated by last node (in LinearExecution) then it can be removed
	const UEdGraphNode* LastExecutedNode = LinearExecutionList.Num() ? LinearExecutionList.Last() : nullptr;
	TArray<FBlueprintCompiledStatement*>* StatementList = StatementsPerNode.Find(LastExecutedNode);
	FBlueprintCompiledStatement* LastStatementInLastNode = (StatementList && StatementList->Num()) ? StatementList->Last() : nullptr;
	if (LastStatementInLastNode && (KCST_GotoReturn == LastStatementInLastNode->Type) && !LastStatementInLastNode->bIsJumpTarget)
	{
		StatementList->RemoveAt(StatementList->Num() - 1);
	}
}

struct FGotoMapUtils
{
	static bool IsUberGraphEventStatement(const FBlueprintCompiledStatement* GotoStatement)
	{
		// Note: Latent function call sites also utilize the UbergraphCallIndex field, so we need to separate them from ubergraph event targets when the latent term is at index 0.
		return GotoStatement
			&& (GotoStatement->Type == KCST_CallFunction) 
			&& (GotoStatement->UbergraphCallIndex == 0)
			&& (GotoStatement->FunctionToCall && !GotoStatement->FunctionToCall->HasMetaData(FBlueprintMetadata::MD_Latent));
	}

	static UEdGraphNode* TargetNodeFromPin(const FBlueprintCompiledStatement* GotoStatement, const UEdGraphPin* ExecNet)
	{
		UEdGraphNode* TargetNode = NULL;
		if (ExecNet && GotoStatement)
		{
			if (IsUberGraphEventStatement(GotoStatement))
			{
				TargetNode = ExecNet->GetOwningNode();
			}
			else if (ExecNet->LinkedTo.Num() > 0)
			{
				TargetNode = ExecNet->LinkedTo[0]->GetOwningNode();
			}
		}
		return TargetNode;
	}

	static UEdGraphNode* TargetNodeFromMap(const FBlueprintCompiledStatement* GotoStatement, const TMap< FBlueprintCompiledStatement*, UEdGraphPin* >& GotoFixupRequestMap)
	{
		UEdGraphPin* const * ExecNetPtr = GotoFixupRequestMap.Find(GotoStatement);
		UEdGraphPin* ExecNet = ExecNetPtr ? *ExecNetPtr : nullptr;
		return TargetNodeFromPin(GotoStatement, ExecNet);
	}
};

void FKismetFunctionContext::ResolveGotoFixups()
{
	if (bCreateDebugData)
	{
		// if we're debugging, go through an insert a wire trace before  
		// every "goto" statement so we can trace what execution pin a node
		// was executed from
		for (auto GotoIt = GotoFixupRequestMap.CreateIterator(); GotoIt; ++GotoIt)
		{
			FBlueprintCompiledStatement* GotoStatement = GotoIt.Key();
			if (FGotoMapUtils::IsUberGraphEventStatement(GotoStatement))
			{
				continue;
			}

			InsertWireTrace(GotoIt.Key(), GotoIt.Value());
		}
	}

	// Resolve the remaining fixups
	for (auto GotoIt = GotoFixupRequestMap.CreateIterator(); GotoIt; ++GotoIt)
	{
		FBlueprintCompiledStatement* GotoStatement = GotoIt.Key();
		const UEdGraphPin* ExecNet = GotoIt.Value();
		const UEdGraphNode* TargetNode = FGotoMapUtils::TargetNodeFromPin(GotoStatement, ExecNet);

		if (TargetNode == NULL)
		{
			// If Execution Flow Stack isn't necessary, then use GotoReturn instead EndOfThread.
			// EndOfThread pops Execution Flow Stack, GotoReturn dosen't.
			GotoStatement->Type = bUseFlowStack
				? ((GotoStatement->Type == KCST_GotoIfNot) ? KCST_EndOfThreadIfNot : KCST_EndOfThread)
				: ((GotoStatement->Type == KCST_GotoIfNot) ? KCST_GotoReturnIfNot : KCST_GotoReturn);
		}
		else
		{
			// Try to resolve the goto
			TArray<FBlueprintCompiledStatement*>* StatementList = StatementsPerNode.Find(TargetNode);

			if ((StatementList == NULL) || (StatementList->Num() == 0))
			{
				MessageLog.Error(TEXT("Statement tried to pass control flow to a node @@ that generates no code"), TargetNode);
				GotoStatement->Type = KCST_CompileError;
			}
			else
			{
				// Wire up the jump target and notify the target that it is targeted
				FBlueprintCompiledStatement& FirstStatement = *((*StatementList)[0]);
				GotoStatement->TargetLabel = &FirstStatement;
				FirstStatement.bIsJumpTarget = true;
			}
		}
	}

	// Clear out the pending fixup map
	GotoFixupRequestMap.Empty();

	//@TODO: Remove any wire debug sites where the next statement is a stack pop
}

void FKismetFunctionContext::FinalSortLinearExecList()
{
	const UEdGraphSchema_K2* K2Schema = Schema;
	LinearExecutionList.RemoveAllSwap([&](UEdGraphNode* CurrentNode)
	{
		TArray<FBlueprintCompiledStatement*>* CurStatementList = StatementsPerNode.Find(CurrentNode);
		return !(CurrentNode && CurStatementList && CurStatementList->Num());
	});

	TSet<UEdGraphNode*> UnsortedExecutionSet(LinearExecutionList);
	LinearExecutionList.Empty();
	TArray<UEdGraphNode*> SortedLinearExecutionList;

	check(EntryPoint);
	SortedLinearExecutionList.Push(EntryPoint);
	UnsortedExecutionSet.Remove(EntryPoint);

	TSet<UEdGraphNode*> NodesToStartNextChain;

	while (UnsortedExecutionSet.Num())
	{
		UEdGraphNode* NextNode = nullptr;

		// get last state target
		const UEdGraphNode* CurrentNode = SortedLinearExecutionList.Last();
		const TArray<FBlueprintCompiledStatement*>* CurStatementList = StatementsPerNode.Find(CurrentNode);
		const bool CurrentNodeIsValid = CurrentNode && CurStatementList && CurStatementList->Num();
		const FBlueprintCompiledStatement* LastStatementInCurrentNode = CurrentNodeIsValid ? CurStatementList->Last() : nullptr;

		// Find next element in current chain
		if (LastStatementInCurrentNode && (LastStatementInCurrentNode->Type == KCST_UnconditionalGoto))
		{
			UEdGraphNode* TargetNode = FGotoMapUtils::TargetNodeFromMap(LastStatementInCurrentNode, GotoFixupRequestMap);
			NextNode = UnsortedExecutionSet.Remove(TargetNode) ? TargetNode : nullptr;
		}

		if (CurrentNode)
		{
			for (UEdGraphPin* Pin : CurrentNode->Pins)
			{
				if (Pin && (EEdGraphPinDirection::EGPD_Output == Pin->Direction) && K2Schema->IsExecPin(*Pin) && Pin->LinkedTo.Num())
				{
					for (UEdGraphPin* Link : Pin->LinkedTo)
					{
						UEdGraphNode* LinkedNode = Link->GetOwningNodeUnchecked();
						if (LinkedNode && (LinkedNode != NextNode) && UnsortedExecutionSet.Contains(LinkedNode))
						{
							NodesToStartNextChain.Add(LinkedNode);
						}
					}
				}
			}
		}

		// Start next chain if the current is done
		while (NodesToStartNextChain.Num() && !NextNode)
		{
			auto Iter = NodesToStartNextChain.CreateIterator();
			NextNode = UnsortedExecutionSet.Remove(*Iter) ? *Iter : NULL;
			Iter.RemoveCurrent();
		}

		if (!NextNode)
		{
			auto Iter = UnsortedExecutionSet.CreateIterator();
			NextNode = *Iter;
			Iter.RemoveCurrent();
		}

		check(NextNode);
		SortedLinearExecutionList.Push(NextNode);
	}

	LinearExecutionList = SortedLinearExecutionList;
}

bool FKismetFunctionContext::DoesStatementRequiresFlowStack(const FBlueprintCompiledStatement* Statement)
{
	return Statement && (
		(Statement->Type == KCST_EndOfThreadIfNot) ||
		(Statement->Type == KCST_EndOfThread) ||
		(Statement->Type == KCST_PushState));
}

void FKismetFunctionContext::ResolveStatements()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ResolveCompiledStatements);
	FinalSortLinearExecList();

	static const FBoolConfigValueHelper OptimizeExecutionFlowStack(TEXT("Kismet"), TEXT("bOptimizeExecutionFlowStack"), GEngineIni);
	if (OptimizeExecutionFlowStack)
	{
		bUseFlowStack = AllGeneratedStatements.ContainsByPredicate(&FKismetFunctionContext::DoesStatementRequiresFlowStack);
	}

	ResolveGotoFixups();

	static const FBoolConfigValueHelper OptimizeAdjacentStates(TEXT("Kismet"), TEXT("bOptimizeAdjacentStates"), GEngineIni);
	if (OptimizeAdjacentStates)
	{
		MergeAdjacentStates();
	}
}

struct FEventGraphUtils
{
	static bool IsEntryPointNode(const UK2Node* Node)
	{
		bool bResult = false;
		if (Node)
		{
			bResult |= Node->IsA<UK2Node_Event>();
			bResult |= Node->IsA<UK2Node_Timeline>();
			if (const UK2Node_CallFunction* CallNode = Cast<const UK2Node_CallFunction>(Node))
			{
				bResult |= CallNode->IsLatentFunction();
			}
		}
		return bResult;
	}

	static void FindEventsCallingTheNodeRecursive(const UK2Node* Node, TSet<const UK2Node*>& Results, TSet<const UK2Node*>& CheckedNodes, const UK2Node* StopOn)
	{
		if (!Node)
		{
			return;
		}

		bool bAlreadyTraversed = false;
		CheckedNodes.Add(Node, &bAlreadyTraversed);
		if (bAlreadyTraversed)
		{
			return;
		}

		if (Node == StopOn)
		{
			return;
		}

		if (IsEntryPointNode(Node))
		{
			Results.Add(Node);
			return;
		}

		const UEdGraphSchema_K2* Schema = CastChecked<const UEdGraphSchema_K2>(Node->GetSchema());
		const bool bIsPure = Node->IsNodePure();
		for (UEdGraphPin* Pin : Node->Pins)
		{
			const bool bProperPure		= bIsPure	&& Pin && (Pin->Direction == EEdGraphPinDirection::EGPD_Output);
			const bool bProperNotPure	= !bIsPure	&& Pin && (Pin->Direction == EEdGraphPinDirection::EGPD_Input) && Schema->IsExecPin(*Pin);
			if (bProperPure || bProperNotPure)
			{
				for (UEdGraphPin* Link : Pin->LinkedTo)
				{
					UEdGraphNode* LinkOwner = Link ? Link->GetOwningNodeUnchecked() : nullptr;
					const UK2Node* NodeToCheck = LinkOwner ? CastChecked<const UK2Node>(LinkOwner) : nullptr;
					FindEventsCallingTheNodeRecursive(NodeToCheck, Results, CheckedNodes, StopOn);
				}
			}
		}
	}

	static TSet<const UK2Node*> FindExecutionNodes(const UK2Node* Node, const UK2Node* StopOn)
	{
		TSet<const UK2Node*> Results;
		TSet<const UK2Node*> CheckedNodes;
		FindEventsCallingTheNodeRecursive(Node, Results, CheckedNodes, StopOn);
		return Results;
	}

	static bool PinRepresentsSharedTerminal(const UEdGraphPin& Net, FCompilerResultsLog& MessageLog)
	{
		// TODO: Strange cases..
		if ((Net.Direction != EEdGraphPinDirection::EGPD_Output)
			|| Net.PinType.IsContainer()
			|| Net.PinType.bIsReference
			|| Net.PinType.bIsConst
			|| Net.SubPins.Num())
		{
			return true;
		}

		// Local term must be created by return value. 
		// If the term is from output- by-reference parameter, then it must be persistent between calls.
		// Fix for UE - 23629
		const UK2Node* OwnerNode = Cast<const UK2Node>(Net.GetOwningNodeUnchecked());
		ensure(OwnerNode);
		const UK2Node_CallFunction* CallFunction = Cast<const UK2Node_CallFunction>(OwnerNode);
		if (!CallFunction || (&Net != CallFunction->GetReturnValuePin()))
		{
			return true;
		}

		// If the function call node is an intermediate node resulting from expansion of an async task node, then the return value term must also be persistent.
		const UObject* SourceNode = MessageLog.FindSourceObject(OwnerNode);
		if (SourceNode && SourceNode->IsA<UK2Node_BaseAsyncTask>())
		{
			return true;
		}

		// NOT CONNECTED, so it doesn't have to be shared
		if (!Net.LinkedTo.Num())
		{
			return false;
		}

		// Terminals from pure nodes will be recreated anyway, so they can be always local
		if (OwnerNode && OwnerNode->IsNodePure())
		{
			return false;
		}

		// 
		if (IsEntryPointNode(OwnerNode))
		{
			return true;
		}

		// 
		TSet<const UK2Node*> SourceEntryPoints = FEventGraphUtils::FindExecutionNodes(OwnerNode, nullptr);
		if (1 != SourceEntryPoints.Num())
		{
			return true;
		}

		//
		for (UEdGraphPin* Link : Net.LinkedTo)
		{
			const UK2Node* LinkOwnerNode = Cast<const UK2Node>(Link->GetOwningNodeUnchecked());
			ensure(LinkOwnerNode);
			if (Link->PinType.bIsReference)
			{
				return true;
			}
			TSet<const UK2Node*> EventsCallingDestination = FEventGraphUtils::FindExecutionNodes(LinkOwnerNode, OwnerNode);
			if (0 != EventsCallingDestination.Num())
			{
				return true;
			}
		}
		return false;
	}
};

FBPTerminal* FKismetFunctionContext::CreateLocalTerminal(ETerminalSpecification Spec)
{
	FBPTerminal* Result = NULL;
	switch (Spec)
	{
	case ETerminalSpecification::TS_ForcedShared:
		ensure(IsEventGraph());
		Result = new FBPTerminal();
		EventGraphLocals.Add(Result);
		break;
	case ETerminalSpecification::TS_Literal:
		Result = new FBPTerminal();
		Literals.Add(Result);
		Result->bIsLiteral = true;
		break;
	default:
		const bool bIsLocal = !IsEventGraph();
		Result = new FBPTerminal();
		if (bIsLocal)
		{
			Locals.Add(Result);
		}
		else
		{
			EventGraphLocals.Add(Result);
		}
		Result->SetVarTypeLocal(bIsLocal);
		break;
	}
	return Result;
}

FBPTerminal* FKismetFunctionContext::CreateLocalTerminalFromPinAutoChooseScope(UEdGraphPin* Net, FString NewName)
{
	check(Net);
	bool bSharedTerm = IsEventGraph();
	static FBoolConfigValueHelper UseLocalGraphVariables(TEXT("Kismet"), TEXT("bUseLocalGraphVariables"), GEngineIni);

	const bool bUseLocalGraphVariables = UseLocalGraphVariables;

	const bool OutputPin = EEdGraphPinDirection::EGPD_Output == Net->Direction;
	if (bSharedTerm && bUseLocalGraphVariables && OutputPin)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ChooseTerminalScope);

		// Pin's connections are checked, to tell if created terminal is shared, or if it could be a local variable.
		bSharedTerm = FEventGraphUtils::PinRepresentsSharedTerminal(*Net, MessageLog);
	}
	FBPTerminal* Term = new FBPTerminal();
	if (bSharedTerm)
	{
		EventGraphLocals.Add(Term);
	}
	else
	{
		Locals.Add(Term);
	}
	Term->CopyFromPin(Net, MoveTemp(NewName));
	return Term;
}

#undef LOCTEXT_NAMESPACE

//////////////////////////////////////////////////////////////////////////
// FBPTerminal

void FBPTerminal::CopyFromPin(UEdGraphPin* Net, FString NewName)
{
	Type = Net->PinType;
	SourcePin = Net;
	Name = MoveTemp(NewName);

	bPassedByReference = Net->PinType.bIsReference;

	const bool bStructCategory = (UEdGraphSchema_K2::PC_Struct == Net->PinType.PinCategory);
	const bool bStructSubCategoryObj = (nullptr != Cast<UScriptStruct>(Net->PinType.PinSubCategoryObject.Get()));
	SetContextTypeStruct(bStructCategory && bStructSubCategoryObj);
}


TArray<TSet<UEdGraphNode*>> FKismetCompilerUtilities::FindUnsortedSeparateExecutionGroups(const TArray<UEdGraphNode*>& Nodes)
{
	TArray<UEdGraphNode*> UnprocessedNodes;
	for (UEdGraphNode* Node : Nodes)
	{
		UK2Node* K2Node = Cast<UK2Node>(Node);
		if (K2Node && !K2Node->IsNodePure())
		{
			UnprocessedNodes.Add(Node);
		}
	}

	TSet<UEdGraphNode*> AlreadyProcessed;
	TArray<TSet<UEdGraphNode*>> Result;
	while (UnprocessedNodes.Num())
	{
		Result.Emplace(TSet<UEdGraphNode*>());
		TSet<UEdGraphNode*>& ExecutionGroup = Result.Last();
		TSet<UEdGraphNode*> ToProcess;

		UEdGraphNode* Seed = UnprocessedNodes.Pop();
		ensure(!AlreadyProcessed.Contains(Seed));
		ToProcess.Add(Seed);
		ExecutionGroup.Add(Seed);
		while (ToProcess.Num())
		{
			UEdGraphNode* Node = *ToProcess.CreateIterator();
			// for each execution pin
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->LinkedTo.Num() && (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec))
				{
					for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
					{
						if (!LinkedPin)
						{
							continue;
						}
						UEdGraphNode* LinkedNode = LinkedPin->GetOwningNodeUnchecked();
						const bool bIsAlreadyProcessed = AlreadyProcessed.Contains(LinkedNode);
						const bool bInCurrentExecutionGroup = ExecutionGroup.Contains(LinkedNode);
						ensure(!bIsAlreadyProcessed || bInCurrentExecutionGroup);
						ensure(bInCurrentExecutionGroup || UnprocessedNodes.Contains(LinkedNode));
						if (!bIsAlreadyProcessed)
						{
							ToProcess.Add(LinkedNode);
							ExecutionGroup.Add(LinkedNode);
							UnprocessedNodes.Remove(LinkedNode);
						}
					}
				}
			}

			const int32 WasRemovedFromProcess = ToProcess.Remove(Node);
			ensure(0 != WasRemovedFromProcess);
			bool bAlreadyProcessed = false;
			AlreadyProcessed.Add(Node, &bAlreadyProcessed);
			ensure(!bAlreadyProcessed);
		}

		if (1 == ExecutionGroup.Num())
		{
			UEdGraphNode* TheOnlyNode = *ExecutionGroup.CreateIterator();
			if (!TheOnlyNode || TheOnlyNode->IsA<UK2Node_FunctionEntry>() || TheOnlyNode->IsA<UK2Node_Timeline>())
			{
				Result.Pop();
			}
		}
	}



	return Result;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectEditorUtils.h"
#include "Internationalization/TextKey.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "EditorCategoryUtils.h"

namespace FObjectEditorUtils
{

	FText GetCategoryText( const FField* InField )
	{
		static const FTextKey CategoryLocalizationNamespace = TEXT("UObjectCategory");
		static const FName CategoryMetaDataKey = TEXT("Category");

		if (InField)
		{
			const FString& NativeCategory = InField->GetMetaData(CategoryMetaDataKey);
			if (!NativeCategory.IsEmpty())
			{
				FText LocalizedCategory;
				if (!FText::FindText(CategoryLocalizationNamespace, NativeCategory, /*OUT*/LocalizedCategory, &NativeCategory))
				{
					LocalizedCategory = FText::AsCultureInvariant(NativeCategory);
				}
				return LocalizedCategory;
			}
		}

		return FText::GetEmpty();
	}

	FText GetCategoryText( const UField* InField )
	{
		static const FTextKey CategoryLocalizationNamespace = TEXT("UObjectCategory");
		static const FName CategoryMetaDataKey = TEXT("Category");

		if (InField)
		{
			const FString& NativeCategory = InField->GetMetaData(CategoryMetaDataKey);
			if (!NativeCategory.IsEmpty())
			{
				FText LocalizedCategory;
				if (!FText::FindText(CategoryLocalizationNamespace, NativeCategory, /*OUT*/LocalizedCategory, &NativeCategory))
				{
					LocalizedCategory = FText::AsCultureInvariant(NativeCategory);
				}
				return LocalizedCategory;
			}
		}

		return FText::GetEmpty();
	}

	FString GetCategory( const FField* InField )
	{
		return GetCategoryText(InField).ToString();
	}

	FString GetCategory( const UField* InField )
	{
		return GetCategoryText(InField).ToString();
	}

	FName GetCategoryFName( const FField* InField )
	{
		static const FName CategoryKey(TEXT("Category"));

		const FField* CurrentField = InField;
		FString CategoryString;
		while (CurrentField != nullptr && CategoryString.IsEmpty())
		{
			CategoryString = CurrentField->GetMetaData(CategoryKey);
			CurrentField = CurrentField->GetOwner<FField>();
		} 

		if (!CategoryString.IsEmpty())
		{
			return FName(*CategoryString);
		}

		return NAME_None;
	}

	FName GetCategoryFName( const UField* InField )
	{
		static const FName CategoryKey(TEXT("Category"));
		if (InField && InField->HasMetaData(CategoryKey))
		{
			return FName(*InField->GetMetaData(CategoryKey));
		}
		return NAME_None;
	}

	bool IsFunctionHiddenFromClass( const UFunction* InFunction,const UClass* Class )
	{
		bool bResult = false;
		if( InFunction )
		{
			bResult = Class->IsFunctionHidden( *InFunction->GetName() );

			static const FName FunctionCategory(TEXT("Category")); // FBlueprintMetadata::MD_FunctionCategory
			if( !bResult && InFunction->HasMetaData( FunctionCategory ) )
			{
				FString const& FuncCategory = InFunction->GetMetaData(FunctionCategory);
				bResult = FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, FuncCategory);
			}
		}
		return bResult;
	}

	bool IsVariableCategoryHiddenFromClass( const FProperty* InVariable,const UClass* Class )
	{
		bool bResult = false;
		if( InVariable && Class )
		{
			bResult = FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, GetCategory(InVariable));
		}
		return bResult;
	}

	void GetClassDevelopmentStatus(UClass* Class, bool& bIsExperimental, bool& bIsEarlyAccess, FString& MostDerivedClassName)
	{
		static const FName DevelopmentStatusKey(TEXT("DevelopmentStatus"));
		static const FString EarlyAccessValue(TEXT("EarlyAccess"));
		static const FString ExperimentalValue(TEXT("Experimental"));

		MostDerivedClassName = FString();
		bIsExperimental = bIsEarlyAccess = false;

		// Determine the development status and introducing class
		FString DevelopmentStatus;
		for (const UStruct* TestStruct = Class; TestStruct != nullptr; TestStruct = TestStruct->GetSuperStruct())
		{
			if (TestStruct->HasMetaData(DevelopmentStatusKey))
			{
				DevelopmentStatus = TestStruct->GetMetaData(DevelopmentStatusKey);
				MostDerivedClassName = TestStruct->GetName();
				break;
			}
		}

		// Update the flags based on the status
		bIsExperimental = DevelopmentStatus == ExperimentalValue;
		bIsEarlyAccess = DevelopmentStatus == EarlyAccessValue;
	}

	static void CopySinglePropertyRecursive(UObject* SourceObject, const void* const InSourcePtr, FProperty* InSourceProperty, void* const InTargetPtr, UObject* InDestinationObject, FProperty* InDestinationProperty)
	{
		bool bNeedsShallowCopy = true;

		if (FStructProperty* const DestStructProperty = CastField<FStructProperty>(InDestinationProperty))
		{
			FStructProperty* const SrcStructProperty = CastField<FStructProperty>(InSourceProperty);

			// Ensure that the target struct is initialized before copying fields from the source.
			DestStructProperty->InitializeValue_InContainer(InTargetPtr);

			const int32 PropertyArrayDim = DestStructProperty->ArrayDim;
			for (int32 ArrayIndex = 0; ArrayIndex < PropertyArrayDim; ArrayIndex++)
			{
				const void* const SourcePtr = SrcStructProperty->ContainerPtrToValuePtr<void>(InSourcePtr, ArrayIndex);
				void* const TargetPtr = DestStructProperty->ContainerPtrToValuePtr<void>(InTargetPtr, ArrayIndex);

				for (TFieldIterator<FProperty> It(SrcStructProperty->Struct); It; ++It)
				{
					FProperty* const InnerProperty = *It;
					CopySinglePropertyRecursive(SourceObject, SourcePtr, InnerProperty, TargetPtr, InDestinationObject, InnerProperty);
				}
			}

			bNeedsShallowCopy = false;
		}
		else if (FArrayProperty* const DestArrayProperty = CastField<FArrayProperty>(InDestinationProperty))
		{
			FArrayProperty* const SrcArrayProperty = CastField<FArrayProperty>(InSourceProperty);

			check(InDestinationProperty->ArrayDim == 1);
			FScriptArrayHelper SourceArrayHelper(SrcArrayProperty, SrcArrayProperty->ContainerPtrToValuePtr<void>(InSourcePtr));
			FScriptArrayHelper TargetArrayHelper(DestArrayProperty, DestArrayProperty->ContainerPtrToValuePtr<void>(InTargetPtr));

			int32 Num = SourceArrayHelper.Num();

			TargetArrayHelper.EmptyAndAddValues(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				CopySinglePropertyRecursive(SourceObject, SourceArrayHelper.GetRawPtr(Index), SrcArrayProperty->Inner, TargetArrayHelper.GetRawPtr(Index), InDestinationObject, DestArrayProperty->Inner);
			}

			bNeedsShallowCopy = false;
		}
		else if ( FMapProperty* const DestMapProperty = CastField<FMapProperty>(InDestinationProperty) )
		{
			FMapProperty* const SrcMapProperty = CastField<FMapProperty>(InSourceProperty);

			check(InDestinationProperty->ArrayDim == 1);
			FScriptMapHelper SourceMapHelper(SrcMapProperty, SrcMapProperty->ContainerPtrToValuePtr<void>(InSourcePtr));
			FScriptMapHelper TargetMapHelper(DestMapProperty, DestMapProperty->ContainerPtrToValuePtr<void>(InTargetPtr));

			TargetMapHelper.EmptyValues();

			for (FScriptMapHelper::FIterator It(SourceMapHelper); It; ++It)
			{
				uint8* SrcPairPtr = SourceMapHelper.GetPairPtr(It);

				int32 NewIndex = TargetMapHelper.AddDefaultValue_Invalid_NeedsRehash();
				TargetMapHelper.Rehash();

				uint8* PairPtr = TargetMapHelper.GetPairPtr(NewIndex);

				CopySinglePropertyRecursive(SourceObject, SrcPairPtr, SrcMapProperty->KeyProp, PairPtr, InDestinationObject, DestMapProperty->KeyProp);
				CopySinglePropertyRecursive(SourceObject, SrcPairPtr, SrcMapProperty->ValueProp, PairPtr, InDestinationObject, DestMapProperty->ValueProp);

				TargetMapHelper.Rehash();
			}

			bNeedsShallowCopy = false;
		}
		else if ( FSetProperty* const DestSetProperty = CastField<FSetProperty>(InDestinationProperty) )
		{
			FSetProperty* const SrcSetProperty = CastField<FSetProperty>(InSourceProperty);

			check(InDestinationProperty->ArrayDim == 1);
			FScriptSetHelper SourceSetHelper(SrcSetProperty, SrcSetProperty->ContainerPtrToValuePtr<void>(InSourcePtr));
			FScriptSetHelper TargetSetHelper(DestSetProperty, DestSetProperty->ContainerPtrToValuePtr<void>(InTargetPtr));

			TargetSetHelper.EmptyElements();

			for (FScriptSetHelper::FIterator It(SourceSetHelper); It; ++It)
			{
				uint8* SrcPtr = SourceSetHelper.GetElementPtr(It);

				int32 NewIndex = TargetSetHelper.AddDefaultValue_Invalid_NeedsRehash();
				TargetSetHelper.Rehash();

				uint8* TargetPtr = TargetSetHelper.GetElementPtr(NewIndex);
				CopySinglePropertyRecursive(SourceObject, SrcPtr, SrcSetProperty->ElementProp, TargetPtr, InDestinationObject, DestSetProperty->ElementProp);

				TargetSetHelper.Rehash();
			}

			bNeedsShallowCopy = false;
		}
		else if ( FObjectPropertyBase* SourceObjectProperty = CastField<FObjectPropertyBase>(InSourceProperty) )
		{
			if ( SourceObjectProperty->HasAllPropertyFlags(CPF_InstancedReference) )
			{
				UObject* Value = SourceObjectProperty->GetObjectPropertyValue_InContainer(InSourcePtr);
				if (Value)
				{
					// If the outer of the value is the source object, then we need to translate that same relationship
					// onto the destination object by deep copying the value and outering it to the destination object.
					if (Value->GetOuter() == SourceObject)
					{
						bNeedsShallowCopy = false;

						UObject* ExistingObject = StaticFindObject(UObject::StaticClass(), InDestinationObject, *Value->GetFName().ToString());
						if (ExistingObject)
						{
							ExistingObject->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
						}

						UObject* DuplicateValue = StaticDuplicateObject(Value, InDestinationObject, Value->GetFName(), RF_AllFlags, nullptr, EDuplicateMode::Normal, EInternalObjectFlags_AllFlags);

						// Ensure that we propagate the necessary flags from the destination object (outer) to the new subobject.
						EObjectFlags FlagsToPropagate = InDestinationObject->GetMaskedFlags(RF_PropagateToSubObjects);
						if (InDestinationObject->HasAnyFlags(RF_ClassDefaultObject) && !DuplicateValue->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject))
						{
							// Mark the new subobject as a template if its outer is a CDO and if it is not already flagged as a default subobject.
							FlagsToPropagate |= RF_ArchetypeObject;
						}

						DuplicateValue->SetFlags(FlagsToPropagate);

						FObjectPropertyBase* DestObjectProperty = CastFieldChecked<FObjectPropertyBase>(InDestinationProperty);
						DestObjectProperty->SetObjectPropertyValue_InContainer(InTargetPtr, DuplicateValue);
					}

					// If the outers match, we should look for a corresponding object already in existence
					// with the same name inside the destination object's outer.
					if (Value->GetOuter() == SourceObject->GetOuter())
					{
						bNeedsShallowCopy = false;

						UObject* DesintationValue = FindObjectFast<UObject>(InDestinationObject->GetOuter(), Value->GetFName());

						FObjectPropertyBase* DestObjectProperty = CastFieldChecked<FObjectPropertyBase>(InDestinationProperty);
						DestObjectProperty->SetObjectPropertyValue_InContainer(InTargetPtr, DesintationValue);
					}
				}
			}
		}

		if ( bNeedsShallowCopy )
		{
			const uint8* SourceAddr = InSourceProperty->ContainerPtrToValuePtr<uint8>(InSourcePtr);
			uint8* DestinationAddr = InDestinationProperty->ContainerPtrToValuePtr<uint8>(InTargetPtr);

			InSourceProperty->CopyCompleteValue(DestinationAddr, SourceAddr);
		}
	}

	bool MigratePropertyValue(UObject* SourceObject, FProperty* SourceProperty, UObject* DestinationObject, FProperty* DestinationProperty)
	{
		if (SourceObject == nullptr || DestinationObject == nullptr)
		{
			return false;
		}

		// Get the property addresses for the source and destination objects.
		uint8* SourceAddr = SourceProperty->ContainerPtrToValuePtr<uint8>(SourceObject);
		uint8* DestionationAddr = DestinationProperty->ContainerPtrToValuePtr<uint8>(DestinationObject);

		if (SourceAddr == nullptr || DestionationAddr == nullptr)
		{
			return false;
		}

		if (!DestinationObject->HasAnyFlags(RF_ClassDefaultObject))
		{
			FEditPropertyChain PropertyChain;
			PropertyChain.AddHead(DestinationProperty);
			DestinationObject->PreEditChange(PropertyChain);
		}

		CopySinglePropertyRecursive(SourceObject, SourceObject, SourceProperty, DestinationObject, DestinationObject, DestinationProperty);

		if (!DestinationObject->HasAnyFlags(RF_ClassDefaultObject))
		{
			FPropertyChangedEvent PropertyEvent(DestinationProperty);
			DestinationObject->PostEditChangeProperty(PropertyEvent);
		}

		return true;
	}
}

#endif // WITH_EDITOR

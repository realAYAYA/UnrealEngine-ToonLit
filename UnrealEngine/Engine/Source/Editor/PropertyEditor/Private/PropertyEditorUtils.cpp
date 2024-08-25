// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorUtils.h"

#include "PropertyCustomizationHelpers.h"
#include "PropertyPathHelpers.h"

namespace PropertyEditorUtils
{
	void GetPropertyOptions(TArray<UObject*>& InOutContainers, FString& InOutPropertyPath,
		TArray<TSharedPtr<FString>>& InOutOptions)
	{
		// Check for external function references
		if (InOutPropertyPath.Contains(TEXT(".")))
		{
			InOutContainers.Empty();
			UFunction* GetOptionsFunction = FindObject<UFunction>(nullptr, *InOutPropertyPath, true);

			if (ensureMsgf(GetOptionsFunction && GetOptionsFunction->HasAnyFunctionFlags(EFunctionFlags::FUNC_Static), TEXT("Invalid GetOptions: %s"), *InOutPropertyPath))
			{
				UObject* GetOptionsCDO = GetOptionsFunction->GetOuterUClass()->GetDefaultObject();
				GetOptionsFunction->GetName(InOutPropertyPath);
				InOutContainers.Add(GetOptionsCDO);
			}
		}

		if (InOutContainers.Num() > 0)
		{
			TArray<FString> OptionIntersection;
			TSet<FString> OptionIntersectionSet;

			for (UObject* Target : InOutContainers)
			{
				TArray<FString> StringOptions;
				{
					FEditorScriptExecutionGuard ScriptExecutionGuard;

					FCachedPropertyPath Path(InOutPropertyPath);
					if (!PropertyPathHelpers::GetPropertyValue(Target, Path, StringOptions))
					{
						TArray<FName> NameOptions;
						if (PropertyPathHelpers::GetPropertyValue(Target, Path, NameOptions))
						{
							Algo::Transform(NameOptions, StringOptions, [](const FName& InName) { return InName.ToString(); });
						}
					}
				}

				// If this is the first time there won't be any options.
				if (OptionIntersection.Num() == 0)
				{
					OptionIntersection = StringOptions;
					OptionIntersectionSet = TSet<FString>(StringOptions);
				}
				else
				{
					TSet<FString> StringOptionsSet(StringOptions);
					OptionIntersectionSet = StringOptionsSet.Intersect(OptionIntersectionSet);
					OptionIntersection.RemoveAll([&OptionIntersectionSet](const FString& Option){ return !OptionIntersectionSet.Contains(Option); });
				}

				// If we're out of possible intersected options, we can stop.
				if (OptionIntersection.Num() == 0)
				{
					break;
				}
			}

			Algo::Transform(OptionIntersection, InOutOptions, [](const FString& InString) { return MakeShared<FString>(InString); });
		}
	}

	void GetAllowedAndDisallowedClasses(const TArray<UObject*>& ObjectList, const FProperty& MetadataProperty, TArray<const UClass*>& AllowedClasses, TArray<const UClass*>& DisallowedClasses, bool bExactClass, const UClass* ObjectClass)
	{
		AllowedClasses = PropertyCustomizationHelpers::GetClassesFromMetadataString(MetadataProperty.GetOwnerProperty()->GetMetaData("AllowedClasses"));
		DisallowedClasses = PropertyCustomizationHelpers::GetClassesFromMetadataString(MetadataProperty.GetOwnerProperty()->GetMetaData("DisallowedClasses"));
		
		bool bMergeAllowedClasses = !AllowedClasses.IsEmpty();

		if (MetadataProperty.GetOwnerProperty()->HasMetaData("GetAllowedClasses"))
		{
			const FString GetAllowedClassesFunctionName = MetadataProperty.GetOwnerProperty()->GetMetaData("GetAllowedClasses");
			if (!GetAllowedClassesFunctionName.IsEmpty())
			{
				for (UObject* Object : ObjectList)
				{
					const UFunction* GetAllowedClassesFunction = Object ? Object->FindFunction(*GetAllowedClassesFunctionName) : nullptr;
					if (GetAllowedClassesFunction)
					{
						DECLARE_DELEGATE_RetVal(TArray<UClass*>, FGetAllowedClasses);
						if (!bMergeAllowedClasses)
						{
							AllowedClasses.Append(FGetAllowedClasses::CreateUFunction(Object, GetAllowedClassesFunction->GetFName()).Execute());
							if (AllowedClasses.IsEmpty())
							{
								// No allowed class means all classes are valid
								continue;
							}
							bMergeAllowedClasses = true;
						}
						else
						{
							TArray<UClass*> MergedClasses = FGetAllowedClasses::CreateUFunction(Object, GetAllowedClassesFunction->GetFName()).Execute();
							if (MergedClasses.IsEmpty())
							{
								// No allowed class means all classes are valid
								continue;
							}
							
							TArray<const UClass*> CurrentAllowedClassFilters = MoveTemp(AllowedClasses);
							ensure(AllowedClasses.IsEmpty());
							for (const UClass* MergedClass : MergedClasses)
							{
								// Keep classes that match both allow list
								for (const UClass* CurrentClass : CurrentAllowedClassFilters)
								{
									if (CurrentClass == MergedClass || (!bExactClass && CurrentClass->IsChildOf(MergedClass)))
									{
										AllowedClasses.Add(CurrentClass);
										break;
									}
									if (!bExactClass && MergedClass->IsChildOf(CurrentClass))
									{
										AllowedClasses.Add(MergedClass);
										break;
									}
								}
							}
							if (AllowedClasses.IsEmpty())
							{
								// An empty AllowedClasses array means that everything is allowed: in this case, forbid UObject
								DisallowedClasses.Add(ObjectClass);
								return;
							}
						}
					}
				}
			}
		}

		if (MetadataProperty.GetOwnerProperty()->HasMetaData("GetDisallowedClasses"))
		{
			const FString GetDisallowedClassesFunctionName = MetadataProperty.GetOwnerProperty()->GetMetaData("GetDisallowedClasses");
			if (!GetDisallowedClassesFunctionName.IsEmpty())
			{
				for (UObject* Object : ObjectList)
				{
					const UFunction* GetDisallowedClassesFunction = Object ? Object->FindFunction(*GetDisallowedClassesFunctionName) : nullptr;
					if (GetDisallowedClassesFunction)
					{
						DECLARE_DELEGATE_RetVal(TArray<UClass*>, FGetDisallowedClasses);
						DisallowedClasses.Append(FGetDisallowedClasses::CreateUFunction(Object, GetDisallowedClassesFunction->GetFName()).Execute());
					}
				}
			}
		}
	}
}

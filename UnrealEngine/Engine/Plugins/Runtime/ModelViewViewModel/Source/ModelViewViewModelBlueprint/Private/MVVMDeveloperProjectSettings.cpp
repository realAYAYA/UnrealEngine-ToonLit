// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMDeveloperProjectSettings.h"

#include "BlueprintEditorSettings.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MVVMBlueprintViewModelContext.h"
#include "PropertyPermissionList.h"
#include "Types/MVVMExecutionMode.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "MVVMDeveloperProjectSettings"

UMVVMDeveloperProjectSettings::UMVVMDeveloperProjectSettings()
{
	AllowedExecutionMode.Add(EMVVMExecutionMode::Immediate);
	AllowedExecutionMode.Add(EMVVMExecutionMode::Delayed);
	AllowedExecutionMode.Add(EMVVMExecutionMode::Tick);
	AllowedExecutionMode.Add(EMVVMExecutionMode::DelayedWhenSharedElseImmediate);
	
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::Manual);
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::CreateInstance);
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection);
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::PropertyPath);
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::PropertyPath);
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::Resolver);
}

FName UMVVMDeveloperProjectSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FText UMVVMDeveloperProjectSettings::GetSectionText() const
{
	return LOCTEXT("MVVMProjectSettings", "UMG Model View Viewmodel");
}

bool UMVVMDeveloperProjectSettings::PropertyHasFiltering(const UStruct* ObjectStruct, const FProperty* Property) const
{
	check(ObjectStruct);
	check(Property);

	const UClass* AuthoritativeClass = Cast<const UClass>(ObjectStruct);
	ObjectStruct = AuthoritativeClass ? AuthoritativeClass->GetAuthoritativeClass() : ObjectStruct;
	if (!FPropertyEditorPermissionList::Get().HasFiltering(ObjectStruct))
	{
		return false;
	}

	TStringBuilder<512> StringBuilder;
	Property->GetOwnerClass()->GetPathName(nullptr, StringBuilder);
	FSoftClassPath StructPath;
	StructPath.SetPath(StringBuilder);

	if (ObjectStruct)
	{
		for (const TPair<FSoftClassPath, FMVVMDeveloperProjectWidgetSettings>& PermissionItem : FieldSelectorPermissions)
		{
			if (UClass* ConcreteClass = PermissionItem.Key.ResolveClass())
			{
				if (ObjectStruct->IsChildOf(ConcreteClass))
				{
					const FMVVMDeveloperProjectWidgetSettings& Settings = PermissionItem.Value;
					if (Settings.DisallowedFieldNames.Contains(Property->GetFName()))
					{
						return false;
					}
				}
			}
		}
	}
	return true;
}

namespace UE::MVVM::Private
{
bool ShouldDoFieldEditorPermission(const UBlueprint* GeneratingFor, const UClass* FieldOwner)
{
	if (GeneratingFor && FieldOwner)
	{
		const UClass* UpToDateClass = FBlueprintEditorUtils::GetMostUpToDateClass(FieldOwner);
		return GeneratingFor->SkeletonGeneratedClass != UpToDateClass;
	}
	return true;
}
}//namespace

bool UMVVMDeveloperProjectSettings::IsPropertyAllowed(const UBlueprint* GeneratingFor, const UStruct* ObjectStruct, const FProperty* Property) const
{
	check(GeneratingFor);
	check(ObjectStruct);
	check(Property);

	const UClass* AuthoritativeClass = Cast<const UClass>(ObjectStruct);
	AuthoritativeClass = AuthoritativeClass ? AuthoritativeClass->GetAuthoritativeClass() : nullptr;

	const bool bDoPropertyEditorPermission = UE::MVVM::Private::ShouldDoFieldEditorPermission(GeneratingFor, AuthoritativeClass);
	if (bDoPropertyEditorPermission)
	{
		if (!FPropertyEditorPermissionList::Get().DoesPropertyPassFilter(AuthoritativeClass, Property->GetFName()))
		{
			return false;
		}
	}

	if (AuthoritativeClass)
	{
		TStringBuilder<512> StringBuilder;
		AuthoritativeClass->GetPathName(nullptr, StringBuilder);
		FSoftClassPath StructPath;
		StructPath.SetPath(StringBuilder.ToView());

		for (const TPair<FSoftClassPath, FMVVMDeveloperProjectWidgetSettings>& PermissionItem : FieldSelectorPermissions)
		{
			if (UClass* ConcreteClass = PermissionItem.Key.ResolveClass())
			{
				if (AuthoritativeClass->IsChildOf(ConcreteClass))
				{
					const FMVVMDeveloperProjectWidgetSettings& Settings = PermissionItem.Value;
					if (Settings.DisallowedFieldNames.Contains(Property->GetFName()))
					{
						return false;
					}
				}
			}
		}
	}
	return true;
}

bool UMVVMDeveloperProjectSettings::IsFunctionAllowed(const UBlueprint* GeneratingFor, const UClass* ObjectClass, const UFunction* Function) const
{
	check(GeneratingFor);
	check(ObjectClass);
	check(Function);

	const UClass* AuthoritativeClass = ObjectClass->GetAuthoritativeClass();
	if (AuthoritativeClass == nullptr)
	{
		return false;
	}

	const bool bDoPropertyEditorPermission = UE::MVVM::Private::ShouldDoFieldEditorPermission(GeneratingFor, AuthoritativeClass);
	if (bDoPropertyEditorPermission)
	{
		const FPathPermissionList& FunctionPermissions = GetMutableDefault<UBlueprintEditorSettings>()->GetFunctionPermissions();
		if (FunctionPermissions.HasFiltering())
		{
			const UFunction* FunctionToTest = AuthoritativeClass->FindFunctionByName(Function->GetFName());
			if (FunctionToTest == nullptr)
			{
				return false;
			}

			TStringBuilder<512> StringBuilder;
			FunctionToTest->GetPathName(nullptr, StringBuilder);
			if (!FunctionPermissions.PassesFilter(StringBuilder.ToView()))
			{
				return false;
			}
		}
	}

	{
		TStringBuilder<512> StringBuilder;
		AuthoritativeClass->GetPathName(nullptr, StringBuilder);
		FSoftClassPath StructPath;
		StructPath.SetPath(StringBuilder);

		for (const TPair<FSoftClassPath, FMVVMDeveloperProjectWidgetSettings>& PermissionItem : FieldSelectorPermissions)
		{
			if (UClass* ConcreteClass = PermissionItem.Key.ResolveClass())
			{
				if (AuthoritativeClass->IsChildOf(ConcreteClass))
				{
					const FMVVMDeveloperProjectWidgetSettings& Settings = PermissionItem.Value;
					if (Settings.DisallowedFieldNames.Contains(Function->GetFName()))
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

bool UMVVMDeveloperProjectSettings::IsConversionFunctionAllowed(const UBlueprint* GeneratingFor, const UFunction* Function) const
{
	if (ConversionFunctionFilter == EMVVMDeveloperConversionFunctionFilterType::BlueprintActionRegistry)
	{
		return IsFunctionAllowed(GeneratingFor, Function->GetOwnerClass(), Function);
	}
	else
	{
		check(ConversionFunctionFilter == EMVVMDeveloperConversionFunctionFilterType::AllowedList);

		if (Function->HasAllFunctionFlags(FUNC_Static))
		{
			TStringBuilder<512> FunctionClassPath;
			Function->GetOwnerClass()->GetPathName(nullptr, FunctionClassPath);
			TStringBuilder<512> AllowedClassPath;
			for (const FSoftClassPath& SoftClass : AllowedClassForConversionFunctions)
			{
				SoftClass.ToString(AllowedClassPath);
				if (AllowedClassPath.ToView() == FunctionClassPath.ToView())
				{
					return true;
				}
				AllowedClassPath.Reset();
			}

			return false;
		}
		else
		{
			// The function is on self and may have been filtered.
			return IsFunctionAllowed(GeneratingFor, Function->GetOwnerClass(), Function);
		}
	}
}

bool UMVVMDeveloperProjectSettings::IsConversionFunctionAllowed(const UBlueprint* Context, const TSubclassOf<UK2Node> Function) const
{
	if (ConversionFunctionFilter == EMVVMDeveloperConversionFunctionFilterType::BlueprintActionRegistry)
	{
		return !Function.Get()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
	}
	else
	{
		check(ConversionFunctionFilter == EMVVMDeveloperConversionFunctionFilterType::AllowedList);

		TStringBuilder<512> FunctionClassPath;
		Function.Get()->GetPathName(nullptr, FunctionClassPath);
		TStringBuilder<512> AllowedClassPath;
		for (const FSoftClassPath& SoftClass : AllowedClassForConversionFunctions)
		{
			SoftClass.ToString(AllowedClassPath);
			if (AllowedClassPath.ToView() == FunctionClassPath.ToView())
			{
				return true;
			}
			AllowedClassPath.Reset();
		}
		return false;
	}
}

TArray<const UClass*> UMVVMDeveloperProjectSettings::GetAllowedConversionFunctionClasses() const
{
	TArray<const UClass*> Result;
	for (const FSoftClassPath& SoftClass : AllowedClassForConversionFunctions)
	{
		if (UClass* Class = SoftClass.ResolveClass())
		{
			Result.Add(Class);
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

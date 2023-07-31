// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMDeveloperProjectSettings.h"

#define LOCTEXT_NAMESPACE "MVVMDeveloperProjectSettings"

FName UMVVMDeveloperProjectSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FText UMVVMDeveloperProjectSettings::GetSectionText() const
{
	return LOCTEXT("MVVMProjectSettings", "Model View Viewmodel");
}

bool UMVVMDeveloperProjectSettings::IsPropertyAllowed(FProperty* Property) const
{
	check(Property);
	TStringBuilder<256> StringBuilder;
	Property->GetOwnerClass()->GetPathName(nullptr, StringBuilder);
	FSoftClassPath StructPath;
	StructPath.SetPath(StringBuilder);
	if (const FMVVMDeveloperProjectWidgetSettings* Settings = FieldSelectorPermissions.Find(StructPath))
	{
		return !Settings->DisallowedFieldNames.Find(Property->GetFName());
	}
	return true;
}

bool UMVVMDeveloperProjectSettings::IsFunctionAllowed(UFunction* Function) const
{
	check(Function);
	TStringBuilder<256> StringBuilder;
	Function->GetOwnerClass()->GetPathName(nullptr, StringBuilder);
	FSoftClassPath StructPath;
	StructPath.SetPath(StringBuilder);
	if (const FMVVMDeveloperProjectWidgetSettings* Settings = FieldSelectorPermissions.Find(StructPath))
	{
		return !Settings->DisallowedFieldNames.Find(Function->GetFName());
	}
	return true;
}

#undef LOCTEXT_NAMESPACE

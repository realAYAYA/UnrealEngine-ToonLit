// Copyright Epic Games, Inc. All Rights Reserved.

#include "CodeGen/WebAPICodeGeneratorSettings.h"

#include "GeneralProjectSettings.h"
#include "WebAPIEditorSettings.h"
#include "CodeGen/Dom/WebAPICodeGenSettings.h"

FWebAPICodeGeneratorSettings::FWebAPICodeGeneratorSettings()
{
	CopyrightNotice = GetDefault<UGeneralProjectSettings>()->CopyrightNotice;
}

TScriptInterface<IWebAPICodeGeneratorInterface> FWebAPICodeGeneratorSettings::GetGeneratorClass() const
{
	if(!CodeGeneratorClass.IsNull() && bOverrideGeneratorClass)
	{
		const TScriptInterface<IWebAPICodeGeneratorInterface> CodeGeneratorInstance = CodeGeneratorClass->GetDefaultObject();
		return CodeGeneratorInstance;		
	}

	const UWebAPIEditorSettings* ProjectSettings = GetDefault<UWebAPIEditorSettings>();
	return ProjectSettings->GetGeneratorClass();
}

const FString& FWebAPICodeGeneratorSettings::GetNamespace() const
{
	return Namespace;
}

void FWebAPICodeGeneratorSettings::SetNamespace(const FString& InNamespace)
{
	if(InNamespace == Namespace)
	{
		return;
	}

	Namespace = InNamespace;
	OnNamespaceChanged().Broadcast(Namespace);
}

FOnNamespaceChangedDelegate& FWebAPICodeGeneratorSettings::OnNamespaceChanged()
{
	return OnNamespaceChangedDelegate;
}

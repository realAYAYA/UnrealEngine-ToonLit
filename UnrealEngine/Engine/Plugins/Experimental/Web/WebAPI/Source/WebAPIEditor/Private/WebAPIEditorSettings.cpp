// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebAPIEditorSettings.h"

#include "IWebAPIEditorModule.h"

TScriptInterface<IWebAPICodeGeneratorInterface> UWebAPIEditorSettings::GetGeneratorClass() const
{
	if(!CodeGeneratorClass.IsNull())
	{
		const TScriptInterface<IWebAPICodeGeneratorInterface> CodeGeneratorInstance = CodeGeneratorClass->GetDefaultObject();
		return CodeGeneratorInstance;		
	}

	return nullptr;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithFacadeKeyValueProperty.h"

FDatasmithFacadeKeyValueProperty::FDatasmithFacadeKeyValueProperty(const TCHAR* InName)
	: FDatasmithFacadeElement(FDatasmithSceneFactory::CreateKeyValueProperty(InName))
{}

FDatasmithFacadeKeyValueProperty::FDatasmithFacadeKeyValueProperty(const TSharedRef<IDatasmithKeyValueProperty>& InKeyValueProperty)
	: FDatasmithFacadeElement(InKeyValueProperty)
{}
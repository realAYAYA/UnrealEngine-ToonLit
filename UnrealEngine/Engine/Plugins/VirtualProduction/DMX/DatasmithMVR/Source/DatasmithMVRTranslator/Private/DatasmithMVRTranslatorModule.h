// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDatasmithMVRTranslatorModule.h"


class FDatasmithMVRTranslatorModule
	: public IDatasmithMVRTranslatorModule
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End IModuleInterface interface

	//~ Begin IDatasmithMVRNativeTanslator inteface
	virtual void SetDatasmithMVRNativeTanslatorEnabled(bool bEnabled) override;
	//~ End IDatasmithMVRNativeTranslator interface
};

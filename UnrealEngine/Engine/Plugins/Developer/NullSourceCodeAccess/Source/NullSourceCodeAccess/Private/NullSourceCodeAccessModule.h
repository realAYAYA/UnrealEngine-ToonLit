// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "NullSourceCodeAccessor.h"

class FNullSourceCodeAccessModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	FNullSourceCodeAccessor& GetAccessor();

private:
	FNullSourceCodeAccessor NullSourceCodeAccessor;
};

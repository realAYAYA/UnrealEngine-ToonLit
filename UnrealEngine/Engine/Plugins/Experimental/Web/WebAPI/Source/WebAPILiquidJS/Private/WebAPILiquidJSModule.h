// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWebAPILiquidJSModule.h"

#include "CoreMinimal.h"

class FWebAPILiquidJSModule final
    : public IWebAPILiquidJSModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool TryStartWebApp() const;

private:
	/** The actual process that runs the middleman server. */
	TSharedPtr<class FWebAPILiquidJSProcess> WebApp;
};

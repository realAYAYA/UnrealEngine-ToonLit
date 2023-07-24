// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebBrowserTextureModule.h"


class FWebBrowserTextureModule : public IWebBrowserTextureModule
{
private:
	// IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FWebBrowserTextureModule, WebBrowserTexture );

void FWebBrowserTextureModule::StartupModule()
{
}

void FWebBrowserTextureModule::ShutdownModule()
{
}

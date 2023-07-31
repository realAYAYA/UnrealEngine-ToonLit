// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebBrowserModule.h"
#include "WebBrowserLog.h"
#include "WebBrowserSingleton.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#if WITH_CEF3
#	include "CEF3Utils.h"
#	if PLATFORM_MAC
#		include "include/wrapper/cef_library_loader.h"
#		define CEF3_BIN_DIR TEXT("Binaries/ThirdParty/CEF3")
#     if PLATFORM_MAC_ARM64
#		define CEF3_FRAMEWORK_DIR CEF3_BIN_DIR TEXT("/Mac/Chromium Embedded Framework arm64.framework")
#     else
#		define CEF3_FRAMEWORK_DIR CEF3_BIN_DIR TEXT("/Mac/Chromium Embedded Framework x86.framework")
#     endif
#		define CEF3_FRAMEWORK_EXE CEF3_FRAMEWORK_DIR TEXT("/Chromium Embedded Framework")
#	endif
#endif

DEFINE_LOG_CATEGORY(LogWebBrowser);

static FWebBrowserSingleton* WebBrowserSingleton = nullptr;

FWebBrowserInitSettings::FWebBrowserInitSettings()
	: ProductVersion(FString::Printf(TEXT("%s/%s UnrealEngine/%s Chrome/90.0.4430.212"), FApp::GetProjectName(), FApp::GetBuildVersion(), *FEngineVersion::Current().ToString()))
{
}

class FWebBrowserModule : public IWebBrowserModule
{
private:
	// IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	virtual bool IsWebModuleAvailable() const override;
	virtual IWebBrowserSingleton* GetSingleton() override;
	virtual bool CustomInitialize(const FWebBrowserInitSettings& WebBrowserInitSettings) override;

private:
#if WITH_CEF3
	bool bLoadedCEFModule = false;
#if PLATFORM_MAC
	// Dynamically load the CEF framework library.
	CefScopedLibraryLoader *CEFLibraryLoader = nullptr;
#endif
#endif
};

IMPLEMENT_MODULE( FWebBrowserModule, WebBrowser );

void FWebBrowserModule::StartupModule()
{
#if WITH_CEF3
	if (!IsRunningCommandlet())
	{
		CEF3Utils::BackupCEF3Logfile(FPaths::ProjectLogDir());
	}
	bLoadedCEFModule = CEF3Utils::LoadCEF3Modules(true);
#if PLATFORM_MAC
	// Dynamically load the CEF framework library into this dylibs memory space.
	// CEF now loads function pointers at runtime so we need this to be dylib specific.
	CEFLibraryLoader = new CefScopedLibraryLoader();
	
	FString CefFrameworkPath(FPaths::Combine(*FPaths::EngineDir(), CEF3_FRAMEWORK_EXE));
	CefFrameworkPath = FPaths::ConvertRelativePathToFull(CefFrameworkPath);
	
	bool bLoaderInitialized = false;
	if (!CEFLibraryLoader->LoadInMain(TCHAR_TO_ANSI(*CefFrameworkPath)))
	{
			UE_LOG(LogWebBrowser, Error, TEXT("Chromium loader initialization failed"));
	}
#endif // PLATFORM_MAC
#endif
}

void FWebBrowserModule::ShutdownModule()
{
	if (WebBrowserSingleton != nullptr)
	{
		delete WebBrowserSingleton;
		WebBrowserSingleton = nullptr;
	}

#if WITH_CEF3
	CEF3Utils::UnloadCEF3Modules();
#if PLATFORM_MAC
	delete CEFLibraryLoader;
	CEFLibraryLoader = nullptr;
#endif // PLATFORM_MAC
#endif
}

bool FWebBrowserModule::CustomInitialize(const FWebBrowserInitSettings& WebBrowserInitSettings)
{
	if (WebBrowserSingleton == nullptr)
	{
		WebBrowserSingleton = new FWebBrowserSingleton(WebBrowserInitSettings);
		return true;
	}
	return false;
}

IWebBrowserSingleton* FWebBrowserModule::GetSingleton()
{
	if (WebBrowserSingleton == nullptr)
	{
		WebBrowserSingleton = new FWebBrowserSingleton(FWebBrowserInitSettings());
	}
	return WebBrowserSingleton;
}


bool FWebBrowserModule::IsWebModuleAvailable() const
{
#if WITH_CEF3
	return bLoadedCEFModule;
#else
	return true;
#endif
}


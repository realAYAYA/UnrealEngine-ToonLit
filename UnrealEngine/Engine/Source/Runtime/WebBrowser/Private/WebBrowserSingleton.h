// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "IWebBrowserSingleton.h"

#if WITH_CEF3
#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
	#include "Windows/AllowWindowsPlatformAtomics.h"
#endif
#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
THIRD_PARTY_INCLUDES_START
#if PLATFORM_APPLE
PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif
#include "include/internal/cef_ptr.h"
#include "include/cef_request_context.h"
#if PLATFORM_APPLE
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
THIRD_PARTY_INCLUDES_END
#pragma pop_macro("OVERRIDE")
#if PLATFORM_WINDOWS
	#include "Windows/HideWindowsPlatformAtomics.h"
	#include "Windows/HideWindowsPlatformTypes.h"
#endif
#include "CEF/CEFSchemeHandler.h"
#include "CEF/CEFResourceContextHandler.h"
class CefListValue;
class FCEFBrowserApp;
class FCEFWebBrowserWindow;
#endif

class IWebBrowserCookieManager;
class IWebBrowserWindow;
struct FWebBrowserWindowInfo;
struct FWebBrowserInitSettings;
class UMaterialInterface;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/**
 * Implementation of singleton class that takes care of general web browser tasks
 */
class FWebBrowserSingleton
	: public IWebBrowserSingleton
	, public FTSTickerObjectBase
{
public:

	/** Constructor. */
	FWebBrowserSingleton(const FWebBrowserInitSettings& WebBrowserInitSettings);

	/** Virtual destructor. */
	virtual ~FWebBrowserSingleton();

	/**
	* Gets the Current Locale Code in the format CEF expects
	*
	* @return Locale code as either "xx" or "xx-YY"
	*/
	static FString GetCurrentLocaleCode();

	virtual FString ApplicationCacheDir() const override;

public:

	// IWebBrowserSingleton Interface

	virtual TSharedRef<IWebBrowserWindowFactory> GetWebBrowserWindowFactory() const override;

	TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(
		TSharedPtr<FCEFWebBrowserWindow>& BrowserWindowParent,
		TSharedPtr<FWebBrowserWindowInfo>& BrowserWindowInfo) override;

	TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(const FCreateBrowserWindowSettings& Settings) override;

#if	BUILD_EMBEDDED_APP
	TSharedPtr<IWebBrowserWindow> CreateNativeBrowserProxy() override;
#endif

	virtual TSharedPtr<IWebBrowserCookieManager> GetCookieManager() const override
	{
		return DefaultCookieManager;
	}

	virtual TSharedPtr<IWebBrowserCookieManager> GetCookieManager(TOptional<FString> ContextId) const override;

	virtual bool RegisterContext(const FBrowserContextSettings& Settings) override;

	virtual bool UnregisterContext(const FString& ContextId) override;

	virtual bool RegisterSchemeHandlerFactory(FString Scheme, FString Domain, IWebBrowserSchemeHandlerFactory* WebBrowserSchemeHandlerFactory) override;

	virtual bool UnregisterSchemeHandlerFactory(IWebBrowserSchemeHandlerFactory* WebBrowserSchemeHandlerFactory) override;

	virtual bool IsDevToolsShortcutEnabled() override
	{
		return bDevToolsShortcutEnabled;
	}

	virtual void SetDevToolsShortcutEnabled(bool Value) override
	{
		bDevToolsShortcutEnabled = Value;
	}

	virtual void SetJSBindingToLoweringEnabled(bool bEnabled) override
	{
		bJSBindingsToLoweringEnabled = bEnabled;
	}

	virtual void ClearOldCacheFolders(const FString& CachePathRoot, const FString& CachePrefix) override;

	/** Set a reference to UWebBrowser's default material*/
	virtual void SetDefaultMaterial(UMaterialInterface* InDefaultMaterial) override
	{
		DefaultMaterial = InDefaultMaterial;
	}

	/** Set a reference to UWebBrowser's translucent material*/
	virtual void SetDefaultTranslucentMaterial(UMaterialInterface* InDefaultMaterial) override
	{
		DefaultTranslucentMaterial = InDefaultMaterial;
	}

	/** Get a reference to UWebBrowser's default material*/
	virtual UMaterialInterface* GetDefaultMaterial() override
	{
		return DefaultMaterial;
	}

	/** Get a reference to UWebBrowser's translucent material*/
	virtual UMaterialInterface* GetDefaultTranslucentMaterial() override
	{
		return DefaultTranslucentMaterial;
	}

public:

	// FTSTickerObjectBase Interface

	virtual bool Tick(float DeltaTime) override;

#if WITH_CEF3
	/** Return true if this URL will support adding an Authorization header to it */
	bool URLRequestAllowsCredentials(const FString& URL);
#endif
private:

	TSharedPtr<IWebBrowserCookieManager> DefaultCookieManager;

#if WITH_CEF3
	/** Helper function to generate the CEF build unique name for the cache_path */
	FString GenerateWebCacheFolderName(const FString &InputPath);
	/** Helper function that blocks until the CEF task queue has processed a posted task, flushing the queue */
	void WaitForTaskQueueFlush();

	/** Pointer to the CEF App implementation */
	CefRefPtr<FCEFBrowserApp>			CEFBrowserApp;

	TMap<FString, CefRefPtr<CefRequestContext>> RequestContexts;
	TMap<FString, CefRefPtr<FCEFResourceContextHandler>> RequestResourceHandlers;
	FCefSchemeHandlerFactories SchemeHandlerFactories;
	bool bAllowCEF;
	bool bTaskFinished;
#endif

	/** List of currently existing browser windows */
#if WITH_CEF3
	TArray<TWeakPtr<FCEFWebBrowserWindow>>	WindowInterfaces;
#elif PLATFORM_IOS || PLATFORM_SPECIFIC_WEB_BROWSER || (PLATFORM_ANDROID && USE_ANDROID_JNI)
	TArray<TWeakPtr<IWebBrowserWindow>>	WindowInterfaces;
#endif

	/** Critical section for thread safe modification of WindowInterfaces array. */
	FCriticalSection WindowInterfacesCS;

	TSharedRef<IWebBrowserWindowFactory> WebBrowserWindowFactory;

	bool bDevToolsShortcutEnabled;

	bool bJSBindingsToLoweringEnabled;

	bool bAppIsFocused;

#if WITH_CEF3
	/** Did CEF successfully initialize itself */
	bool bCEFInitialized;
#endif

	/** Reference to UWebBrowser's default material*/
	UMaterialInterface* DefaultMaterial;

	/** Reference to UWebBrowser's translucent material*/
	UMaterialInterface* DefaultTranslucentMaterial;

};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if WITH_CEF3

class CefCookieManager;

class FCefWebBrowserCookieManagerFactory
{
public:
	static TSharedRef<IWebBrowserCookieManager> Create(
		const CefRefPtr<CefCookieManager>& CookieManager);
};

#endif

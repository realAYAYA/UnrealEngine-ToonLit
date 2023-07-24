// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericApplication.h"
#include "AndroidWindow.h"
#if USE_ANDROID_JNI
#include "Android/AndroidJavaEnv.h"
#endif

namespace FAndroidAppEntry
{
	void PlatformInit();

	// if the native window handle has changed then the new handle is required.
	void ReInitWindow(void* NewNativeWindowHandle = nullptr);

	void ReleaseEGL();
	void OnPauseEvent();
}

struct FPlatformOpenGLContext;
namespace FAndroidEGL
{
	// Back door into more intimate Android OpenGL variables (a.k.a. a hack)
	FPlatformOpenGLContext*	GetRenderingContext();
	FPlatformOpenGLContext*	CreateContext();
	void					MakeCurrent(FPlatformOpenGLContext*);
	void					ReleaseContext(FPlatformOpenGLContext*);
	void					SwapBuffers(FPlatformOpenGLContext*);
	void					SetFlipsEnabled(bool Enabled);
	void					BindDisplayToContext(FPlatformOpenGLContext*);
}

//disable warnings from overriding the deprecated forcefeedback.  
//calls to the deprecated function will still generate warnings.
PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FAndroidApplication : public GenericApplication
{
public:

	static FAndroidApplication* CreateAndroidApplication();

#if USE_ANDROID_JNI
	// Returns the java environment
	static FORCEINLINE void InitializeJavaEnv(JavaVM* VM, jint Version, jobject GlobalThis)
	{
		AndroidJavaEnv::InitializeJavaEnv(VM, Version, GlobalThis);
    }
	static FORCEINLINE jobject GetGameActivityThis()
	{
		return AndroidJavaEnv::GetGameActivityThis();
	} 
	static FORCEINLINE jobject GetClassLoader()
	{
		return AndroidJavaEnv::GetClassLoader();
	} 
	static FORCEINLINE JNIEnv* GetJavaEnv(bool bRequireGlobalThis = true)
	{
		return AndroidJavaEnv::GetJavaEnv(bRequireGlobalThis);
	}
	static FORCEINLINE jclass FindJavaClass(const char* name)
	{
		return AndroidJavaEnv::FindJavaClass(name);
	}
	static FORCEINLINE jclass FindJavaClassGlobalRef(const char* name)
	{
		return AndroidJavaEnv::FindJavaClassGlobalRef(name);
	}
	static FORCEINLINE void DetachJavaEnv()
	{
		AndroidJavaEnv::DetachJavaEnv();
	}
	static FORCEINLINE bool CheckJavaException()
	{
		return AndroidJavaEnv::CheckJavaException();
	}
#endif

	static FAndroidApplication* Get() { return _application; }

public:	
	
	virtual ~FAndroidApplication() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;

	virtual IInputInterface* GetInputInterface() override;

	virtual TSharedRef< FGenericWindow > MakeWindow() override;

	virtual void AddExternalInputDevice(TSharedPtr<class IInputDevice> InputDevice);

	void InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately );

	static void OnWindowSizeChanged();

	virtual void Tick(const float TimeDelta) override;

	virtual bool IsGamepadAttached() const override;

protected:

	FAndroidApplication();
	FAndroidApplication(TSharedPtr<class FAndroidInputInterface> InInputInterface);


private:

	TSharedPtr< class FAndroidInputInterface > InputInterface;
	bool bHasLoadedInputPlugins;

	TArray< TSharedRef< FAndroidWindow > > Windows;

	static bool bWindowSizeChanged;

	static FAndroidApplication* _application;

    EDeviceScreenOrientation DeviceOrientation;
    void HandleDeviceOrientation();
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if USE_ANDROID_JNI

#include "Android/AndroidPlatform.h"
#include "Android/AndroidJava.h"
#include "RHI.h"
#include "RHIResources.h"

// Wrapper for com/epicgames/unreal/CameraPlayer*.java.
class FJavaAndroidWebBrowser : public FJavaClassObject
{
public:
	FJavaAndroidWebBrowser(bool swizzlePixels, bool vulkanRenderer, int32 width, int32 height, jlong widgetPtr, bool bEnableRemoteDebugging, bool bUseTransparency, bool bEnableDomStorage);
	virtual ~FJavaAndroidWebBrowser();
	void Release();
	bool GetVideoLastFrameData(void* & outPixels, int64 & outCount, bool *bRegionChanged);
	bool GetVideoLastFrame(int32 destTexture);
	bool DidResolutionChange();
	bool UpdateVideoFrame(int32 ExternalTextureId, bool *bRegionChanged);
	void ExecuteJavascript(const FString& Script);
	void LoadURL(const FString& NewURL);
	void LoadString(const FString& Contents, const FString& BaseUrl);
	void StopLoad();
	void Reload();
	void Close();
	void GoBack();
	void GoForward();
	void SetAndroid3DBrowser(bool InIsAndroid3DBrowser);
	void SetVisibility(bool InIsVisible);
	void Update(const int posX, const int posY, const int sizeX, const int sizeY);
private:
	static FName GetClassName();

	FJavaClassMethod ReleaseMethod;
	FJavaClassMethod GetVideoLastFrameDataMethod;
	FJavaClassMethod GetVideoLastFrameMethod;
	FJavaClassMethod DidResolutionChangeMethod;
	FJavaClassMethod UpdateVideoFrameMethod;
	FJavaClassMethod UpdateMethod;
	FJavaClassMethod ExecuteJavascriptMethod;
	FJavaClassMethod LoadURLMethod;
	FJavaClassMethod LoadStringMethod;
	FJavaClassMethod StopLoadMethod;
	FJavaClassMethod ReloadMethod;
	FJavaClassMethod CloseMethod;
	FJavaClassMethod GoBackOrForwardMethod;
	FJavaClassMethod SetAndroid3DBrowserMethod;
	FJavaClassMethod SetVisibilityMethod;

	// FrameUpdateInfo member field ids
	jclass FrameUpdateInfoClass;
	jfieldID FrameUpdateInfo_Buffer;
	jfieldID FrameUpdateInfo_FrameReady;
	jfieldID FrameUpdateInfo_RegionChanged;
	
	FTextureRHIRef VideoTexture;
	bool bVideoTextureValid;

public:
	FTextureRHIRef GetVideoTexture()
	{
		return VideoTexture;
	}

	void SetVideoTexture(FTextureRHIRef Texture)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Fetch RT: SetVideoTexture: %d"), Texture.IsValid());

		VideoTexture = Texture;
	}

	void SetVideoTextureValid(bool Condition)
	{
		bVideoTextureValid = Condition;
	}

	bool IsVideoTextureValid()
	{
		return bVideoTextureValid;
	}

};

#endif // USE_ANDROID_JNI

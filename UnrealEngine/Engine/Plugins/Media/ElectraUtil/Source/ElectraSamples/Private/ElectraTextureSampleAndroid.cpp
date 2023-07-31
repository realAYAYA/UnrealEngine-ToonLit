// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if PLATFORM_ANDROID

#include "ElectraTextureSample.h"

#include "Android/AndroidPlatform.h"
#include "Android/AndroidJava.h"
#include "Android/AndroidApplication.h"

#include "RenderingThread.h"


#define ELECTRA_INIT_ON_RENDERTHREAD	1	// set to 1 if context & surface init should be on render thread (seems safer for compatibility - TODO: research why)

// ---------------------------------------------------------------------------------------------------------------------

#if UE_BUILD_SHIPPING
// always clear any exceptions in shipping
#define CHECK_JNI_RESULT(Id) if (Id == 0) { JEnv->ExceptionClear(); }
#else
#define CHECK_JNI_RESULT(Id)								\
	if (Id == 0)											\
	{														\
		if (bIsOptional)									\
		{													\
			JEnv->ExceptionClear();							\
		}													\
		else												\
		{													\
			JEnv->ExceptionDescribe();						\
			checkf(Id != 0, TEXT("Failed to find " #Id));	\
		}													\
	}
#endif

// ---------------------------------------------------------------------------------------------------------------------

class FElectraTextureSampleSupport : public FJavaClassObject
{
public:
	FElectraTextureSampleSupport();
	~FElectraTextureSampleSupport();

	struct FFrameUpdateInfo
	{
		int64   Timestamp = 0;
		int64   Duration = 0;
		float   UScale = 0.0f;
		float   UOffset = 0.0f;
		float   VScale = 0.0f;
		float   VOffset = 0.0f;
		bool	bFrameReady = false;
		bool	bRegionChanged = false;
		int 	NumPending = 0;
	};

	int32 GetFrameDataAndUpdateInfo(FFrameUpdateInfo& OutFrameUpdateInfo, FElectraTextureSample* InTargetSample);
	int32 GetFrameData(FElectraTextureSample* InTargetSample);
	jobject GetCodecSurface();

private:
	// Java methods
	FJavaClassMethod	InitializeFN;
	FJavaClassMethod	ReleaseFN;
	FJavaClassMethod	GetCodecSurfaceFN;
	FJavaClassMethod	GetVideoFrameUpdateInfoFN;

	// FFrameUpdateInfo member field IDs
	jclass				FFrameUpdateInfoClass;
	jfieldID			FFrameUpdateInfo_Buffer;
	jfieldID			FFrameUpdateInfo_Timestamp;
	jfieldID			FFrameUpdateInfo_Duration;
	jfieldID			FFrameUpdateInfo_bFrameReady;
	jfieldID			FFrameUpdateInfo_bRegionChanged;
	jfieldID			FFrameUpdateInfo_UScale;
	jfieldID			FFrameUpdateInfo_UOffset;
	jfieldID			FFrameUpdateInfo_VScale;
	jfieldID			FFrameUpdateInfo_VOffset;
	jfieldID			FFrameUpdateInfo_NumPending;

	jobject				CodecSurface;
	FEvent*				SurfaceInitEvent;


	static FName GetClassName()
	{
		return FName("com/epicgames/unreal/ElectraTextureSample");
	}

	jfieldID FindField(JNIEnv* JEnv, jclass InClass, const ANSICHAR* InFieldName, const ANSICHAR* InFieldType, bool bIsOptional)
	{
		jfieldID Field = InClass == nullptr ? nullptr : JEnv->GetFieldID(InClass, InFieldName, InFieldType);
		CHECK_JNI_RESULT(Field);
		return Field;
	}

	jmethodID FindMethod(JNIEnv* JEnv, jclass InClass, const ANSICHAR* InMethodName, const ANSICHAR* InMethodSignature, bool bIsOptional)
	{
		jmethodID Method = InClass == nullptr ? nullptr : JEnv->GetMethodID(InClass, InMethodName, InMethodSignature);
		CHECK_JNI_RESULT(Method);
		return Method;
	}

	// Create a Java byte array. Must DeleteLocalRef() after use or handling over to Java.
	jbyteArray MakeJavaByteArray(const uint8* InData, int32 InNumBytes)
	{
		JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
		jbyteArray RawBuffer = JEnv->NewByteArray(InNumBytes);
		JEnv->SetByteArrayRegion(RawBuffer, 0, InNumBytes, reinterpret_cast<const jbyte*>(InData));
		return RawBuffer;
	}

};

// ---------------------------------------------------------------------------------------------------------------------

FElectraTextureSampleSupport::FElectraTextureSampleSupport()
	: FJavaClassObject(GetClassName(), "()V")
	, InitializeFN(GetClassMethod("Initialize", "(Z)V"))
	, ReleaseFN(GetClassMethod("Release", "()V"))
	, GetCodecSurfaceFN(GetClassMethod("GetCodecSurface", "()Landroid/view/Surface;"))
	, GetVideoFrameUpdateInfoFN(GetClassMethod("GetVideoFrameUpdateInfo", "(III)Lcom/epicgames/unreal/ElectraTextureSample$FFrameUpdateInfo;"))
	, CodecSurface(nullptr)
	, SurfaceInitEvent(nullptr)
{
	JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();

	// Get field IDs for FFrameUpdateInfo class members
	jclass localFrameUpdateInfoClass = FAndroidApplication::FindJavaClass("com/epicgames/unreal/ElectraTextureSample$FFrameUpdateInfo");
	FFrameUpdateInfoClass = (jclass)JEnv->NewGlobalRef(localFrameUpdateInfoClass);
	JEnv->DeleteLocalRef(localFrameUpdateInfoClass);
	FFrameUpdateInfo_Buffer = FindField(JEnv, FFrameUpdateInfoClass, "Buffer", "Ljava/nio/Buffer;", false);
	FFrameUpdateInfo_Timestamp = FindField(JEnv, FFrameUpdateInfoClass, "Timestamp", "J", false);
	FFrameUpdateInfo_Duration = FindField(JEnv, FFrameUpdateInfoClass, "Duration", "J", false);
	FFrameUpdateInfo_bFrameReady = FindField(JEnv, FFrameUpdateInfoClass, "bFrameReady", "Z", false);
	FFrameUpdateInfo_bRegionChanged = FindField(JEnv, FFrameUpdateInfoClass, "bRegionChanged", "Z", false);
	FFrameUpdateInfo_UScale = FindField(JEnv, FFrameUpdateInfoClass, "UScale", "F", false);
	FFrameUpdateInfo_UOffset = FindField(JEnv, FFrameUpdateInfoClass, "UOffset", "F", false);
	FFrameUpdateInfo_VScale = FindField(JEnv, FFrameUpdateInfoClass, "VScale", "F", false);
	FFrameUpdateInfo_VOffset = FindField(JEnv, FFrameUpdateInfoClass, "VOffset", "F", false);
	FFrameUpdateInfo_NumPending = FindField(JEnv, FFrameUpdateInfoClass, "NumPending", "I", false);

#if ELECTRA_INIT_ON_RENDERTHREAD
	// enqueue to RT to ensure GL resources are created on the appropriate thread.
	SurfaceInitEvent = FPlatformProcess::GetSynchEventFromPool(true);
	ENQUEUE_RENDER_COMMAND(InitElectraTextureSample)([this](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.EnqueueLambda([this](FRHICommandListImmediate& CmdList)
				{
					JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
					// Setup Java side of things
					JEnv->CallVoidMethod(Object, InitializeFN.Method, FAndroidMisc::ShouldUseVulkan());
					// Query surface to be used for decoder
					jobject Surface = JEnv->CallObjectMethod(Object, GetCodecSurfaceFN.Method);
					CodecSurface = JEnv->NewGlobalRef(Surface);
					JEnv->DeleteLocalRef(Surface);
					SurfaceInitEvent->Trigger();
				});
		});
	FlushRenderingCommands();
#else
	// Setup Java side of things
	JEnv->CallVoidMethod(Object, InitializeFN.Method, FAndroidMisc::ShouldUseVulkan());

	// Query surface to be used for decoder
	jobject Surface = JEnv->CallObjectMethod(Object, GetCodecSurfaceFN.Method);
	CodecSurface = JEnv->NewGlobalRef(Surface);
	JEnv->DeleteLocalRef(Surface);
#endif
}


FElectraTextureSampleSupport::~FElectraTextureSampleSupport()
{
	// When initialization of the surface was triggered on the rendering thread we need to wait for its completion.
	if (SurfaceInitEvent)
	{
		// Wait for the surface initialization event to have been signaled.
		// Do not wait if we are on the render thread. In this case the initialization has already completed anyway.
		if (!IsInRenderingThread())
		{
			SurfaceInitEvent->Wait();
		}
		FPlatformProcess::ReturnSynchEventToPool(SurfaceInitEvent);
		SurfaceInitEvent = nullptr;
	}

	if (IsInGameThread())
	{
		// enqueue to RT to ensure GL resources are released on the appropriate thread.
		ENQUEUE_RENDER_COMMAND(DestroyElectraTextureSample)([this](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.EnqueueLambda([this](FRHICommandListImmediate& CmdList)
			{
				JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
				JEnv->CallVoidMethod(Object, ReleaseFN.Method);
				JEnv->DeleteGlobalRef(CodecSurface);
				JEnv->DeleteGlobalRef(FFrameUpdateInfoClass);
				CodecSurface = 0;
				FFrameUpdateInfoClass = 0;
			});
		});
		FlushRenderingCommands();
	}
	else
	{
		JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
		JEnv->CallVoidMethod(Object, ReleaseFN.Method);
		JEnv->DeleteGlobalRef(CodecSurface);
		JEnv->DeleteGlobalRef(FFrameUpdateInfoClass);
		CodecSurface = 0;
		FFrameUpdateInfoClass = 0;
	}
}

//-----------------------------------------------------------------------------
/**
 *
 * @note Call this from an RHI thread! It will need a valid rendering environment!
 */
int32 FElectraTextureSampleSupport::GetFrameDataAndUpdateInfo(FFrameUpdateInfo& OutFrameUpdateInfo, FElectraTextureSample* InTargetSample)
{
	// In case this is called with a ES renderer, we need to pass in the destination texture we'd like to be used to receive the data
	// (for Vulkan we'll just receive a simple byte buffer)
	int32 DestTexture = 0;
	if (FRHITexture* Texture = InTargetSample->GetTexture())
	{
		DestTexture = *reinterpret_cast<int32*>(Texture->GetNativeResource());
	}

	// Update frame info and get data...
	JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
	jobject OutputInfo = JEnv->CallObjectMethod(Object, GetVideoFrameUpdateInfoFN.Method, DestTexture, InTargetSample->GetDim().X, InTargetSample->GetDim().Y);
	if (JEnv->ExceptionCheck())
	{
		JEnv->ExceptionDescribe();
		JEnv->ExceptionClear();
	}
	// Failure will return no object.
	if (OutputInfo != nullptr)
	{
		OutFrameUpdateInfo.Timestamp = JEnv->GetLongField(OutputInfo, FFrameUpdateInfo_Timestamp);
		OutFrameUpdateInfo.Duration = JEnv->GetLongField(OutputInfo, FFrameUpdateInfo_Duration);
		OutFrameUpdateInfo.UScale = JEnv->GetFloatField(OutputInfo, FFrameUpdateInfo_UScale);
		OutFrameUpdateInfo.UOffset = JEnv->GetFloatField(OutputInfo, FFrameUpdateInfo_UOffset);
		OutFrameUpdateInfo.VScale = JEnv->GetFloatField(OutputInfo, FFrameUpdateInfo_VScale);
		OutFrameUpdateInfo.VOffset = JEnv->GetFloatField(OutputInfo, FFrameUpdateInfo_VOffset);
		OutFrameUpdateInfo.bFrameReady = JEnv->GetBooleanField(OutputInfo, FFrameUpdateInfo_bFrameReady);
		OutFrameUpdateInfo.bRegionChanged = JEnv->GetBooleanField(OutputInfo, FFrameUpdateInfo_bRegionChanged);
		OutFrameUpdateInfo.NumPending = JEnv->GetIntField(OutputInfo, FFrameUpdateInfo_NumPending);
		if (InTargetSample)
		{
			jobject buffer = JEnv->GetObjectField(OutputInfo, FFrameUpdateInfo_Buffer);
			if (buffer != nullptr)
			{
				const void* outPixels = JEnv->GetDirectBufferAddress(buffer);
				const int32 outCount = JEnv->GetDirectBufferCapacity(buffer);
				InTargetSample->SetupFromBuffer(outPixels, outCount);
				JEnv->DeleteLocalRef(buffer);
			}
		}

		JEnv->DeleteLocalRef(OutputInfo);
		return 0;
	}
	return 1;
}

//-----------------------------------------------------------------------------
/**
 *
 * @note Call this from an RHI thread! It will need a valid rendering environment!
 */
int32 FElectraTextureSampleSupport::GetFrameData(FElectraTextureSample* InTargetSample)
{
	// In case this is called with a ES renderer, we need to pass in the destination texture we'd like to be used to receive the data
	// (for Vulkan we'll just receive a simple byte buffer)
	int32 DestTexture = 0;
	if (FRHITexture* Texture = InTargetSample->GetTexture())
	{
		DestTexture = *reinterpret_cast<int32*>(Texture->GetNativeResource());
	}

	// Update frame info and get data...
	JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
	jobject OutputInfo = JEnv->CallObjectMethod(Object, GetVideoFrameUpdateInfoFN.Method, DestTexture, InTargetSample->GetDim().X, InTargetSample->GetDim().Y);
	if (JEnv->ExceptionCheck())
	{
		JEnv->ExceptionDescribe();
		JEnv->ExceptionClear();
	}
	// Failure will return no object.
	if (OutputInfo != nullptr)
	{
		if (InTargetSample)
		{
			jobject buffer = JEnv->GetObjectField(OutputInfo, FFrameUpdateInfo_Buffer);
			if (buffer != nullptr)
			{
				const void* outPixels = JEnv->GetDirectBufferAddress(buffer);
				const int32 outCount = JEnv->GetDirectBufferCapacity(buffer);
				InTargetSample->SetupFromBuffer(outPixels, outCount);
				JEnv->DeleteLocalRef(buffer);
			}
		}

		JEnv->DeleteLocalRef(OutputInfo);
		return 0;
	}
	return 1;
}


jobject FElectraTextureSampleSupport::GetCodecSurface()
{
	if (SurfaceInitEvent)
	{
		// Wait for the surface initialization event to have been signaled.
		// Do not wait if we are on the render thread. In this case the initialization has already completed anyway.
		if (!IsInRenderingThread())
		{
			// Only wait for a little while here just in case this would prevent the render thread from
			// even starting its jobs and us causing a deadlock here.
			bool bInitDone = SurfaceInitEvent->Wait(FTimespan::FromMilliseconds(2000.0));
			if (bInitDone)
			{
				// When init has completed we can free the event and do not have to wait for it any more in the future.
				FPlatformProcess::ReturnSynchEventToPool(SurfaceInitEvent);
				SurfaceInitEvent = nullptr;
			}
		}
	}

	return CodecSurface;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

FElectraTextureSamplePool::FElectraTextureSamplePool()
	: TMediaObjectPool<TextureSample, FElectraTextureSamplePool>(this)
	, Support(new FElectraTextureSampleSupport())
{}


void* FElectraTextureSamplePool::GetCodecSurface()
{
	return (void*)Support->GetCodecSurface();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/**
* Init code for reallocating an image from the the pool
*/
void FElectraTextureSample::InitializePoolable()
{
}

/**
*  Return the object to the pool and inform the renderer about this...
*/
void FElectraTextureSample::ShutdownPoolable()
{
	VideoDecoderOutput.Reset();
}


void FElectraTextureSample::Initialize(FVideoDecoderOutput* InVideoDecoderOutput)
{
	VideoDecoderOutput = StaticCastSharedPtr<FVideoDecoderOutputAndroid, IDecoderOutputPoolable, ESPMode::ThreadSafe>(InVideoDecoderOutput->AsShared());

	if (VideoDecoderOutput->GetOutputType() == FVideoDecoderOutputAndroid::EOutputType::DirectToSurfaceAsQueue)
	{
		ENQUEUE_RENDER_COMMAND(InitTextureSample)([WeakThis{ TWeakPtr<FElectraTextureSample, ESPMode::ThreadSafe>(AsShared()) }](FRHICommandListImmediate& RHICmdList) {
			if (TSharedPtr<FElectraTextureSample, ESPMode::ThreadSafe> This = WeakThis.Pin())
			{
				This->InitializeTexture();

				if (This->Texture)
				{
					RHICmdList.EnqueueLambda([WeakThis](FRHICommandListImmediate& CmdList)
					{
						QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandUpdateDecoderExternaTexture_Execute);
						if (TSharedPtr<FElectraTextureSample, ESPMode::ThreadSafe> This = WeakThis.Pin())
						{
							This->Support->GetFrameData(This.Get());
						}
					});
				}
				else
				{
					// CPU side buffer case: we do not need to do that on an RHI thread
					This->Support->GetFrameData(This.Get());
				}
			}
		});
	}
}


void FElectraTextureSample::InitializeTexture()
{
	check(IsInRenderingThread() || IsInRHIThread());

	if (FAndroidMisc::ShouldUseVulkan())
	{
		Texture = nullptr;
		return;
	}

	FIntPoint Dim = VideoDecoderOutput->GetDim();

	if (Texture.IsValid() && (Texture->GetSizeXY() == Dim))
	{
		return;
	}

	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("FElectraTextureSample"))
		.SetExtent(Dim)
		.SetFormat(PF_B8G8R8A8)
		.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::SRGB | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask);

	Texture = RHICreateTexture(Desc);

	return;
}


void FElectraTextureSample::SetupFromBuffer(const void* InBuffer, int32 InBufferSize)
{
	if (BufferSize < InBufferSize)
	{
		if (BufferSize == 0)
		{
			Buffer = FMemory::Malloc(InBufferSize);
		}
		else
		{
			Buffer = FMemory::Realloc(Buffer, InBufferSize);
		}
		BufferSize = InBufferSize;
	}
	FMemory::Memcpy(Buffer, InBuffer, InBufferSize);
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if PLATFORM_ANDROID

#include "ElectraTextureSample.h"

#include "Android/AndroidPlatform.h"
#include "Android/AndroidJava.h"
#include "Android/AndroidApplication.h"

#include "RenderingThread.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"

DECLARE_GPU_STAT_NAMED(MediaAndroidDecoder_Convert, TEXT("MediaAndroidDecoder_Convert"));

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
	FCriticalSection	CodecSurfaceLock;
	jobject				CodecSurfaceToDelete;


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
	, GetVideoFrameUpdateInfoFN(GetClassMethod("GetVideoFrameUpdateInfo", "(IIIZ)Lcom/epicgames/unreal/ElectraTextureSample$FFrameUpdateInfo;"))
	, CodecSurface(nullptr)
	, SurfaceInitEvent(nullptr)
	, CodecSurfaceToDelete(nullptr)
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

	CodecSurfaceLock.Lock();
	CodecSurfaceToDelete = CodecSurface;
	CodecSurface = nullptr;
	CodecSurfaceLock.Unlock();

	if (IsInGameThread())
	{
		// enqueue to RT to ensure GL resources are released on the appropriate thread.
		ENQUEUE_RENDER_COMMAND(DestroyElectraTextureSample)([this](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.EnqueueLambda([this](FRHICommandListImmediate& CmdList)
			{
				JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
				JEnv->CallVoidMethod(Object, ReleaseFN.Method);
				if (CodecSurfaceToDelete)
				{
					JEnv->DeleteGlobalRef(CodecSurfaceToDelete);
				}
				JEnv->DeleteGlobalRef(FFrameUpdateInfoClass);
				CodecSurfaceToDelete = nullptr;
				FFrameUpdateInfoClass = 0;
			});
		});
		FlushRenderingCommands();
	}
	else
	{
		JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
		JEnv->CallVoidMethod(Object, ReleaseFN.Method);
		if (CodecSurfaceToDelete)
		{
			JEnv->DeleteGlobalRef(CodecSurfaceToDelete);
		}
		JEnv->DeleteGlobalRef(FFrameUpdateInfoClass);
		CodecSurfaceToDelete = nullptr;
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
	jobject OutputInfo = JEnv->CallObjectMethod(Object, GetVideoFrameUpdateInfoFN.Method, DestTexture, InTargetSample->GetDim().X, InTargetSample->GetDim().Y, InTargetSample->GetFormat() == EMediaTextureSampleFormat::CharBGR10A2);
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
	jobject OutputInfo = JEnv->CallObjectMethod(Object, GetVideoFrameUpdateInfoFN.Method, DestTexture, InTargetSample->GetDim().X, InTargetSample->GetDim().Y, InTargetSample->GetFormat() == EMediaTextureSampleFormat::CharBGR10A2);
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

	// Create a new global ref to return.
	jobject NewSurfaceHandle = nullptr;
	CodecSurfaceLock.Lock();
	if (CodecSurface)
	{
		JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
		NewSurfaceHandle = JEnv->NewGlobalRef(CodecSurface);
	}
	CodecSurfaceLock.Unlock();
	return NewSurfaceHandle;
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

void FElectraTextureSample::Initialize(FVideoDecoderOutput* InVideoDecoderOutput)
{
	IElectraTextureSampleBase::Initialize(InVideoDecoderOutput);
	VideoDecoderOutputAndroid = static_cast<FVideoDecoderOutputAndroid*>(InVideoDecoderOutput);

	if (VideoDecoderOutputAndroid->GetOutputType() == FVideoDecoderOutputAndroid::EOutputType::DirectToSurfaceAsQueue)
	{
		ENQUEUE_RENDER_COMMAND(InitTextureSample)([WeakThis{ AsWeak() }](FRHICommandListImmediate& RHICmdList) {
			if (TSharedPtr<FElectraTextureSample, ESPMode::ThreadSafe> This = WeakThis.Pin())
			{
				This->InitializeTexture(This->VideoDecoderOutputAndroid->GetFormat());

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


void FElectraTextureSample::InitializeTexture(EPixelFormat PixelFormat)
{
	check(IsInRenderingThread() || IsInRHIThread());

	if (FAndroidMisc::ShouldUseVulkan())
	{
		// For Vulkan we use a CPU-side buffer to transport the data
		Texture = nullptr;
		return;
	}

	FIntPoint Dim = VideoDecoderOutput->GetDim();

	if (Texture.IsValid() && (Texture->GetSizeXY() == Dim))
	{
		// The existing texture is just fine...
		return;
	}

	// Make linear texture of appropriate bit depth to carry data...
	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("FElectraTextureSample"))
		.SetExtent(Dim)
		.SetInitialState(ERHIAccess::SRVMask)
		.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
		.SetFormat(PixelFormat);
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


EMediaTextureSampleFormat FElectraTextureSample::GetFormat() const
{
	if (VideoDecoderOutput)
	{
		return (VideoDecoderOutput->GetFormat() == EPixelFormat::PF_A2B10G10R10) ? EMediaTextureSampleFormat::CharBGR10A2 : EMediaTextureSampleFormat::CharBGRA;
	}
	return EMediaTextureSampleFormat::Undefined;
}


uint32 FElectraTextureSample::GetStride() const
{
	// note: we expect RGBA8 or RGB10A2 -> it's always 32 bits
	return GetDim().X * sizeof(uint32);
}


bool FElectraTextureSample::Convert(FTexture2DRHIRef& InDstTexture, const FConversionHints& Hints)
{
	check(IsInRenderingThread());

	if (GDynamicRHI->RHIIsRenderingSuspended())
	{
		return false;
	}

	TRefCountPtr<FRHITexture2D> InputTexture;

	// Either use a texture we have around as a payload or make a temporary one from buffer contents...
	if (!Texture.IsValid())
	{
		auto SampleDim = GetDim();

		// Make a source texture so we can convert from it...
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("FMediaTextureResource"), SampleDim, VideoDecoderOutputAndroid->GetFormat())
			.SetFlags(TexCreate_Dynamic)
			.SetInitialState(ERHIAccess::SRVMask);
		InputTexture = RHICreateTexture(Desc);
		if (!InputTexture.IsValid())
		{
			return false;
		}

		// copy sample data to input render target
		FUpdateTextureRegion2D Region(0, 0, 0, 0, SampleDim.X, SampleDim.Y);
		RHIUpdateTexture2D(InputTexture, 0, Region, GetStride(), (const uint8*)GetBuffer());
	}
	else
	{
		InputTexture = Texture;
	}

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	SCOPED_DRAW_EVENT(RHICmdList, AndroidMediaOutputConvertTexture);
	SCOPED_GPU_STAT(RHICmdList, MediaAndroidDecoder_Convert);

	FIntPoint Dim = VideoDecoderOutput->GetDim();
	FIntPoint OutputDim = VideoDecoderOutput->GetOutputDim();

	RHICmdList.Transition(FRHITransitionInfo(InDstTexture, ERHIAccess::Unknown, ERHIAccess::RTV));
	FRHIRenderPassInfo RPInfo(InDstTexture, ERenderTargetActions::DontLoad_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("AndroidProcessVideo"));

	// Update viewport.
	RHICmdList.SetViewport(0, 0, 0.f, OutputDim.X, OutputDim.Y, 1.f);

	// Setup conversion from Rec2020 to current working color space
	const UE::Color::FColorSpace& Working = UE::Color::FColorSpace::GetWorking();
	FMatrix44f ColorSpaceMtx = FMatrix44f(Working.GetXYZToRgb().GetTransposed() * GetGamutToXYZMatrix());
	if (GetEncodingType() == UE::Color::EEncoding::ST2084)
	{
		// Normalize output (e.g. 80 or 100 nits == 1.0)
		ColorSpaceMtx = ColorSpaceMtx.ApplyScale(GetHDRNitsNormalizationFactor());
	}

	// Get shaders.
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FRGBConvertPS> PixelShader(GlobalShaderMap);
	TShaderMapRef<FMediaShadersVS> VertexShader(GlobalShaderMap);

	// Set the graphic pipeline state.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMediaVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	// Update shader uniform parameters.
	SetShaderParametersLegacyPS(RHICmdList, PixelShader, InputTexture, OutputDim, GetEncodingType(), ColorSpaceMtx);

	RHICmdList.SetStreamSource(0, CreateTempMediaVertexBuffer(), 0);

	RHICmdList.DrawPrimitive(0, 2, 1);

	RHICmdList.EndRenderPass();
	RHICmdList.Transition(FRHITransitionInfo(InDstTexture, ERHIAccess::RTV, ERHIAccess::SRVMask));

	return true;
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptiXDenoiser.h"
#include "Renderer/Public/PathTracingDenoiser.h"
#include <mutex>

#define LOCTEXT_NAMESPACE "FOptiXDenoiseModule"

DEFINE_LOG_CATEGORY(LogOptiXDenoise);

namespace
{
	TAutoConsoleVariable<int32> CVarOptixDenoiseTileWidth(
		TEXT("r.OptixDenoise.TileWidth"),
		256,
		TEXT("Set the tile width for denosing. 0 to use the image size\n"),
		ECVF_RenderThreadSafe
	);
	
	TAutoConsoleVariable<int32> CVarOptixDenoiseTileHeight(
		TEXT("r.OptixDenoise.TileHeight"),
		256,
		TEXT("Set the tile height for denosing\n"),
		ECVF_RenderThreadSafe
	);

}

using namespace UE::OptiXDenoiser;

class FOptiXDenoiseModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static TUniquePtr<FOptiXGPUDenoiser> Denoiser;
	static TUniquePtr<FOptiXFlowEstimator> FlowEstimator;
	static TUniquePtr<FOptiXImageFactory> OptiXImageFactory;

	void* OptiXDenoiseBaseDLLHandle;
};

TUniquePtr<FOptiXGPUDenoiser> FOptiXDenoiseModule::Denoiser = MakeUnique<FOptiXGPUDenoiser>();
TUniquePtr<FOptiXFlowEstimator> FOptiXDenoiseModule::FlowEstimator = MakeUnique<FOptiXFlowEstimator>();
TUniquePtr<FOptiXImageFactory> FOptiXDenoiseModule::OptiXImageFactory = MakeUnique<FOptiXImageFactory>();

static void OpticalFlow(
	FRHICommandListImmediate& RHICmdList,
	FRHITexture* InputTex,// previous frame
	FRHITexture* ReferenceTex,// current frame
	FRHITexture* FlowOutputTex, // empty flow texture to write to
	float PreExposure,
	FRHIGPUMask GPUMask
	)
{
	FOptiXCudaFunctionList::Get().InitializeCudaModule();

	FIntPoint Size = InputTex->GetSizeXY();

	const CUstream& CudaStream = FOptiXDenoiseModule::FlowEstimator->GetCudaStream();
	{
		auto UploadTextureToDenoiser = [&CudaStream, 
			&ImageFactory = FOptiXDenoiseModule::OptiXImageFactory, 
			&FlowEstimator = FOptiXDenoiseModule::FlowEstimator,
			PreExposure]
		(FRHITexture* RHITexture, EOpticalFlowImageType ImageType, const TCHAR* Name)
		{
			if (RHITexture)
			{
				auto OptiXImage = ImageFactory->GetOptiXImage2DAndSetTexture(RHITexture);
				
				if (ImageType != EOpticalFlowImageType::FLOWOUTPUT)
				{
					FOptiXCudaFunctionList::Get().
						CopySurfaceToCudaBuffer(
							OptiXImage->GetCudaBuffer().SurfaceObject,
							OptiXImage->GetRawImage(),
							CudaStream,
							false,
							PreExposure);
				}

				FlowEstimator->SetOptiXImage2D(ImageType, OptiXImage);
			}
			else
			{
				FlowEstimator->SetOptiXImage2D(ImageType, nullptr);
			}
		};

		UploadTextureToDenoiser(InputTex, EOpticalFlowImageType::INPUTFRAME, TEXT("InputFrameTex"));
		UploadTextureToDenoiser(ReferenceTex, EOpticalFlowImageType::REFERENCE, TEXT("ReferenceTex"));
		UploadTextureToDenoiser(FlowOutputTex, EOpticalFlowImageType::FLOWOUTPUT, TEXT("FlowOutputTex"));
	}

	FOptiXDenoiseModule::FlowEstimator->Init(Size.X, Size.Y);
	FOptiXDenoiseModule::FlowEstimator->Commit();

	auto OutputImage = FOptiXDenoiseModule::FlowEstimator->GetOptiXImage2D(EOpticalFlowImageType::FLOWOUTPUT);
	OutputImage->Flush(ECUDASurfaceCopyType::BufferToSurface, CudaStream);

	CUDA_CHECK(FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext()));
	CUDA_CHECK(FCUDAModule::CUDA().cuStreamSynchronize(CudaStream));
	CUDA_CHECK(FCUDAModule::CUDA().cuCtxPopCurrent(NULL));

	FOptiXDenoiseModule::OptiXImageFactory->FlushImages();

	UE_LOG(LogOptiXDenoise, Log, TEXT("Estimate Optical Flow (%d x %d)"), Size.X, Size.Y);
}

static void Denoise(
	FRHICommandListImmediate& RHICmdList,
	FRHITexture* ColorTex,
	FRHITexture* AlbedoTex,
	FRHITexture* NormalTex,
	FRHITexture* OutputTex,
	const FDenoisingArgumentsExt* DenoisingArgumentExt,
	FRHIGPUMask GPUMask)
{
	FOptiXCudaFunctionList::Get().InitializeCudaModule();

	FIntPoint Size = ColorTex->GetSizeXY();

	static std::once_flag CallOnceFlag;
	std::call_once(CallOnceFlag, []() {
			CUcontext CudaContext = FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext();
			FCUDAModule::CUDA().cuCtxPushCurrent(CudaContext);
			unsigned int CUDAAPIVersion = 0;
			FCUDAModule::CUDA().cuCtxGetApiVersion(CudaContext, &CUDAAPIVersion);

			FCUDAModule::CUDA().cuCtxPopCurrent(NULL);
			UE_LOG(LogOptiXDenoise, Log, TEXT("CUDA API Version:%d"), CUDAAPIVersion);
		});

	// (Re)initialize the denoiser if this is the first frame (e.g., new cut) or the image size has changed
	if (DenoisingArgumentExt->bForceSpatialDenoiserOnly ||
		FOptiXDenoiseModule::Denoiser->IsImageSizeChanged(Size.X, Size.Y))
	{
		FOptiXDenoiseModule::Denoiser->Finish();
		FOptiXDenoiseModule::Denoiser->Init(
			Size.X,
			Size.Y,
			EOptiXImageFormat::CUDA_A32B32G32R32_F,
			CVarOptixDenoiseTileWidth.GetValueOnAnyThread(),
			CVarOptixDenoiseTileHeight.GetValueOnAnyThread());

		UE_LOG(LogOptiXDenoise, Log, TEXT("Denoising Frame %d (%d x %d) Spatially"),
			DenoisingArgumentExt->DenoisingFrameId, Size.X, Size.Y);
	}
	else
	{
		UE_LOG(LogOptiXDenoise, Log, TEXT("Denoising Frame %d (%d x %d) temporally"),
			DenoisingArgumentExt->DenoisingFrameId, Size.X, Size.Y);
	}

	const CUstream& CudaStream = FOptiXDenoiseModule::Denoiser->GetCudaStream();
	{
		auto UploadTextureToDenoiser = [&CudaStream, &ImageFactory = FOptiXDenoiseModule::OptiXImageFactory, &Denoiser = FOptiXDenoiseModule::Denoiser]
		(FRHITexture* RHITexture, EDenoisingImageType ImageType, const TCHAR* Name)
		{
			if (RHITexture)
			{
				auto OptiXImage = ImageFactory->GetOptiXImage2DAndSetTexture(RHITexture);
				
				if (ImageType == EDenoisingImageType::NORMAL)
				{
					const bool bIsNormalInUEViewSpace = true;
					
					FOptiXCudaFunctionList::Get().
						CopySurfaceToCudaBuffer(
							OptiXImage->GetCudaBuffer().SurfaceObject,
							OptiXImage->GetRawImage(),
							CudaStream,
							bIsNormalInUEViewSpace);
				}
				else if (ImageType != EDenoisingImageType::OUTPUT)
				{
					OptiXImage->Flush(ECUDASurfaceCopyType::SurfaceToBuffer, CudaStream);
				}
				Denoiser->SetOptiXImage2D(ImageType, OptiXImage);
			}
			else
			{
				Denoiser->SetOptiXImage2D(ImageType, nullptr);
			}
		};

		UploadTextureToDenoiser(ColorTex, EDenoisingImageType::COLOR, TEXT("ColorTex"));
		UploadTextureToDenoiser(AlbedoTex, EDenoisingImageType::ALBEDO, TEXT("AlbedoTex"));
		UploadTextureToDenoiser(NormalTex, EDenoisingImageType::NORMAL, TEXT("NormalTex"));
		UploadTextureToDenoiser(DenoisingArgumentExt->FlowTex, EDenoisingImageType::FLOW, TEXT("FlowTex"));
		UploadTextureToDenoiser(DenoisingArgumentExt->PreviousOutputTex, EDenoisingImageType::PREVOUTPUT, TEXT("PreviousOutputTex"));
		UploadTextureToDenoiser(OutputTex, EDenoisingImageType::OUTPUT, TEXT("OutputTex"));
	}

	FOptiXDenoiseModule::Denoiser->Commit();

	auto OutputImage = FOptiXDenoiseModule::Denoiser->GetOptiXImage2D(EDenoisingImageType::OUTPUT);
	OutputImage->Flush(ECUDASurfaceCopyType::BufferToSurface, CudaStream);

	// Sync with RDG instead?
	CUDA_CHECK(FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext()));
	CUDA_CHECK(FCUDAModule::CUDA().cuStreamSynchronize(CudaStream));
	CUDA_CHECK(FCUDAModule::CUDA().cuCtxPopCurrent(NULL));

	FOptiXDenoiseModule::OptiXImageFactory->FlushImages();
}

void FOptiXDenoiseModule::StartupModule()
{
#if WITH_EDITOR
	UE_LOG(LogOptiXDenoise, Log, TEXT("OptiXDenoise starting up"));
#endif

	// Ensure we have the access to CUDA
	FModuleManager::LoadModuleChecked<FCUDAModule>("CUDA");

	// Register custom CUDA function list for OptiX
	// The initialization should be postponed during function call;
	FString OptiXDenoiseBaseDllPath = IPluginManager::Get().FindPlugin("OptiXDenoise")->GetBaseDir() + "/Source/ThirdParty/OptiXDenoiseBase/lib/OptiXDenoiseBase.dll";
	OptiXDenoiseBaseDLLHandle = FPlatformProcess::GetDllHandle(*OptiXDenoiseBaseDllPath);
	FOptiXCudaFunctionList::Get().RegisterFunctionInstance<FOptiXDenoiserFunctionInstance>();

	GPathTracingSpatialTemporalDenoiserFunc = &Denoise;
	GPathTracingMotionVectorFunc = &OpticalFlow;
}

void FOptiXDenoiseModule::ShutdownModule()
{
#if WITH_EDITOR
	UE_LOG(LogOptiXDenoise, Log, TEXT("OptiXDenoise shutting down"));
#endif
	GPathTracingSpatialTemporalDenoiserFunc = nullptr;
	GPathTracingMotionVectorFunc = nullptr;

	// Assure resources related to CUDA is released before the releasing of CUDA module.
	Denoiser.Release();
	FlowEstimator.Release();
	OptiXImageFactory.Release();

	// Unload function list manually to ensure CUDA model is still active.
	FOptiXCudaFunctionList::Get().ShutDown();
	FPlatformProcess::FreeDllHandle(OptiXDenoiseBaseDLLHandle);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FOptiXDenoiseModule, OptiXDenoise)
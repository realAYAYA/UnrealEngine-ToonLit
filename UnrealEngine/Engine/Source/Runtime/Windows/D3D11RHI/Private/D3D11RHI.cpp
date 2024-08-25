// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11RHI.cpp: Unreal D3D RHI library implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"
#include "RHIStaticStates.h"
#include "StaticBoundShaderState.h"
#include "Engine/GameViewportClient.h"
#include "ProfilingDebugging/MemoryTrace.h"
#include "RHICoreStats.h"

#if WITH_DX_PERF
	// For perf events
	#include "Windows/AllowWindowsPlatformTypes.h"
		#include "d3d9.h"
	#include "Windows/HideWindowsPlatformTypes.h"
#endif	//WITH_DX_PERF
#include "OneColorShader.h"

DEFINE_LOG_CATEGORY(LogD3D11RHI);

extern void UniformBufferBeginFrame();

// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
// The following line is to favor the high performance NVIDIA GPU if there are multiple GPUs
// Has to be .exe module to be correctly detected.
// extern "C" { _declspec(dllexport) uint32 NvOptimusEnablement = 0x00000001; }

void FD3D11DynamicRHI::RHIBeginFrame()
{
	UniformBufferBeginFrame();
	GPUProfilingData.BeginFrame(this);
}

template <int32 Frequency>
void ClearShaderResource(ID3D11DeviceContext* Direct3DDeviceIMContext, uint32 ResourceIndex)
{
	ID3D11ShaderResourceView* NullView = NULL;
	switch(Frequency)
	{
	case SF_Pixel:   Direct3DDeviceIMContext->PSSetShaderResources(ResourceIndex,1,&NullView); break;
	case SF_Compute: Direct3DDeviceIMContext->CSSetShaderResources(ResourceIndex,1,&NullView); break;
	case SF_Geometry:Direct3DDeviceIMContext->GSSetShaderResources(ResourceIndex,1,&NullView); break;
	case SF_Vertex:  Direct3DDeviceIMContext->VSSetShaderResources(ResourceIndex,1,&NullView); break;
	};
}

void FD3D11DynamicRHI::ClearState()
{
	StateCache.ClearState();

	FMemory::Memzero(CurrentResourcesBoundAsSRVs, sizeof(CurrentResourcesBoundAsSRVs));
	FMemory::Memzero(CurrentResourcesBoundAsVBs, sizeof(CurrentResourcesBoundAsVBs));
	CurrentResourceBoundAsIB = nullptr;
	for (int32 Frequency = 0; Frequency < SF_NumStandardFrequencies; Frequency++)
	{
		MaxBoundShaderResourcesIndex[Frequency] = INDEX_NONE;
	}
	MaxBoundVertexBufferIndex = INDEX_NONE;
}

void GetMipAndSliceInfoFromSRV(ID3D11ShaderResourceView* SRV, int32& MipLevel, int32& NumMips, int32& ArraySlice, int32& NumSlices)
{
	MipLevel = -1;
	NumMips = -1;
	ArraySlice = -1;
	NumSlices = -1;

	if (SRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRV->GetDesc(&SRVDesc);
		switch (SRVDesc.ViewDimension)
		{			
			case D3D11_SRV_DIMENSION_TEXTURE1D:
				MipLevel	= SRVDesc.Texture1D.MostDetailedMip;
				NumMips		= SRVDesc.Texture1D.MipLevels;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
				MipLevel	= SRVDesc.Texture1DArray.MostDetailedMip;
				NumMips		= SRVDesc.Texture1DArray.MipLevels;
				ArraySlice	= SRVDesc.Texture1DArray.FirstArraySlice;
				NumSlices	= SRVDesc.Texture1DArray.ArraySize;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE2D:
				MipLevel	= SRVDesc.Texture2D.MostDetailedMip;
				NumMips		= SRVDesc.Texture2D.MipLevels;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
				MipLevel	= SRVDesc.Texture2DArray.MostDetailedMip;
				NumMips		= SRVDesc.Texture2DArray.MipLevels;
				ArraySlice	= SRVDesc.Texture2DArray.FirstArraySlice;
				NumSlices	= SRVDesc.Texture2DArray.ArraySize;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE2DMS:
				MipLevel	= 0;
				NumMips		= 1;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
				MipLevel	= 0;
				NumMips		= 1;
				ArraySlice	= SRVDesc.Texture2DMSArray.FirstArraySlice;
				NumSlices	= SRVDesc.Texture2DMSArray.ArraySize;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE3D:
				MipLevel	= SRVDesc.Texture3D.MostDetailedMip;
				NumMips		= SRVDesc.Texture3D.MipLevels;
				break;
			case D3D11_SRV_DIMENSION_TEXTURECUBE:
				MipLevel = SRVDesc.TextureCube.MostDetailedMip;
				NumMips		= SRVDesc.TextureCube.MipLevels;
				break;
			case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
				MipLevel	= SRVDesc.TextureCubeArray.MostDetailedMip;
				NumMips		= SRVDesc.TextureCubeArray.MipLevels;
				ArraySlice	= SRVDesc.TextureCubeArray.First2DArrayFace;
				NumSlices	= SRVDesc.TextureCubeArray.NumCubes;
				break;
			case D3D11_SRV_DIMENSION_BUFFER:
			case D3D11_SRV_DIMENSION_BUFFEREX:
			default:
				break;
		}
	}
}

template <EShaderFrequency ShaderFrequency>
void FD3D11DynamicRHI::InternalSetShaderResourceView(FD3D11ViewableResource* Resource, ID3D11ShaderResourceView* SRV, int32 ResourceIndex)
{
	// Check either both are set, or both are null.
	check((Resource && SRV) || (!Resource && !SRV));

	//avoid state cache crash
	if (!((Resource && SRV) || (!Resource && !SRV)))
	{
		//UE_LOG(LogRHI, Warning, TEXT("Bailing on InternalSetShaderResourceView on resource: %i, %s"), ResourceIndex, *SRVName.ToString());
		return;
	}

	FD3D11ViewableResource*& ResourceSlot = CurrentResourcesBoundAsSRVs[ShaderFrequency][ResourceIndex];
	int32& MaxResourceIndex = MaxBoundShaderResourcesIndex[ShaderFrequency];

	if (Resource)
	{
		// We are binding a new SRV.
		// Update the max resource index to the highest bound resource index.
		MaxResourceIndex = FMath::Max(MaxResourceIndex, ResourceIndex);
		ResourceSlot = Resource;
	}
	else if (ResourceSlot != nullptr)
	{
		// Unbind the resource from the slot.
		ResourceSlot = nullptr;

		// If this was the highest bound resource...
		if (MaxResourceIndex == ResourceIndex)
		{
			// Adjust the max resource index downwards until we
			// hit the next non-null slot, or we've run out of slots.
			do
			{
				MaxResourceIndex--;
			}
			while (MaxResourceIndex >= 0 && CurrentResourcesBoundAsSRVs[ShaderFrequency][MaxResourceIndex] == nullptr);
		} 
	}

	// Set the SRV we have been given (or null).
	StateCache.SetShaderResourceView<ShaderFrequency>(SRV, ResourceIndex);
}

template void FD3D11DynamicRHI::InternalSetShaderResourceView<SF_Vertex>  (FD3D11ViewableResource* Resource, ID3D11ShaderResourceView* SRV, int32 ResourceIndex);
template void FD3D11DynamicRHI::InternalSetShaderResourceView<SF_Pixel>   (FD3D11ViewableResource* Resource, ID3D11ShaderResourceView* SRV, int32 ResourceIndex);
template void FD3D11DynamicRHI::InternalSetShaderResourceView<SF_Geometry>(FD3D11ViewableResource* Resource, ID3D11ShaderResourceView* SRV, int32 ResourceIndex);
template void FD3D11DynamicRHI::InternalSetShaderResourceView<SF_Compute> (FD3D11ViewableResource* Resource, ID3D11ShaderResourceView* SRV, int32 ResourceIndex);

void FD3D11DynamicRHI::TrackResourceBoundAsVB(FD3D11ViewableResource* Resource, int32 StreamIndex)
{
	check(StreamIndex >= 0 && StreamIndex < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
	if (Resource)
	{
		// We are binding a new VB.
		// Update the max resource index to the highest bound resource index.
		MaxBoundVertexBufferIndex = FMath::Max(MaxBoundVertexBufferIndex, StreamIndex);
		CurrentResourcesBoundAsVBs[StreamIndex] = Resource;
	}
	else if (CurrentResourcesBoundAsVBs[StreamIndex] != nullptr)
	{
		// Unbind the resource from the slot.
		CurrentResourcesBoundAsVBs[StreamIndex] = nullptr;

		// If this was the highest bound resource...
		if (MaxBoundVertexBufferIndex == StreamIndex)
		{
			// Adjust the max resource index downwards until we
			// hit the next non-null slot, or we've run out of slots.
			do
			{
				MaxBoundVertexBufferIndex--;
			} while (MaxBoundVertexBufferIndex >= 0 && CurrentResourcesBoundAsVBs[MaxBoundVertexBufferIndex] == nullptr);
		}
	}
}

void FD3D11DynamicRHI::TrackResourceBoundAsIB(FD3D11ViewableResource* Resource)
{
	CurrentResourceBoundAsIB = Resource;
}

template <EShaderFrequency ShaderFrequency>
void FD3D11DynamicRHI::ClearShaderResourceViews(FD3D11ViewableResource* Resource)
{
	int32 MaxIndex = MaxBoundShaderResourcesIndex[ShaderFrequency];
	for (int32 ResourceIndex = MaxIndex; ResourceIndex >= 0; --ResourceIndex)
	{
		if (CurrentResourcesBoundAsSRVs[ShaderFrequency][ResourceIndex] == Resource)
		{
			// Unset the SRV from the device context
			SetShaderResourceView<ShaderFrequency>(nullptr, nullptr, ResourceIndex);
		}
	}
}

void FD3D11DynamicRHI::ConditionalClearShaderResource(FD3D11ViewableResource* Resource, bool bCheckBoundInputAssembler)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11ClearShaderResourceTime);
	check(Resource);
	ClearShaderResourceViews<SF_Vertex>(Resource);
	ClearShaderResourceViews<SF_Pixel>(Resource);
	ClearShaderResourceViews<SF_Geometry>(Resource);
	ClearShaderResourceViews<SF_Compute>(Resource);

	if (bCheckBoundInputAssembler)
	{
		for (int32 ResourceIndex = MaxBoundVertexBufferIndex; ResourceIndex >= 0; --ResourceIndex)
		{
			if (CurrentResourcesBoundAsVBs[ResourceIndex] == Resource)
			{
				// Unset the vertex buffer from the device context
				TrackResourceBoundAsVB(nullptr, ResourceIndex);
				StateCache.SetStreamSource(nullptr, ResourceIndex, 0);
			}
		}

		if (Resource == CurrentResourceBoundAsIB)
		{
			TrackResourceBoundAsIB(nullptr);
			StateCache.SetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
		}
	}
}

template <EShaderFrequency ShaderFrequency>
void FD3D11DynamicRHI::ClearAllShaderResourcesForFrequency()
{
	int32 MaxIndex = MaxBoundShaderResourcesIndex[ShaderFrequency];
	for (int32 ResourceIndex = MaxIndex; ResourceIndex >= 0; --ResourceIndex)
	{
		if (CurrentResourcesBoundAsSRVs[ShaderFrequency][ResourceIndex] != nullptr)
		{
			// Unset the SRV from the device context
			SetShaderResourceView<ShaderFrequency>(nullptr, nullptr, ResourceIndex);
		}
	}
	StateCache.ClearConstantBuffers<ShaderFrequency>();
}

void FD3D11DynamicRHI::ClearAllShaderResources()
{
	ClearAllShaderResourcesForFrequency<SF_Vertex>();
	ClearAllShaderResourcesForFrequency<SF_Geometry>();
	ClearAllShaderResourcesForFrequency<SF_Pixel>();
	ClearAllShaderResourcesForFrequency<SF_Compute>();
}

void FD3DGPUProfiler::BeginFrame(FD3D11DynamicRHI* InRHI)
{
	CurrentEventNode = NULL;
	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	static auto* CrashCollectionEnableCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.gpucrash.collectionenable"));
	static auto* CrashCollectionDataDepth = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.gpucrash.datadepth"));

	bTrackingGPUCrashData = CrashCollectionEnableCvar ? CrashCollectionEnableCvar->GetValueOnRenderThread() != 0 : false;
	GPUCrashDataDepth = CrashCollectionDataDepth ? CrashCollectionDataDepth->GetValueOnRenderThread() : -1;
	
	// latch the bools from the game thread into our private copy
	bLatchedGProfilingGPU = GTriggerGPUProfile;
	bLatchedGProfilingGPUHitches = GTriggerGPUHitchProfile;
	if (bLatchedGProfilingGPUHitches)
	{
		bLatchedGProfilingGPU = false; // we do NOT permit an ordinary GPU profile during hitch profiles
	}

	// if we are starting a hitch profile or this frame is a gpu profile, then save off the state of the draw events
	if (bLatchedGProfilingGPU || (!bPreviousLatchedGProfilingGPUHitches && bLatchedGProfilingGPUHitches))
	{
		bOriginalGEmitDrawEvents = GetEmitDrawEvents();
	}

	if (bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches)
	{
		if (bLatchedGProfilingGPUHitches && GPUHitchDebounce)
		{
			// if we are doing hitches and we had a recent hitch, wait to recover
			// the reasoning is that collecting the hitch report may itself hitch the GPU
			GPUHitchDebounce--; 
		}
		else
		{
			SetEmitDrawEvents(true);  // thwart an attempt to turn this off on the game side
			bTrackingEvents = true;
			CurrentEventNodeFrame = new FD3D11EventNodeFrame(InRHI);
			CurrentEventNodeFrame->StartFrame();
		}
	}
	else if (bPreviousLatchedGProfilingGPUHitches)
	{
		// hitch profiler is turning off, clear history and restore draw events
		GPUHitchEventNodeFrames.Empty();
		SetEmitDrawEvents(bOriginalGEmitDrawEvents);
	}
	bPreviousLatchedGProfilingGPUHitches = bLatchedGProfilingGPUHitches;

	FrameTiming.StartTiming();

	if (GetEmitDrawEvents())
	{
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
	}
}

void FD3D11DynamicRHI::RHIEndFrame()
{
	GPUProfilingData.EndFrame();
	CurrentComputeShader = nullptr;
}

void FD3DGPUProfiler::EndFrame()
{
	if (GetEmitDrawEvents())
	{
		PopEvent();
	}

	FrameTiming.EndTiming();

	if (FrameTiming.IsSupported())
	{
		uint64 GPUTiming = FrameTiming.GetTiming();
		uint64 GPUFreq = FrameTiming.GetTimingFrequency();
		GGPUFrameTime = FMath::TruncToInt( double(GPUTiming) / double(GPUFreq) / FPlatformTime::GetSecondsPerCycle() );
	}
	else
	{
		GGPUFrameTime = 0;
	}

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
	}

	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	check(!bTrackingEvents || CurrentEventNodeFrame);
	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			SetEmitDrawEvents(bOriginalGEmitDrawEvents);
			UE_LOG(LogD3D11RHI, Warning, TEXT(""));
			UE_LOG(LogD3D11RHI, Warning, TEXT(""));
			CurrentEventNodeFrame->DumpEventTree();
			GTriggerGPUProfile = false;
			bLatchedGProfilingGPU = false;

			if (RHIConfig::ShouldSaveScreenshotAfterProfilingGPU()
				&& GEngine->GameViewport)
			{
				GEngine->GameViewport->Exec( NULL, TEXT("SCREENSHOT"), *GLog);
			}
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		//@todo this really detects any hitch, even one on the game thread.
		// it would be nice to restrict the test to stalls on D3D, but for now...
		// this needs to be out here because bTrackingEvents is false during the hitch debounce
		static double LastTime = -1.0;
		double Now = FPlatformTime::Seconds();
		if (bTrackingEvents)
		{
			/** How long, in seconds a frame much be to be considered a hitch **/
			const float HitchThreshold = RHIConfig::GetGPUHitchThreshold();
			float ThisTime = Now - LastTime;
			bool bHitched = (ThisTime > HitchThreshold) && LastTime > 0.0 && CurrentEventNodeFrame;
			if (bHitched)
			{
				UE_LOG(LogD3D11RHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogD3D11RHI, Warning, TEXT("********** Hitch detected on CPU, frametime = %6.1fms"),ThisTime * 1000.0f);
				UE_LOG(LogD3D11RHI, Warning, TEXT("*******************************************************************************"));

				for (int32 Frame = 0; Frame < GPUHitchEventNodeFrames.Num(); Frame++)
				{
					UE_LOG(LogD3D11RHI, Warning, TEXT(""));
					UE_LOG(LogD3D11RHI, Warning, TEXT(""));
					UE_LOG(LogD3D11RHI, Warning, TEXT("********** GPU Frame: Current - %d"),GPUHitchEventNodeFrames.Num() - Frame);
					GPUHitchEventNodeFrames[Frame].DumpEventTree();
				}
				UE_LOG(LogD3D11RHI, Warning, TEXT(""));
				UE_LOG(LogD3D11RHI, Warning, TEXT(""));
				UE_LOG(LogD3D11RHI, Warning, TEXT("********** GPU Frame: Current"));
				CurrentEventNodeFrame->DumpEventTree();

				UE_LOG(LogD3D11RHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogD3D11RHI, Warning, TEXT("********** End Hitch GPU Profile"));
				UE_LOG(LogD3D11RHI, Warning, TEXT("*******************************************************************************"));
				if (GEngine->GameViewport)
				{
					GEngine->GameViewport->Exec( NULL, TEXT("SCREENSHOT"), *GLog);
				}

				GPUHitchDebounce = 5; // don't trigger this again for a while
				GPUHitchEventNodeFrames.Empty(); // clear history
			}
			else if (CurrentEventNodeFrame) // this will be null for discarded frames while recovering from a recent hitch
			{
				/** How many old frames to buffer for hitch reports **/
				static const int32 HitchHistorySize = 4;

				if (GPUHitchEventNodeFrames.Num() >= HitchHistorySize)
				{
					GPUHitchEventNodeFrames.RemoveAt(0);
				}
				GPUHitchEventNodeFrames.Add((FD3D11EventNodeFrame*)CurrentEventNodeFrame);
				CurrentEventNodeFrame = NULL;  // prevent deletion of this below; ke kept it in the history
			}
		}
		LastTime = Now;
	}
	bTrackingEvents = false;
	bTrackingGPUCrashData = false;
	delete CurrentEventNodeFrame;
	CurrentEventNodeFrame = NULL;
}

float FD3D11EventNode::GetTiming()
{
	float Result = 0;

	if (Timing.IsSupported())
	{
		// Get the timing result and block the CPU until it is ready
		const uint64 GPUTiming = Timing.GetTiming(true);
		const uint64 GPUFreq = Timing.GetTimingFrequency();

		Result = double(GPUTiming) / double(GPUFreq);
	}

	return Result;
}

void FD3D11DynamicRHI::RHIBeginScene()
{
	// Increment the frame counter. INDEX_NONE is a special value meaning "uninitialized", so if
	// we hit it just wrap around to zero.
	SceneFrameCounter++;
	if (SceneFrameCounter == INDEX_NONE)
	{
		SceneFrameCounter++;
	}

	static auto* ResourceTableCachingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("rhi.ResourceTableCaching"));
	if (ResourceTableCachingCvar == NULL || ResourceTableCachingCvar->GetValueOnAnyThread() == 1)
	{
		ResourceTableFrameCounter = SceneFrameCounter;
	}
}

void FD3D11DynamicRHI::RHIEndScene()
{
	ResourceTableFrameCounter = INDEX_NONE;
}

static FString EventDeepString(TEXT("EventTooDeep"));
static const uint32 EventDeepCRC = FCrc::StrCrc32<TCHAR>(*EventDeepString);

FD3DGPUProfiler::FD3DGPUProfiler(class FD3D11DynamicRHI* InD3DRHI) :
	FGPUProfiler(),
	FrameTiming(InD3DRHI, 4),
	D3D11RHI(InD3DRHI)
{
	// Initialize Buffered timestamp queries 
	FrameTiming.InitResource(FRHICommandListImmediate::Get());
	CachedStrings.Emplace(EventDeepCRC, EventDeepString);
}

void FD3DGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
#if NV_AFTERMATH
	if(GDX11NVAfterMathEnabled && bTrackingGPUCrashData && GDX11NVAfterMathMarkers)
	{
		uint32 CRC = 0;
		if (GPUCrashDataDepth < 0 || PushPopStack.Num() < GPUCrashDataDepth)
		{
			CRC = FCrc::StrCrc32<TCHAR>(Name);

		if (CachedStrings.Num() > 10000)
		{
			CachedStrings.Empty(10000);
				CachedStrings.Emplace(EventDeepCRC, EventDeepString);
		}

		if (CachedStrings.Find(CRC) == nullptr)
		{
			CachedStrings.Emplace(CRC, FString(Name));
		}
				
		}
		else
		{
			CRC = EventDeepCRC;				
		}
		PushPopStack.Push(CRC);

		auto AftermathContext = D3D11RHI->GetNVAftermathContext();
		GFSDK_Aftermath_SetEventMarker(AftermathContext, &PushPopStack[0], PushPopStack.Num() * sizeof(uint32));
	}
#endif

#if WITH_DX_PERF
	D3DPERF_BeginEvent(Color.DWColor(),Name);
#endif

	FGPUProfiler::PushEvent(Name, Color);
}

void FD3DGPUProfiler::PopEvent()
{
#if NV_AFTERMATH
	if (GDX11NVAfterMathEnabled && bTrackingGPUCrashData && GDX11NVAfterMathMarkers)
	{
		PushPopStack.Pop(EAllowShrinking::No);
	}
#endif

#if WITH_DX_PERF
	D3DPERF_EndEvent();
#endif

	FGPUProfiler::PopEvent();
}

extern CORE_API bool GIsGPUCrashed;
bool FD3DGPUProfiler::CheckGpuHeartbeat(bool bShowActiveStatus) const
{
#if NV_AFTERMATH
#define NVAFTERMATH_ON_ERROR() do { if (D3D11RHI) { D3D11RHI->StopNVAftermath(); GDX11NVAfterMathEnabled = false; } } while (false)

	if (GDX11NVAfterMathEnabled && bTrackingGPUCrashData)
	{
		GFSDK_Aftermath_Device_Status Status;
		GFSDK_Aftermath_Result Result;
		{
			FScopedD3D11RHIThreadStaller StallRHIThread;
			Result = GFSDK_Aftermath_GetDeviceStatus(&Status);
		}
		if (Result == GFSDK_Aftermath_Result_Success)
		{
			if (Status != GFSDK_Aftermath_Device_Status_Active || bShowActiveStatus)
			{
				GIsGPUCrashed = true;
				const TCHAR* AftermathReason[] = { TEXT("Active"), TEXT("Timeout"), TEXT("OutOfMemory"), TEXT("PageFault"), TEXT("Stopped"), TEXT("Reset"), TEXT("Unknown"), TEXT("DmaFault") };
				if (Status < UE_ARRAY_COUNT(AftermathReason))
				{
					UE_LOG(LogRHI, Error, TEXT("[Aftermath] Status: %s"), AftermathReason[Status]);
				}
				else
				{
					UE_LOG(LogRHI, Error, TEXT("[Aftermath] Invalid Status result value: %u"), Status);
				}
				GFSDK_Aftermath_ContextHandle AftermathContext = D3D11RHI->GetNVAftermathContext();

				if (AftermathContext)
				{
					GFSDK_Aftermath_ContextData ContextDataOut;
					{
						FScopedD3D11RHIThreadStaller StallRHIThread;
						Result = GFSDK_Aftermath_GetData(1, &AftermathContext, &ContextDataOut);
					}
					if (Result == GFSDK_Aftermath_Result_Success)
					{
						UE_LOG(LogRHI, Error, TEXT("[Aftermath] GPU Stack Dump"));
						uint32 NumCRCs = ContextDataOut.markerSize / sizeof(uint32);
						uint32* Data = (uint32*)ContextDataOut.markerData;
						for (uint32 i = 0; i < NumCRCs; i++)
						{
							const FString* Frame = CachedStrings.Find(Data[i]);
							if (Frame != nullptr)
							{
								UE_LOG(LogRHI, Error, TEXT("[Aftermath] %i: %s"), i, *(*Frame));
							}
						}
						UE_LOG(LogRHI, Error, TEXT("[Aftermath] GPU Stack Dump"));
					}
					else
					{
						UE_LOG(LogRHI, Error, TEXT("[Aftermath] GFSDK_Aftermath_GetData failed with result: 0x%08X"), (uint32)Result);
						NVAFTERMATH_ON_ERROR();
					}
				}
				else
				{
					UE_LOG(LogRHI, Error, TEXT("[Aftermath] Invalid context handle"));
					NVAFTERMATH_ON_ERROR();
				}
				GLog->Flush();
				return false;
			}
		}
		else
		{
			UE_LOG(LogRHI, Error, TEXT("[Aftermath] GFSDK_Aftermath_GetDeviceStatus failed with result: 0x%08X"), (uint32)Result);
			NVAFTERMATH_ON_ERROR();
		}
	}
#undef NVAFTERMATH_ON_ERROR
#endif
	return true;
}

/** Start this frame of per tracking */
void FD3D11EventNodeFrame::StartFrame()
{
	EventTree.Reset();
	DisjointQuery.StartTracking();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FD3D11EventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
	DisjointQuery.EndTracking();
}

float FD3D11EventNodeFrame::GetRootTimingResults()
{
	double RootResult = 0.0f;
	if (RootEventTiming.IsSupported())
	{
		const uint64 GPUTiming = RootEventTiming.GetTiming(true);
		const uint64 GPUFreq = RootEventTiming.GetTimingFrequency();

		RootResult = double(GPUTiming) / double(GPUFreq);
	}

	return (float)RootResult;
}

void FD3D11EventNodeFrame::LogDisjointQuery()
{
	if (!DisjointQuery.IsResultValid())
	{
		UE_LOG(LogRHI, Warning, TEXT("%s"), TEXT("Profiled range was disjoint!  GPU switched to doing something else while profiling."));
	}
}

static void D3D11UpdateBufferStatsCommon(ID3D11Buffer* Buffer, int64 BufferSize, bool bAllocating)
{
	// this is a work-around on Windows. Due to the fact that there is no way
	// to hook the actual d3d allocations we can't track the memory in the normal way.
	// Instead we simply tell LLM the size of these resources.

	LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(ELLMTag::GraphicsPlatform, bAllocating ? BufferSize : -BufferSize, ELLMTracker::Platform, ELLMAllocType::None);

#if UE_MEMORY_TRACE_ENABLED
	if (bAllocating)
	{
		MemoryTrace_Alloc((uint64)Buffer, BufferSize, 0, EMemoryTraceRootHeap::VideoMemory);
	}
	else
	{
		MemoryTrace_Free((uint64)Buffer, EMemoryTraceRootHeap::VideoMemory);
	}
#endif
}

void D3D11BufferStats::UpdateUniformBufferStats(ID3D11Buffer* Buffer, int64 BufferSize, bool bAllocating)
{
	UE::RHICore::UpdateGlobalUniformBufferStats(BufferSize, bAllocating);
	D3D11UpdateBufferStatsCommon(Buffer, BufferSize, bAllocating);
}

void D3D11BufferStats::UpdateBufferStats(FD3D11Buffer& Buffer, bool bAllocating)
{
	if (ID3D11Buffer* Resource = Buffer.Resource)
	{
		const FRHIBufferDesc& BufferDesc = Buffer.GetDesc();

		UE::RHICore::UpdateGlobalBufferStats(BufferDesc, BufferDesc.Size, bAllocating);
		D3D11UpdateBufferStatsCommon(Resource, BufferDesc.Size, bAllocating);
	}
}

ID3D11Device* FD3D11DynamicRHI::RHIGetDevice() const
{
	return GetDevice();
}

ID3D11DeviceContext* FD3D11DynamicRHI::RHIGetDeviceContext() const
{
	return GetDeviceContext();
}

IDXGIAdapter* FD3D11DynamicRHI::RHIGetAdapter() const
{
	return GetAdapter().DXGIAdapter;
}

IDXGISwapChain* FD3D11DynamicRHI::RHIGetSwapChain(FRHIViewport* InViewport) const
{
	FD3D11Viewport* Viewport = static_cast<FD3D11Viewport*>(InViewport);
	return Viewport->GetSwapChain();
}

DXGI_FORMAT FD3D11DynamicRHI::RHIGetSwapChainFormat(EPixelFormat InFormat) const
{
	const DXGI_FORMAT PlatformFormat = UE::DXGIUtilities::FindDepthStencilFormat(static_cast<DXGI_FORMAT>(GPixelFormats[InFormat].PlatformFormat));
	return UE::DXGIUtilities::FindShaderResourceFormat(PlatformFormat, true);
}

ID3D11Buffer* FD3D11DynamicRHI::RHIGetResource(FRHIBuffer* InBuffer) const
{
	FD3D11Buffer* Buffer = ResourceCast(InBuffer);
	return Buffer->Resource;
}

ID3D11Resource* FD3D11DynamicRHI::RHIGetResource(FRHITexture* InTexture) const
{
	FD3D11Texture* D3D11Texture = ResourceCast(InTexture);
	return D3D11Texture->GetResource();
}

int64 FD3D11DynamicRHI::RHIGetResourceMemorySize(FRHITexture* InTexture) const
{
	FD3D11Texture* D3D11Texture = ResourceCast(InTexture);
	return D3D11Texture->GetMemorySize();
}

ID3D11RenderTargetView* FD3D11DynamicRHI::RHIGetRenderTargetView(FRHITexture* InTexture, int32 InMipIndex, int32 InArraySliceIndex) const
{
	FD3D11Texture* D3D11Texture = ResourceCast(InTexture);
	return D3D11Texture->GetRenderTargetView(InMipIndex, InArraySliceIndex);
}

ID3D11ShaderResourceView* FD3D11DynamicRHI::RHIGetShaderResourceView(FRHITexture* InTexture) const
{
	FD3D11Texture* D3D11Texture = ResourceCast(InTexture);
	return D3D11Texture->GetShaderResourceView();
}

void FD3D11DynamicRHI::RHIRegisterWork(uint32 NumPrimitives)
{
	RegisterGPUWork(NumPrimitives);
}

void FD3D11DynamicRHI::RHIVerifyResult(ID3D11Device* Device, HRESULT Result, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line) const
{
	VerifyD3D11Result(Result, Code, Filename, Line, Device);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "SharedMemoryMediaCapture.h"
#include "SharedMemoryMediaOutput.h"

#include "GenericPlatform/GenericPlatformMemory.h"
#include "Internationalization/TextLocalizationResource.h"
#include "RenderGraphUtils.h"

#include "SharedMemoryMediaModule.h"
#include "SharedMemoryMediaPlatform.h"
#include "SharedMemoryMediaTypes.h"

DECLARE_GPU_STAT(SharedMemory_Capture);


bool USharedMemoryMediaCapture::InitializeCapture()
{
	// Validate Media Output type

	USharedMemoryMediaOutput* SharedMemoryMediaOutput = Cast<USharedMemoryMediaOutput>(MediaOutput);

	if (!SharedMemoryMediaOutput)
	{
		UE_LOG(LogSharedMemoryMedia, Error, TEXT("Invalid MediaOutput, cannot InitializeCapture"));
		return false;
	}

	// Get an RHI type specific implementation.
	if (!PlatformData.IsValid())
	{
		const ERHIInterfaceType RhiInterfaceType = GDynamicRHI->GetInterfaceType();

		PlatformData = FSharedMemoryMediaPlatformFactory::Get()->CreateInstanceForRhi(RhiInterfaceType);

		if (!PlatformData.IsValid())
		{
			UE_LOG(LogSharedMemoryMedia, Error, TEXT("Unfortunately, SharedMemoryMedia doesn't support the current RHI type '%s'"),
				*FSharedMemoryMediaPlatformFactory::GetRhiTypeString(RhiInterfaceType));

			return false;
		}
	}

	const SIZE_T SharedMemorySize = sizeof(FSharedMemoryMediaFrameMetadata);

	for (int32 BufferIdx = 0; BufferIdx < NUMBUFFERS; ++BufferIdx)
	{
		// Generate the shared memory Guid from the MediaOutput user set unique name.
		const FGuid Guid = UE::SharedMemoryMedia::GenerateSharedMemoryGuid(SharedMemoryMediaOutput->UniqueName, BufferIdx);

		const FString SharedMemoryRegionName = Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces);

		// Open existing shared memory region, in case it exists:

		const uint32 AccessMode = FPlatformMemory::ESharedMemoryAccess::Read | FPlatformMemory::ESharedMemoryAccess::Write;

		FPlatformMemory::FSharedMemoryRegion* SharedMemoryRegion = FPlatformMemory::MapNamedSharedMemoryRegion(
			*SharedMemoryRegionName, false /* bCreate */, AccessMode, SharedMemorySize
		);

		// If it doesn't exist, then we allocate and zero-initialize it.
		if (!SharedMemoryRegion)
		{
			// Create
			SharedMemoryRegion = FPlatformMemory::MapNamedSharedMemoryRegion(
				*SharedMemoryRegionName,
				true /* bCreate */,
				AccessMode,
				SharedMemorySize
			);

			// Zero
			if (SharedMemoryRegion)
			{
				check(SharedMemoryRegion->GetAddress());
				FMemory::Memzero(SharedMemoryRegion->GetAddress(), SharedMemoryRegion->GetSize());

				// Except some special data
				FSharedMemoryMediaFrameMetadata* Data = static_cast<FSharedMemoryMediaFrameMetadata*>(SharedMemoryRegion->GetAddress());
				Data->Receiver.FrameNumberAcked = ~0;

				UE_LOG(LogSharedMemoryMedia, Verbose, TEXT("Created SharedMemoryRegion[%d] = %s"), BufferIdx, *SharedMemoryRegionName);
			}
		}

		// Verify that the shared memory creation succeeded
		if (!SharedMemoryRegion || !SharedMemoryRegion->GetAddress())
		{
			SetState(EMediaCaptureState::Error);
			return false;
		}

		SharedMemory[BufferIdx] = SharedMemoryRegion;
	}

	for (uint32 BufferIdx = 0; BufferIdx < NUMBUFFERS; BufferIdx++)
	{
		// Initialize fences
		if (!TextureReadyFences[BufferIdx])
		{
			TextureReadyFences[BufferIdx] = RHICreateGPUFence(*FString::Printf(TEXT("SharedMemoryMediaOutputFence_%d"), BufferIdx));
		}
	}			

	SetState(EMediaCaptureState::Capturing);

	return true;
}

void USharedMemoryMediaCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	// Note: This gets called by StopCapture which already changed the state to Stopped and called FlushRenderingCommands.

	using namespace UE::SharedMemoryMedia;

	// Since FlushRenderingCommands was already called by StopCapture, we can safely release all the resources

	// Wait for any pending tasks to finish, which could be trying to use the resources as well.
	while (RunningTasksCount > 0)
	{
		FPlatformProcess::SleepNoStats(SpinWaitTimeSeconds);
	}

	check(RunningTasksCount == 0);

	for (int32 BufferIdx = 0; BufferIdx < NUMBUFFERS; ++BufferIdx)
	{
		check(!bTextureReadyFenceBusy[BufferIdx]);
		TextureReadyFences[BufferIdx].SafeRelease();

		if (SharedMemory[BufferIdx])
		{
			// Let the receivers know that we're closed
			{
				FSharedMemoryMediaFrameMetadata* SharedMemoryData = static_cast<FSharedMemoryMediaFrameMetadata*>(SharedMemory[BufferIdx]->GetAddress());

				if (SharedMemoryData)
				{
					SharedMemoryData->Sender.Magic = 0;
					SharedMemoryData->Sender.TextureGuid = ZeroGuid;
				}
			}

			FPlatformMemory::UnmapNamedSharedMemoryRegion(SharedMemory[BufferIdx]);
			SharedMemory[BufferIdx] = nullptr;
		}

		SharedCrossGpuTextures[BufferIdx].SafeRelease(); // This should release the platform specific resources

		if (PlatformData.IsValid())
		{
			PlatformData->ReleaseSharedCrossGpuTexture(BufferIdx);
		}

		SharedCrossGpuTextureGuids[BufferIdx] = FGuid();
	}

	// Free platform specific resources
	PlatformData.Reset();
}

bool USharedMemoryMediaCapture::ShouldCaptureRHIResource() const
{
	return true;
}

FIntPoint USharedMemoryMediaCapture::GetCustomOutputSize(const FIntPoint& InSize) const
{
	// We pass back the desired size
	return InSize;
}

EMediaCaptureResourceType USharedMemoryMediaCapture::GetCustomOutputResourceType() const
{
	return EMediaCaptureResourceType::Texture;
}

void USharedMemoryMediaCapture::OnCustomCapture_RenderingThread(
	FRDGBuilder& GraphBuilder, 
	const FCaptureBaseData& InBaseData, 
	TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, 
	FRDGTextureRef InSourceTexture, 
	FRDGTextureRef OutputTexture, 
	const FRHICopyTextureInfo& CopyInfo, 
	FVector2D CropU, 
	FVector2D CropV)
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, SharedMemory_Capture)
	TRACE_CPUPROFILER_EVENT_SCOPE(USharedMemoryMediaCapture::OnCustomCapture_RenderingThread);

	// Initialize shared gpu textures if needed.

	check(PlatformData.IsValid());

	for (uint32 Idx = 0; Idx < NUMBUFFERS; Idx++)
	{
		if (!SharedCrossGpuTextures[Idx].IsValid())
		{
			const FGuid Guid = FGuid::NewGuid();
						
			SharedCrossGpuTextures[Idx] = PlatformData->CreateSharedCrossGpuTexture(
				InSourceTexture->Desc.Format,
				EnumHasAnyFlags(InSourceTexture->Desc.Flags, TexCreate_SRGB),
				CopyInfo.Size.X, 
				CopyInfo.Size.Y, 
				Guid, 
				Idx
			);

			if (!SharedCrossGpuTextures[Idx].IsValid())
			{
				UE_LOG(LogSharedMemoryMedia, Error, TEXT("Unable to create cross GPU texture of the requested type."));

				SetState(EMediaCaptureState::Error);
				return;
			}

			SharedCrossGpuTextureGuids[Idx] = Guid;
			UE_LOG(LogSharedMemoryMedia, Verbose, TEXT("Created SharedGpuTextureGuid[%d] = %s"), Idx, *SharedCrossGpuTextureGuids[Idx].ToString());
		}
	}

	// Add the copy texture pass
	AddCopyToSharedGpuTexturePass(GraphBuilder, InSourceTexture, GFrameCounterRenderThread % NUMBUFFERS);
}

BEGIN_SHADER_PARAMETER_STRUCT(FCopyToSharedGpuTexturePass, )
	RDG_TEXTURE_ACCESS(SrcTexture, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(DstTexture, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

void USharedMemoryMediaCapture::AddCopyToSharedGpuTexturePass(FRDGBuilder& GraphBuilder, FRDGTextureRef InSourceTexture, uint32 SharedTextureIdx)
{
	using namespace UE::SharedMemoryMedia;

	FCopyToSharedGpuTexturePass* PassParameters = GraphBuilder.AllocParameters<FCopyToSharedGpuTexturePass>();

	PassParameters->SrcTexture = InSourceTexture;

	PassParameters->DstTexture = GraphBuilder.RegisterExternalTexture(
		CreateRenderTarget(SharedCrossGpuTextures[SharedTextureIdx], *FString::Printf(TEXT("SharedCrossGpuTextures_%d"), SharedTextureIdx)));

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Copy%sToSharedGpuTexture", InSourceTexture->Name),
		PassParameters,
		ERDGPassFlags::Copy,
		[InSourceTexture, SharedTextureIdx, this](FRHICommandList& RHICmdList)
		{
			// bTextureReadyFenceBusy will also signal that the resource is safe to reuse
			if (bTextureReadyFenceBusy[SharedTextureIdx])
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SharedMemMediaOutputFenceBusy);

				UE_LOG(LogSharedMemoryMedia, Verbose, TEXT("bTextureReadyFenceBusy[%d] for frame %d was busy, so we wait"), SharedTextureIdx, GFrameCounterRenderThread);

				while (bTextureReadyFenceBusy[SharedTextureIdx])
				{
					FPlatformProcess::SleepNoStats(SpinWaitTimeSeconds);
				}
			}

			// This flag will be cleared by the async task when the receiver is done with the shared cross gpu texture
			bTextureReadyFenceBusy[SharedTextureIdx] = true;

			// Do the copy
			FRHICopyTextureInfo CopyInfo;
			RHICmdList.CopyTexture(InSourceTexture->GetRHI(), SharedCrossGpuTextures[SharedTextureIdx], CopyInfo);

			// Write GPU fence
			RHICmdList.WriteGPUFence(TextureReadyFences[SharedTextureIdx]);

			// Spawn a thread that via shared ram will notify receiver that data is ready
			// It will also verify that the data has been consumed (with a timeout).
			RunningTasksCount++;
			UE::Tasks::Launch(UE_SOURCE_LOCATION, [FrameNumber = GFrameCounterRenderThread, SharedTextureIdx, this]()
			{
				// Decrement RunningTasksCount when the task exits
				ON_SCOPE_EXIT
				{
					RunningTasksCount--;
				};

				const FString CopyThreadName = FString::Printf(TEXT("SharedMemMediaOutputGpuTextureInTransitForFrame_%d"), FrameNumber);
				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*CopyThreadName);

				// Wait for fence that indicates that the gpu texture has the data
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(WaitForGpuTextureReadyFence);

					while (TextureReadyFences[SharedTextureIdx] && !TextureReadyFences[SharedTextureIdx]->Poll())
					{
						FPlatformProcess::SleepNoStats(0);
					}
				}

				// Update shared memory metadata to indicate to the receiver that there is new data

				FSharedMemoryMediaFrameMetadata* SharedMemoryData = static_cast<FSharedMemoryMediaFrameMetadata*>(SharedMemory[SharedTextureIdx]->GetAddress());
				{
					FSharedMemoryMediaFrameMetadata::FSender SenderMetadata;
					SenderMetadata.FrameNumber = FrameNumber;
					SenderMetadata.TextureGuid = SharedCrossGpuTextureGuids[SharedTextureIdx];

					// We only send the sender structure
					FMemory::Memcpy(&SharedMemoryData->Sender, &SenderMetadata, sizeof(FSharedMemoryMediaFrameMetadata::FSender));
				}

				// Wait for FrameNumber ack

				if (SharedMemoryData->Receiver.KeepAliveShiftRegister)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(WaitForGpuTextureAck)

					const double StartTimeSeconds = FPlatformTime::Seconds();
					constexpr double TimeoutSeconds = 0.5;

					while (SharedMemoryData->Receiver.FrameNumberAcked < FrameNumber && SharedMemoryData->Receiver.KeepAliveShiftRegister)
					{
						FPlatformProcess::SleepNoStats(SpinWaitTimeSeconds);

						if ((FPlatformTime::Seconds() - StartTimeSeconds) > TimeoutSeconds)
						{
							// @todo use proper log category
							UE_LOG(LogSharedMemoryMedia, Warning, TEXT("FSharedMemoryMediaCapture timed out waiting for its receiver to ack frame %d"), FrameNumber);

							break;
						}
					};
				}

				// Shift the keep alive. The bit depth of the keep alive is the number of frames the receiver has to re-set it before it expires.
				SharedMemoryData->Receiver.KeepAliveShiftRegister >>= 1;

				// Clear fence and flag that we're ready for a new frame

				TextureReadyFences[SharedTextureIdx]->Clear();
				bTextureReadyFenceBusy[SharedTextureIdx] = false;
			});
		});
}

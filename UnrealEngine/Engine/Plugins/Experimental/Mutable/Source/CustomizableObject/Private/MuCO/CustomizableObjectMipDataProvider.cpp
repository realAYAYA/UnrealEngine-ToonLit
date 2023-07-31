// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectMipDataProvider.h"

#include "Async/Fundamental/Task.h"
#include "Containers/ArrayView.h"
#include "Containers/IndirectArray.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/ScopeLock.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuR/Instance.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/MutableTrace.h"
#include "MuR/Ptr.h"
#include "MuR/Types.h"
#include "Serialization/BulkData.h"
#include "Templates/Casts.h"
#include "Templates/Function.h"
#include "TextureResource.h"
#include "Trace/Detail/Channel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectMipDataProvider)

UMutableTextureMipDataProviderFactory::UMutableTextureMipDataProviderFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}


FMutableTextureMipDataProvider::FMutableTextureMipDataProvider(const UTexture* Texture, UCustomizableObjectInstance* InCustomizableObjectInstance, const FMutableImageReference& InImageRef)
	: FTextureMipDataProvider(Texture, ETickState::Init, ETickThread::Async),
	CustomizableObjectInstance(InCustomizableObjectInstance), ImageRef(InImageRef)
{
	check(ImageRef.ImageID > 0);
}


void FMutableTextureMipDataProvider::Init(const FTextureUpdateContext& Context, const FTextureUpdateSyncOptions& SyncOptions)
{
	AdvanceTo(ETickState::GetMips, ETickThread::Async);
}

namespace impl
{
	void Task_Mutable_UpdateImage(TSharedPtr<FMutableImageOperationData> OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_UpdateImage);

		// Any external texture that may be needed for this update will be requested from Mutable Core's GetImage
		// which will safely access the GlobalExternalImages map, and then just get the cached image or issue a disk read

		// This runs in a worker thread.
		check(OperationData.IsValid());
		check(OperationData->UpdateContext->System.get());
		check(OperationData->UpdateContext->Model.get());
		check(OperationData->UpdateContext->Parameters.get());

		mu::SystemPtr System = OperationData->UpdateContext->System;
		mu::ModelPtr Model = OperationData->UpdateContext->Model;

		// For now, we are forcing the recreation of mutable-side instances with every update.
		mu::Instance::ID InstanceID = System->NewInstance(Model.get());
		UE_LOG(LogMutable, Log, TEXT("Creating instance with id [%d] "), InstanceID)

		const mu::Instance* Instance = nullptr;

		// Main instance generation step
		{
			// LOD mask, set to all ones to build all LODs
			uint32 LODMask = 0xFFFFFFFF;

			Instance = System->BeginUpdate(InstanceID, OperationData->UpdateContext->Parameters, OperationData->UpdateContext->State, LODMask);
			check(Instance);
		}


		// Generate the required image
		{
			MUTABLE_CPUPROFILER_SCOPE(RequestedImage);

			const FMutableImageReference& ImageRef = OperationData->RequestedImage;

			int32 SurfaceIndex = Instance->FindSurfaceById(ImageRef.LOD, ImageRef.Component, ImageRef.SurfaceId);
			check(SurfaceIndex>=0);

			// This ID may be different than the ID obtained the first time the image was generated, because the mutable
			// runtime cannot remember all the resources it has built, and only remembers a fixed amount.
			mu::RESOURCE_ID MipImageID = Instance->GetImageId(ImageRef.LOD, ImageRef.Component, SurfaceIndex, ImageRef.Image);

			// TODO: Why do we need to do this again? The full size should be stored in the initial image creation.
			mu::FImageDesc ImageDesc;
			mu::ImagePtrConst Image;
			{
				MUTABLE_CPUPROFILER_SCOPE(GetImage);

				Image = System->GetImage(InstanceID, MipImageID, OperationData->MipsToSkip);
			}

			check(Image);

			OperationData->Result = Image;
		}

		// End update
		{
			MUTABLE_CPUPROFILER_SCOPE(EndUpdate);
			System->EndUpdate(InstanceID);
		}

		{
			// The request could be cancelled in parallel from CancelCounterSafely and its value be changed
			// between reading it and actually running Decrement() and RescheduleCallback(), so lock
			FScopeLock Lock(&OperationData->CounterTaskLock);

			if (OperationData->Counter) // If the request has been cancelled the counter will be null
			{
				// Make the FMutableTextureMipDataProvider continue
				OperationData->Counter->Decrement();

				if (OperationData->Counter->GetValue() == 0)
				{
					OperationData->RescheduleCallback();
				}
			}
		}
	}

} // namespace


int32 FMutableTextureMipDataProvider::GetMips(const FTextureUpdateContext& Context, int32 StartingMipIndex, const FTextureMipInfoArray& MipInfos, const FTextureUpdateSyncOptions& SyncOptions)
{
	const UTexture2D* Texture = Cast<UTexture2D>(Context.Texture);
	check(Texture);
	check(!Texture->NeverStream);
	const TIndirectArray<FTexture2DMipMap>& OwnerMips = Texture->GetPlatformMips();

	int32 NumMips = OwnerMips.Num();
	check(ImageRef.ImageID > 0);

	// Maximum value to skip, will be minimized by the first mip level requested
	int32 MipsToSkip = 256;

	for (int32 MipIndex = StartingMipIndex; MipIndex < CurrentFirstLODIdx; ++MipIndex)
	{
		check(Context.MipsView.IsValidIndex(MipIndex) && MipInfos.IsValidIndex(MipIndex));

		const FTexture2DMipMap& MipMap = *Context.MipsView[MipIndex];
		FByteBulkData* AuxBulkData = const_cast<FByteBulkData*>(&MipMap.BulkData);

		const FTextureMipInfo& MipInfo = MipInfos[MipIndex];
		void* Dest = MipInfo.DestData;

		if (AuxBulkData->GetBulkDataSize() > 0)
		{
			// Mips are already generated, no need for Mutable progressive mip streaming, just normal CPU->GPU streaming
			AuxBulkData->GetCopy(&Dest, false);
		}
		else // Generate a Mip Request to Mutable
		{
			check(UCustomizableObjectSystem::GetInstance()->GetPrivate()->EnableMutableProgressiveMipStreaming == 1);

			if (!OperationData.IsValid())
			{
				OperationData = MakeShared<FMutableImageOperationData>();
			}

			const int32 LODBias = static_cast<int32>(Context.MipsView.GetData() - Texture->GetPlatformData()->Mips.GetData());
			check(LODBias >= 0);
			check(NumMips == Context.MipsView.Num() + LODBias);

			OperationData->Levels.Add({ MipIndex + LODBias, Dest, (int32)MipInfo.SizeX, (int32)MipInfo.SizeY, (int32)MipInfo.DataSize, MipInfo.Format });
			MipsToSkip = FMath::Min(MipsToSkip, MipIndex + LODBias);
		}
	}

	FCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
	if (CustomizableObjectSystem)
	{
		if (OperationData.IsValid())
		{
			OperationData->RequestedImage = ImageRef;
			OperationData->UpdateContext = UpdateContext;
			OperationData->MipsToSkip = MipsToSkip;

			check(SyncOptions.Counter);
			// Increment to stop PollMips from running until the Mutable request task finishes. 
			// If a request completes immediately, then it will call the callback
			// but that won't do anything because the tick would not try to acquire the lock since it is already locked.
			SyncOptions.Counter->Increment();
			OperationData->Counter = SyncOptions.Counter;
			OperationData->RescheduleCallback = SyncOptions.RescheduleCallback;

			TSharedPtr<FMutableImageOperationData> LocalOperationData = OperationData;
			UpdateImageMutableTaskEvent = CustomizableObjectSystem->AddMutableThreadTask(
				TEXT("Mutable_MipUpdate"),
				[LocalOperationData]()
				{
					impl::Task_Mutable_UpdateImage(LocalOperationData);
				},
				UE::Tasks::ETaskPriority::BackgroundHigh
					);
		}

		AdvanceTo(ETickState::PollMips, ETickThread::Async);
	}
	else
	{
		AdvanceTo(ETickState::CleanUp, ETickThread::Async);
	}

	return CurrentFirstLODIdx;
}


bool FMutableTextureMipDataProvider::PollMips(const FTextureUpdateSyncOptions& SyncOptions)
{
	if (!bRequestAborted && UpdateImageMutableTaskEvent)
	{
		if (OperationData && OperationData->Result && OperationData->Levels.Num())
		{
			// The counter must be zero meaning the Mutable image operation has finished
			check(SyncOptions.Counter->GetValue() == 0);

			mu::ImagePtrConst Mip = OperationData->Result;
			int32 MipIndex = 0;
			check(Mip->GetSizeX() == OperationData->Levels[0].SizeX);
			check(Mip->GetSizeY() == OperationData->Levels[0].SizeY);

			for (FMutableMipUpdateLevel& Level : OperationData->Levels)
			{
				// Check Mip DataSize for consistency, but skip if 0 because it's optional and might be zero in cooked mips
				check(Level.DataSize == 0 || Mip->GetLODDataSize(MipIndex) == Level.DataSize);
				void* Dest = Level.Dest;
				FMemory::Memcpy(Dest, Mip->GetMipData(MipIndex), Mip->GetLODDataSize(MipIndex));
				++MipIndex;
			}
		}
	}
	else if (bRequestAborted)
	{
		OperationData = nullptr;
		AdvanceTo(ETickState::CleanUp, ETickThread::Async);
		return false;
	}

	OperationData = nullptr;
	AdvanceTo(ETickState::Done, ETickThread::None);
	return true;
}


void FMutableTextureMipDataProvider::CleanUp(const FTextureUpdateSyncOptions& SyncOptions)
{
	check(!SyncOptions.Counter || SyncOptions.Counter->GetValue() == 0);
	AdvanceTo(ETickState::Done, ETickThread::None);
}


void FMutableTextureMipDataProvider::Cancel(const FTextureUpdateSyncOptions& SyncOptions)
{
	bRequestAborted = true;

	CancelCounterSafely();
}


FTextureMipDataProvider::ETickThread FMutableTextureMipDataProvider::GetCancelThread() const
{
	//return OperationData ? FTextureMipDataProvider::ETickThread::Async : FTextureMipDataProvider::ETickThread::None;
	return FTextureMipDataProvider::ETickThread::None;
}


void FMutableTextureMipDataProvider::AbortPollMips()
{
	bRequestAborted = true;

	CancelCounterSafely();
}


void FMutableTextureMipDataProvider::CancelCounterSafely()
{
	// The Counter could be read in parallel from Task_Mutable_UpdateImage, so lock
	FScopeLock Lock(&OperationData->CounterTaskLock);

	if (OperationData->Counter)
	{
		OperationData->Counter->Set(0);
		OperationData->Counter = nullptr;
	}
}

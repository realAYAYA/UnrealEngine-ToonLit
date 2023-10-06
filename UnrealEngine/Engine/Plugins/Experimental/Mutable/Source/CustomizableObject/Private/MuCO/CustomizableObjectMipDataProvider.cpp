// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectMipDataProvider.h"

#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuR/Model.h"
#include "TextureResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectMipDataProvider)

UMutableTextureMipDataProviderFactory::UMutableTextureMipDataProviderFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}


FMutableUpdateContext::FMutableUpdateContext(mu::Ptr<mu::System> InSystem,
	TSharedPtr<mu::Model, ESPMode::ThreadSafe> InModel, mu::Ptr<const mu::Parameters> InParameters, int32 InState):
	System(InSystem),
	Model(InModel),
	Parameters(InParameters),
	State(InState)
{
	if (Parameters)
	{
		const FCustomizableObjectSystemPrivate* Private = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		Private->GetImageProviderChecked()->CacheImages(*Parameters);
	}
}


FMutableUpdateContext::~FMutableUpdateContext()
{
	if (Parameters &&
		UCustomizableObjectSystem::IsCreated())
	{
		const FCustomizableObjectSystemPrivate* Private = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		Private->GetImageProviderChecked()->UnCacheImages(*Parameters);
	}
}


mu::Ptr<mu::System> FMutableUpdateContext::GetSystem() const
{
	return System;
}


TSharedPtr<mu::Model, ESPMode::ThreadSafe> FMutableUpdateContext::GetModel() const
{
	return Model;
}


mu::Ptr<const mu::Parameters> FMutableUpdateContext::GetParameters() const
{
	return Parameters;
}


int32 FMutableUpdateContext::GetState() const
{
	return State;
}


const TArray<mu::Ptr<const mu::Image>>& FMutableUpdateContext::GetImageParameterValues() const
{
	return ImageParameterValues;
}


FMutableTextureMipDataProvider::FMutableTextureMipDataProvider(const UTexture* Texture, UCustomizableObjectInstance* InCustomizableObjectInstance, const FMutableImageReference& InImageRef)
	: FTextureMipDataProvider(Texture, ETickState::Init, ETickThread::Async),
	CustomizableObjectInstance(InCustomizableObjectInstance), ImageRef(InImageRef)
{
	check(ImageRef.ImageID > 0);
}


void FMutableTextureMipDataProvider::PrintWarningAndAdvanceToCleanup()
{
	UE_LOG(LogMutable, Warning, TEXT("Tried to update a mip from a Customizable Object being compiled, cancelling mip update."));
	AdvanceTo(ETickState::CleanUp, ETickThread::Async);
}


void FMutableTextureMipDataProvider::Init(const FTextureUpdateContext& Context, const FTextureUpdateSyncOptions& SyncOptions)
{
#if WITH_EDITOR
	check(Context.Texture->HasPendingInitOrStreaming());
	check(CustomizableObjectInstance->GetCustomizableObject());
	if (CustomizableObjectInstance->GetCustomizableObject()->IsLocked())
	{
		PrintWarningAndAdvanceToCleanup();

		return;
	}
#endif

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
		check(OperationData->UpdateContext->GetSystem().get());
		check(OperationData->UpdateContext->GetModel());
		check(OperationData->UpdateContext->GetParameters().get());

		if (OperationData.IsValid())
		{
			mu::SystemPtr System = OperationData->UpdateContext->GetSystem();
			const TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = OperationData->UpdateContext->GetModel();

			// For now, we are forcing the recreation of mutable-side instances with every update.
			mu::Instance::ID InstanceID = System->NewInstance(Model);
			UE_LOG(LogMutable, Verbose, TEXT("Creating Mutable instance with id [%d] for a single UpdateImage"), InstanceID)

				const mu::Instance* Instance = nullptr;

			// Main instance generation step
			{
				// LOD mask, set to all ones to build all LODs
				uint32 LODMask = 0xFFFFFFFF;

				Instance = System->BeginUpdate(InstanceID, OperationData->UpdateContext->GetParameters(), OperationData->UpdateContext->GetState(), LODMask);
				check(Instance);
			}


			// Generate the required image
			{
				MUTABLE_CPUPROFILER_SCOPE(RequestedImage);

				const FMutableImageReference& ImageRef = OperationData->RequestedImage;

				int32 SurfaceIndex = Instance->FindSurfaceById(ImageRef.LOD, ImageRef.Component, ImageRef.SurfaceId);
				check(SurfaceIndex >= 0);

				// This ID may be different than the ID obtained the first time the image was generated, because the mutable
				// runtime cannot remember all the resources it has built, and only remembers a fixed amount.
				mu::FResourceID MipImageID = Instance->GetImageId(ImageRef.LOD, ImageRef.Component, SurfaceIndex, ImageRef.Image);

				UCustomizableObjectSystem* COSystem = UCustomizableObjectSystem::GetInstance();
				FCustomizableObjectSystemPrivate* SystemPrivate = COSystem ? COSystem->GetPrivate() : nullptr;
				int32 MaxTextureSizeToGenerate = SystemPrivate ? SystemPrivate->MaxTextureSizeToGenerate : 0;

				int32 ExtraMipsToSkip = 0;
				
				if (MaxTextureSizeToGenerate > 0)
				{
					// TODO: Why do we need to do this again? The full size should be stored in the initial image creation.
					mu::FImageDesc ImageDesc;
					System->GetImageDesc(InstanceID, MipImageID, ImageDesc);

					uint16 MaxSize = FMath::Max(ImageDesc.m_size[0], ImageDesc.m_size[1]);

					if (MaxSize > MaxTextureSizeToGenerate)
					{
						ExtraMipsToSkip = FMath::CeilLogTwo(MaxSize / MaxTextureSizeToGenerate);
					}
				}

				mu::ImagePtrConst Image;
				{
					MUTABLE_CPUPROFILER_SCOPE(GetImage);

					Image = System->GetImage(InstanceID, MipImageID, OperationData->MipsToSkip + ExtraMipsToSkip, ImageRef.LOD);
				}

				check(Image);

				int32 FullMipCount = Image->GetMipmapCount(Image->GetSizeX(), Image->GetSizeY());
				int32 RealMipCount = Image->GetLODCount();

				bool bForceMipchain =
					// Did we fail to generate the entire mipchain (if we have mips at all)?
					(RealMipCount != 1) && (RealMipCount != FullMipCount);

				if (bForceMipchain)
				{
					MUTABLE_CPUPROFILER_SCOPE(GetImage_MipFix);

					UE_LOG(LogMutable, Warning, TEXT("Mutable generated an incomplete mip chain for image."));

					// Force the right number of mips. The missing data will be black.
					mu::Ptr<mu::Image> NewImage = new mu::Image(Image->GetSizeX(), Image->GetSizeY(), FullMipCount, Image->GetFormat(), mu::EInitializationType::Black);
					check(NewImage);
					if (NewImage->GetDataSize() >= Image->GetDataSize())
					{
						FMemory::Memcpy(NewImage->GetData(), Image->GetData(), Image->GetDataSize());
					}
					Image = NewImage;
				}

				OperationData->Result = Image;
			}

			// End update
			{
				MUTABLE_CPUPROFILER_SCOPE(EndUpdate);
				System->EndUpdate(InstanceID);
				System->ReleaseInstance(InstanceID);

				if (CVarClearWorkingMemoryOnUpdateEnd.GetValueOnAnyThread())
				{
					System->ClearWorkingMemory();
				}
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
	}

} // namespace


int32 FMutableTextureMipDataProvider::GetMips(const FTextureUpdateContext& Context, int32 StartingMipIndex, const FTextureMipInfoArray& MipInfos, const FTextureUpdateSyncOptions& SyncOptions)
{
	MUTABLE_CPUPROFILER_SCOPE(FMutableTextureMipDataProvider::GetMips)

#if WITH_EDITOR
	check(Context.Texture->HasPendingInitOrStreaming());
	check(CustomizableObjectInstance->GetCustomizableObject());
	if (CustomizableObjectInstance->GetCustomizableObject()->IsLocked())
	{
		PrintWarningAndAdvanceToCleanup();

		return CurrentFirstLODIdx;
	}
#endif

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
			MutableTaskId = CustomizableObjectSystem->MutableTaskGraph.AddMutableThreadTaskLowPriority(
				TEXT("Mutable_MipUpdate"),
				[LocalOperationData]()
				{
					impl::Task_Mutable_UpdateImage(LocalOperationData);
				});
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
	MUTABLE_CPUPROFILER_SCOPE(FMutableTextureMipDataProvider::PollMips)

	// Once this point is reached, even if the task has not been completed, we know that all the work we need from it has been completed.
	// Furthermore, checking if the task is completed is incorrect since PollMips could have been called by RescheduleCallback (before completing the task).
	
#if WITH_EDITOR
	check(CustomizableObjectInstance->GetCustomizableObject());
	if (CustomizableObjectInstance->GetCustomizableObject()->IsLocked())
	{
		PrintWarningAndAdvanceToCleanup();

		return false;
	}
#endif

	if (bRequestAborted)
	{
		OperationData = nullptr;
		AdvanceTo(ETickState::CleanUp, ETickThread::Async);
		return false;
	}
	
	if (OperationData && OperationData->Result && OperationData->Levels.Num())
	{
		// The counter must be zero meaning the Mutable image operation has finished
		check(SyncOptions.Counter->GetValue() == 0);

		mu::Ptr<const mu::Image> Mip = OperationData->Result;
		int32 MipIndex = 0;
		check(Mip->GetSizeX() == OperationData->Levels[0].SizeX);
		check(Mip->GetSizeY() == OperationData->Levels[0].SizeY);

		for (FMutableMipUpdateLevel& Level : OperationData->Levels)
		{
			void* Dest = Level.Dest;

			if (MipIndex >= Mip->GetLODCount())
			{
				// Mutable didn't generate all the expected mips
				UE_LOG(LogMutable, Warning, TEXT("Mutable image is missing mips."));
				FMemory::Memzero(Dest, Level.DataSize);
			}
			else
			{
				int32 MipDataSize = Mip->GetLODDataSize(MipIndex);

				// Check Mip DataSize for consistency, but skip if 0 because it's optional and might be zero in cooked mips
				check(Level.DataSize == 0 || MipDataSize == Level.DataSize);
				FMemory::Memcpy(Dest, Mip->GetMipData(MipIndex), MipDataSize);
			}
			++MipIndex;
		}
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

	if (UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance())
	{
		System->GetPrivate()->MutableTaskGraph.CancelMutableThreadTaskLowPriority(MutableTaskId);	
	}
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

	if (UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance())
	{
		System->GetPrivate()->MutableTaskGraph.CancelMutableThreadTaskLowPriority(MutableTaskId);	
	}
}


void FMutableTextureMipDataProvider::CancelCounterSafely()
{
	if (OperationData.IsValid())
	{
		// The Counter could be read in parallel from Task_Mutable_UpdateImage, so lock
		FScopeLock Lock(&OperationData->CounterTaskLock);

		if (OperationData->Counter)
		{
			OperationData->Counter->Set(0);
			OperationData->Counter = nullptr;
		}
	}
}

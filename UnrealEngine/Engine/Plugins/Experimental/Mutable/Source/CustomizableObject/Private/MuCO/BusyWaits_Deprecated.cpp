// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/BusyWaits_Deprecated.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/CustomizableObjectMipDataProvider.h"
#include "MuCO/CustomizableObjectMeshUpdate.h"

#include "MuR/Model.h"
#include "MuR/MutableTrace.h"


namespace CustomizableObjectSystem::ImplDeprecated
{
	void Subtask_Mutable_GetImages(const TSharedRef<FUpdateContextPrivate>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_GetImages)

		const UCustomizableObjectSystemPrivate* CustomizableObjectSystemPrivateData = UCustomizableObjectSystem::GetInstanceChecked()->GetPrivate();
		mu::System* System = CustomizableObjectSystemPrivateData->MutableSystem.get();
		check(System != nullptr);

		// Generate all the required resources, that are not cached
		TArray<mu::FResourceID> ImagesInThisInstance;
		for (FInstanceUpdateData::FImage& Image : OperationData->InstanceUpdateData.Images)
		{
			MUTABLE_CPUPROFILER_SCOPE(GetImage);

			// If the image is a reference to an engine texture, we are done.
			if (Image.bIsPassThrough)
			{
				continue;
			}

			mu::FImageDesc ImageDesc;

			// This should only be done when using progressive images, since GetImageDesc does some actual processing.
			{
				System->GetImageDescInline(OperationData->InstanceID, Image.ImageID, ImageDesc);

				const uint16 MaxTextureSizeToGenerate = static_cast<uint16>(CustomizableObjectSystemPrivateData->MaxTextureSizeToGenerate);
				const uint16 MaxSize = FMath::Max(ImageDesc.m_size[0], ImageDesc.m_size[1]);
				uint16 Reduction = 1;

				if (MaxTextureSizeToGenerate > 0 && MaxSize > MaxTextureSizeToGenerate)
				{
					// Find the reduction factor, and the BaseMip of the texture.
					const uint32 NextPowerOfTwo = FMath::RoundUpToPowerOfTwo(FMath::DivideAndRoundUp(MaxSize, MaxTextureSizeToGenerate));
					Reduction = FMath::Max(NextPowerOfTwo, 2U); // At least divide the texture by a factor of two
					Image.BaseMip = FMath::FloorLog2(Reduction);
				}

				Image.FullImageSizeX = ImageDesc.m_size[0] / Reduction;
				Image.FullImageSizeY = ImageDesc.m_size[1] / Reduction;
			}

			const bool bCached = ImagesInThisInstance.Contains(Image.ImageID) || // See if it is cached from this same instance (can happen with LODs)
				(CVarReuseImagesBetweenInstances.GetValueOnAnyThread() && CustomizableObjectSystemPrivateData->ProtectedObjectCachedImages.Contains(Image.ImageID)); // See if it is cached from another instance

			if (bCached)
			{
				UE_LOG(LogMutable, VeryVerbose, TEXT("Texture resource with id [%llu] is cached."), Image.ImageID);
			}
			else
			{
				const int32 MaxSize = FMath::Max(Image.FullImageSizeX, Image.FullImageSizeY);
				const int32 FullLODCount = FMath::CeilLogTwo(MaxSize) + 1;
				const int32 MinMipsInImage = FMath::Min(FullLODCount, UTexture::GetStaticMinTextureResidentMipCount());
				const int32 MaxMipsToSkip = FullLODCount - MinMipsInImage;
				int32 MipsToSkip = FMath::Min(MaxMipsToSkip, OperationData->MipsToSkip);

				if (!FMath::IsPowerOfTwo(Image.FullImageSizeX) || !FMath::IsPowerOfTwo(Image.FullImageSizeY))
				{
					// It doesn't make sense to skip mips as non-power-of-two size textures cannot be streamed anyway
					MipsToSkip = 0;
				}

				const int32 MipSizeX = FMath::Max(Image.FullImageSizeX >> MipsToSkip, 1);
				const int32 MipSizeY = FMath::Max(Image.FullImageSizeY >> MipsToSkip, 1);
				if (MipsToSkip > 0 && CustomizableObjectSystemPrivateData->EnableSkipGenerateResidentMips != 0 && OperationData->LowPriorityTextures.Find(Image.Name.ToString()) != INDEX_NONE)
				{
					Image.Image = new mu::Image(MipSizeX, MipSizeY, FullLODCount - MipsToSkip, ImageDesc.m_format, mu::EInitializationType::Black);
				}
				else
				{
					Image.Image = System->GetImageInline(OperationData->InstanceID, Image.ImageID, Image.BaseMip + MipsToSkip, Image.BaseLOD);
				}

				check(Image.Image);

				// We should have generated exactly this size.
				const bool bSizeMissmatch = Image.Image->GetSizeX() != MipSizeX || Image.Image->GetSizeY() != MipSizeY;
				if (bSizeMissmatch)
				{
					// Generate a correctly-sized but empty image instead, to avoid crashes.
					UE_LOG(LogMutable, Warning, TEXT("Mutable generated a wrongly-sized image %llu."), Image.ImageID);
					Image.Image = new mu::Image(MipSizeX, MipSizeY, FullLODCount - MipsToSkip, Image.Image->GetFormat(), mu::EInitializationType::Black);
				}

				// We need one mip or the complete chain. Otherwise there was a bug.
				const int32 FullMipCount = Image.Image->GetMipmapCount(Image.Image->GetSizeX(), Image.Image->GetSizeY());
				const int32 RealMipCount = Image.Image->GetLODCount();

				bool bForceMipchain = 
					// Did we fail to generate the entire mipchain (if we have mips at all)?
					(RealMipCount != 1) && (RealMipCount != FullMipCount);

				if (bForceMipchain)
				{
					MUTABLE_CPUPROFILER_SCOPE(GetImage_MipFix);

					UE_LOG(LogMutable, Warning, TEXT("Mutable generated an incomplete mip chain for image %llu."), Image.ImageID);

					// Force the right number of mips. The missing data will be black.
					const mu::Ptr<mu::Image> NewImage = new mu::Image(Image.Image->GetSizeX(), Image.Image->GetSizeY(), FullMipCount, Image.Image->GetFormat(), mu::EInitializationType::Black);
					check(NewImage);	
					// Formats with BytesPerBlock == 0 will not allocate memory. This type of images are not expected here.
					check(!NewImage->DataStorage.IsEmpty());

					for (int32 L = 0; L < RealMipCount; ++L)
					{
						TArrayView<uint8> DestView = NewImage->DataStorage.GetLOD(L);
						TArrayView<const uint8> SrcView = Image.Image->DataStorage.GetLOD(L);

						check(DestView.Num() == SrcView.Num());
						FMemory::Memcpy(DestView.GetData(), SrcView.GetData(), DestView.Num());
					}

					Image.Image = NewImage;
				}

				ImagesInThisInstance.Add(Image.ImageID);
			}
		}
	}


	void Task_Mutable_GetImages(const TSharedRef<FUpdateContextPrivate>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_GetImages)
		FMutableScopeTimer Timer(OperationData->TaskGetImagesTime);

#if WITH_EDITOR
		uint32 StartCycles = FPlatformTime::Cycles();
#endif

		Subtask_Mutable_GetImages(OperationData);

		// TODO: Not strictly mutable: move to another worker thread task to free mutable access?
		impl::Subtask_Mutable_PrepareTextures(OperationData);

#if WITH_EDITOR
		const uint32 EndCycles = FPlatformTime::Cycles();
		OperationData->MutableRuntimeCycles += EndCycles - StartCycles;
#endif
	}


	void Subtask_Mutable_BeginUpdate_GetMesh(const TSharedRef<FUpdateContextPrivate>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_BeginUpdate_GetMesh)

		check(OperationData->Parameters);
		OperationData->InstanceUpdateData.Clear();

		check(UCustomizableObjectSystem::GetInstance() != nullptr);
		check(UCustomizableObjectSystem::GetInstance()->GetPrivate() != nullptr);
		mu::System* System = UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem.get();
		check(System != nullptr);

		const UCustomizableObject* CustomizableObject = OperationData->Instance->GetCustomizableObject();
		const FModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResources();

		UCustomizableInstancePrivate* CustomizableObjectInstancePrivateData = OperationData->Instance->GetPrivate();

		CustomizableObjectInstancePrivateData->PassThroughTexturesToLoad.Empty();

		if (OperationData->PixelFormatOverride)
		{
			System->SetImagePixelConversionOverride( OperationData->PixelFormatOverride );
		}

		if (!OperationData->bUseMeshCache)
		{
			impl::CreateMutableInstance(OperationData);
			impl::FixLODs(OperationData);
		}

		// Main instance generation step
		// LOD mask, set to all ones to build  all LODs
		const mu::Instance* Instance = OperationData->MutableInstance; // TODO GMTFuture remove
		if (!Instance)
		{
			UE_LOG(LogMutable, Warning, TEXT("An Instace update has failed."));
			return;
		}

		const TArray<uint16>& RequestedLODs = OperationData->GetRequestedLODs();

		// Map SharedSurfaceId to surface index
		TArray<int32> SurfacesSharedId;

		// Generate the mesh and gather all the required resource Ids
		OperationData->InstanceUpdateData.LODs.SetNum(OperationData->NumLODsAvailable);
		for (int32 MutableLODIndex = 0; MutableLODIndex < OperationData->NumLODsAvailable; ++MutableLODIndex)
		{
			// Skip LODs outside the range we want to generate
			if (MutableLODIndex < OperationData->GetMinLOD())
			{
				continue;
			}

			FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[MutableLODIndex];
			LOD.FirstComponent = OperationData->InstanceUpdateData.Components.Num();
			LOD.ComponentCount = Instance->GetComponentCount(MutableLODIndex);

			for (int32 ComponentIndex = 0; ComponentIndex < LOD.ComponentCount; ++ComponentIndex)
			{
				OperationData->InstanceUpdateData.Components.Push(FInstanceUpdateData::FComponent());
				FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components.Last();
				Component.Id = Instance->GetComponentId(MutableLODIndex, ComponentIndex);
				Component.FirstSurface = OperationData->InstanceUpdateData.Surfaces.Num();
				Component.SurfaceCount = 0;

				const bool bGenerateLOD = RequestedLODs.IsValidIndex(Component.Id) ? RequestedLODs[Component.Id] <= MutableLODIndex : true;

				// Mesh
				if (Instance->GetMeshCount(MutableLODIndex, ComponentIndex) > 0)
				{
					MUTABLE_CPUPROFILER_SCOPE(GetMesh);

					Component.MeshID = Instance->GetMeshId(MutableLODIndex, ComponentIndex, 0);

					if (bGenerateLOD)
					{
						Component.Mesh = System->GetMeshInline(OperationData->InstanceID, Component.MeshID);
					}
				}

				if (!Component.Mesh)
				{
					continue;
				}

				Component.bGenerated = true;


				// Materials and images
				const int32 SurfaceCount = Component.Mesh->GetSurfaceCount();
				for (int32 MeshSurfaceIndex = 0; MeshSurfaceIndex < SurfaceCount; ++MeshSurfaceIndex)
				{
					const uint32 SurfaceId = Component.Mesh->GetSurfaceId(MeshSurfaceIndex);
					const int32 InstanceSurfaceIndex = Instance->FindSurfaceById(MutableLODIndex, ComponentIndex, SurfaceId);
					check(Component.Mesh->GetVertexCount() > 0 || InstanceSurfaceIndex >= 0);

					int32 BaseSurfaceIndex = InstanceSurfaceIndex;
					int32 BaseLODIndex = MutableLODIndex;

					if (InstanceSurfaceIndex >= 0)
					{
						OperationData->InstanceUpdateData.Surfaces.Push({});
						FInstanceUpdateData::FSurface& Surface = OperationData->InstanceUpdateData.Surfaces.Last();
						++Component.SurfaceCount;

						// Now Surface.MaterialIndex is decoded from a parameter at the end of this if()
						Surface.SurfaceId = SurfaceId;

						const int32 SharedSurfaceId = Instance->GetSharedSurfaceId(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex);
						const int32 SharedSurfaceIndex = SurfacesSharedId.Find(SharedSurfaceId);

						SurfacesSharedId.Add(SharedSurfaceId);

						if (SharedSurfaceId != INDEX_NONE)
						{
							if (SharedSurfaceIndex >= 0)
							{
								Surface = OperationData->InstanceUpdateData.Surfaces[SharedSurfaceIndex];
								continue;
							}

							// Find the first LOD where this surface can be found
							Instance->FindBaseSurfaceBySharedId(ComponentIndex, SharedSurfaceId, BaseSurfaceIndex, BaseLODIndex);

							Surface.SurfaceId = Instance->GetSurfaceId(BaseLODIndex, ComponentIndex, BaseSurfaceIndex);
						}

						// Images
						Surface.FirstImage = OperationData->InstanceUpdateData.Images.Num();
						Surface.ImageCount = Instance->GetImageCount(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 ImageIndex = 0; ImageIndex < Surface.ImageCount; ++ImageIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetImageId);

							OperationData->InstanceUpdateData.Images.Push({});
							FInstanceUpdateData::FImage& Image = OperationData->InstanceUpdateData.Images.Last();
							Image.Name = Instance->GetImageName(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, ImageIndex);
							Image.ImageID = Instance->GetImageId(BaseLODIndex, ComponentIndex, BaseSurfaceIndex, ImageIndex);
							Image.FullImageSizeX = 0;
							Image.FullImageSizeY = 0;
							Image.BaseLOD = BaseLODIndex;
							Image.BaseMip = 0;

							FString KeyName = Image.Name.ToString();
							int32 ImageKey = FCString::Atoi(*KeyName);

							if (ImageKey >= 0 && ImageKey < ModelResources.ImageProperties.Num())
							{
								const FMutableModelImageProperties& Props = ModelResources.ImageProperties[ImageKey];

								if (Props.IsPassThrough)
								{
									Image.bIsPassThrough = true;

									// Since it's known it's a pass-through texture there is no need to cache or convert it so we can generate it here already.
									Image.Image = System->GetImageInline(OperationData->InstanceID, Image.ImageID, 0, 0);

									check(Image.Image->IsReference());

									uint32 ReferenceID = Image.Image->GetReferencedTexture();

									if (ModelResources.PassThroughTextures.IsValidIndex(ReferenceID))
									{
										TSoftObjectPtr<UTexture> Ref = ModelResources.PassThroughTextures[ReferenceID];
										CustomizableObjectInstancePrivateData->PassThroughTexturesToLoad.Add(Ref);
									}
									else
									{
										// internal error.
										UE_LOG(LogMutable, Error, TEXT("Referenced image [%d] was not stored in the resource array."), ReferenceID);
									}
								}
							}
							else
							{
								// This means the compiled model (maybe coming from derived data) has images that the asset doesn't know about.
								UE_LOG(LogMutable, Error, TEXT("CustomizableObject derived data out of sync with asset for [%s]. Try recompiling it."), *CustomizableObject->GetName());
							}
						}

						// Vectors
						Surface.FirstVector = OperationData->InstanceUpdateData.Vectors.Num();
						Surface.VectorCount = Instance->GetVectorCount(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 VectorIndex = 0; VectorIndex < Surface.VectorCount; ++VectorIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetVector);
							OperationData->InstanceUpdateData.Vectors.Push({});
							FInstanceUpdateData::FVector& Vector = OperationData->InstanceUpdateData.Vectors.Last();
							Vector.Name = Instance->GetVectorName(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, VectorIndex);
							Vector.Vector = Instance->GetVector(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, VectorIndex);
						}

						// Scalars
						Surface.FirstScalar = OperationData->InstanceUpdateData.Scalars.Num();
						Surface.ScalarCount = Instance->GetScalarCount(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 ScalarIndex = 0; ScalarIndex < Surface.ScalarCount; ++ScalarIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetScalar)

							const FName ScalarName = Instance->GetScalarName(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, ScalarIndex);
							const float ScalarValue = Instance->GetScalar(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, ScalarIndex);
							
							FString EncodingMaterialIdString = "__MutableMaterialId";
							
							// Decoding Material Switch from Mutable parameter name
							if (ScalarName.ToString().Equals(EncodingMaterialIdString))
							{
								Surface.MaterialIndex = static_cast<uint32>(ScalarValue);
							
								// This parameter is not needed in the final material instance
								Surface.ScalarCount -= 1;
							}
							else
							{
								OperationData->InstanceUpdateData.Scalars.Push({ ScalarName, ScalarValue });
							}
						}
					}
				}
			}
		}

		// Copy ExtensionData Object node input from the Instance to the InstanceUpdateData
		for (int32 ExtensionDataIndex = 0; ExtensionDataIndex < Instance->GetExtensionDataCount(); ExtensionDataIndex++)
		{
			mu::Ptr<const mu::ExtensionData> ExtensionData;
			FName Name;
			Instance->GetExtensionData(ExtensionDataIndex, ExtensionData, Name);

			check(ExtensionData);

			FInstanceUpdateData::FNamedExtensionData& NewEntry = OperationData->InstanceUpdateData.ExtendedInputPins.AddDefaulted_GetRef();
			NewEntry.Data = ExtensionData;
			NewEntry.Name = Name;
			check(NewEntry.Name != NAME_None);
		}
	}


	void Task_Mutable_GetMeshes(const TSharedRef<FUpdateContextPrivate>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_GetMesh)
		FMutableScopeTimer Timer(OperationData->TaskGetMeshTime);
			
#if WITH_EDITOR
		const uint32 StartCycles = FPlatformTime::Cycles();
#endif

		Subtask_Mutable_BeginUpdate_GetMesh(OperationData);

		// TODO: Not strictly mutable: move to another worker thread task to free mutable access?
		impl::Subtask_Mutable_PrepareSkeletonData(OperationData);

		if (OperationData->GetCapturedDescriptor().GetBuildParameterRelevancy())
		{
			impl::Subtask_Mutable_UpdateParameterRelevancy(OperationData);
		}
		else
		{
			OperationData->RelevantParametersInProgress.Reset();
		}

#if WITH_EDITOR
		const uint32 EndCycles = FPlatformTime::Cycles();
		OperationData->MutableRuntimeCycles = EndCycles - StartCycles;
#endif
	}
}

namespace CustomizableObjectMipDataProvider::ImplDeprecated
{
	void Task_Mutable_UpdateImage(TSharedPtr<FMutableImageOperationData> OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_UpdateImage);
		const double StartTime = FPlatformTime::Seconds();
		
		// Cache memory used when starting the update of the image
		OperationData->ImageUpdateStartBytes = mu::FGlobalMemoryCounter::GetCounter();
		mu::FGlobalMemoryCounter::Zero();
		
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

#if WITH_EDITOR
			// Recompiling a CO in the editor will invalidate the previously generated Model. Check that it is valid before accessing the streamed data.
			if (Model && Model->IsValid())
#endif
			{

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


					mu::Ptr<const mu::Image> ResultImage;
					{
						MUTABLE_CPUPROFILER_SCOPE(GetImage);

						ResultImage = System->GetImageInline(InstanceID, MipImageID, ImageRef.BaseMip + OperationData->MipsToSkip, ImageRef.LOD);
					}

					check(ResultImage);

					int32 FullMipCount = ResultImage->GetMipmapCount(ResultImage->GetSizeX(), ResultImage->GetSizeY());
					int32 RealMipCount = ResultImage->GetLODCount();

					bool bForceMipchain =
						// Did we fail to generate the entire mipchain (if we have mips at all)?
						(RealMipCount != 1) && (RealMipCount != FullMipCount);

					if (bForceMipchain)
					{
						MUTABLE_CPUPROFILER_SCOPE(GetImage_MipFix);

						UE_LOG(LogMutable, Warning, TEXT("Mutable generated an incomplete mip chain for image."));

						// Force the right number of mips. The missing data will be black.
						mu::Ptr<mu::Image> NewImage = new mu::Image(ResultImage->GetSizeX(), ResultImage->GetSizeY(), FullMipCount, ResultImage->GetFormat(), mu::EInitializationType::Black);

						// Formats with BytesPerBlock == 0 will not allocate memory. This type of images are not expected here.
						check(!NewImage->DataStorage.IsEmpty());

						for (int32 L = 0; L < RealMipCount; ++L)
						{
							TArrayView<uint8> DestView = NewImage->DataStorage.GetLOD(L);
							TArrayView<const uint8> SrcView = ResultImage->DataStorage.GetLOD(L);

							check(DestView.Num() == SrcView.Num());
							FMemory::Memcpy(DestView.GetData(), SrcView.GetData(), DestView.Num());
						}

						ResultImage = NewImage;
					}

					OperationData->Result = ResultImage;
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
			}
		}

		if (CVarEnableBenchmark.GetValueOnAnyThread())
		{
			double Time = FPlatformTime::Seconds() - StartTime;
			// Report the peak memory used by the operation
			const int64 PeakMemory = mu::FGlobalMemoryCounter::GetPeak();
			// Report the peak memory used during the operation (operation + baseline)
			const int64 RealMemoryPeak = PeakMemory + OperationData->ImageUpdateStartBytes;

			const FString& CustomizableObjectPathName = OperationData->UpdateContext->GetCustomizableObjectPathName();
			const FString& InstancePathName = OperationData->UpdateContext->GetInstancePathName();

			const FString& Descriptor = OperationData->UpdateContext->CapturedDescriptor;

			FFunctionGraphTask::CreateAndDispatchWhenReady(
			[CustomizableObjectPathName, InstancePathName, Time, PeakMemory, RealMemoryPeak, Descriptor]()
			{
				if (!UCustomizableObjectSystem::IsCreated()) // We are shutting down
				{
					return;	
				}
				
				UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
				if (!System)
				{
					return;
				}

				System->GetPrivate()->LogBenchmarkUtil.FinishUpdateImage(CustomizableObjectPathName, InstancePathName, Descriptor, Time, PeakMemory, RealMemoryPeak);
			},
			TStatId{},
			nullptr,
			ENamedThreads::GameThread);
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

namespace CustomizableObjectMeshUpdate::ImplDeprecated
{
	void Task_Mutable_UpdateMesh(const TSharedPtr<FMutableMeshOperationData>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_UpdateMesh);

		mu::SystemPtr System = OperationData->System;
		const TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = OperationData->Model;

#if WITH_EDITOR
		// Recompiling a CO in the editor will invalidate the previously generated Model. Check that it is valid before accessing the streamed data.
		if (!Model || !Model->IsValid())
		{
			return;
		}
#endif

		// For now, we are forcing the recreation of mutable-side instances with every update.
		mu::Instance::ID InstanceID = System->NewInstance(Model);
		UE_LOG(LogMutable, Verbose, TEXT("Creating Mutable instance with id [%d] for a mesh update"), InstanceID);

		// LOD mask, set to all ones to build all LODs
		const uint32 LODMask = 0xFFFFFFFF;

		// Main instance generation step
		const mu::Instance* Instance = System->BeginUpdate(InstanceID, OperationData->Parameters, OperationData->State, LODMask);
		check(Instance);

		// Generate the required meshes
		for (int32 Index = OperationData->PendingFirstLODIdx; Index < OperationData->CurrentFirstLODIdx; ++Index)
		{
			const mu::FResourceID MeshID = OperationData->MeshIDs[Index];
			OperationData->Meshes[Index] = System->GetMeshInline(InstanceID, MeshID);
		}

		// End update
		System->EndUpdate(InstanceID);
		System->ReleaseInstance(InstanceID);

		if (CVarClearWorkingMemoryOnUpdateEnd.GetValueOnAnyThread())
		{
			System->ClearWorkingMemory();
		}
	}
}

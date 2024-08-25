// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraBakerRendererOutputSparseVolumeTexture.h"
#include "NiagaraBakerOutputSparseVolumeTexture.h"

#include "NiagaraBakerSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraComputeExecutionContext.h"
#include "NiagaraDataInterfaceGrid3DCollection.h"
#include "NiagaraDataInterfaceRenderTargetVolume.h"
#include "NiagaraBatchedElements.h"

#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/VolumeTexture.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PathViews.h"
#include "Modules/ModuleManager.h"
#include "Factories/VolumeTextureFactory.h"
#include "TextureResource.h"
#include "UObject/UObjectGlobals.h"

#include "TextureRenderTargetVolumeResource.h"
#include "Engine/TextureRenderTargetVolume.h"

#include "Editor/SparseVolumeTexture/Public/SparseVolumeTextureFactory.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "SparseVolumeTexture/SparseVolumeTextureData.h"
#include "SparseVolumeTexture/ISparseVolumeTextureStreamingManager.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "Misc/MessageDialog.h"
#include "AssetToolsModule.h"
#endif //WITH_EDITOR

TArray<FNiagaraBakerOutputBinding> FNiagaraBakerRendererOutputSparseVolumeTexture::GetRendererBindings(UNiagaraBakerOutput* InBakerOutput) const
{
	TArray<FNiagaraBakerOutputBinding> OutBindings;
	if (UNiagaraSystem* NiagaraSystem = InBakerOutput->GetTypedOuter<UNiagaraSystem>())
	{
		FNiagaraBakerOutputBindingHelper::ForEachEmitterDataInterface(
			NiagaraSystem,
			[&](const FString& EmitterName, const FString& VariableName, UNiagaraDataInterface* DataInterface)
			{
				if (UNiagaraDataInterfaceGrid3DCollection* Grid3D = Cast<UNiagaraDataInterfaceGrid3DCollection>(DataInterface))
				{
					TArray<FNiagaraVariableBase> GridVariables;
					TArray<uint32> GridVariableOffsets;
					int32 NumAttribChannelsFound;
					Grid3D->FindAttributes(GridVariables, GridVariableOffsets, NumAttribChannelsFound);

					for (const FNiagaraVariableBase& GridVariable : GridVariables)
					{
						const FString GridVariableString = GridVariable.GetName().ToString();

						FNiagaraBakerOutputBinding& NewBinding = OutBindings.AddDefaulted_GetRef();
						NewBinding.BindingName = FName(EmitterName + "." + VariableName + "." + GridVariableString);
						NewBinding.MenuCategory = FText::FromString(EmitterName + " " + TEXT("Grid3DCollection"));
						NewBinding.MenuEntry = FText::FromString(VariableName + "." + GridVariableString);
					}
				}
				else if (UNiagaraDataInterfaceRenderTargetVolume* VolumeRT = Cast<UNiagaraDataInterfaceRenderTargetVolume>(DataInterface))
				{
					FNiagaraBakerOutputBinding& NewBinding = OutBindings.AddDefaulted_GetRef();
					NewBinding.BindingName = FName(EmitterName + "." + VariableName);
					NewBinding.MenuCategory = FText::FromString(EmitterName + " " + TEXT("VolumeRenderTarget"));
					NewBinding.MenuEntry = FText::FromString(VariableName);
				}
			}
		);
	}
	return OutBindings;
}

FIntPoint FNiagaraBakerRendererOutputSparseVolumeTexture::GetPreviewSize(UNiagaraBakerOutput* InBakerOutput, FIntPoint InAvailableSize) const
{
	return InAvailableSize;
}

void FNiagaraBakerRendererOutputSparseVolumeTexture::RenderPreview(UNiagaraBakerOutput* InBakerOutput, const FNiagaraBakerRenderer& BakerRenderer, UTextureRenderTarget2D* RenderTarget, TOptional<FString>& OutErrorString) const
{
	BakerRenderer.RenderSceneCapture(RenderTarget, ESceneCaptureSource::SCS_SceneColorHDR);
}

FIntPoint FNiagaraBakerRendererOutputSparseVolumeTexture::GetGeneratedSize(UNiagaraBakerOutput* InBakerOutput, FIntPoint InAvailableSize) const
{
	return InAvailableSize;
}

void FNiagaraBakerRendererOutputSparseVolumeTexture::RenderGenerated(UNiagaraBakerOutput* InBakerOutput, const FNiagaraBakerRenderer& BakerRenderer, UTextureRenderTarget2D* RenderTarget, TOptional<FString>& OutErrorString) const
{
	static FString SVTNotFoundError(TEXT("Sparse Volume Texture asset not found.\nPlease bake to see the result."));

	UNiagaraBakerOutputSparseVolumeTexture* BakerOutput = CastChecked<UNiagaraBakerOutputSparseVolumeTexture>(InBakerOutput);
	UNiagaraBakerSettings* BakerSettings = BakerOutput->GetTypedOuter<UNiagaraBakerSettings>();

	UAnimatedSparseVolumeTexture* SVT = BakerOutput->GetAsset<UAnimatedSparseVolumeTexture>(BakerOutput->SparseVolumeTextureAssetPathFormat, 0);
	if (SVT == nullptr)
	{
		OutErrorString = SVTNotFoundError;
		return;
	}

	const float WorldTime = BakerRenderer.GetWorldTime();
	FCanvas Canvas(RenderTarget->GameThread_GetRenderTargetResource(), nullptr, FGameTime::CreateUndilated(WorldTime, FApp::GetDeltaTime()), BakerRenderer.GetFeatureLevel());

	const FNiagaraBakerOutputFrameIndices FrameIndices = BakerSettings->GetOutputFrameIndices(BakerOutput, WorldTime);

	BakerRenderer.RenderSparseVolumeTexture(RenderTarget, FrameIndices, SVT);
}

bool FNiagaraBakerRendererOutputSparseVolumeTexture::BeginBake(FNiagaraBakerFeedbackContext& FeedbackContext, UNiagaraBakerOutput* InBakerOutput)
{
	UNiagaraBakerOutputSparseVolumeTexture* OutputVolumeTexture = CastChecked<UNiagaraBakerOutputSparseVolumeTexture>(InBakerOutput);

	const FString AssetFullName = OutputVolumeTexture->GetAssetPath(OutputVolumeTexture->SparseVolumeTextureAssetPathFormat, 0);
	
	
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	TArray<FAssetData> FoundAssets;
	bool FoundAsset = false;
	if (AssetRegistry->GetAssetsByPackageName(FName(AssetFullName), FoundAssets))
	{
		if (FoundAssets.Num() > 0)
		{
			if (UObject* ExistingOject = StaticLoadObject(UAnimatedSparseVolumeTexture::StaticClass(), nullptr, *AssetFullName))
			{
				FoundAsset = true;
			}
		}
	}	

	UNiagaraBakerOutputSparseVolumeTexture* BakerOutput = CastChecked<UNiagaraBakerOutputSparseVolumeTexture>(InBakerOutput);
	UAnimatedSparseVolumeTexture* SVT = BakerOutput->GetAsset<UAnimatedSparseVolumeTexture>(BakerOutput->SparseVolumeTextureAssetPathFormat, 0);

	if (SVT == nullptr)
	{
		SVTAsset = UNiagaraBakerOutput::GetOrCreateAsset<UAnimatedSparseVolumeTexture, USparseVolumeTextureFactory>(AssetFullName);
	}
	else
	{				
		SVTAsset = NewObject<UAnimatedSparseVolumeTexture>(SVT->GetOuter(), UAnimatedSparseVolumeTexture::StaticClass(), *SVT->GetName(), RF_Public | RF_Standalone);
		SVTAsset->PostEditChange();				
	}
	
	if (!SVTAsset->BeginInitialize(1))
	{
		UE_LOG(LogNiagaraBaker, Error, TEXT("Cannot initialize SVT for baking"));
		return false;
	}

	return true;
}

void FNiagaraBakerRendererOutputSparseVolumeTexture::BakeFrame(FNiagaraBakerFeedbackContext& FeedbackContext, UNiagaraBakerOutput* InBakerOutput, int FrameIndex, const FNiagaraBakerRenderer& BakerRenderer)
{
	UNiagaraBakerOutputSparseVolumeTexture* OutputVolumeTexture = CastChecked<UNiagaraBakerOutputSparseVolumeTexture>(InBakerOutput);

	FVolumeDataInterfaceHelper DataInterface;

	TArray<FString> DataInterfacePath;
	OutputVolumeTexture->SourceBinding.SourceName.ToString().ParseIntoArray(DataInterfacePath, TEXT("."));
	if ( DataInterface.Initialize(DataInterfacePath, BakerRenderer.GetPreviewComponent()) == false )
	{
		return;
	}
	
	FNiagaraDataInterfaceProxyRenderTargetVolumeProxy* RT_Proxy_RenderTargetVolume = nullptr;
	FNiagaraDataInterfaceProxyGrid3DCollectionProxy* RT_Proxy_Grid3D = nullptr;
	if (DataInterface.VolumeRenderTargetDataInterface != nullptr)
	{					
		RT_Proxy_RenderTargetVolume = DataInterface.VolumeRenderTargetProxy;		
	}
	else if (DataInterface.Grid3DDataInterface != nullptr)
	{
		if (!DataInterface.Grid3DInstanceData_GameThread->UseRGBATexture)
		{
			UE_LOG(LogNiagaraBaker, Error, TEXT("Cannot bake SVTs from non RGBA Grid3D Collections"));
		}		
		RT_Proxy_Grid3D = DataInterface.Grid3DProxy;	
	}
	else
	{
		UE_LOG(LogNiagaraBaker, Error, TEXT("Cannot bake from data interface"));
	}
					
	//-OPT: Currently we are flushing rendering commands.  Do not remove this until making access to the frame data safe across threads.
	TArray<uint8> TextureData;
	FIntVector VolumeResolution = FIntVector(-1, -1, -1);
	EPixelFormat VolumeFormat = EPixelFormat::PF_A1;

	ENQUEUE_RENDER_COMMAND(NDIRenderTargetVolume_CacheFrame)
		(
			[RT_Proxy_RenderTargetVolume, RT_Proxy_Grid3D, RT_InstanceID = DataInterface.SystemInstance->GetId(), 
			RT_TextureData = &TextureData, RT_VolumeResolution = &VolumeResolution, RT_VolumeFormat = &VolumeFormat](FRHICommandListImmediate& RHICmdList)
			{
				FRHIGPUTextureReadback RenderTargetReadback("ReadVolumeTexture");
				uint32 BlockBytes = -1;				

				if (RT_Proxy_RenderTargetVolume)
				{
					if (const FRenderTargetVolumeRWInstanceData_RenderThread* InstanceData_RT = RT_Proxy_RenderTargetVolume->SystemInstancesToProxyData_RT.Find(RT_InstanceID))
					{
						*RT_VolumeResolution = InstanceData_RT->Size;
						RenderTargetReadback.EnqueueCopy(RHICmdList, InstanceData_RT->RenderTarget->GetRHI(), FIntVector(0, 0, 0), 0, *RT_VolumeResolution);
						BlockBytes = GPixelFormats[InstanceData_RT->RenderTarget->GetRHI()->GetFormat()].BlockBytes;
						*RT_VolumeFormat = InstanceData_RT->RenderTarget->GetRHI()->GetFormat();
					}
					else
					{
						UE_LOG(LogNiagaraBaker, Error, TEXT("No valid volume RT DI to do readback from"));
						return;
					}
				}
				else if (RT_Proxy_Grid3D)
				{
					if (const FGrid3DCollectionRWInstanceData_RenderThread* InstanceData_RT = RT_Proxy_Grid3D->SystemInstancesToProxyData_RT.Find(RT_InstanceID))
					{
						if (InstanceData_RT->CurrentData)
						{
							*RT_VolumeResolution = InstanceData_RT->NumCells;
							RenderTargetReadback.EnqueueCopy(RHICmdList, InstanceData_RT->CurrentData->GetPooledTexture()->GetRHI(), FIntVector(0, 0, 0), 0, *RT_VolumeResolution);
							BlockBytes = GPixelFormats[InstanceData_RT->CurrentData->GetPooledTexture()->GetRHI()->GetFormat()].BlockBytes;
							*RT_VolumeFormat = InstanceData_RT->CurrentData->GetPooledTexture()->GetRHI()->GetFormat();
						}
						else
						{
							UE_LOG(LogNiagaraBaker, Error, TEXT("No valid grid DI to do readback from"));
							return;
						}
					}
					else
					{
						UE_LOG(LogNiagaraBaker, Error, TEXT("No valid grid DI to do readback from"));
						return;
					}
				}
				else
				{
					UE_LOG(LogNiagaraBaker, Error, TEXT("No valid grid DI to do readback from"));
					return;
				}

				// Sync the GPU. Unfortunately we can't use the fences because not all RHIs implement them yet.
				RHICmdList.BlockUntilGPUIdle();
				RHICmdList.FlushResources();

				//Lock the readback staging texture
				int32 RowPitchInPixels;
				int32 BufferHeight;
				const uint8* LockedData = (const uint8*)RenderTargetReadback.Lock(RowPitchInPixels, &BufferHeight);
						
				int32 Count = RT_VolumeResolution->X * RT_VolumeResolution->Y * RT_VolumeResolution->Z * BlockBytes;
				RT_TextureData->AddUninitialized(Count);

				const uint8* SliceStart = LockedData;
				for (int32 Z = 0; Z < RT_VolumeResolution->Z; ++Z)
				{
					const uint8* RowStart = SliceStart;
					for (int32 Y = 0; Y < RT_VolumeResolution->Y; ++Y)
					{
						int32 Offset = 0 + Y * RT_VolumeResolution->X + Z * RT_VolumeResolution->X * RT_VolumeResolution->Y;
						FMemory::Memcpy(RT_TextureData->GetData() + Offset * BlockBytes, RowStart, BlockBytes * RT_VolumeResolution->X);

						RowStart += RowPitchInPixels * BlockBytes;
					}

					SliceStart += BufferHeight * RowPitchInPixels * BlockBytes;
				}

				//Unlock the staging texture
				RenderTargetReadback.Unlock();					
			}
	);
	FlushRenderingCommands();

	if (TextureData.Num() > 0)
	{

#if WITH_EDITOR			
		UE::SVT::FTextureDataCreateInfo SVTCreateInfo;
		SVTCreateInfo.VirtualVolumeAABBMin = FIntVector3::ZeroValue;
		SVTCreateInfo.VirtualVolumeAABBMax = VolumeResolution;
		SVTCreateInfo.FallbackValues[0] = FVector4f(0, 0, 0, 0);
		SVTCreateInfo.FallbackValues[1] = FVector4f(0, 0, 0, 0);
		SVTCreateInfo.AttributesFormats[0] = VolumeFormat;
		SVTCreateInfo.AttributesFormats[1] = PF_Unknown;

		UE::SVT::FTextureData SparseTextureData{};
		bool Success = SparseTextureData.CreateFromDense(SVTCreateInfo, TArrayView<uint8, int64>((uint8*)TextureData.GetData(), (int64)TextureData.Num() * sizeof(TextureData[0])), TArrayView<uint8>());

		if (!Success)
		{
			UE_LOG(LogNiagaraBaker, Error, TEXT("Cannot create SVT for data interface"));

			return;
		}

		if (!SVTAsset->AppendFrame(SparseTextureData, FTransform::Identity))
		{
			UE_LOG(LogNiagaraBaker, Error, TEXT("Cannot append frame to SVT"));
		}
#endif			
	}

}

void FNiagaraBakerRendererOutputSparseVolumeTexture::EndBake(FNiagaraBakerFeedbackContext& FeedbackContext, UNiagaraBakerOutput* InBakerOutput)
{
	UNiagaraBakerOutputSparseVolumeTexture* OutputVolumeTexture = CastChecked<UNiagaraBakerOutputSparseVolumeTexture>(InBakerOutput);
		
	if (!SVTAsset->EndInitialize())
	{
		UE_LOG(LogNiagaraBaker, Error, TEXT("Cannot EndInitialize on creating SVT"));
	}

	SVTAsset->PostLoad();
}


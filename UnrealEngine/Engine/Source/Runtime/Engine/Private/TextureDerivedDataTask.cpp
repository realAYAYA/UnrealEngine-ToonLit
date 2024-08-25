// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureDerivedDataTask.cpp: Tasks to update texture DDC.
=============================================================================*/

#include "TextureDerivedDataTask.h"
#include "IImageWrapperModule.h"
#include "Misc/ScopedSlowTask.h"
#include "TextureResource.h"
#include "Engine/Texture2DArray.h"

#if WITH_EDITOR

#include "Algo/Accumulate.h"
#include "Algo/AnyOf.h"
#include "ChildTextureFormat.h"
#include "DerivedDataBuild.h"
#include "DerivedDataBuildInputResolver.h"
#include "DerivedDataBuildInputs.h"
#include "DerivedDataBuildOutput.h"
#include "DerivedDataBuildSession.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataThreadPoolTask.h"
#include "Engine/VolumeTexture.h"
#include "EngineLogs.h"
#include "ImageCoreUtils.h"
#include "Interfaces/ITextureFormat.h"
#include "Serialization/BulkDataRegistry.h"
#include "Serialization/MemoryReader.h"
#include "TextureBuildUtilities.h"
#include "TextureCompiler.h"
#include "TextureDerivedDataBuildUtils.h"
#include "TextureFormatManager.h"
#include "VT/VirtualTextureChunkDDCCache.h"
#include "VT/VirtualTextureDataBuilder.h"

static TAutoConsoleVariable<int32> CVarVTValidateCompressionOnLoad(
	TEXT("r.VT.ValidateCompressionOnLoad"),
	0,
	TEXT("Validates that VT data contains no compression errors when loading from DDC")
	TEXT("This is slow, but allows debugging corrupt VT data (and allows recovering from bad DDC)")
);

static TAutoConsoleVariable<int32> CVarVTValidateCompressionOnSave(
	TEXT("r.VT.ValidateCompressionOnSave"),
	0,
	TEXT("Validates that VT data contains no compression errors before saving to DDC")
	TEXT("This is slow, but allows debugging corrupt VT data")
);

static TAutoConsoleVariable<int32> CVarForceRetileTextures(
	TEXT("r.ForceRetileTextures"),
	0,
	TEXT("If Shared Linear Texture Encoding is enabled in project settings, this will force the tiling build step to rebuild,")
	TEXT("however the linear texture is allowed to fetch from cache.")
);


void GetTextureDerivedDataKeyFromSuffix(const FString& KeySuffix, FString& OutKey);
static void PackTextureBuildMetadataInPlatformData(FTexturePlatformData* PlatformData, const UE::TextureBuildUtilities::FTextureBuildMetadata& BuildMetadata)
{
	PlatformData->PreEncodeMipsHash = BuildMetadata.PreEncodeMipsHash;
}

static FTextureEngineParameters GenerateTextureEngineParameters()
{
	FTextureEngineParameters EngineParameters;
	EngineParameters.bEngineSupportsTexture2DArrayStreaming = GSupportsTexture2DArrayStreaming;
	EngineParameters.bEngineSupportsVolumeTextureStreaming = GSupportsVolumeTextureStreaming;
	EngineParameters.NumInlineDerivedMips = NUM_INLINE_DERIVED_MIPS;
	return EngineParameters;
}

class FTextureStatusMessageContext : public FScopedSlowTask
{
public:
	explicit FTextureStatusMessageContext(const FText& InMessage)
		: FScopedSlowTask(0, InMessage, IsInGameThread())
	{
		UE_LOG(LogTexture,Display,TEXT("%s"),*InMessage.ToString());
	}
};


static FText ComposeTextureBuildText(const FString& TexturePathName, int32 SizeX, int32 SizeY, int32 NumSlices, int32 NumBlocks, int32 NumLayers, const FTextureBuildSettings& BuildSettings, ETextureEncodeSpeed InEncodeSpeed, int64 RequiredMemoryEstimate, bool bIsVT)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("TextureName"), FText::FromString(TexturePathName));
	Args.Add(TEXT("TextureFormatName"), FText::FromString(BuildSettings.TextureFormatName.GetPlainNameString()));
	Args.Add(TEXT("IsVT"), FText::FromString( FString( bIsVT ? TEXT(" VT") : TEXT("") ) ) );
	Args.Add(TEXT("TextureResolutionX"), FText::FromString(FString::FromInt(SizeX)));
	Args.Add(TEXT("TextureResolutionY"), FText::FromString(FString::FromInt(SizeY)));
	Args.Add(TEXT("NumBlocks"), FText::FromString(FString::FromInt(NumBlocks)));
	Args.Add(TEXT("NumLayers"), FText::FromString(FString::FromInt(NumLayers)));
	Args.Add(TEXT("NumSlices"), FText::FromString(FString::FromInt(NumSlices)));
	Args.Add(TEXT("EstimatedMemory"), FText::FromString(FString::SanitizeFloat(double(RequiredMemoryEstimate) / (1024.0*1024.0), 3)));
	
	const TCHAR* SpeedText = TEXT("");
	switch (InEncodeSpeed)
	{
	case ETextureEncodeSpeed::Final: SpeedText = TEXT("Final"); break;
	case ETextureEncodeSpeed::Fast: SpeedText = TEXT("Fast"); break;
	case ETextureEncodeSpeed::FinalIfAvailable: SpeedText = TEXT("FinalIfAvailable"); break;
	}

	Args.Add(TEXT("Speed"), FText::FromString(FString(SpeedText)));

	return FText::Format(
		NSLOCTEXT("Engine", "BuildTextureStatus", "Building textures: {TextureName} ({TextureFormatName}{IsVT}, {TextureResolutionX}x{TextureResolutionY} x{NumSlices}x{NumLayers}x{NumBlocks}) (Required Memory Estimate: {EstimatedMemory} MB), EncodeSpeed: {Speed}"), 
		Args
	);
}

static FText ComposeTextureBuildText(const FString& TexturePathName, const FTextureSourceData& TextureData, const FTextureBuildSettings& BuildSettings, ETextureEncodeSpeed InEncodeSpeed, int64 RequiredMemoryEstimate, bool bIsVT)
{
	const FImage & MipImage = TextureData.Blocks[0].MipsPerLayer[0][0];
	return ComposeTextureBuildText(TexturePathName, MipImage.SizeX, MipImage.SizeY, MipImage.NumSlices, TextureData.Blocks.Num(), TextureData.Layers.Num(), BuildSettings, InEncodeSpeed, RequiredMemoryEstimate, bIsVT);
}

static FText ComposeTextureBuildText(const UTexture& Texture, const FTextureBuildSettings& BuildSettings, ETextureEncodeSpeed InEncodeSpeed, int64 RequiredMemoryEstimate, bool bIsVT)
{
	return ComposeTextureBuildText(Texture.GetPathName(), Texture.Source.GetSizeX(), Texture.Source.GetSizeY(), Texture.Source.GetNumSlices(), Texture.Source.GetNumBlocks(), Texture.Source.GetNumLayers(), BuildSettings, InEncodeSpeed, RequiredMemoryEstimate, bIsVT);
}

static bool ValidateTexture2DPlatformData(const FTexturePlatformData& TextureData, const UTexture2D& Texture, bool bFromDDC)
{
	// Temporarily disable as the size check reports false negatives on some platforms
#if 0
	bool bValid = true;
	for (int32 MipIndex = 0; MipIndex < TextureData.Mips.Num(); ++MipIndex)
	{
		const FTexture2DMipMap& MipMap = TextureData.Mips[MipIndex];
		const int64 BulkDataSize = MipMap.BulkData.GetBulkDataSize();
		if (BulkDataSize > 0)
		{
			const int64 ExpectedMipSize = CalcTextureMipMapSize(TextureData.SizeX, TextureData.SizeY, TextureData.PixelFormat, MipIndex);
			if (BulkDataSize != ExpectedMipSize)
			{
				//UE_LOG(LogTexture,Warning,TEXT("Invalid mip data. Texture will be rebuilt. MipIndex %d [%dx%d], Expected size %lld, BulkData size %lld, PixelFormat %s, LoadedFromDDC %d, Texture %s"), 
				//	MipIndex, 
				//	MipMap.SizeX, 
				//	MipMap.SizeY, 
				//	ExpectedMipSize, 
				//	BulkDataSize, 
				//	GPixelFormats[TextureData.PixelFormat].Name, 
				//	bFromDDC ? 1 : 0,
				//	*Texture.GetFullName());
				
				bValid = false;
			}
		}
	}

	return bValid;
#else
	return true;
#endif
}

void FTextureSourceData::InitAsPlaceholder()
{
	ReleaseMemory();

	// This needs to be a tiny texture that can encode on all hardware. It's job is to
	// take up as little memory as possible for textures where we'd rather they not create
	// hw resources at all, but we don't want to hack in a ton of redirects/tests all over
	// the rendering codebase.

	// So we make a 4x4 black RGBA8 texture.
	FTextureSourceBlockData& Block = Blocks.AddDefaulted_GetRef();
	{
		Block.NumMips = 1;
		TArray<FImage>& MipsPerLayer = Block.MipsPerLayer.AddDefaulted_GetRef();
		FImage& Mip = MipsPerLayer.AddDefaulted_GetRef();
		UE::TextureBuildUtilities::GetPlaceholderTextureImage(&Mip);

		Block.NumSlices = Mip.NumSlices;
		Block.SizeX = Mip.SizeX;
		Block.SizeY = Mip.SizeY;
	}

	FTextureSourceLayerData& Layer = Layers.AddDefaulted_GetRef();
	{
		Layer.ImageFormat = ERawImageFormat::BGRA8;
		Layer.SourceGammaSpace = EGammaSpace::Linear;
	}

	bValid = true;
}

void FTextureSourceData::Init(UTexture& InTexture, TextureMipGenSettings InMipGenSettings, bool bInCubeMap, bool bInTextureArray, bool bInVolumeTexture, bool bAllowAsyncLoading)
{
	check( bValid == false ); // we set to true at the end, acts as our return value

	// Copy the channel min/max if we have it to avoid redoing it.
	if (InTexture.Source.GetLayerColorInfo().Num())
	{
		LayerChannelMinMax.Reset();
		for (const FTextureSourceLayerColorInfo& LayerColorInfo : InTexture.Source.GetLayerColorInfo())
		{
			TPair<FLinearColor, FLinearColor>& MinMax = LayerChannelMinMax.AddDefaulted_GetRef();
			MinMax.Key = LayerColorInfo.ColorMin;
			MinMax.Value = LayerColorInfo.ColorMax;
		}
	}

	const int32 NumBlocks = InTexture.Source.GetNumBlocks();
	const int32 NumLayers = InTexture.Source.GetNumLayers();
	if (NumBlocks < 1 || NumLayers < 1)
	{
		UE_LOG(LogTexture, Warning, TEXT("Texture has no source data: %s"), *InTexture.GetPathName());
		return;
	}

	Layers.Reserve(NumLayers);
	for (int LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FTextureSourceLayerData& LayerData = Layers.AddDefaulted_GetRef();

		LayerData.ImageFormat = FImageCoreUtils::ConvertToRawImageFormat( InTexture.Source.GetFormat(LayerIndex) );

		LayerData.SourceGammaSpace = InTexture.Source.GetGammaSpace(LayerIndex);
	}

	Blocks.Reserve(NumBlocks);
	for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
	{
		FTextureSourceBlock SourceBlock;
		InTexture.Source.GetBlock(BlockIndex, SourceBlock);

		if (SourceBlock.NumMips > 0 && SourceBlock.NumSlices > 0)
		{
			FTextureSourceBlockData& BlockData = Blocks.AddDefaulted_GetRef();
			BlockData.BlockX = SourceBlock.BlockX;
			BlockData.BlockY = SourceBlock.BlockY;
			BlockData.SizeX = SourceBlock.SizeX;
			BlockData.SizeY = SourceBlock.SizeY;
			BlockData.NumMips = SourceBlock.NumMips;
			BlockData.NumSlices = SourceBlock.NumSlices;

			if (InMipGenSettings != TMGS_LeaveExistingMips)
			{
				BlockData.NumMips = 1;
			}

			if (!bInCubeMap && !bInTextureArray && !bInVolumeTexture)
			{
				BlockData.NumSlices = 1;
			}

			BlockData.MipsPerLayer.SetNum(NumLayers);

			SizeInBlocksX = FMath::Max(SizeInBlocksX, SourceBlock.BlockX + 1);
			SizeInBlocksY = FMath::Max(SizeInBlocksY, SourceBlock.BlockY + 1);
			BlockSizeX = FMath::Max(BlockSizeX, SourceBlock.SizeX);
			BlockSizeY = FMath::Max(BlockSizeY, SourceBlock.SizeY);
		}
	}

	for (FTextureSourceBlockData& Block : Blocks)
	{
		// for the common case of NumBlocks == 1, BlockSizeX == Block.SizeX, MipBiasX/Y will both be zero
		const int32 MipBiasX = FMath::CeilLogTwo(BlockSizeX / Block.SizeX);
		const int32 MipBiasY = FMath::CeilLogTwo(BlockSizeY / Block.SizeY);
		if (MipBiasX != MipBiasY)
		{
			// @todo Oodle: this is failing even if "pad to pow2 square" is set, can we allow it through in that case?
			UE_LOG(LogTexture, Warning, TEXT("VT has blocks with mismatched aspect ratios, cannot build."), *InTexture.GetPathName());  // <- should be an Error, not a Warning
			return;
		}

		Block.MipBias = MipBiasX;
	}

	TextureFullName = InTexture.GetFullName();

	if (bAllowAsyncLoading && !InTexture.Source.IsBulkDataLoaded())
	{
		// Prepare the async source to be later able to load it from file if required.
		AsyncSource = InTexture.Source.CopyTornOff(); // This copies information required to make a safe IO load async.
	}

	bValid = true;
}


void FTextureSourceData::GetSourceMips(FTextureSource& Source, IImageWrapperModule* InImageWrapper)
{
	if (bValid)
	{
		if (Source.HasHadBulkDataCleared())
		{	// don't do any work we can't reload this
			UE_LOG(LogTexture, Error, TEXT("Unable to get texture source mips because its bulk data was released. %s"), *TextureFullName);
			ReleaseMemory();
			bValid = false;
			return;
		}
		if (!Source.HasPayloadData())
		{	// don't do any work we can't reload this
			UE_LOG(LogTexture, Warning, TEXT("Unable to get texture source mips because its bulk data has no payload. This may happen if it was duplicated from cooked data. %s"), *TextureFullName);
			ReleaseMemory();
			bValid = false;
			return;
		}

		// Grab a copy of ALL the mip data, we'll get views in to this later.
		const FTextureSource::FMipData ScopedMipData = Source.GetMipData(InImageWrapper);
		if (!ScopedMipData.IsValid())
		{
			UE_LOG(LogTexture, Warning, TEXT("Cannot retrieve source data for mips of %s"), *TextureFullName);
			ReleaseMemory();
			bValid = false;
			return;
		}

		// If we didn't get this from the texture source. As time goes on this will get hit less and less.
		if (LayerChannelMinMax.Num() != Layers.Num())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSourceData::GetSourceMips_ChannelMinMax);
			LayerChannelMinMax.Reset();
			for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
			{
				TPair<FLinearColor, FLinearColor>& LayerInfo = LayerChannelMinMax.AddDefaulted_GetRef();

				FLinearColor TotalMin(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
				FLinearColor TotalMax(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);

				for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); BlockIndex++)
				{
					FImageView MipImageView;
					FSharedBuffer MipData = ScopedMipData.GetMipDataWithInfo(BlockIndex, LayerIndex, 0, MipImageView);

					MipImageView.RawData = (void*)MipData.GetData();

					FLinearColor MinColor, MaxColor;
					FImageCore::ComputeChannelLinearMinMax(MipImageView, MinColor, MaxColor);

					TotalMin.R = FMath::Min(MinColor.R, TotalMin.R);
					TotalMin.G = FMath::Min(MinColor.G, TotalMin.G);
					TotalMin.B = FMath::Min(MinColor.B, TotalMin.B);
					TotalMin.A = FMath::Min(MinColor.A, TotalMin.A);

					TotalMax.R = FMath::Max(MaxColor.R, TotalMax.R);
					TotalMax.G = FMath::Max(MaxColor.G, TotalMax.G);
					TotalMax.B = FMath::Max(MaxColor.B, TotalMax.B);
					TotalMax.A = FMath::Max(MaxColor.A, TotalMax.A);
				}

				LayerInfo.Key = TotalMin;
				LayerInfo.Value = TotalMax;
			}
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSourceData::GetSourceMips_CopyMips);
			for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); ++BlockIndex)
			{
				FTextureSourceBlock SourceBlock;
				Source.GetBlock(BlockIndex, SourceBlock);

				FTextureSourceBlockData& BlockData = Blocks[BlockIndex];
				for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
				{
					const FTextureSourceLayerData& LayerData = Layers[LayerIndex];
					if (!BlockData.MipsPerLayer[LayerIndex].Num()) // If we already got valid data, nothing to do.
					{
						for (int32 MipIndex = 0; MipIndex < BlockData.NumMips; ++MipIndex)
						{
							FImageInfo MipImageInfo;
							FSharedBuffer MipData = ScopedMipData.GetMipDataWithInfo(BlockIndex, LayerIndex, MipIndex, MipImageInfo);

							FImage& SourceMip = BlockData.MipsPerLayer[LayerIndex].Emplace_GetRef(
								MipImageInfo.SizeX, MipImageInfo.SizeY, MipImageInfo.NumSlices,
								MipImageInfo.Format,
								MipImageInfo.GammaSpace);
							check(MipImageInfo.GammaSpace == LayerData.SourceGammaSpace);
							check(MipImageInfo.Format == LayerData.ImageFormat);

							SourceMip.RawData.Reset(MipData.GetSize());
							SourceMip.RawData.Append((const uint8*)MipData.GetData(), MipData.GetSize());
						}
					}
				}
			}
		}
	}
}


void FTextureSourceData::GetAsyncSourceMips(IImageWrapperModule* InImageWrapper)
{
	if (bValid && !Blocks[0].MipsPerLayer[0].Num() && AsyncSource.HasPayloadData())
	{
		GetSourceMips(AsyncSource, InImageWrapper);
	}
}


// When texture streaming is disabled, all of the mips are packed into a single FBulkData/FDerivedData
// and "inlined", meaning they are saved and loaded as part of the serialized asset data.
static bool GetBuildSettingsDisablesStreaming(const FTextureBuildSettings& InBuildSettings, const FTextureEngineParameters& InEngineParameters)
{
	if (InBuildSettings.bVirtualStreamable)
	{
		// Only basic 2d textures can be virtual streamable.
		return InBuildSettings.bCubemap || InBuildSettings.bVolume || InBuildSettings.bTextureArray;
	}
	else
	{
		return GetStreamingDisabledForNonVirtualTextureProperties(InBuildSettings.bCubemap, InBuildSettings.bVolume, InBuildSettings.bTextureArray, InEngineParameters);
	}
}

namespace UE::TextureDerivedData
{

using namespace UE::DerivedData;

// Handle converting the tiling build's inputs from names to concrete values.
class FTilingTextureBuildInputResolver final : public IBuildInputResolver
{
public:
	explicit FTilingTextureBuildInputResolver(UE::DerivedData::FBuildSession& InParentBuild_Session, UE::DerivedData::FBuildDefinition& InParentBuild_Definition, UE::DerivedData::FBuildPolicy& InParentBuild_Policy)
		: ParentBuild_Session(InParentBuild_Session),
		ParentBuild_Definition(InParentBuild_Definition),
		ParentBuild_Policy(InParentBuild_Policy)
	{
	}

	UE::DerivedData::FBuildSession& ParentBuild_Session;
	UE::DerivedData::FBuildDefinition ParentBuild_Definition;
	UE::DerivedData::FBuildPolicy ParentBuild_Policy;

	UE::DerivedData::FOptionalBuildOutput ParentBuild_Output;
	bool bParentBuild_HitCache = false;

	// Convert from named keys to hash/size pairs. There is no expectation that the results are
	// ready when this function returns - InResolvedCallback is called when the results arrive.
	void ResolveInputMeta(
		const FBuildDefinition& InDefinition,
		IRequestOwner& InOwner,
		FOnBuildInputMetaResolved&& InResolvedCallback) final
	{
		// This should only ever be called once, so the build should never be ready.
		check(!ParentBuild_Output);

		// Kick the parent build so we can get access to the results. When we get the results,
		// do the actual input resolution.
		ParentBuild_Session.Build(ParentBuild_Definition, {}, ParentBuild_Policy, InOwner,
			[this, InResolvedCallback = MoveTemp(InResolvedCallback), &InDefinition](UE::DerivedData::FBuildCompleteParams&& InParams)
		{
			this->ParentBuild_Output = MoveTemp(InParams.Output);

			if (InParams.Status == UE::DerivedData::EStatus::Canceled)
			{
				return;
			}

			this->bParentBuild_HitCache = EnumHasAnyFlags(InParams.BuildStatus, EBuildStatus::CacheQueryHit);

			TArray<UE::DerivedData::FBuildInputMetaByKey, TInlineAllocator<8>> Inputs;

			UE::DerivedData::EStatus Status = UE::DerivedData::EStatus::Ok;
			InDefinition.IterateInputBuilds([this, &Status, &Inputs](FUtf8StringView InOurKey, const UE::DerivedData::FBuildValueKey& InBuildValueKey)
			{
				if (InBuildValueKey.BuildKey != this->ParentBuild_Definition.GetKey())
				{
					Status = UE::DerivedData::EStatus::Error;
					return;
				}
				const UE::DerivedData::FValueWithId& ParentBuildValue = this->ParentBuild_Output.Get().GetValue(InBuildValueKey.Id);
				if (ParentBuildValue.IsNull())
				{
					Status = UE::DerivedData::EStatus::Error;
					return;
				}
				Inputs.Add({InOurKey, ParentBuildValue.GetRawHash(), ParentBuildValue.GetRawSize()});
			});

			if (Status != UE::DerivedData::EStatus::Ok)
			{
				return InResolvedCallback({{}, Status});
			}
			return InResolvedCallback({Inputs, UE::DerivedData::EStatus::Ok});
		});
	} // end ResolvedInputMeta

	void ResolveInputData(
		const FBuildDefinition & Definition,
		IRequestOwner & Owner,
		FOnBuildInputDataResolved && OnResolved,
		FBuildInputFilter && Filter) final
	{
		EStatus Status = EStatus::Ok;
		TArray<FBuildInputDataByKey, TInlineAllocator<8>> Inputs;
		Definition.IterateInputBuilds([this, &Filter, &Status, &Inputs](FUtf8StringView InOurKey, const UE::DerivedData::FBuildValueKey& InBuildValueKey)
		{
			if (Filter && Filter(InOurKey) == false)
			{
				return;
			}

			const UE::DerivedData::FValueWithId& ParentBuildValue = ParentBuild_Output.Get().GetValue(InBuildValueKey.Id);
			if (!ParentBuildValue || !ParentBuildValue.HasData())
			{
				Status = UE::DerivedData::EStatus::Error;
				return;
			}
			
			Inputs.Add({InOurKey, ParentBuildValue.GetData()});
		});

		OnResolved({ Inputs, Status });
	}
private:

};

class FTextureBuildInputResolver final : public IBuildInputResolver
{
public:
	explicit FTextureBuildInputResolver(UTexture& InTexture)
		: Texture(InTexture)
	{
	}

	const FCompressedBuffer& FindSource(FCompressedBuffer& Buffer, FTextureSource& Source, const FGuid& BulkDataId)
	{
		if (Source.GetPersistentId() != BulkDataId)
		{
			return FCompressedBuffer::Null;
		}
		if (!Buffer)
		{
			Source.OperateOnLoadedBulkData([&Buffer](const FSharedBuffer& BulkDataBuffer)
			{
				Buffer = FCompressedBuffer::Compress(BulkDataBuffer);
			});
		}
		return Buffer;
	}

	void ResolveInputMeta(
		const FBuildDefinition& Definition,
		IRequestOwner& Owner,
		FOnBuildInputMetaResolved&& OnResolved) final
	{
		EStatus Status = EStatus::Ok;
		TArray<FBuildInputMetaByKey> Inputs;
		Definition.IterateInputBulkData([this, &Status, &Inputs](FUtf8StringView Key, const FGuid& BulkDataId)
		{
			const FCompressedBuffer& Buffer = Key == UTF8TEXTVIEW("Source")
				? FindSource(SourceBuffer, Texture.Source, BulkDataId)
				: FindSource(CompositeSourceBuffer, Texture.GetCompositeTexture()->Source, BulkDataId);
			if (Buffer)
			{
				Inputs.Add({Key, Buffer.GetRawHash(), Buffer.GetRawSize()});
			}
			else
			{
				Status = EStatus::Error;
			}
		});
		OnResolved({Inputs, Status});
	}

	void ResolveInputData(
		const FBuildDefinition& Definition,
		IRequestOwner& Owner,
		FOnBuildInputDataResolved&& OnResolved,
		FBuildInputFilter&& Filter) final
	{
		EStatus Status = EStatus::Ok;
		TArray<FBuildInputDataByKey> Inputs;
		Definition.IterateInputBulkData([this, &Filter, &Status, &Inputs](FUtf8StringView Key, const FGuid& BulkDataId)
		{
			if (!Filter || Filter(Key))
			{
				const FCompressedBuffer& Buffer = Key == UTF8TEXTVIEW("Source")
					? FindSource(SourceBuffer, Texture.Source, BulkDataId)
					: FindSource(CompositeSourceBuffer, Texture.GetCompositeTexture()->Source, BulkDataId);
				if (Buffer)
				{
					Inputs.Add({Key, Buffer});
				}
				else
				{
					Status = EStatus::Error;
				}
			}
		});
		OnResolved({Inputs, Status});
	}

private:
	UTexture& Texture;
	FCompressedBuffer SourceBuffer;
	FCompressedBuffer CompositeSourceBuffer;
};

} // UE::TextureDerivedData

static void DDC1_StoreClassicTextureInDerivedData(
	TArray<FCompressedImage2D>& CompressedMips, FTexturePlatformData* DerivedData, bool bVolume, bool bTextureArray, bool bCubemap, uint32 NumMipsInTail,
	uint32 ExtData, bool bReplaceExistingDDC, const FString& TexturePathName, const FString& KeySuffix, int64& BytesCached
	)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Texture.DDC1_StoreClassicTextureInDerivedData);

	const int32 MipCount = CompressedMips.Num();

	// VT can be bigger than (1<<(MAX_TEXTURE_MIP_COUNT-1)) , but doesn't actually make all those mips
	// bForVirtualTextureStreamingBuild is false in this branch
	check(MipCount <= MAX_TEXTURE_MIP_COUNT);

	for (int32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
	{
		const FCompressedImage2D& CompressedImage = CompressedMips[MipIndex];
		FTexture2DMipMap* NewMip = new FTexture2DMipMap(CompressedImage.SizeX, CompressedImage.SizeY, CompressedImage.SizeZ);
		DerivedData->Mips.Add(NewMip);
		NewMip->FileRegionType = FFileRegion::SelectType(EPixelFormat(CompressedImage.PixelFormat));
		check(NewMip->SizeZ == 1 || bVolume || bTextureArray); // Only volume & arrays can have SizeZ != 1

		check(CompressedImage.RawData.GetTypeSize() == 1);
		int64 CompressedDataSize = CompressedImage.RawData.Num();

		// CompressedDataSize can exceed int32 ; eg. 16k x 16k x RGBA16F == 2 GB
		// DDC1 should be 64-bit safe now

		// CompressedImage sizes were padded up to multiple of 4 for d3d, no longer ; log the align-up-to-4 sizes for debugging?

		// log what the TextureFormat built :
		//	(todo: change to "LogTexture" instead of "LogTextureUpload" and remove the align-up-to-4 debug logs)
		UE_LOG(LogTextureUpload, Verbose, TEXT("Built texture: %s Compressed Mip %d PF=%d=%s : %dx%dx%d : %lld ; up4 %dx%d=%d"),
			*TexturePathName,
			MipIndex, (int)CompressedImage.PixelFormat,
			GetPixelFormatString((EPixelFormat)CompressedImage.PixelFormat),
			CompressedImage.SizeX, CompressedImage.SizeY, CompressedImage.SizeZ, 
			CompressedDataSize,
			(CompressedImage.SizeX + 3) & (~3),
			(CompressedImage.SizeY + 3) & (~3),
			((CompressedImage.SizeX + 3) & (~3)) * ((CompressedImage.SizeY + 3) & (~3)));

		NewMip->BulkData.Lock(LOCK_READ_WRITE);
		void* NewMipData = NewMip->BulkData.Realloc(CompressedDataSize);
		FMemory::Memcpy(NewMipData, CompressedImage.RawData.GetData(), CompressedDataSize);
		NewMip->BulkData.Unlock();

		if (MipIndex == 0)
		{
			DerivedData->SizeX = CompressedImage.SizeX;
			DerivedData->SizeY = CompressedImage.SizeY;
			DerivedData->PixelFormat = (EPixelFormat)CompressedImage.PixelFormat;

			// it would be better if CompressedImage just stored NumSlices, rather than recomputing it here
			if (bVolume || bTextureArray)
			{
				DerivedData->SetNumSlices(CompressedImage.SizeZ);
			}
			else if (bCubemap)
			{
				DerivedData->SetNumSlices(6);
			}
			else
			{
				DerivedData->SetNumSlices(1);
			}
			DerivedData->SetIsCubemap(bCubemap);
		}
		else
		{
			check(CompressedImage.PixelFormat == DerivedData->PixelFormat);
		}
	}

	FOptTexturePlatformData OptData;
	OptData.NumMipsInTail = NumMipsInTail;
	OptData.ExtData = ExtData;
	DerivedData->SetOptData(OptData);

	// Store it in the cache.
	// @todo: This will remove the streaming bulk data, which we immediately reload below!
	// Should ideally avoid this redundant work, but it only happens when we actually have 
	// to build the texture, which should only ever be once.
	BytesCached = PutDerivedDataInCache(DerivedData, KeySuffix, TexturePathName, bCubemap || (bVolume && !GSupportsVolumeTextureStreaming) || (bTextureArray && !GSupportsTexture2DArrayStreaming), bReplaceExistingDDC);
}

// Synchronous DDC1 texture build function
static void DDC1_BuildTexture(
	ITextureCompressorModule* Compressor,
	IImageWrapperModule* ImageWrapper,
	const UTexture& Texture, // should be able to get rid of this and just check CompositeTextureData.IsValid()
	const FString& TexturePathName,
	ETextureCacheFlags CacheFlags,
	FTextureSourceData& TextureData,
	FTextureSourceData& CompositeTextureData,
	const TArrayView<FTextureBuildSettings>& InBuildSettingsPerLayer,
	const FTexturePlatformData::FTextureEncodeResultMetadata& InBuildResultMetadata,

	const FString& KeySuffix,
	bool bReplaceExistingDDC,
	int64 RequiredMemoryEstimate,

	FTexturePlatformData* DerivedData,
	int64& BytesCached,
	bool& bSucceeded
	)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCacheDerivedDataWorker::BuildTexture);

	const bool bHasValidMip0 = TextureData.Blocks.Num() && TextureData.Blocks[0].MipsPerLayer.Num() && TextureData.Blocks[0].MipsPerLayer[0].Num();
	const bool bForVirtualTextureStreamingBuild = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::ForVirtualTextureStreamingBuild);

	check( bSucceeded == false ); // we set it to true if we succeed

	if (!ensure(Compressor))
	{
		UE_LOG(LogTexture, Warning, TEXT("Missing Compressor required to build texture %s"), *TexturePathName);
		return;
	}

	if (!bHasValidMip0)
	{
		return;
	}

	// this logs the "Building textures: " message :
	FTextureStatusMessageContext StatusMessage(
		ComposeTextureBuildText(TexturePathName, TextureData, InBuildSettingsPerLayer[0], (ETextureEncodeSpeed)InBuildSettingsPerLayer[0].RepresentsEncodeSpeedNoSend, RequiredMemoryEstimate, bForVirtualTextureStreamingBuild)
		);

	DerivedData->Reset();

	if (bForVirtualTextureStreamingBuild)
	{
		if (DerivedData->VTData == nullptr)
		{
			DerivedData->VTData = new FVirtualTextureBuiltData();
		}
		
		FVirtualTextureBuilderDerivedInfo PredictedInfo;
		if ( ! PredictedInfo.InitializeFromBuildSettings(TextureData, InBuildSettingsPerLayer.GetData()) )
		{
			UE_LOG(LogTexture, Warning, TEXT("VT InitializeFromBuildSettings failed: %s"), *TexturePathName);
			delete DerivedData->VTData;
			DerivedData->VTData = nullptr;
			bSucceeded = false;
			return;		
		}

		FVirtualTextureDataBuilder Builder(*DerivedData->VTData, TexturePathName, Compressor, ImageWrapper);
		if ( ! Builder.Build(TextureData, CompositeTextureData, &InBuildSettingsPerLayer[0], true) )
		{
			UE_LOG(LogTexture, Warning, TEXT("VT Build failed: %s"), *TexturePathName);
			delete DerivedData->VTData;
			DerivedData->VTData = nullptr;
			bSucceeded = false;
			return;		
		}

		// TextureData was freed by Build (FTextureSourceData.ReleaseMemory), don't use it from here down

		DerivedData->SizeX = DerivedData->VTData->Width;
		DerivedData->SizeY = DerivedData->VTData->Height;
		DerivedData->PixelFormat = DerivedData->VTData->LayerTypes[0];
		DerivedData->SetNumSlices(1);
		DerivedData->ResultMetadata = InBuildResultMetadata;

		// Verify our predicted count matches.
		check(PredictedInfo.NumMips == DerivedData->VTData->GetNumMips());

		bool bCompressionValid = true;
		if (CVarVTValidateCompressionOnSave.GetValueOnAnyThread())
		{
			bCompressionValid = DerivedData->VTData->ValidateData(TexturePathName, true);
		}

		if (ensureMsgf(bCompressionValid, TEXT("Corrupt Virtual Texture compression for %s, can't store to DDC"), *TexturePathName))
		{
			// Store it in the cache.
			// @todo: This will remove the streaming bulk data, which we immediately reload below!
			// Should ideally avoid this redundant work, but it only happens when we actually have 
			// to build the texture, which should only ever be once.
			BytesCached = PutDerivedDataInCache(DerivedData, KeySuffix, TexturePathName, InBuildSettingsPerLayer[0].bCubemap || InBuildSettingsPerLayer[0].bVolume || InBuildSettingsPerLayer[0].bTextureArray, bReplaceExistingDDC);

			if (DerivedData->VTData->Chunks.Num())
			{
				const bool bInlineMips = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::InlineMips);
				bSucceeded = !bInlineMips || DerivedData->TryInlineMipData(InBuildSettingsPerLayer[0].LODBiasWithCinematicMips, TexturePathName);
				if (!bSucceeded)
				{
					UE_LOG(LogTexture, Display, TEXT("Failed to put and then read back mipmap data from DDC for %s"), *TexturePathName);
				}
			}
			else
			{
				UE_LOG(LogTexture, Warning, TEXT("Failed to build %s derived data for %s"), *InBuildSettingsPerLayer[0].TextureFormatName.GetPlainNameString(), *TexturePathName);
			}
		}
	}
	else
	{
		// Only support single Block/Layer here (Blocks and Layers are intended for VT support)
		if (TextureData.Blocks.Num() > 1)
		{
			// This can happen if user attempts to import a UDIM without VT enabled
			UE_LOG(LogTexture, Log, TEXT("Texture %s was imported as UDIM with %d blocks but VirtualTexturing is not enabled, only the first block will be available"),
				*TexturePathName, TextureData.Blocks.Num());
		}
		if (TextureData.Layers.Num() > 1)
		{
			// This can happen if user attempts to use lightmaps or other layered VT without VT enabled
			UE_LOG(LogTexture, Log, TEXT("Texture %s has %d layers but VirtualTexturing is not enabled, only the first layer will be available"),
				*TexturePathName, TextureData.Layers.Num());
		}

		if (InBuildSettingsPerLayer[0].bCPUAccessible)
		{
			// Copy out the unaltered top mip for cpu access.
			FSharedImage* CPUCopy = new FSharedImage();
			TextureData.Blocks[0].MipsPerLayer[0][0].CopyTo(*CPUCopy);

			DerivedData->CPUCopy = FSharedImageConstRef(CPUCopy);
			DerivedData->SetHasCpuCopy(true);
			
			// Divert the texture source data to a tiny placeholder texture.
			TextureData.InitAsPlaceholder();
		}

		uint32 NumMipsInTail;
		uint32 ExtData;

		// Compress the texture by calling texture compressor directly.
		TArray<FCompressedImage2D> CompressedMips;
		if (Compressor->BuildTexture(TextureData.Blocks[0].MipsPerLayer[0],
			((bool)Texture.GetCompositeTexture() && CompositeTextureData.Blocks.Num() && CompositeTextureData.Blocks[0].MipsPerLayer.Num()) ? CompositeTextureData.Blocks[0].MipsPerLayer[0] : TArray<FImage>(),
			InBuildSettingsPerLayer[0],
			TexturePathName,
			CompressedMips,
			NumMipsInTail,
			ExtData,
			nullptr))
		{
			check(CompressedMips.Num());

			DDC1_StoreClassicTextureInDerivedData(
				CompressedMips, DerivedData, InBuildSettingsPerLayer[0].bVolume, InBuildSettingsPerLayer[0].bTextureArray, InBuildSettingsPerLayer[0].bCubemap, 
				NumMipsInTail, ExtData, bReplaceExistingDDC, TexturePathName, KeySuffix, BytesCached);

			bSucceeded = true;
			DerivedData->ResultMetadata = InBuildResultMetadata;

			const bool bInlineMips = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::InlineMips);
			if (bInlineMips) // Note that mips are inlined when cooking.
			{
				bSucceeded = DerivedData->TryInlineMipData(InBuildSettingsPerLayer[0].LODBiasWithCinematicMips, TexturePathName);
				if (bSucceeded == false)
				{
					// This should only ever happen with DDC issues - it can technically be a transient issue if you lose connection
					// in the middle of a build, but with a stable connection it's probably a ddc bug.
					UE_LOG(LogTexture, Warning, TEXT("Failed to put and then read back mipmap data from DDC for %s"), *TexturePathName);
				}
			}
		}
		else
		{
			// BuildTexture failed
			// will log below
			check( DerivedData->Mips.Num() == 0 );
			DerivedData->Mips.Empty();
			bSucceeded = false;

			UE_LOG(LogTexture, Warning, TEXT("BuildTexture failed to build %s derived data for %s"), *InBuildSettingsPerLayer[0].TextureFormatName.GetPlainNameString(), *TexturePathName);
		}
	}
}


static EPixelFormat GetOutputPixelFormat(const FTextureBuildSettings & BuildSettings, bool bHasAlpha)
{
	// get the TextureFormat so we can get the output pixel format :
		
	const ITextureFormat* TextureFormat = nullptr;

	ITextureFormatManagerModule* TFM = GetTextureFormatManager();
	if (TFM)
	{
		TextureFormat = TFM->FindTextureFormat(BuildSettings.TextureFormatName);
	}
	if (TextureFormat == nullptr)
	{
		UE_LOG(LogTexture, Warning,
			TEXT("Failed to find compressor for texture format '%s'."),
			*BuildSettings.TextureFormatName.ToString()
		);
			
		return PF_Unknown; /* Unknown */
	}

	EPixelFormat PixelFormat = TextureFormat->GetEncodedPixelFormat(BuildSettings,bHasAlpha);
	check( PixelFormat != PF_Unknown );

	return PixelFormat;
}

static int GetWithinSliceRDOMemoryUsePerPixel(const FName TextureFormatName, bool bHasAlpha)
{
	// Memory use of RDO data structures, per pixel, within each slice
	// not counting per-image memory use
	const int MemUse_BC1 = 57;
	const int MemUse_BC4 = 90;
	const int MemUse_BC5 = 2*MemUse_BC4;
	const int MemUse_BC6 = 8;
	const int MemUse_BC7 = 30;
	const int MemUse_BC3 = MemUse_BC4; // max of BC1,BC4
	
	if ( TextureFormatName == "DXT1" ||
		(TextureFormatName == "AutoDXT" && ! bHasAlpha) )
	{
		return MemUse_BC1;
	}
	else if ( TextureFormatName == "DXT3" || 
		TextureFormatName == "DXT5" ||
		TextureFormatName == "DXT5n" ||
		TextureFormatName == "AutoDXT")
	{
		return MemUse_BC3;
	}
	else if ( TextureFormatName == "BC4" )
	{
		return MemUse_BC4;
	}
	else if ( TextureFormatName == "BC5" )
	{
		return MemUse_BC5;
	}
	else if ( TextureFormatName == "BC6H" )
	{
		return MemUse_BC6;
	}
	else if ( TextureFormatName == "BC7" )
	{
		return MemUse_BC7;
	}
	else
	{
		// is this possible?
		UE_CALL_ONCE( [&](){
			UE_LOG(LogTexture, Display, TEXT("Unexpected non-BC TextureFormatName: %s."), *TextureFormatName.ToString());
		} );

		return 100;
	}
}

static int64 GetBuildRequiredMemoryEstimate(UTexture* InTexture,
	const FTextureBuildSettings* InSettingsPerLayerFetchFirst)
{
	const FTextureSource & Source = InTexture->Source;

	const bool bIsVT = InSettingsPerLayerFetchFirst[0].bVirtualStreamable;

	const bool bHasAlpha = true; 
	// @todo Oodle : need bHasAlpha for AutoDXT ; we currently over-estimate, treat all AutoDXT as BC3
	// BEWARE : you must use the larger mem use of the two to be conservative
	// BC1 has twice as many pixels per slice as BC3 so it's not trivially true that the mem use for BC3 is higher

	const bool bRDO = true;
	// @todo Oodle : be careful about using BuildSettings for this as there are two buildsettingses, just assume its on for now
	//   <- FIX ME, allow lower mem estimates for non-RDO

	// over-estimate is okay
	// try not to over-estimate by too much (reduces parallelism of cook)
	
	int64 MaxNumberOfWorkers = FMath::Max(1, FTaskGraphInterface::Get().GetNumWorkerThreads());

	if ( bIsVT )
	{
		// VT build does :
		// load all source images
		// for each layer/block :
		//    generate mips (requires F32 copy)
		//    output to intermediate format
		//    intermediate format copy is then used to make tiles
		//    for each tile :
		//       make padded tile in intermediate format
		//       encode to output format
		//       discard padded tile in intermediate format
		// all output tiles are then aggregated

		// Compute the memory it should take to uncompress the bulkdata in memory
		int64 TotalSourceBytes = 0;
		int64 TotalTopMipNumPixelsPerLayer = 0;
		int64 LargestBlockTopMipNumPixels = 0;

		for (int32 BlockIndex = 0; BlockIndex < Source.GetNumBlocks(); ++BlockIndex)
		{
			FTextureSourceBlock SourceBlock;
			Source.GetBlock(BlockIndex, SourceBlock);

			for (int32 LayerIndex = 0; LayerIndex < Source.GetNumLayers(); ++LayerIndex)
			{
				for (int32 MipIndex = 0; MipIndex < SourceBlock.NumMips; ++MipIndex)
				{
					TotalSourceBytes += Source.CalcMipSize(BlockIndex, LayerIndex, MipIndex);
				}
			}
		
			// assume pow2 options are the same for all layers, just use layer 0 here :
			const FTextureBuildSettings & BuildSettings = InSettingsPerLayerFetchFirst[0];

			int32 TargetSizeX, TargetSizeY, TargetSizeZ;
			UE::TextureBuildUtilities::GetPowerOfTwoTargetTextureSize(SourceBlock.SizeX,SourceBlock.SizeY,SourceBlock.NumSlices,
				BuildSettings.bVolume, (ETexturePowerOfTwoSetting::Type)BuildSettings.PowerOfTwoMode, 
				BuildSettings.ResizeDuringBuildX, BuildSettings.ResizeDuringBuildY, 
				TargetSizeX, TargetSizeY, TargetSizeZ);

			int64 CurrentBlockTopMipNumPixels = (int64)TargetSizeX * TargetSizeY * TargetSizeZ;

			TotalTopMipNumPixelsPerLayer += CurrentBlockTopMipNumPixels;

			LargestBlockTopMipNumPixels = FMath::Max( CurrentBlockTopMipNumPixels , LargestBlockTopMipNumPixels );
		}
		
		if ( TotalSourceBytes <= 0 )
		{
			return -1; /* Unknown */
		}
		
		// assume full mip chain :
		int64 TotalPixelsPerLayer = (TotalTopMipNumPixelsPerLayer * 4) / 3;

		int64 TotalNumPixels = TotalPixelsPerLayer * Source.GetNumLayers();

		// only one block of one layer does the float image mip build at a time :
		int64 IntermediateFloatColorBytes = (LargestBlockTopMipNumPixels * sizeof(FLinearColor) * 4) / 3;
		
		int64 TileSize = InSettingsPerLayerFetchFirst[0].VirtualTextureTileSize;
		int64 BorderSize = InSettingsPerLayerFetchFirst[0].VirtualTextureBorderSize;

		int64 NumTilesPerLayer = FMath::DivideAndRoundUp<int64>(TotalPixelsPerLayer,TileSize*TileSize);
		int64 NumTiles = NumTilesPerLayer * Source.GetNumLayers();
		int64 TilePixels = (TileSize + 2*BorderSize)*(TileSize + 2*BorderSize);

		int64 NumOutputPixelsPerLayer = NumTilesPerLayer * TilePixels;

		// intermediate is created just once per block, use max size estimate
		int64 VTIntermediateSizeBytes = IntermediateFloatColorBytes;
		int64 OutputSizeBytes = 0;
	
		int64 MaxPerPixelEncoderMemUse = 0;

		for (int32 LayerIndex = 0; LayerIndex < Source.GetNumLayers(); ++LayerIndex)
		{
			const FTextureBuildSettings & BuildSettings = InSettingsPerLayerFetchFirst[LayerIndex];
		
			// VT builds to an intermediate format.
			
			ERawImageFormat::Type IntermediateImageFormat = UE::TextureBuildUtilities::GetVirtualTextureBuildIntermediateFormat(BuildSettings);

			int64 IntermediateBytesPerPixel = ERawImageFormat::GetBytesPerPixel(IntermediateImageFormat);

			// + output bytes? (but can overlap with IntermediateFloatColorBytes)
			//	almost always less than IntermediateFloatColorBytes
			//  exception would be lots of udim blocks + lots of layers
			//  because IntermediateFloatColorBytes is per block/layer but output is held for all
			
			EPixelFormat PixelFormat = GetOutputPixelFormat(BuildSettings,bHasAlpha);

			if ( PixelFormat == PF_Unknown )
			{
				return -1; /* Unknown */
			}

			const FPixelFormatInfo & PFI = GPixelFormats[PixelFormat];

			OutputSizeBytes += ( NumOutputPixelsPerLayer * PFI.BlockBytes ) / ( PFI.BlockSizeX * PFI.BlockSizeY );

			// is it a blocked format :
			if ( PFI.BlockSizeX > 1 )
			{
				// another copy of Intermediate in BlockSurf swizzle :
				int CurPerPixelEncoderMemUse = IntermediateBytesPerPixel;

				if ( bRDO )
				{
					const FName TextureFormatName = UE::TextureBuildUtilities::TextureFormatRemovePrefixFromName(BuildSettings.TextureFormatName);
				
					int RDOMemUse = GetWithinSliceRDOMemoryUsePerPixel(TextureFormatName,bHasAlpha);
					CurPerPixelEncoderMemUse += 4; // activity
					CurPerPixelEncoderMemUse += RDOMemUse;
					CurPerPixelEncoderMemUse += 1; // output again
				}

				// max over any layer :
				MaxPerPixelEncoderMemUse = FMath::Max(MaxPerPixelEncoderMemUse,CurPerPixelEncoderMemUse);
			}
		}
				
		// after we make the Intermediate layer, it is cut into tiles
		// we then need mem for the intermediate format padded up to tiles
		// and then working encoder mem & compressed output space for each tile
		//	(tiles are made one by one in the ParallelFor to make the compressed output)
		// but at that point the FloatColorBytes is freed
		
		int64 NumberOfWorkingTiles = FMath::Min(NumTiles,MaxNumberOfWorkers);
		
		// VT tile encode mem :  
		int64 MemoryUsePerTile = MaxPerPixelEncoderMemUse * TilePixels; // around 1.8 MB
		{
			 // MemoryUsePerTile
			 // makes tile in IntermediateBytesPerPixel
			 // encodes out to OutputSizeBytes
			 // encoder (Oodle) temp mem
			 // TilePixels * IntermediateBytesPerPixel (twice: surf+blocksurf)
			 // TilePixels * Output bytes (twice: baseline+rdo output) (output already counted)
			 // TilePixels * activity mask
			 // MaxPerPixelEncoderMemUse is around 100
		}

		int64 TileCompressionBytes = NumberOfWorkingTiles * MemoryUsePerTile;

		int64 MemoryEstimate = TotalSourceBytes + VTIntermediateSizeBytes;
		// @todo Oodle : After we make the VT Intermediate, is the source BulkData freed?
		//   -> it seems no at the moment, but it could be
		
		// take larger of mem use during float image filter phase or tile compression phase
		MemoryEstimate += FMath::Max( IntermediateFloatColorBytes , TileCompressionBytes + OutputSizeBytes );

		MemoryEstimate += 64 * 1024; // overhead room

		//UE_LOG(LogTexture,Display,TEXT("GetBuildRequiredMemoryEstimate VT : %.3f MB"),MemoryEstimate/(1024*1024.f));

		return MemoryEstimate;
	}
	else
	{
		// non VT
		
		if ( Source.GetNumBlocks() != 1 || Source.GetNumLayers() != 1 )
		{
			return -1; /* Requires VT enabled. */
		}

		// Compute the memory it should take to uncompress the bulkdata in memory
		int64 TotalSourceBytes = 0;

		FTextureSourceBlock SourceBlock;
		Source.GetBlock(0, SourceBlock);

		for (int32 MipIndex = 0; MipIndex < SourceBlock.NumMips; ++MipIndex)
		{
			TotalSourceBytes += Source.CalcMipSize(0, 0, MipIndex);
		}
		
		if ( TotalSourceBytes <= 0 )
		{
			return -1; /* Unknown */
		}
		
		const FTextureBuildSettings & BuildSettings = InSettingsPerLayerFetchFirst[0];
		
		int32 TargetSizeX, TargetSizeY, TargetSizeZ;
		UE::TextureBuildUtilities::GetPowerOfTwoTargetTextureSize(SourceBlock.SizeX,SourceBlock.SizeY,SourceBlock.NumSlices,
			BuildSettings.bVolume, (ETexturePowerOfTwoSetting::Type)BuildSettings.PowerOfTwoMode, 
			BuildSettings.ResizeDuringBuildX, BuildSettings.ResizeDuringBuildY, 
			TargetSizeX, TargetSizeY, TargetSizeZ);

		int64 TotalTopMipNumPixels = (int64)TargetSizeX * TargetSizeY * TargetSizeZ;

		// assume full mip chain :
		int64 TotalNumPixels = (TotalTopMipNumPixels * 4)/3;

		// actually we have each mip twice for the float image filter phase so this is under-counting
		//	but that isn't held allocated while the output is made, so it can overlap with that mem
		int64 IntermediateFloatColorBytes = TotalNumPixels * sizeof(FLinearColor);
		
		int64 MemoryEstimate = TotalSourceBytes + IntermediateFloatColorBytes;
	
		EPixelFormat PixelFormat = GetOutputPixelFormat(BuildSettings,bHasAlpha);

		if ( PixelFormat == PF_Unknown )
		{
			return -1; /* Unknown */
		}

		const FPixelFormatInfo & PFI = GPixelFormats[PixelFormat];

		const int64 OutputSizeBytes = ( TotalNumPixels * PFI.BlockBytes ) / ( PFI.BlockSizeX * PFI.BlockSizeY );

		MemoryEstimate += OutputSizeBytes;

		// check to see if it's uncompressed or a BCN format :
		if ( IsDXTCBlockCompressedTextureFormat(PixelFormat) )
		{
			// block-compressed format ; assume it's using Oodle Texture
			
			if ( bRDO )
			{
				// two more copies in outputsize
				// baseline encode + UT or Layout
				MemoryEstimate += OutputSizeBytes*2;
			}

			// you also have to convert the float surface to an input format for Oodle
			//	this copy is done in TFO
			//  Oodle then allocs another copy to swizzle into blocks before encoding

			const FName TextureFormatName = UE::TextureBuildUtilities::TextureFormatRemovePrefixFromName(BuildSettings.TextureFormatName);
				
			int IntermediateBytesPerPixel;
			bool bNeedsIntermediateCopy = true;

			// this matches the logic in TextureFormatOodle :
			if ( TextureFormatName == "BC6H" )
			{
				IntermediateBytesPerPixel = 16; //RGBAF32
				bNeedsIntermediateCopy = false; // no intermediate used in TFO (float source kept), 1 blocksurf
			}
			else if ( TextureFormatName == "BC4" || TextureFormatName == "BC5" )
			{
				// changed: TFO uses 2_U16 now (4 byte intermediate)
				IntermediateBytesPerPixel = 8; // RGBA16
			}
			else
			{
				IntermediateBytesPerPixel = 4; // RGBA8
			}

			int NumIntermediateCopies = 1; // BlockSurf
			if ( bNeedsIntermediateCopy ) NumIntermediateCopies ++;

			MemoryEstimate += NumIntermediateCopies * IntermediateBytesPerPixel * TotalNumPixels;
			
			if ( bRDO )
			{
				// activity map for whole image :
				// (this has changed in newer versions of Oodle Texture)

				// Phase1 = computing activity map
				int ActivityBytesPerPixel;

				if ( TextureFormatName == "BC4" ) ActivityBytesPerPixel = 12;
				else if ( TextureFormatName == "BC5" ) ActivityBytesPerPixel = 16;
				else ActivityBytesPerPixel = 24;

				int64 RDOPhase1MemUse = ActivityBytesPerPixel * TotalNumPixels;

				// Phase2 = cut into slices, encode each slice
				// per-slice data structure memory use
				// non-RDO is all on stack so zero

				// fewer workers for small images ; roughly one slice per 64 KB of output
				//int64 NumberofSlices = FMath::DivideAndRoundUp<int64>(OutputSizeBytes,64*1024);
				int64 PixelsPerSlice = (64*1024*TotalNumPixels)/OutputSizeBytes;
				int64 NumberofSlices = FMath::DivideAndRoundUp<int64>(TotalNumPixels,PixelsPerSlice);
				if ( NumberofSlices <= 4 )
				{
					PixelsPerSlice = TotalNumPixels / NumberofSlices;
				}
			
				int64 MemoryUsePerWorker = PixelsPerSlice * GetWithinSliceRDOMemoryUsePerPixel(TextureFormatName,bHasAlpha);
					// MemoryUsePerWorker is around 10 MB
				int64 NumberOfWorkers = FMath::Min(NumberofSlices,MaxNumberOfWorkers);
			
				int64 RDOPhase2MemUse = 4 * TotalNumPixels; // activity map held on whole image
				RDOPhase2MemUse += NumberOfWorkers * MemoryUsePerWorker;

				// usually phase2 is higher
				// but on large BC6 images on machines with low core counts, phase1 can be higher

				MemoryEstimate += FMath::Max(RDOPhase1MemUse,RDOPhase2MemUse);
			}
		}
		else if (IsASTCBlockCompressedTextureFormat(PixelFormat))
		{
			// ASTCenc does an entermediate copy to RGBA16F for HDR formats and RGBA8 for LDR
			MemoryEstimate += (IsHDR(PixelFormat) ? 8 : 4) * TotalNumPixels;
			// internal memory use of ASTCenc is not estimated
			// @todo : fix me
		}
		else
		{
			// note: memory ues of non-Oodle encoders is not estimated
			// @todo : fix me
		}
		
		MemoryEstimate += 64 * 1024; // overhead room
		
		//UE_LOG(LogTexture,Display,TEXT("GetBuildRequiredMemoryEstimate non-VT : %.3f MB"),MemoryEstimate/(1024*1024.f));

		return MemoryEstimate;

		// @todo Oodle : not right for volumes & latlong cubes
		// @todo Oodle : not right with Composite , CPU textures
	}
}

FTextureCacheDerivedDataWorker::FTextureCacheDerivedDataWorker(
	ITextureCompressorModule* InCompressor,
	FTexturePlatformData* InDerivedData,
	UTexture* InTexture,
	const FTextureBuildSettings* InSettingsPerLayerFetchFirst,
	const FTextureBuildSettings* InSettingsPerLayerFetchOrBuild,
	const FTexturePlatformData::FTextureEncodeResultMetadata* InFetchFirstMetadata, // can be nullptr if not needed
	const FTexturePlatformData::FTextureEncodeResultMetadata* InFetchOrBuildMetadata, // can be nullptr if not needed
	ETextureCacheFlags InCacheFlags
	)
	: Compressor(InCompressor)
	, ImageWrapper(nullptr)
	, DerivedData(InDerivedData)
	, Texture(*InTexture)
	, TexturePathName(InTexture->GetPathName())
	, CacheFlags(InCacheFlags)
	, bSucceeded(false)
{
	check(DerivedData);
	
	RequiredMemoryEstimate = GetBuildRequiredMemoryEstimate(InTexture,InSettingsPerLayerFetchOrBuild);

	if (InSettingsPerLayerFetchFirst)
	{
		BuildSettingsPerLayerFetchFirst.SetNum(InTexture->Source.GetNumLayers());
		for (int32 LayerIndex = 0; LayerIndex < BuildSettingsPerLayerFetchFirst.Num(); ++LayerIndex)
		{
			BuildSettingsPerLayerFetchFirst[LayerIndex] = InSettingsPerLayerFetchFirst[LayerIndex];
		}
		if (InFetchFirstMetadata)
		{
			FetchFirstMetadata = *InFetchFirstMetadata;
		}
	}
	
	BuildSettingsPerLayerFetchOrBuild.SetNum(InTexture->Source.GetNumLayers());
	for (int32 LayerIndex = 0; LayerIndex < BuildSettingsPerLayerFetchOrBuild.Num(); ++LayerIndex)
	{
		BuildSettingsPerLayerFetchOrBuild[LayerIndex] = InSettingsPerLayerFetchOrBuild[LayerIndex];
	}
	if (InFetchOrBuildMetadata)
	{
		FetchOrBuildMetadata = *InFetchOrBuildMetadata;
	}

	// Keys need to be assigned on the create thread.
	{
		FString LocalKeySuffix;
		GetTextureDerivedDataKeySuffix(Texture, BuildSettingsPerLayerFetchOrBuild.GetData(), LocalKeySuffix);
		FString DDK;
		GetTextureDerivedDataKeyFromSuffix(LocalKeySuffix, DDK);
		InDerivedData->FetchOrBuildDerivedDataKey.Emplace<FString>(DDK);
	}
	if (BuildSettingsPerLayerFetchFirst.Num())
	{
		FString LocalKeySuffix;
		GetTextureDerivedDataKeySuffix(Texture, BuildSettingsPerLayerFetchFirst.GetData(), LocalKeySuffix);
		FString DDK;
		GetTextureDerivedDataKeyFromSuffix(LocalKeySuffix, DDK);
		InDerivedData->FetchFirstDerivedDataKey.Emplace<FString>(DDK);
	}

	// At this point, the texture *MUST* have a valid GUID.
	if (!Texture.Source.GetId().IsValid())
	{
		UE_LOG(LogTexture, Warning, TEXT("Building texture with an invalid GUID: %s"), *TexturePathName);
		Texture.Source.ForceGenerateGuid();
	}
	check(Texture.Source.GetId().IsValid());

	// Dump any existing mips.
	DerivedData->Reset();
	UTexture::GetPixelFormatEnum();
		
	const bool bAllowAsyncBuild = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::AllowAsyncBuild);
	const bool bAllowAsyncLoading = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::AllowAsyncLoading);
	const bool bForVirtualTextureStreamingBuild = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::ForVirtualTextureStreamingBuild);

	// FVirtualTextureDataBuilder always wants to load ImageWrapper module
	// This is not strictly necessary, used only for debug output, but seems simpler to just always load this here, doesn't seem like it should be too expensive
	if (bAllowAsyncLoading || bForVirtualTextureStreamingBuild)
	{
		ImageWrapper = &FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
	}

	// All of these settings are fixed across build settings and are derived directly from the texture.
	// So we can just use layer 0 of whatever we have.
	TextureData.Init(Texture, (TextureMipGenSettings)BuildSettingsPerLayerFetchOrBuild[0].MipGenSettings, BuildSettingsPerLayerFetchOrBuild[0].bCubemap, BuildSettingsPerLayerFetchOrBuild[0].bTextureArray, BuildSettingsPerLayerFetchOrBuild[0].bVolume, bAllowAsyncLoading);

	bool bNeedsCompositeData = Texture.GetCompositeTexture() && Texture.CompositeTextureMode != CTM_Disabled && Texture.GetCompositeTexture()->Source.IsValid();
	if (BuildSettingsPerLayerFetchOrBuild[0].bCPUAccessible)
	{
		// CPU accessible textures don't run image processing and thus don't need the composite data.
		bNeedsCompositeData = false;
	}

	if (bNeedsCompositeData)
	{
		bool bMatchingBlocks = Texture.GetCompositeTexture()->Source.GetNumBlocks() == Texture.Source.GetNumBlocks();
		
		if (!bMatchingBlocks)
		{
			UE_LOG(LogTexture, Warning, TEXT("Issue while building %s : Composite texture UDIM Block counts do not match. Composite texture will be ignored"), *TexturePathName);
			// note: does not fail, fill not warn again
		}

		if ( bMatchingBlocks )
		{
			CompositeTextureData.Init(*Texture.GetCompositeTexture(), (TextureMipGenSettings)BuildSettingsPerLayerFetchOrBuild[0].MipGenSettings, BuildSettingsPerLayerFetchOrBuild[0].bCubemap, BuildSettingsPerLayerFetchOrBuild[0].bTextureArray, BuildSettingsPerLayerFetchOrBuild[0].bVolume, bAllowAsyncLoading);
		}
	}
}

// Currently only used for prefetching (pulling data down from shared ddc to local ddc).
static bool TryCacheStreamingMips(const FString& TexturePathName, int32 FirstMipToLoad, int32 FirstMipToPrefetch, FTexturePlatformData* DerivedData)
{
	using namespace UE::DerivedData;
	check(DerivedData->DerivedDataKey.IsType<FString>());

	TArray<FCacheGetValueRequest, TInlineAllocator<16>> MipRequests;

	const int32 LowestMipIndexToPrefetchOrLoad = FMath::Min(FirstMipToPrefetch, FirstMipToLoad);
	const int32 NumMips = DerivedData->Mips.Num();
	const FSharedString Name(TexturePathName);
	for (int32 MipIndex = LowestMipIndexToPrefetchOrLoad; MipIndex < NumMips; ++MipIndex)
	{
		const FTexture2DMipMap& Mip = DerivedData->Mips[MipIndex];
		if (Mip.IsPagedToDerivedData())
		{
			const FCacheKey MipKey = ConvertLegacyCacheKey(DerivedData->GetDerivedDataMipKeyString(MipIndex, Mip));
			const ECachePolicy Policy
				= (MipIndex >= FirstMipToLoad) ? ECachePolicy::Default
				: (MipIndex >= FirstMipToPrefetch) ? ECachePolicy::Default | ECachePolicy::SkipData
				: ECachePolicy::Query | ECachePolicy::SkipData;
			MipRequests.Add({Name, MipKey, Policy, uint64(MipIndex)});
		}
	}

	if (MipRequests.IsEmpty())
	{
		return true;
	}

	bool bOk = true;
	FRequestOwner BlockingOwner(EPriority::Blocking);
	GetCache().GetValue(MipRequests, BlockingOwner, [DerivedData, &bOk](FCacheGetValueResponse&& Response)
	{
		bOk &= Response.Status == EStatus::Ok;
		if (const FSharedBuffer MipBuffer = Response.Value.GetData().Decompress())
		{
			FTexture2DMipMap& Mip = DerivedData->Mips[int32(Response.UserData)];
			Mip.BulkData.Lock(LOCK_READ_WRITE);
			void* MipData = Mip.BulkData.Realloc(int64(MipBuffer.GetSize()));
			FMemory::Memcpy(MipData, MipBuffer.GetData(), MipBuffer.GetSize());
			Mip.BulkData.Unlock();
		}
	});
	BlockingOwner.Wait();
	return bOk;
}

static void DDC1_FetchAndFillDerivedData(
	/* inputs */
	const UTexture& Texture,
	const FString& TexturePathName,
	ETextureCacheFlags CacheFlags,
	const TArrayView<FTextureBuildSettings>& BuildSettingsPerLayerFetchFirst,
	const FTexturePlatformData::FTextureEncodeResultMetadata& FetchFirstMetadata,

	const TArrayView<FTextureBuildSettings>& BuildSettingsPerLayerFetchOrBuild,
	const FTexturePlatformData::FTextureEncodeResultMetadata& FetchOrBuildMetadata,

	/* outputs */
	FTexturePlatformData* DerivedData,
	FString& KeySuffix,
	bool& bSucceeded,
	bool& bInvalidVirtualTextureCompression,
	int64& BytesCached
	)
{
	using namespace UE;
	using namespace UE::DerivedData;

	TRACE_CPUPROFILER_EVENT_SCOPE(Texture.DDC1_FetchAndFillDerivedData);

	bool bForceRebuild = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::ForceRebuild);
	FString FetchOrBuildKeySuffix;
	GetTextureDerivedDataKeySuffix(Texture, BuildSettingsPerLayerFetchOrBuild.GetData(), FetchOrBuildKeySuffix);

	if (bForceRebuild)
	{
		// If we know we are rebuilding, don't touch the cache.
		bSucceeded = false;
		bInvalidVirtualTextureCompression = false;
		KeySuffix = MoveTemp(FetchOrBuildKeySuffix);

		FString FetchOrBuildKey;
		GetTextureDerivedDataKeyFromSuffix(KeySuffix, FetchOrBuildKey);
		DerivedData->DerivedDataKey.Emplace<FString>(MoveTemp(FetchOrBuildKey));
		DerivedData->ResultMetadata = FetchOrBuildMetadata;
		return;
	}
		
	bool bForVirtualTextureStreamingBuild = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::ForVirtualTextureStreamingBuild);

	FSharedBuffer RawDerivedData;
	const FSharedString SharedTexturePathName(TexturePathName);
	const FSharedString SharedTextureFastPathName(WriteToString<256>(TexturePathName, TEXTVIEW(" [Fast]")));

	FString LocalDerivedDataKeySuffix;
	FString LocalDerivedDataKey;

	bool bGotDDCData = false;
	bool bUsedFetchFirst = false;
	if (BuildSettingsPerLayerFetchFirst.Num() && !bForceRebuild)
	{
		FString FetchFirstKeySuffix;
		GetTextureDerivedDataKeySuffix(Texture, BuildSettingsPerLayerFetchFirst.GetData(), FetchFirstKeySuffix);

		// If the suffixes are the same, then use fetchorbuild to avoid a get()
		if (FetchFirstKeySuffix != FetchOrBuildKeySuffix)
		{
			FString FetchFirstKey;
			GetTextureDerivedDataKeyFromSuffix(FetchFirstKeySuffix, FetchFirstKey);

			TArray<FCacheGetValueRequest, TInlineAllocator<1>> Requests;
			const FSharedString& TexturePathRequestName = (FetchFirstMetadata.EncodeSpeed == (uint8) ETextureEncodeSpeed::Fast) ? SharedTextureFastPathName : SharedTexturePathName;
			Requests.Add({ TexturePathRequestName, ConvertLegacyCacheKey(FetchFirstKey), ECachePolicy::Default, 0 /* UserData */});

			FRequestOwner BlockingOwner(EPriority::Blocking);

			GetCache().GetValue(Requests, BlockingOwner, [&RawDerivedData](FCacheGetValueResponse&& Response)
			{
				if (Response.UserData == 0)
				{
					RawDerivedData = Response.Value.GetData().Decompress();
				}
			});
			BlockingOwner.Wait();

			bGotDDCData = !RawDerivedData.IsNull();
			if (bGotDDCData)
			{
				bUsedFetchFirst = true;
				LocalDerivedDataKey = MoveTemp(FetchFirstKey);
				LocalDerivedDataKeySuffix = MoveTemp(FetchFirstKeySuffix);
			}
		}
	}

	if (bGotDDCData == false)
	{
		// Didn't get the initial fetch, so we're using fetch/build.
		LocalDerivedDataKeySuffix = MoveTemp(FetchOrBuildKeySuffix);
		GetTextureDerivedDataKeyFromSuffix(LocalDerivedDataKeySuffix, LocalDerivedDataKey);

		TArray<FCacheGetValueRequest, TInlineAllocator<1>> Requests;
		const FSharedString& TexturePathRequestName = (FetchOrBuildMetadata.EncodeSpeed == (uint8) ETextureEncodeSpeed::Fast) ? SharedTextureFastPathName : SharedTexturePathName;
		Requests.Add({ TexturePathRequestName, ConvertLegacyCacheKey(LocalDerivedDataKey), ECachePolicy::Default, 0 /* UserData */ });

		FRequestOwner BlockingOwner(EPriority::Blocking);

		GetCache().GetValue(Requests, BlockingOwner, [&RawDerivedData](FCacheGetValueResponse&& Response)
		{
			if (Response.UserData == 0)
			{
				RawDerivedData = Response.Value.GetData().Decompress();
			}
		});
		BlockingOwner.Wait();

		bGotDDCData = !RawDerivedData.IsNull();
	}

	KeySuffix = LocalDerivedDataKeySuffix;
	DerivedData->DerivedDataKey.Emplace<FString>(LocalDerivedDataKey);
	DerivedData->ResultMetadata = bUsedFetchFirst ? FetchFirstMetadata : FetchOrBuildMetadata;

	if (bGotDDCData)
	{
		const bool bInlineMips = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::InlineMips);
		const bool bForDDC = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::ForDDCBuild);
		int32 FirstResidentMipIndex = 0;

		BytesCached = RawDerivedData.GetSize();
		FMemoryReaderView Ar(RawDerivedData.GetView(), /*bIsPersistent=*/ true);
		DerivedData->Serialize(Ar, NULL);
		bSucceeded = true;

		if (bForVirtualTextureStreamingBuild)
		{
			if (DerivedData->VTData && DerivedData->VTData->IsInitialized())
			{
				const FSharedString Name(TexturePathName);
				for (FVirtualTextureDataChunk& Chunk : DerivedData->VTData->Chunks)
				{
					if (!Chunk.DerivedDataKey.IsEmpty())
					{
						Chunk.DerivedData = FDerivedData(Name, ConvertLegacyCacheKey(Chunk.DerivedDataKey));
					}
				}
			}
		}
		else
		{
			if (Algo::AnyOf(DerivedData->Mips, [](const FTexture2DMipMap& Mip) { return !Mip.BulkData.IsBulkDataLoaded(); }))
			{
				int32 MipIndex = 0;
				FirstResidentMipIndex = DerivedData->Mips.Num();
				const FSharedString Name(TexturePathName);
				for (FTexture2DMipMap& Mip : DerivedData->Mips)
				{
					PRAGMA_DISABLE_DEPRECATION_WARNINGS;
					const bool bPagedToDerivedData = Mip.bPagedToDerivedData;
					PRAGMA_ENABLE_DEPRECATION_WARNINGS;
					if (bPagedToDerivedData)
					{
						Mip.DerivedData = FDerivedData(Name, ConvertLegacyCacheKey(DerivedData->GetDerivedDataMipKeyString(MipIndex, Mip)));
					}
					else
					{
						FirstResidentMipIndex = FMath::Min(FirstResidentMipIndex, MipIndex);
					}
					++MipIndex;
				}
			}
		}

		// Load any streaming (not inline) mips that are necessary for our platform.
		if (bForDDC)
		{
			bSucceeded = DerivedData->TryLoadMips(0, nullptr, TexturePathName);

			if (bForVirtualTextureStreamingBuild)
			{
				if (DerivedData->VTData != nullptr &&
					DerivedData->VTData->IsInitialized())
				{
					FCacheGetValueRequest Request;
					Request.Name = TexturePathName;
					Request.Policy = ECachePolicy::Default | ECachePolicy::SkipData;

					TArray<FCacheGetValueRequest, TInlineAllocator<16>> ChunkKeys;
					for (const FVirtualTextureDataChunk& Chunk : DerivedData->VTData->Chunks)
					{
						if (!Chunk.DerivedDataKey.IsEmpty())
						{
							ChunkKeys.Add_GetRef(Request).Key = ConvertLegacyCacheKey(Chunk.DerivedDataKey);
						}
					}

					FRequestOwner BlockingOwner(EPriority::Blocking);
					GetCache().GetValue(ChunkKeys, BlockingOwner, [](FCacheGetValueResponse&&){});
					BlockingOwner.Wait();
				}
			}

			if (!bSucceeded)
			{
				UE_LOG(LogTexture, Display, TEXT("Texture %s is missing mips. The texture will be rebuilt."), *TexturePathName);
			}
		}
		else if (bInlineMips)
		{
			bSucceeded = DerivedData->TryInlineMipData(BuildSettingsPerLayerFetchOrBuild[0].LODBiasWithCinematicMips, TexturePathName);

			if (!bSucceeded)
			{
				UE_LOG(LogTexture, Display, TEXT("Texture %s is missing streaming mips when loading for an inline request. The texture will be rebuilt."), *TexturePathName);
			}
		}
		else
		{
			if (bForVirtualTextureStreamingBuild)
			{
				bSucceeded = DerivedData->VTData != nullptr &&
					DerivedData->VTData->IsInitialized() &&
					DerivedData->AreDerivedVTChunksAvailable(TexturePathName);

				if (!bSucceeded)
				{
					UE_LOG(LogTexture, Display, TEXT("Texture %s is missing VT Chunks. The texture will be rebuilt."), *TexturePathName);
				}
			}
			else
			{
				const bool bDisableStreaming = ! Texture.IsPossibleToStream();
				const int32 FirstMipToLoad = FirstResidentMipIndex;
				const int32 FirstNonStreamingMipIndex = DerivedData->Mips.Num() - DerivedData->GetNumNonStreamingMips(!bDisableStreaming);
				const int32 FirstMipToPrefetch = IsInGameThread() ? FirstMipToLoad : bDisableStreaming ? 0 : FirstNonStreamingMipIndex;
				bSucceeded = TryCacheStreamingMips(TexturePathName, FirstMipToLoad, FirstMipToPrefetch, DerivedData);
				if (!bSucceeded)
				{
					UE_LOG(LogTexture, Display, TEXT("Texture %s is missing derived mips. The texture will be rebuilt."), *TexturePathName);
				}
			}
		}

		if (bSucceeded && bForVirtualTextureStreamingBuild && CVarVTValidateCompressionOnLoad.GetValueOnAnyThread())
		{
			check(DerivedData->VTData);
			bSucceeded = DerivedData->VTData->ValidateData(TexturePathName, false);
			if (!bSucceeded)
			{
				UE_LOG(LogTexture, Display, TEXT("Texture %s has invalid cached VT data. The texture will be rebuilt."), *TexturePathName);
				bInvalidVirtualTextureCompression = true;
			}
		}
		
		// Reset everything derived data so that we can do a clean load from the source data
		if (!bSucceeded)
		{
			DerivedData->Mips.Empty();
			if (DerivedData->VTData)
			{
				delete DerivedData->VTData;
				DerivedData->VTData = nullptr;
			}
		}
	}

}

static bool DDC1_IsTextureDataValid(const FTextureSourceData& TextureData, const FTextureSourceData& CompositeTextureData)
{
	bool bTextureDataValid = TextureData.Blocks.Num() && TextureData.Blocks[0].MipsPerLayer.Num() && TextureData.Blocks[0].MipsPerLayer[0].Num();
	if (CompositeTextureData.IsValid()) // Says IsValid, but means whether or not we _need_ composite texture data.
	{
		// here we know we _need_ composite texture data, so we actually check if the stuff we loaded is valid.
		bool bCompositeDataValid = CompositeTextureData.Blocks.Num() && CompositeTextureData.Blocks[0].MipsPerLayer.Num() && CompositeTextureData.Blocks[0].MipsPerLayer[0].Num();
		return bTextureDataValid && bCompositeDataValid;
	}
	return bTextureDataValid;
}

// Tries to get the source texture data resident for building the texture.
static bool DDC1_LoadAndValidateTextureData(
	UTexture& Texture,
	FTextureSourceData& TextureData,
	FTextureSourceData& CompositeTextureData,
	IImageWrapperModule* ImageWrapper,
	bool bAllowAsyncLoading
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Texture.DDC1_LoadAndValidateTextureData);

	bool bHasTextureSourceMips = false;
	if (TextureData.IsValid() && Texture.Source.IsBulkDataLoaded())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GetSourceMips);
		TextureData.GetSourceMips(Texture.Source, ImageWrapper);
		bHasTextureSourceMips = true;
	}

	bool bHasCompositeTextureSourceMips = false;
	if (CompositeTextureData.IsValid() && Texture.GetCompositeTexture() && Texture.GetCompositeTexture()->Source.IsBulkDataLoaded())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GetCompositeSourceMips);
		check( Texture.GetCompositeTexture()->Source.IsValid() );
		CompositeTextureData.GetSourceMips(Texture.GetCompositeTexture()->Source, ImageWrapper);
		bHasCompositeTextureSourceMips = true;
	}

	if (bAllowAsyncLoading)
	{
		if (!bHasTextureSourceMips)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(GetAsyncSourceMips);
			TextureData.GetAsyncSourceMips(ImageWrapper);
			TextureData.AsyncSource.RemoveBulkData();
		}

		if (!bHasCompositeTextureSourceMips)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(GetAsyncCompositeSourceMips);
			CompositeTextureData.GetAsyncSourceMips(ImageWrapper);
			CompositeTextureData.AsyncSource.RemoveBulkData();
		}
	}

	return DDC1_IsTextureDataValid(TextureData, CompositeTextureData);
}


bool DDC1_BuildTiledClassicTexture(
	ITextureCompressorModule* Compressor,
	IImageWrapperModule* ImageWrapper,
	UTexture& Texture,
	const FString& TexturePathName,
	const TArrayView<FTextureBuildSettings> BuildSettingsPerLayerFetchFirst,
	const TArrayView<FTextureBuildSettings> BuildSettingsPerLayerFetchOrBuild,
	const FTexturePlatformData::FTextureEncodeResultMetadata& FetchFirstMetadata,
	const FTexturePlatformData::FTextureEncodeResultMetadata& FetchOrBuildMetadata,
	FTextureSourceData& TextureData,
	FTextureSourceData& CompositeTextureData,
	ETextureCacheFlags CacheFlags,
	int32 RequiredMemoryEstimate,
	const FString& KeySuffix,
	// outputs
	FTexturePlatformData* DerivedData,
	int64& BytesCached
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCacheDerivedDataWorker::BuildTiledClassicTexture);

	// we know we are a child format if we have a tiler.
	const ITextureTiler* Tiler = BuildSettingsPerLayerFetchOrBuild[0].Tiler;
	const FChildTextureFormat* ChildFormat = GetTextureFormatManager()->FindTextureFormat(BuildSettingsPerLayerFetchOrBuild[0].TextureFormatName)->GetChildFormat();

	// NonVT textures only have one layer.
	// We need to get the linear texture, which means we have to create the settings for it.
	TArray<FTextureBuildSettings, TInlineAllocator<1>> LinearSettingsPerLayerFetchFirst;
	if (BuildSettingsPerLayerFetchFirst.Num())
	{
		LinearSettingsPerLayerFetchFirst.Add(ChildFormat->GetBaseTextureBuildSettings(BuildSettingsPerLayerFetchFirst[0]));
	}

	TArray<FTextureBuildSettings, TInlineAllocator<1>> LinearSettingsPerLayerFetchOrBuild;
	LinearSettingsPerLayerFetchOrBuild.Add(ChildFormat->GetBaseTextureBuildSettings(BuildSettingsPerLayerFetchOrBuild[0]));

	// Now try and fetch.
	FTexturePlatformData LinearDerivedData;
	FString LinearKeySuffix;
	int64 LinearBytesCached = 0;
	bool bLinearDDCCorrupted = false;
	bool bLinearSucceeded = false;
	DDC1_FetchAndFillDerivedData(
		Texture, TexturePathName, CacheFlags,
		LinearSettingsPerLayerFetchFirst, FetchFirstMetadata, 
		LinearSettingsPerLayerFetchOrBuild, FetchOrBuildMetadata,
		&LinearDerivedData, LinearKeySuffix, bLinearSucceeded, bLinearDDCCorrupted, LinearBytesCached);

	BytesCached = LinearBytesCached;
	bool bHasLinearDerivedData = bLinearSucceeded;

	void* LinearMipData[MAX_TEXTURE_MIP_COUNT] = {};
	int64 LinearMipSizes[MAX_TEXTURE_MIP_COUNT];
	if (bHasLinearDerivedData)
	{
		// The linear bits are built - need to fetch
		if (LinearDerivedData.TryLoadMipsWithSizes(0, LinearMipData, LinearMipSizes, TexturePathName) == false)
		{
			// This can technically happen with a DDC failure and there is an expectation that we can recover and regenerate in such situations.
			// However, it should be very rare and most likely indicated a backend bug, so we still warn.
			UE_LOG(LogTexture, Warning, TEXT("Tiling texture build was unable to load the linear texture mips after fetching, will try to build: %s"), *TexturePathName);
			bHasLinearDerivedData = false;
		}

	}

	if (bHasLinearDerivedData == false)
	{
		// Linear data didn't exist, need to build it.
		bool bGotSourceTextureData = DDC1_LoadAndValidateTextureData(Texture, TextureData, CompositeTextureData, ImageWrapper, EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::AllowAsyncLoading));
		if (bGotSourceTextureData)
		{
			// We know we want all the mips for tiling, so force inline
			ETextureCacheFlags LinearCacheFlags = CacheFlags;
			EnumAddFlags(LinearCacheFlags, ETextureCacheFlags::InlineMips);

			// Note that this will update the DDC with the linear texture if we end up building _before_ the linear platforms!
			DDC1_BuildTexture(
				Compressor,
				ImageWrapper,
				Texture,
				TexturePathName,
				CacheFlags,
				TextureData,
				CompositeTextureData,
				LinearSettingsPerLayerFetchOrBuild,
				FetchOrBuildMetadata,
				LinearKeySuffix,
				bLinearDDCCorrupted,
				RequiredMemoryEstimate,
				&LinearDerivedData,
				LinearBytesCached,
				bHasLinearDerivedData);

			// This should succeed because we asked for inline mips if the build succeeded
			if (bHasLinearDerivedData && 
				LinearDerivedData.TryLoadMipsWithSizes(0, LinearMipData, LinearMipSizes, TexturePathName) == false)
			{
				UE_LOG(LogTexture, Error, TEXT("Tiling texture build was unable to load the linear texture mips after a successful build, bad bug!: %s"), *TexturePathName);
				return false;
			}
		}
	}

	if (bHasLinearDerivedData == false)
	{
		UE_LOG(LogTexture, Error, TEXT("Tiling texture build was unable to fetch or build the linear texture source: %s"), *TexturePathName);
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCacheDerivedDataWorker::TileTexture);

	check(LinearDerivedData.GetNumMipsInTail() == 0);

	// Have all the data - do some sanity checks as we convert to the metadata format the tiler expects.
	TArray<FMemoryView, TInlineAllocator<MAX_TEXTURE_MIP_COUNT>> InputTextureMipViews;
	FEncodedTextureDescription TextureDescription;
	FEncodedTextureExtendedData TextureExtendedData;
	int32 OutputTextureNumStreamingMips;
	{
#if DO_CHECK
		TArray<FImage>& SourceMips = TextureData.Blocks[0].MipsPerLayer[0];
		if (SourceMips.Num()) // if we're tiling existing linear data then the texture data isn't actually loaded so we can't do this sanity check.
		{
			int32 LinearMip0SizeX, LinearMip0SizeY, LinearMip0NumSlices;
			int32 LinearMipCount = Compressor->GetMipCountForBuildSettings(SourceMips[0].SizeX, SourceMips[0].SizeY, SourceMips[0].NumSlices, SourceMips.Num(), LinearSettingsPerLayerFetchOrBuild[0],
				LinearMip0SizeX,
				LinearMip0SizeY,
				LinearMip0NumSlices);
			check(LinearDerivedData.Mips[0].SizeX == LinearMip0SizeX);
			check(LinearDerivedData.Mips[0].SizeY == LinearMip0SizeY);
			check(LinearDerivedData.Mips.Num() == LinearMipCount);
			check(LinearDerivedData.GetNumSlices() == LinearMip0NumSlices);
		}
#endif

		LinearSettingsPerLayerFetchOrBuild[0].GetEncodedTextureDescriptionWithPixelFormat(
			&TextureDescription,
			LinearDerivedData.PixelFormat, LinearDerivedData.Mips[0].SizeX, LinearDerivedData.Mips[0].SizeY, LinearDerivedData.GetNumSlices(), LinearDerivedData.Mips.Num());

		for (int32 MipIndex = 0; MipIndex < TextureDescription.NumMips; MipIndex++)
		{
			check(LinearMipSizes[MipIndex] == TextureDescription.GetMipSizeInBytes(MipIndex));
		}

		TextureExtendedData = Tiler->GetExtendedDataForTexture(TextureDescription, LinearSettingsPerLayerFetchOrBuild[0].LODBias);
		OutputTextureNumStreamingMips = TextureDescription.GetNumStreamingMips(&TextureExtendedData, GenerateTextureEngineParameters());

		for (int32 MipIndex = 0; MipIndex < TextureDescription.NumMips; MipIndex++)
		{
			InputTextureMipViews.Add(FMemoryView(LinearMipData[MipIndex], LinearMipSizes[MipIndex]));
		}
	}

	TArray<FCompressedImage2D> TiledMips;
	TiledMips.AddDefaulted(TextureDescription.NumMips);

	// If the platform packs mip tails, we need to pass all the relevant mip buffers at once.
	int32 MipTailIndex, MipsInTail;
	TextureDescription.GetEncodedMipIterators(&TextureExtendedData, MipTailIndex, MipsInTail);

	UE_LOG(LogTexture, Display, TEXT("Tiling %s"), *TexturePathName);

	// Do the actual tiling.
	for (int32 EncodedMipIndex = 0; EncodedMipIndex < MipTailIndex + 1; EncodedMipIndex++)
	{
		int32 MipsRepresentedThisIndex = EncodedMipIndex == MipTailIndex ? MipsInTail : 1;

		TArrayView<FMemoryView> MipsForLevel = MakeArrayView(InputTextureMipViews.GetData() + EncodedMipIndex, MipsRepresentedThisIndex);

		FSharedBuffer MipData = Tiler->ProcessMipLevel(TextureDescription, TextureExtendedData, MipsForLevel, EncodedMipIndex);
		FIntVector3 MipDims = TextureDescription.GetMipDimensions(EncodedMipIndex);

		// Make sure we got the size we advertised prior to the build. If this ever fires then we
		// have a critical mismatch!
		check(TextureExtendedData.MipSizesInBytes[EncodedMipIndex] == MipData.GetSize());

		FCompressedImage2D& TiledMip = TiledMips[EncodedMipIndex];
		TiledMip.PixelFormat = LinearDerivedData.PixelFormat;
		TiledMip.SizeX = MipDims.X;
		TiledMip.SizeY = MipDims.Y;
		TiledMip.SizeZ = TextureDescription.GetNumSlices_WithDepth(EncodedMipIndex);
		if (TextureDescription.bCubeMap && !TextureDescription.bTextureArray)
		{
			TiledMip.SizeZ = 1; // compressed image 2d wants it this way...
		}

		// \todo try and Move this data rather than copying? We use FSharedBuffer as that's the future way,
		// but we're interacting with older systems that didn't have it, and we can't Move() from an FSharedBuffer.
		TiledMip.RawData.AddUninitialized(MipData.GetSize());
		FMemory::Memcpy(TiledMip.RawData.GetData(), MipData.GetData(), MipData.GetSize());
	} // end for each mip

	for (int32 MipIndex = 0; MipIndex < TextureDescription.NumMips; ++MipIndex)
	{
		FMemory::Free(LinearMipData[MipIndex]);
	}

	// The derived data expects to have mips (with no data) for the packed tail, if there is one
	for (int32 MipIndex = MipTailIndex + 1; MipIndex < TextureDescription.NumMips; ++MipIndex)
	{
		FCompressedImage2D& PrevMip = TiledMips[MipIndex - 1];
		FCompressedImage2D& DestMip = TiledMips[MipIndex];
		DestMip.SizeX = FMath::Max(1, PrevMip.SizeX >> 1);
		DestMip.SizeY = FMath::Max(1, PrevMip.SizeY >> 1);
		DestMip.SizeZ = TextureDescription.bVolumeTexture ? FMath::Max(1, PrevMip.SizeZ >> 1) : PrevMip.SizeZ;
		DestMip.PixelFormat = PrevMip.PixelFormat;
	}

	// We now have the final (tiled) data, and need to fill out the actual build output
	int64 TiledBytesCached;
	DDC1_StoreClassicTextureInDerivedData(TiledMips, DerivedData, TextureDescription.bVolumeTexture, TextureDescription.bTextureArray, TextureDescription.bCubeMap,
		TextureExtendedData.NumMipsInTail, TextureExtendedData.ExtData, false, TexturePathName, KeySuffix, TiledBytesCached);
	
	BytesCached += TiledBytesCached;

	// Do we need to reload streaming mips (evicted during DDC store)
	if (EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::InlineMips))
	{
		if (DerivedData->TryInlineMipData(LinearSettingsPerLayerFetchOrBuild[0].LODBiasWithCinematicMips, TexturePathName) == false)
		{
			UE_LOG(LogTexture, Display, TEXT("Tiled texture build failed to put and then read back tiled mipmap data from DDC for %s"), *TexturePathName);
		}
	}

	return true;
}


// DDC1 primary fetch/build work function
void FTextureCacheDerivedDataWorker::DoWork()
{
	using namespace UE;
	using namespace UE::DerivedData;

	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCacheDerivedDataWorker::DoWork);

	const bool bAllowAsyncBuild = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::AllowAsyncBuild);
	const bool bAllowAsyncLoading = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::AllowAsyncLoading);
	const bool bForVirtualTextureStreamingBuild = EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::ForVirtualTextureStreamingBuild);
	bool bInvalidVirtualTextureCompression = false;

	bSucceeded = false;
	bLoadedFromDDC = false;

	DDC1_FetchAndFillDerivedData(
		/* inputs */ Texture, TexturePathName, CacheFlags, BuildSettingsPerLayerFetchFirst, FetchFirstMetadata, BuildSettingsPerLayerFetchOrBuild, FetchOrBuildMetadata, 
		/* outputs */ DerivedData, KeySuffix, bSucceeded, bInvalidVirtualTextureCompression, BytesCached);
	if (bSucceeded)
	{
		bLoadedFromDDC = true;
	}

	if (BuildSettingsPerLayerFetchOrBuild[0].Tiler && !bForVirtualTextureStreamingBuild)
	{
		if (CVarForceRetileTextures.GetValueOnAnyThread())
		{
			// We do this after the fetch so it can fill out the metadata and key suffix that gets used.
			bSucceeded = false;
			bLoadedFromDDC = false;

			DerivedData->Mips.Empty();
			delete DerivedData->VTData;
			DerivedData->VTData = nullptr;
		}
	}

	check( ! bTriedAndFailed );

	if (!bSucceeded && bAllowAsyncBuild)
	{
		if (DDC1_LoadAndValidateTextureData(Texture, TextureData, CompositeTextureData, ImageWrapper, bAllowAsyncLoading))
		{
			for (int32 LayerIndex = 0; LayerIndex < BuildSettingsPerLayerFetchOrBuild.Num(); LayerIndex++)
			{
				if (LayerIndex < TextureData.LayerChannelMinMax.Num())
				{
					BuildSettingsPerLayerFetchOrBuild[LayerIndex].bKnowAlphaTransparency = Compressor->DetermineAlphaChannelTransparency(
						BuildSettingsPerLayerFetchOrBuild[LayerIndex], 
						TextureData.LayerChannelMinMax[LayerIndex].Key,
						TextureData.LayerChannelMinMax[LayerIndex].Value,
						BuildSettingsPerLayerFetchOrBuild[LayerIndex].bHasTransparentAlpha);
				}
			}

			// Replace any existing DDC data, if corrupt compression was detected
			const bool bReplaceExistingDDC = bInvalidVirtualTextureCompression;

			if (BuildSettingsPerLayerFetchOrBuild[0].Tiler && !bForVirtualTextureStreamingBuild)
			{
				bSucceeded = DDC1_BuildTiledClassicTexture(
					Compressor, ImageWrapper, Texture, TexturePathName, BuildSettingsPerLayerFetchFirst, BuildSettingsPerLayerFetchOrBuild, FetchFirstMetadata, FetchOrBuildMetadata,
					TextureData, CompositeTextureData, CacheFlags, RequiredMemoryEstimate, KeySuffix, DerivedData, BytesCached);
			}
			else
			{
				DDC1_BuildTexture(
					Compressor, ImageWrapper, Texture, TexturePathName, CacheFlags, TextureData, CompositeTextureData, BuildSettingsPerLayerFetchOrBuild, FetchOrBuildMetadata, 
					KeySuffix, bReplaceExistingDDC, RequiredMemoryEstimate, DerivedData, BytesCached, bSucceeded);
			}

			if (bInvalidVirtualTextureCompression && DerivedData->VTData)
			{
				// If we loaded data that turned out to be corrupt, flag it here so we can also recreate the VT data cached to local /DerivedDataCache/VT/ directory
				for (FVirtualTextureDataChunk& Chunk : DerivedData->VTData->Chunks)
				{
					Chunk.bCorruptDataLoadedFromDDC = true;
				}
			}

			if ( ! bSucceeded )
			{
				bTriedAndFailed = true;
			}
		}
		else
		{
			bSucceeded = false;

			// Excess logging to try and nail down a spurious failure.
			UE_LOG(LogTexture, Display, TEXT("Texture was not found in DDC and couldn't build as the texture source was unable to load or validate (%s)"), *TexturePathName);
			int32 TextureDataBlocks = TextureData.Blocks.Num();
			int32 TextureDataBlocksLayers = TextureDataBlocks > 0 ? TextureData.Blocks[0].MipsPerLayer.Num() : -1;
			int32 TextureDataBlocksLayerMips = TextureDataBlocksLayers > 0 ? TextureData.Blocks[0].MipsPerLayer[0].Num() : -1;

			UE_LOG(LogTexture, Display, TEXT("Texture Data Blocks: %d Layers: %d Mips: %d"), TextureDataBlocks, TextureDataBlocksLayers, TextureDataBlocksLayerMips);
			if (CompositeTextureData.IsValid()) // Says IsValid, but means whether or not we _need_ composite texture data.
			{
				int32 CompositeTextureDataBlocks = CompositeTextureData.Blocks.Num();
				int32 CompositeTextureDataBlocksLayers = CompositeTextureDataBlocks > 0 ? CompositeTextureData.Blocks[0].MipsPerLayer.Num() : -1;
				int32 CompositeTextureDataBlocksLayerMips = CompositeTextureDataBlocksLayers > 0 ? CompositeTextureData.Blocks[0].MipsPerLayer[0].Num() : -1;

				UE_LOG(LogTexture, Display, TEXT("Composite Texture Data Blocks: %d Layers: %d Mips: %d"), CompositeTextureDataBlocks, CompositeTextureDataBlocksLayers, CompositeTextureDataBlocksLayerMips);
			}

			// bTriedAndFailed = true; // no retry in Finalize ?  @todo ?
		}
	}

	// there are actually 3 states to bSucceeded
	//	tried & succeeded, tried & failed, not tried yet
	// we may try the build again in Finalize (eg. if !bAllowAsyncBuild)

	if (bSucceeded || bTriedAndFailed)
	{
		TextureData.ReleaseMemory();
		CompositeTextureData.ReleaseMemory();
	}

	if ( bSucceeded )
	{
		// Populate the VT DDC Cache now if we're asynchronously loading to avoid too many high prio/synchronous request on the render thread
		if (!IsInGameThread() && DerivedData->VTData && !DerivedData->VTData->Chunks.Last().DerivedDataKey.IsEmpty())
		{
			GetVirtualTextureChunkDDCCache()->MakeChunkAvailable_Concurrent(&DerivedData->VTData->Chunks.Last());
		}
	}
}

void FTextureCacheDerivedDataWorker::Finalize()
{
	// Building happens here whenever the ddc is missed and async builds aren't allowed.
	// This generally doesn't happen, but does in a few cases:
	// --	always happens with a ForceRebuildPlatformData, which is called whenever mip data is requested
	//		in the editor and is missing for some reason.
	// --	always with a lighting build, as the async light/shadowmap tasks will disallow async builds. 
	// --	if the texture compiler cvar disallows async texture compilation

	if ( bTriedAndFailed )
	{
		UE_LOG(LogTexture, Warning, TEXT("Texture build failed for %s.  Will not retry in Finalize."), *TexturePathName);
		return;
	}

	if (!bSucceeded)
	{
		if (!TextureData.HasPayload() && !Texture.Source.HasPayloadData())
		{
			UE_LOG(LogTexture, Warning, TEXT("Unable to build texture source data, no available payload for %s. This may happen if it was duplicated from cooked data."), *TexturePathName);
			return;
		}

		TextureData.GetSourceMips(Texture.Source, ImageWrapper);
		if (Texture.GetCompositeTexture() && Texture.GetCompositeTexture()->Source.IsValid())
		{
			CompositeTextureData.GetSourceMips(Texture.GetCompositeTexture()->Source, ImageWrapper);
		}

		if (DDC1_IsTextureDataValid(TextureData, CompositeTextureData) == false)
		{
			UE_LOG(LogTexture, Error, TEXT("Unable to get texture source data for synchronous build of %s"), *TexturePathName);
		}
		else
		{
			if (BuildSettingsPerLayerFetchOrBuild[0].Tiler && !EnumHasAnyFlags(CacheFlags, ETextureCacheFlags::ForVirtualTextureStreamingBuild))
			{
				bSucceeded = DDC1_BuildTiledClassicTexture(
					Compressor,
					ImageWrapper,
					Texture,
					TexturePathName,
					BuildSettingsPerLayerFetchFirst,
					BuildSettingsPerLayerFetchOrBuild,
					FetchFirstMetadata,
					FetchOrBuildMetadata,
					TextureData,
					CompositeTextureData,
					CacheFlags,
					RequiredMemoryEstimate,
					KeySuffix,
					DerivedData,
					BytesCached);
			}
			else
			{
				DDC1_BuildTexture(Compressor, ImageWrapper, Texture, TexturePathName, CacheFlags,
					TextureData, CompositeTextureData, BuildSettingsPerLayerFetchOrBuild, FetchOrBuildMetadata, KeySuffix, false /* currently corrupt vt data is not routed out of DoWork() */,
					RequiredMemoryEstimate, DerivedData, BytesCached, bSucceeded);
			}

			if ( ! bSucceeded )
			{
				bTriedAndFailed = true;
			}
		}
	}
		
	if (bSucceeded && BuildSettingsPerLayerFetchOrBuild[0].bVirtualStreamable) // Texture.VirtualTextureStreaming is more a hint that might be overruled by the buildsettings
	{
		check((DerivedData->VTData != nullptr) == Texture.VirtualTextureStreaming); 
	}
}



struct FBuildResultOptions
{
	bool bLoadStreamingMips;
	int32 FirstStreamingMipToLoad;
};

static bool UnpackPlatformDataFromBuild(FTexturePlatformData& OutPlatformData, UE::DerivedData::FBuildCompleteParams&& InBuildCompleteParams, FBuildResultOptions InBuildResultOptions)
{
	using namespace UE::DerivedData;
	UE::DerivedData::FBuildOutput& BuildOutput = InBuildCompleteParams.Output;

	FImage CPUCopy;
	CPUCopy.Format = ERawImageFormat::Invalid;
	{
		// CPUCopy might not exist if the build didn't request it
		const FValueWithId& MetadataValue = BuildOutput.GetValue(FValueId::FromName(ANSITEXTVIEW("CPUCopyImageInfo")));
		if (MetadataValue.IsValid())
		{
			if (CPUCopy.ImageInfoFromCompactBinary(FCbObject(MetadataValue.GetData().Decompress())) == false)
			{
				UE_LOG(LogTexture, Error, TEXT("Invalid CPUCopyImageInfo in build output '%s' by %s."), *BuildOutput.GetName(), *WriteToString<32>(BuildOutput.GetFunction()));
				return false;
			}

			const FValueWithId& DataValue = BuildOutput.GetValue(FValueId::FromName(ANSITEXTVIEW("CPUCopyRawData")));
			if (DataValue.IsValid() == false)
			{
				UE_LOG(LogTexture, Error, TEXT("Missing CPUCopyRawData in build output '%s' by %s."), *BuildOutput.GetName(), *WriteToString<32>(BuildOutput.GetFunction()));
				return false;
			}

			FSharedBuffer Data = DataValue.GetData().Decompress();
			CPUCopy.RawData.AddUninitialized(Data.GetSize());
			FMemory::Memcpy(CPUCopy.RawData.GetData(), Data.GetData(), Data.GetSize());
		}
	}

	// this will get pulled from the build metadata in a later cl...
	//{
	//	const FValueWithId& Value = BuildOutput.GetValue(FValueId::FromName(UTF8TEXTVIEW("TextureBuildMetadata")));
	//	PackTextureBuildMetadataInPlatformData(&OutPlatformData, FCbObject(Value.GetData().Decompress()));
	//}

	// We take this as a build output, however in ideal (future) situations, this is generated prior to build launch and
	// just routed through the build. Since we currently handle several varying situations, we just always consume it from
	// the build no matter where it came from. (Both TextureDescription and ExtendedData)
	FEncodedTextureDescription EncodedTextureDescription;
	{
		const FValueWithId& Value = BuildOutput.GetValue(FValueId::FromName(ANSITEXTVIEW("EncodedTextureDescription")));
		UE::TextureBuildUtilities::EncodedTextureDescription::FromCompactBinary(EncodedTextureDescription, FCbObject(Value.GetData().Decompress()));
	}

	FEncodedTextureExtendedData EncodedTextureExtendedData;
	{
		const FValueWithId& Value = BuildOutput.GetValue(FValueId::FromName(ANSITEXTVIEW("EncodedTextureExtendedData")));
		UE::TextureBuildUtilities::EncodedTextureExtendedData::FromCompactBinary(EncodedTextureExtendedData, FCbObject(Value.GetData().Decompress()));
	}

	// consider putting this in the build output so that it's only ever polled in one place.
	const FTextureEngineParameters EngineParameters = GenerateTextureEngineParameters();
	int32 NumStreamingMips = EncodedTextureDescription.GetNumStreamingMips(&EncodedTextureExtendedData, EngineParameters);
	int32 NumEncodedMips = EncodedTextureDescription.GetNumEncodedMips(&EncodedTextureExtendedData);
	check(NumEncodedMips >= NumStreamingMips);

	//
	// Mips are split up:
	//	Streaming mips are all stored independently under value FTexturePlatformData::MakeMipId(MipIndex);
	//	Nonstreaming ("inlined") mips are stored in one buffer under value "MipTail". To disentangle the separate mips
	//	we need their size. If EncodedTextureExtendedData is present, we have the ::MipSizesInBytes field. If that isn't
	//	present, then we know it's nothing fancy and we can directly compute mip sizes.
	//
	TArray<uint64, TInlineAllocator<MAX_TEXTURE_MIP_COUNT>> MipSizesFromBuild;
	if (EncodedTextureExtendedData.MipSizesInBytes.Num() == 0)
	{
		// Normal mip sizes - generate from texture description
		MipSizesFromBuild.AddUninitialized(EncodedTextureDescription.NumMips);
		for (int32 MipIndex = 0; MipIndex < EncodedTextureDescription.NumMips; MipIndex++)
		{
			MipSizesFromBuild[MipIndex] = EncodedTextureDescription.GetMipSizeInBytes(MipIndex);
		}
	}

	// For the moment, we don't necessarily have the sizes from the extended data, in that case we must have them from the build.
	TArray<uint64, TInlineAllocator<MAX_TEXTURE_MIP_COUNT>>& MipSizesInBytes = EncodedTextureExtendedData.MipSizesInBytes.Num() ? EncodedTextureExtendedData.MipSizesInBytes : MipSizesFromBuild;
	check(MipSizesInBytes.Num() == NumEncodedMips);


	//
	//
	// We have all the metadata we need, we can grab the data
	//
	//
	OutPlatformData.PixelFormat = EncodedTextureDescription.PixelFormat;
	OutPlatformData.SizeX = EncodedTextureDescription.TopMipSizeX;
	OutPlatformData.SizeY = EncodedTextureDescription.TopMipSizeY;
	OutPlatformData.OptData.NumMipsInTail = EncodedTextureExtendedData.NumMipsInTail;
	OutPlatformData.OptData.ExtData = EncodedTextureExtendedData.ExtData;
	{
		const bool bHasOptData = (EncodedTextureExtendedData.NumMipsInTail != 0) || (EncodedTextureExtendedData.ExtData != 0);
		const bool bHasCPUCopy = CPUCopy.Format != ERawImageFormat::Invalid;
		OutPlatformData.SetPackedData(EncodedTextureDescription.GetNumSlices_WithDepth(0), bHasOptData, EncodedTextureDescription.bCubeMap, bHasCPUCopy);
	}
	OutPlatformData.Mips.Empty(EncodedTextureDescription.NumMips);
	EFileRegionType FileRegion = FFileRegion::SelectType(EncodedTextureDescription.PixelFormat);


	FSharedBuffer MipTailData;
	if (EncodedTextureDescription.NumMips > NumStreamingMips)
	{
		const FValueWithId& MipTailValue = BuildOutput.GetValue(FValueId::FromName(ANSITEXTVIEW("MipTail")));
		if (!MipTailValue)
		{
			UE_LOG(LogTexture, Error, TEXT("Missing texture mip tail for build of '%s' by %s."), *BuildOutput.GetName(), *WriteToString<32>(BuildOutput.GetFunction()));
			return false;
		}
		MipTailData = MipTailValue.GetData().Decompress();
	}



	uint64 CurrentMipTailOffset = 0;
	for (int32 MipIndex = 0; MipIndex < EncodedTextureDescription.NumMips; MipIndex++)
	{
		const FIntVector3 MipDims = EncodedTextureDescription.GetMipDimensions(MipIndex);
		FTexture2DMipMap* NewMip = new FTexture2DMipMap(MipDims.X, MipDims.Y, MipDims.Z);
		OutPlatformData.Mips.Add(NewMip);

		NewMip->FileRegionType = FileRegion;
		if (EncodedTextureDescription.bTextureArray)
		{
			// FTexture2DMipMap expects SizeZ to be the array count, potentially with cubemap slices.
			NewMip->SizeZ = EncodedTextureDescription.ArraySlices;

			if (EncodedTextureDescription.bCubeMap)
			{
				NewMip->SizeZ *= 6; 
			}
		}

		if (MipIndex >= NumEncodedMips)
		{
			// Packed mip tail data is inside the outermost mip for the pack, so we don't have
			// any bulk data to pull out.
			continue;
		}

		if (MipIndex >= NumStreamingMips)
		{			
			// This mip is packed inside a single buffer. This is distinct from a "packed mip tail", but might
			// coincidentally be the same. All mips past NumStreamingMips need to be copied into the bulk data
			// and are always resident in memory with the texture.

			uint64 MipSizeInBytes = MipSizesInBytes[MipIndex];
			FMemoryView MipView = MipTailData.GetView().Mid(CurrentMipTailOffset, MipSizeInBytes);
			CurrentMipTailOffset += MipSizeInBytes;

			NewMip->BulkData.Lock(LOCK_READ_WRITE);
			void* MipAllocData = NewMip->BulkData.Realloc(int64(MipSizeInBytes));
			MakeMemoryView(MipAllocData, MipSizeInBytes).CopyFrom(MipView);
			NewMip->BulkData.Unlock();
		}
		else
		{
			const FValueId MipId = FTexturePlatformData::MakeMipId(MipIndex);
			const FValueWithId& MipValue = BuildOutput.GetValue(MipId);
			if (!MipValue)
			{
				UE_LOG(LogTexture, Error, TEXT("Missing streaming texture mip %d for build of '%s' by %s."), MipIndex, *BuildOutput.GetName(), *WriteToString<32>(BuildOutput.GetFunction()));
				return false;
			}

			// Did whoever launched the build want the streaming mips in memory?
			if (InBuildResultOptions.bLoadStreamingMips && (MipIndex >= InBuildResultOptions.FirstStreamingMipToLoad))
			{
				NewMip->BulkData.Lock(LOCK_READ_WRITE);
				const uint64 MipSize = MipValue.GetRawSize();
				void* MipData = NewMip->BulkData.Realloc(IntCastChecked<int64>(MipSize));
				ON_SCOPE_EXIT{ NewMip->BulkData.Unlock(); };
				if (!MipValue.GetData().TryDecompressTo(MakeMemoryView(MipData, MipSize)))
				{
					UE_LOG(LogTexture, Error, TEXT("Failed to decompress streaming texture mip %d for build of '%s' by %s."), MipIndex, *BuildOutput.GetName(), *WriteToString<32>(BuildOutput.GetFunction()));
					return false;
				}
			}

			FSharedString MipName(WriteToString<256>(BuildOutput.GetName(), TEXT(" [MIP "), MipIndex, TEXT("]")));
			NewMip->DerivedData = UE::FDerivedData(MoveTemp(MipName), InBuildCompleteParams.CacheKey, MipId);
			PRAGMA_DISABLE_DEPRECATION_WARNINGS;
			NewMip->bPagedToDerivedData = true;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS;
		}
	}
	return true;
}

static void HandleBuildOutputThenUnpack(FTexturePlatformData& OutPlatformData, UE::DerivedData::FBuildCompleteParams&& InBuildCompleteParams, FBuildResultOptions InBuildResultOptions)
{
	using namespace UE::DerivedData;

	const FBuildOutput& Output = InBuildCompleteParams.Output;
	const FSharedString& Name = Output.GetName();
	const FUtf8SharedString& Function = Output.GetFunction();

	for (const FBuildOutputMessage& Message : Output.GetMessages())
	{
		switch (Message.Level)
		{
		case EBuildOutputMessageLevel::Error:
			UE_LOG(LogTexture, Warning, TEXT("[Error] %s (Build of '%s' by %s.)"),
				*WriteToString<256>(Message.Message), *Name, *WriteToString<32>(Function));
			break;
		case EBuildOutputMessageLevel::Warning:
			UE_LOG(LogTexture, Warning, TEXT("%s (Build of '%s' by %s.)"),
				*WriteToString<256>(Message.Message), *Name, *WriteToString<32>(Function));
			break;
		case EBuildOutputMessageLevel::Display:
			UE_LOG(LogTexture, Display, TEXT("%s (Build of '%s' by %s.)"),
				*WriteToString<256>(Message.Message), *Name, *WriteToString<32>(Function));
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	for (const FBuildOutputLog& Log : Output.GetLogs())
	{
		switch (Log.Level)
		{
		case EBuildOutputLogLevel::Error:
			UE_LOG(LogTexture, Warning, TEXT("[Error] %s: %s (Build of '%s' by %s.)"),
				*WriteToString<64>(Log.Category), *WriteToString<256>(Log.Message),
				*Name, *WriteToString<32>(Function));
			break;
		case EBuildOutputLogLevel::Warning:
			UE_LOG(LogTexture, Warning, TEXT("%s: %s (Build of '%s' by %s.)"),
				*WriteToString<64>(Log.Category), *WriteToString<256>(Log.Message),
				*Name, *WriteToString<32>(Function));
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	if (Output.HasError())
	{
		UE_LOG(LogTexture, Warning, TEXT("Failed to build derived data for build of '%s' by %s."),
			*Name, *WriteToString<32>(Function));
		return;
	}

	UnpackPlatformDataFromBuild(OutPlatformData, MoveTemp(InBuildCompleteParams), InBuildResultOptions);
}

struct FBuildResults
{
	FTexturePlatformData& PlatformData;
	bool bCacheHit = false;
	uint64 BuildOutputSize = 0;

	FBuildResults(FTexturePlatformData& InPlatformData) : PlatformData(InPlatformData) {}
};

static void GetBuildResultsFromCompleteParams(
	FBuildResults& OutBuildResults, 
	FBuildResultOptions InBuildResultOptions, 
	UE::DerivedData::FBuildCompleteParams&& InBuildCompleteParams,
	UE::TextureDerivedData::FTilingTextureBuildInputResolver* InTilingTextureBuildInputResolver // if we are completing a build that had a child build, we need this to get metadata.
)
{
	using namespace UE::DerivedData;
	OutBuildResults.PlatformData.DerivedDataKey.Emplace<FCacheKeyProxy>(InBuildCompleteParams.CacheKey);

	if (InTilingTextureBuildInputResolver)
	{
		if (InTilingTextureBuildInputResolver->bParentBuild_HitCache &&
			EnumHasAnyFlags(InBuildCompleteParams.BuildStatus, EBuildStatus::CacheQueryHit))
		{
			OutBuildResults.bCacheHit = true;
		}
	}
	else
	{
		if (EnumHasAnyFlags(InBuildCompleteParams.BuildStatus, EBuildStatus::CacheQueryHit))
		{
			OutBuildResults.bCacheHit = true;
		}
	}

	OutBuildResults.BuildOutputSize = Algo::TransformAccumulate(InBuildCompleteParams.Output.GetValues(),
		[](const FValue& Value) { return Value.GetData().GetRawSize(); }, uint64(0));
	if (InBuildCompleteParams.Status != EStatus::Canceled)
	{
		HandleBuildOutputThenUnpack(OutBuildResults.PlatformData, MoveTemp(InBuildCompleteParams), InBuildResultOptions);
	}
}


struct FBuildInfo
{
	UE::DerivedData::FBuildSession& BuildSession;
	UE::DerivedData::FBuildDefinition BuildDefinition;
	UE::DerivedData::FBuildPolicy BuildPolicy;
	TOptional<FTexturePlatformData::FTextureEncodeResultMetadata> ResultMetadata;
	UE::TextureDerivedData::FTilingTextureBuildInputResolver* TilingInputResolver = nullptr; // nullptr if no tiling build.

	explicit FBuildInfo(UE::DerivedData::FBuildSession& InBuildSession, UE::DerivedData::FBuildDefinition InBuildDefinition, UE::DerivedData::FBuildPolicy InBuildPolicy, TOptional<FTexturePlatformData::FTextureEncodeResultMetadata> InResultMetadata)
		: BuildSession(InBuildSession)
		, BuildDefinition(InBuildDefinition)
		, BuildPolicy(InBuildPolicy)
		, ResultMetadata(InResultMetadata)
	{}
};

static void LaunchBuildWithFallback(
	FBuildResults& OutBuildResults,
	FBuildResultOptions BuildResultOptions,
	FBuildInfo& InInitialBuild,
	TOptional<FBuildInfo> InFallbackBuild,
	UE::DerivedData::FRequestOwner& InRequestOwner // Owner must be valid for the duration of the build.
)
{
	using namespace UE::DerivedData;

	if (InInitialBuild.ResultMetadata.IsSet())
	{
		OutBuildResults.PlatformData.ResultMetadata = *InInitialBuild.ResultMetadata;
	}

	LaunchTaskInThreadPool(InRequestOwner, FTextureCompilingManager::Get().GetThreadPool(),
		[
			FallbackBuild = MoveTemp(InFallbackBuild),
			RequestOwner = &InRequestOwner,
			OutBuildResults = &OutBuildResults,
			BuildResultOptions,
			BuildDefinition = InInitialBuild.BuildDefinition,
			BuildPolicy = InInitialBuild.BuildPolicy,
			BuildSession = &InInitialBuild.BuildSession,
			InitialBuildTilingInputResolver = InInitialBuild.TilingInputResolver
		]() mutable
	{
		BuildSession->Build(BuildDefinition, {}, BuildPolicy, *RequestOwner,
			[
				FallbackBuild = MoveTemp(FallbackBuild),
				RequestOwner,
				OutBuildResults,
				BuildResultOptions,
				InitialBuildTilingInputResolver
			](FBuildCompleteParams&& Params) mutable
		{
			if (Params.Status == EStatus::Error &&
				FallbackBuild.IsSet())
			{
				if (FallbackBuild->ResultMetadata.IsSet())
				{
					OutBuildResults->PlatformData.ResultMetadata = *FallbackBuild->ResultMetadata;
				}
				FallbackBuild->BuildSession.Build(FallbackBuild->BuildDefinition, {}, FallbackBuild->BuildPolicy, *RequestOwner,
					[
						OutBuildResults = OutBuildResults,
						BuildResultOptions,
						FallbackBuildTilingInputResolver = FallbackBuild->TilingInputResolver
					](FBuildCompleteParams&& Params) mutable
				{
					GetBuildResultsFromCompleteParams(*OutBuildResults, BuildResultOptions, MoveTemp(Params), FallbackBuildTilingInputResolver);
				});
			}
			else
			{
				GetBuildResultsFromCompleteParams(*OutBuildResults, BuildResultOptions, MoveTemp(Params), InitialBuildTilingInputResolver);
			}
		});
	});
};


//
// DDC2 texture fetch/build task.
//
class FTextureBuildTask final : public FTextureAsyncCacheDerivedDataTask
{
public:
	FTextureBuildTask(
		UTexture& Texture,
		FTexturePlatformData& InDerivedData,
		const UE::DerivedData::FUtf8SharedString& FunctionName,
		const UE::DerivedData::FUtf8SharedString& TilingFunctionName,
		const FTextureBuildSettings* InSettingsFetchFirst, // can be nullptr
		const FTextureBuildSettings& InSettingsFetchOrBuild,
		const FTexturePlatformData::FTextureEncodeResultMetadata* InFetchFirstMetadata, // can be nullptr
		const FTexturePlatformData::FTextureEncodeResultMetadata* InFetchOrBuildMetadata, // can be nullptr
		EQueuedWorkPriority Priority,
		ETextureCacheFlags Flags)
		: BuildResults(InDerivedData)
		, InputResolver(Texture)
	{
		static bool bLoadedModules = LoadModules();

		BuildResultOptions.bLoadStreamingMips = EnumHasAnyFlags(Flags, ETextureCacheFlags::InlineMips);
		BuildResultOptions.FirstStreamingMipToLoad = InSettingsFetchOrBuild.LODBiasWithCinematicMips;

		// Can't fetch first if we are rebuilding.
		if (InSettingsFetchFirst &&
			EnumHasAnyFlags(Flags, ETextureCacheFlags::ForceRebuild))
		{
			InSettingsFetchFirst = nullptr;
			InFetchFirstMetadata = nullptr;
		}

		using namespace UE::DerivedData;

		EPriority OwnerPriority = EnumHasAnyFlags(Flags, ETextureCacheFlags::Async) ? ConvertFromQueuedWorkPriority(Priority) : EPriority::Blocking;
		Owner.Emplace(OwnerPriority);

		bool bUseCompositeTexture = false;
		if (!IsTextureValidForBuilding(Texture, Flags, InSettingsFetchOrBuild.bCPUAccessible, bUseCompositeTexture))
		{
			return;
		}
		
		// we don't support VT layers here (no SettingsPerLayer)
		check( Texture.Source.GetNumLayers() == 1 );
		int64 RequiredMemoryEstimate = GetBuildRequiredMemoryEstimate(&Texture,&InSettingsFetchOrBuild);

		// Debug string.
		FSharedString TexturePath;
		{
			TStringBuilder<256> TexturePathBuilder;
			Texture.GetPathName(nullptr, TexturePathBuilder);
			TexturePath = TexturePathBuilder.ToView();
		}

		TOptional<FTextureStatusMessageContext> StatusMessage;
		if (IsInGameThread() && OwnerPriority == EPriority::Blocking)
		{
			// this gets set whether or not we are building the texture, and is a rare edge case for UI feedback.
			// We don't actually know whether we're using fetchfirst or actually building, so if we have two keys,
			// we just assume we're FinalIfAvailable.
			ETextureEncodeSpeed EncodeSpeed = (ETextureEncodeSpeed)InSettingsFetchOrBuild.RepresentsEncodeSpeedNoSend;
			if (InSettingsFetchFirst)
			{
				EncodeSpeed = ETextureEncodeSpeed::FinalIfAvailable;
			}

			StatusMessage.Emplace(ComposeTextureBuildText(Texture, InSettingsFetchOrBuild, EncodeSpeed, RequiredMemoryEstimate, EnumHasAnyFlags(Flags, ETextureCacheFlags::ForVirtualTextureStreamingBuild)));
		}
		

		TOptional<FTexturePlatformData::FTextureEncodeResultMetadata> FetchFirstResultMetadata;
		if (InFetchFirstMetadata)
		{
			FetchFirstResultMetadata = *InFetchFirstMetadata;
		}

		TOptional<FTexturePlatformData::FTextureEncodeResultMetadata> FetchOrBuildResultMetadata;
		if (InFetchOrBuildMetadata)
		{
			FetchOrBuildResultMetadata = *InFetchOrBuildMetadata;
		}

		// Description and MipTail should always cache. Everything else (i.e. Mip# i.e. streaming mips) should skip data
		// when we are not inlining.
		FBuildPolicy FetchFirstBuildPolicy = FetchFirst_CreateBuildPolicy(BuildResultOptions);
		FBuildPolicy FetchOrBuildPolicy = FetchOrBuild_CreateBuildPolicy(Flags, BuildResultOptions);

		//
		// Set up the build
		//
		IBuild& Build = GetBuild();
		{
			IBuildInputResolver* GlobalResolver = GetGlobalBuildInputResolver();
			BuildSession = Build.CreateSession(TexturePath, GlobalResolver ? GlobalResolver : &InputResolver);
		}

		/////////////////////////////////////////////////////////////////////////////////////

		//
		// We have a 2x2 possibility space, and this is the cleanest way to break that down:
		// FetchFirst exists yes/no
		// Tiling build exists yes/no
		//
		if (InSettingsFetchFirst)
		{
			if (TilingFunctionName.Len())
			{
				//
				// FetchFirst_Tiling builds FetchFirst_Parent, if we miss in either place
				// we go to FetchOrBuild_Tiling which builds FetchOrBuild_Parent.
				//
				FBuildDefinition FetchOrBuild_ParentDefinition = CreateDefinition(Build, Texture, TexturePath, FunctionName, InSettingsFetchOrBuild, bUseCompositeTexture, RequiredMemoryEstimate);
				FBuildDefinition FetchOrBuild_TilingDefinition = CreateTilingDefinition(Build, &Texture, InSettingsFetchOrBuild, nullptr, nullptr, FetchOrBuild_ParentDefinition, TexturePath, TilingFunctionName);

				FBuildDefinition FetchFirst_ParentDefinition = CreateDefinition(Build, Texture, TexturePath, FunctionName, *InSettingsFetchFirst, bUseCompositeTexture, RequiredMemoryEstimate);
				FBuildDefinition FetchFirst_TilingDefinition = CreateTilingDefinition(Build, &Texture, *InSettingsFetchFirst, nullptr, nullptr, FetchFirst_ParentDefinition, TexturePath, TilingFunctionName);

				BuildResults.PlatformData.FetchOrBuildDerivedDataKey.Emplace<FTexturePlatformData::FStructuredDerivedDataKey>(GetKey(FetchOrBuild_ParentDefinition, &FetchOrBuild_TilingDefinition, Texture, bUseCompositeTexture));
				BuildResults.PlatformData.FetchFirstDerivedDataKey.Emplace<FTexturePlatformData::FStructuredDerivedDataKey>(GetKey(FetchFirst_ParentDefinition, &FetchFirst_TilingDefinition, Texture, bUseCompositeTexture));

				FetchOrBuild_ChildInputResolver.Emplace(BuildSession.Get(), FetchOrBuild_ParentDefinition, FetchOrBuildPolicy);
				FetchOrBuild_ChildBuildSession = Build.CreateSession(TexturePath, FetchOrBuild_ChildInputResolver.GetPtrOrNull());

				// Must be done after the policy is copied in to the child input resolver
				if (CVarForceRetileTextures.GetValueOnAnyThread())
				{
					FetchOrBuildPolicy = EBuildPolicy::Build;
				}

				FBuildInfo FetchOrBuildInfo(FetchOrBuild_ChildBuildSession.GetValue(), FetchOrBuild_TilingDefinition, FetchOrBuildPolicy, FetchOrBuildResultMetadata);
				FetchOrBuildInfo.TilingInputResolver = FetchOrBuild_ChildInputResolver.GetPtrOrNull();

				if (FetchOrBuild_ParentDefinition.GetKey() == FetchFirst_ParentDefinition.GetKey() &&
					FetchOrBuild_TilingDefinition.GetKey() == FetchFirst_TilingDefinition.GetKey())
				{
					// Same definition, just do FetchOrBuild.
					LaunchBuildWithFallback(BuildResults, BuildResultOptions, FetchOrBuildInfo, {}, *Owner);
				}
				else
				{	
					FetchFirst_ChildInputResolver.Emplace(BuildSession.Get(), FetchFirst_ParentDefinition, FetchFirstBuildPolicy);
					FetchFirst_ChildBuildSession = Build.CreateSession(TexturePath, FetchFirst_ChildInputResolver.GetPtrOrNull());
					// Must be done after the policy is copied in to the child input resolver
					if (CVarForceRetileTextures.GetValueOnAnyThread())
					{
						FetchFirstBuildPolicy = EBuildPolicy::Build;
					}

					FBuildInfo FetchFirstInfo(FetchFirst_ChildBuildSession.GetValue(), FetchFirst_TilingDefinition, FetchFirstBuildPolicy, FetchFirstResultMetadata);
					FetchFirstInfo.TilingInputResolver = FetchFirst_ChildInputResolver.GetPtrOrNull();

					LaunchBuildWithFallback(BuildResults, BuildResultOptions, FetchFirstInfo, FetchOrBuildInfo, *Owner);
				}
			}
			else
			{
				// FetchFirst runs, if we miss we build FetchOrBuild
				FBuildDefinition FetchOrBuild_Definition = CreateDefinition(Build, Texture, TexturePath, FunctionName, InSettingsFetchOrBuild, bUseCompositeTexture, RequiredMemoryEstimate);
				FBuildDefinition FetchFirst_Definition = CreateDefinition(Build, Texture, TexturePath, FunctionName, *InSettingsFetchFirst, bUseCompositeTexture, RequiredMemoryEstimate);

				BuildResults.PlatformData.FetchOrBuildDerivedDataKey.Emplace<FTexturePlatformData::FStructuredDerivedDataKey>(GetKey(FetchOrBuild_Definition, nullptr, Texture, bUseCompositeTexture));
				BuildResults.PlatformData.FetchFirstDerivedDataKey.Emplace<FTexturePlatformData::FStructuredDerivedDataKey>(GetKey(FetchFirst_Definition, nullptr, Texture, bUseCompositeTexture));

				FBuildInfo FetchOrBuildInfo(BuildSession.Get(), FetchOrBuild_Definition, FetchOrBuildPolicy, FetchOrBuildResultMetadata);
				if (FetchOrBuild_Definition.GetKey() == FetchFirst_Definition.GetKey())
				{
					LaunchBuildWithFallback(BuildResults, BuildResultOptions, FetchOrBuildInfo, {}, *Owner);
				}
				else
				{
					FBuildInfo FetchFirstInfo(BuildSession.Get(), FetchFirst_Definition, FetchFirstBuildPolicy, FetchFirstResultMetadata);
					LaunchBuildWithFallback(BuildResults, BuildResultOptions, FetchFirstInfo, FetchOrBuildInfo, *Owner);
				}
			}
		}
		else
		{
			if (TilingFunctionName.Len())
			{
				// Tiling runs which builds Parent.
				FBuildDefinition FetchOrBuild_ParentDefinition = CreateDefinition(Build, Texture, TexturePath, FunctionName, InSettingsFetchOrBuild, bUseCompositeTexture, RequiredMemoryEstimate);
				FBuildDefinition FetchOrBuild_TilingDefinition = CreateTilingDefinition(Build, &Texture, InSettingsFetchOrBuild, nullptr, nullptr, FetchOrBuild_ParentDefinition, TexturePath, TilingFunctionName);

				BuildResults.PlatformData.FetchOrBuildDerivedDataKey.Emplace<FTexturePlatformData::FStructuredDerivedDataKey>(GetKey(FetchOrBuild_ParentDefinition, &FetchOrBuild_TilingDefinition, Texture, bUseCompositeTexture));

				FetchOrBuild_ChildInputResolver.Emplace(BuildSession.Get(), FetchOrBuild_ParentDefinition, FetchOrBuildPolicy);
				// Must be done after the policy is copied in to the child input resolver
				if (CVarForceRetileTextures.GetValueOnAnyThread())
				{
					FetchOrBuildPolicy = EBuildPolicy::Build;
				}

				FetchOrBuild_ChildBuildSession = Build.CreateSession(TexturePath, FetchOrBuild_ChildInputResolver.GetPtrOrNull());

				FBuildInfo FetchOrBuildInfo(FetchOrBuild_ChildBuildSession.GetValue(), FetchOrBuild_TilingDefinition, FetchOrBuildPolicy, FetchOrBuildResultMetadata);
				FetchOrBuildInfo.TilingInputResolver = FetchOrBuild_ChildInputResolver.GetPtrOrNull();

				LaunchBuildWithFallback(BuildResults, BuildResultOptions, FetchOrBuildInfo, {}, *Owner);
			}
			else
			{
				// we just directly build the single definition.
				FBuildDefinition FetchOrBuild_Definition = CreateDefinition(Build, Texture, TexturePath, FunctionName, InSettingsFetchOrBuild, bUseCompositeTexture, RequiredMemoryEstimate);

				BuildResults.PlatformData.FetchOrBuildDerivedDataKey.Emplace<FTexturePlatformData::FStructuredDerivedDataKey>(GetKey(FetchOrBuild_Definition, nullptr, Texture, bUseCompositeTexture));

				FBuildInfo FetchOrBuildInfo(BuildSession.Get(), FetchOrBuild_Definition, FetchOrBuildPolicy, FetchOrBuildResultMetadata);

				LaunchBuildWithFallback(BuildResults, BuildResultOptions, FetchOrBuildInfo, {}, *Owner);
			}
		}

		if (StatusMessage.IsSet())
		{
			Owner->Wait();
		}
	}

	static UE::DerivedData::FBuildDefinition CreateDefinition(
		UE::DerivedData::IBuild& Build,
		UTexture& Texture,
		const UE::DerivedData::FSharedString& TexturePath,
		const UE::DerivedData::FUtf8SharedString& FunctionName,
		const FTextureBuildSettings& Settings,
		const bool bUseCompositeTexture,
		const int64 RequiredMemoryEstimate)
	{
		UE::DerivedData::FBuildDefinitionBuilder DefinitionBuilder = Build.CreateDefinition(TexturePath, FunctionName);
		DefinitionBuilder.AddConstant(UTF8TEXTVIEW("EngineParameters"), UE::TextureBuildUtilities::TextureEngineParameters::ToCompactBinaryWithDefaults(GenerateTextureEngineParameters()));
		DefinitionBuilder.AddConstant(UTF8TEXTVIEW("Settings"), SaveTextureBuildSettings(Texture, Settings, 0, bUseCompositeTexture, RequiredMemoryEstimate));
		DefinitionBuilder.AddInputBulkData(UTF8TEXTVIEW("Source"), Texture.Source.GetPersistentId());
		if (Texture.GetCompositeTexture() && bUseCompositeTexture)
		{
			DefinitionBuilder.AddInputBulkData(UTF8TEXTVIEW("CompositeSource"), Texture.GetCompositeTexture()->Source.GetPersistentId());
		}
		return DefinitionBuilder.Build();
	}

private:

	static constexpr FAnsiStringView NonStreamingMipOutputValueNames[] = 
	{
		ANSITEXTVIEW("EncodedTextureDescription"),
		ANSITEXTVIEW("EncodedTextureExtendedData"),
		ANSITEXTVIEW("MipTail"),
		ANSITEXTVIEW("CPUCopyImageInfo"),
		ANSITEXTVIEW("CPUCopyRawData")
	};


	static UE::DerivedData::FBuildPolicy FetchFirst_CreateBuildPolicy(FBuildResultOptions InBuildResultOptions)
	{
		using namespace UE::DerivedData;

		if (InBuildResultOptions.bLoadStreamingMips)
		{
			// We want all of the output values.
			return EBuildPolicy::Cache;
		}
		else
		{
			// Cache everything except the streaming mips.
			FBuildPolicyBuilder FetchFirstBuildPolicyBuilder(EBuildPolicy::CacheQuery | EBuildPolicy::SkipData);
			for (FAnsiStringView NonStreamingValue : NonStreamingMipOutputValueNames)
			{
				FetchFirstBuildPolicyBuilder.AddValuePolicy(FValueId::FromName(NonStreamingValue), EBuildPolicy::Cache);
			}
			return FetchFirstBuildPolicyBuilder.Build();
		}		
	}

	static UE::DerivedData::FBuildPolicy FetchOrBuild_CreateBuildPolicy(ETextureCacheFlags Flags, FBuildResultOptions InBuildResultOptions)
	{
		using namespace UE::DerivedData;

		if (EnumHasAnyFlags(Flags, ETextureCacheFlags::ForceRebuild))
		{
			return EBuildPolicy::Default & ~EBuildPolicy::CacheQuery;
		}
		else if (InBuildResultOptions.bLoadStreamingMips)
		{
			return EBuildPolicy::Default;
		}
		else
		{
			FBuildPolicyBuilder BuildPolicyBuilder(EBuildPolicy::Build | EBuildPolicy::CacheQuery | EBuildPolicy::CacheStoreOnBuild | EBuildPolicy::SkipData);
			for (FAnsiStringView NonStreamingValue : NonStreamingMipOutputValueNames)
			{
				BuildPolicyBuilder.AddValuePolicy(FValueId::FromName(NonStreamingValue), EBuildPolicy::Cache);
			}
			return BuildPolicyBuilder.Build();
		}
	}

	void Finalize(bool& bOutFoundInCache, uint64& OutProcessedByteCount) final
	{
		bOutFoundInCache = BuildResults.bCacheHit;
		OutProcessedByteCount = BuildResults.BuildOutputSize;
	}
public:

	EQueuedWorkPriority GetPriority() const final
	{
		using namespace UE::DerivedData;
		return ConvertToQueuedWorkPriority(Owner->GetPriority());
	}

	bool SetPriority(EQueuedWorkPriority QueuedWorkPriority) final
	{
		using namespace UE::DerivedData;
		Owner->SetPriority(ConvertFromQueuedWorkPriority(QueuedWorkPriority));
		return true;
	}

	bool Cancel() final
	{
		Owner->Cancel();
		return true;
	}

	void Wait() final
	{
		Owner->Wait();
	}

	bool WaitWithTimeout(float TimeLimitSeconds) final
	{
		const double TimeLimit = FPlatformTime::Seconds() + TimeLimitSeconds;
		if (Poll())
		{
			return true;
		}
		do
		{
			FPlatformProcess::Sleep(0.005);
			if (Poll())
			{
				return true;
			}
		}
		while (FPlatformTime::Seconds() < TimeLimit);
		return false;
	}

	bool Poll() const final
	{
		return Owner->Poll();
	}

	static bool IsTextureValidForBuilding(UTexture& Texture, ETextureCacheFlags Flags, bool bInCPUAccessible, bool& bOutUseCompositeTexture)
	{
		bOutUseCompositeTexture = false;

		const int32 NumBlocks = Texture.Source.GetNumBlocks();
		const int32 NumLayers = Texture.Source.GetNumLayers();
		if (NumBlocks < 1 || NumLayers < 1)
		{
			UE_LOG(LogTexture, Error, TEXT("Texture has no source data: %s"), *Texture.GetPathName());
			return false;
		}

		for (int LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
		{
			ETextureSourceFormat TSF = Texture.Source.GetFormat(LayerIndex);
			ERawImageFormat::Type RawFormat = FImageCoreUtils::ConvertToRawImageFormat(TSF);

			if ( RawFormat == ERawImageFormat::Invalid )
			{
				UE_LOG(LogTexture, Error, TEXT("Texture %s has source art in an invalid format."), *Texture.GetPathName());
				return false;
			}

			// valid TSF should round-trip :
			check( FImageCoreUtils::ConvertToTextureSourceFormat( RawFormat ) == TSF );
		}

		int32 BlockSizeX = 0;
		int32 BlockSizeY = 0;
		TArray<FIntPoint> BlockSizes;
		BlockSizes.Reserve(NumBlocks);
		for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
		{
			FTextureSourceBlock SourceBlock;
			Texture.Source.GetBlock(BlockIndex, SourceBlock);
			if (SourceBlock.NumMips > 0 && SourceBlock.NumSlices > 0)
			{
				BlockSizes.Emplace(SourceBlock.SizeX, SourceBlock.SizeY);
				BlockSizeX = FMath::Max(BlockSizeX, SourceBlock.SizeX);
				BlockSizeY = FMath::Max(BlockSizeY, SourceBlock.SizeY);
			}
		}

		for (int32 BlockIndex = 0; BlockIndex < BlockSizes.Num(); ++BlockIndex)
		{
			const int32 MipBiasX = FMath::CeilLogTwo(BlockSizeX / BlockSizes[BlockIndex].X);
			const int32 MipBiasY = FMath::CeilLogTwo(BlockSizeY / BlockSizes[BlockIndex].Y);
			if (MipBiasX != MipBiasY)
			{
				UE_LOG(LogTexture, Error, TEXT("Texture %s has blocks with mismatched aspect ratios"), *Texture.GetPathName());
				return false;
			}
		}
		
		bool bCompositeTextureViable = Texture.GetCompositeTexture() && Texture.CompositeTextureMode != CTM_Disabled && Texture.GetCompositeTexture()->Source.IsValid();
		if (bInCPUAccessible)
		{
			bCompositeTextureViable = false;
		}
		bool bMatchingBlocks = bCompositeTextureViable && (Texture.GetCompositeTexture()->Source.GetNumBlocks() == Texture.Source.GetNumBlocks());
		
		if (bCompositeTextureViable)
		{
			if (!bMatchingBlocks)
			{
				UE_LOG(LogTexture, Warning, TEXT("Issue while building %s : Composite texture UDIM block counts do not match. Composite texture will be ignored"), *Texture.GetPathName());
			}
		}

		bOutUseCompositeTexture = bMatchingBlocks;

		// TODO: Add validation equivalent to that found in FTextureCacheDerivedDataWorker::BuildTexture for virtual textures
		//		 if virtual texture support is added for this code path.
		if (!EnumHasAnyFlags(Flags, ETextureCacheFlags::ForVirtualTextureStreamingBuild))
		{
			// Only support single Block/Layer here (Blocks and Layers are intended for VT support)
			if (NumBlocks > 1)
			{
				// This can happen if user attempts to import a UDIM without VT enabled
				UE_LOG(LogTexture, Log, TEXT("Texture %s was imported as UDIM with %d blocks but VirtualTexturing is not enabled, only the first block will be available"),
					*Texture.GetPathName(), NumBlocks);
			}
			if (NumLayers > 1)
			{
				// This can happen if user attempts to use lightmaps or other layered VT without VT enabled
				UE_LOG(LogTexture, Log, TEXT("Texture %s has %d layers but VirtualTexturing is not enabled, only the first layer will be available"),
					*Texture.GetPathName(), NumLayers);
			}
		}

		return true;
	}

	static FTexturePlatformData::FStructuredDerivedDataKey GetKey(const UE::DerivedData::FBuildDefinition& BuildDefinition, const UE::DerivedData::FBuildDefinition* TilingBuildDefinitionKey, const UTexture& Texture, bool bUseCompositeTexture)
	{
		// DDC2 Key SerializeForKey is here!
		FTexturePlatformData::FStructuredDerivedDataKey Key;
		if (TilingBuildDefinitionKey != nullptr)
		{
			Key.TilingBuildDefinitionKey = TilingBuildDefinitionKey->GetKey().Hash;
		}
		Key.BuildDefinitionKey = BuildDefinition.GetKey().Hash;
		Key.SourceGuid = Texture.Source.GetId();
		if (bUseCompositeTexture && Texture.GetCompositeTexture())
		{
			Key.CompositeSourceGuid = Texture.GetCompositeTexture()->Source.GetId();
		}
		return Key;
	}

	static UE::DerivedData::FBuildDefinition CreateTilingDefinition(
		UE::DerivedData::IBuild& InBuild,
		UTexture* InTexture,
		const FTextureBuildSettings& InBuildSettings,
		FEncodedTextureDescription* InTextureDescription, // only valid if our textures can generate this pre build
		FEncodedTextureExtendedData* InTextureExtendedData, // only valid if our textures can generate this pre build
		const UE::DerivedData::FBuildDefinition& InParentBuildDefinition,
		const UE::DerivedData::FSharedString& InDefinitionDebugName,
		const UE::DerivedData::FUtf8SharedString& InBuildFunctionName
	)
	{			
		const FTextureEngineParameters EngineParameters = GenerateTextureEngineParameters();

		//
		// We always consume an unpacked texture (i.e. extended data == nullptr)
		//
		const FTextureSource& Source = InTexture->Source;
		int32 InputTextureMip0SizeX, InputTextureMip0SizeY, InputTextureMip0NumSlices;
		int32 InputTextureNumMips = TextureCompressorModule->GetMipCountForBuildSettings(Source.GetSizeX(), Source.GetSizeY(), Source.GetNumSlices(), Source.GetNumMips(), InBuildSettings, InputTextureMip0SizeX, InputTextureMip0SizeY, InputTextureMip0NumSlices);
		int32 InputTextureNumStreamingMips = GetNumStreamingMipsDirect(InputTextureNumMips, InBuildSettings.bCubemap, InBuildSettings.bVolume, InBuildSettings.bTextureArray, nullptr, EngineParameters);

		//
		// A child definition consumes a parent definition and swizzles it. However it
		// needs to know ahead of time the total mip count and the streaming mip count
		//
		const UE::DerivedData::FBuildKey InParentBuildKey = InParentBuildDefinition.GetKey();

		UE::DerivedData::FBuildDefinitionBuilder DefinitionBuilder = InBuild.CreateDefinition(InDefinitionDebugName, InBuildFunctionName);

		// The tiler needs the description, which either comes from us (new style) or the linear build (old style)
		if (InTextureDescription == nullptr)
		{
			// old style
			DefinitionBuilder.AddInputBuild(UTF8TEXTVIEW("EncodedTextureDescriptionInput"), { InParentBuildKey, UE::DerivedData::FValueId::FromName(UTF8TEXTVIEW("EncodedTextureDescription")) });			

			// The tiling build can generate the extended data - however it needs the LODBias to do so.
			FCbWriter Writer;
			Writer.BeginObject();
			Writer.AddInteger("LODBias", InBuildSettings.LODBias);
			Writer.EndObject();

			DefinitionBuilder.AddConstant(UTF8TEXTVIEW("LODBias"), Writer.Save().AsObject());
		}
		else
		{
			// new style - we want to provide everything so that the only outputs are bulk data that we can
			// just hold references to.
			DefinitionBuilder.AddConstant(UTF8TEXTVIEW("EncodedTextureDescriptionConstant"), UE::TextureBuildUtilities::EncodedTextureDescription::ToCompactBinary(*InTextureDescription));
			DefinitionBuilder.AddConstant(UTF8TEXTVIEW("EncodedTextureExtendedDataConstant"), UE::TextureBuildUtilities::EncodedTextureExtendedData::ToCompactBinary(*InTextureExtendedData));
		}
		
		DefinitionBuilder.AddInputBuild(UTF8TEXTVIEW("TextureBuildMetadata"), { InParentBuildKey, UE::DerivedData::FValueId::FromName(UTF8TEXTVIEW("TextureBuildMetadata")) });

		if (InputTextureNumMips > InputTextureNumStreamingMips)
		{
			DefinitionBuilder.AddInputBuild(UTF8TEXTVIEW("MipTail"), { InParentBuildKey, UE::DerivedData::FValueId::FromName(UTF8TEXTVIEW("MipTail")) });
		}

		for (int32 MipIndex = 0; MipIndex < InputTextureNumStreamingMips; MipIndex++)
		{
			TUtf8StringBuilder<10> MipName;
			MipName << "Mip" << MipIndex;
			DefinitionBuilder.AddInputBuild(MipName, { InParentBuildKey, UE::DerivedData::FValueId::FromName(MipName) });
		}

		return DefinitionBuilder.Build();
	}

	static ITextureCompressorModule* TextureCompressorModule;
	static bool LoadModules()
	{
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TextureCompressorModule = &FModuleManager::LoadModuleChecked<ITextureCompressorModule>(TEXTURE_COMPRESSOR_MODULENAME);
		return true;
	}
	
	// Stuff that we get as a result of the build.
	FBuildResults BuildResults;

	// Controls for what optional build outputs we want.
	FBuildResultOptions BuildResultOptions;

	// Build bureaucracy
	TOptional<UE::DerivedData::FRequestOwner> Owner;

	UE::DerivedData::FOptionalBuildSession BuildSession;
	UE::TextureDerivedData::FTextureBuildInputResolver InputResolver;
	
	TOptional<UE::TextureDerivedData::FBuildSession> FetchOrBuild_ChildBuildSession;
	TOptional<UE::TextureDerivedData::FBuildSession> FetchFirst_ChildBuildSession;
	TOptional<UE::TextureDerivedData::FTilingTextureBuildInputResolver> FetchOrBuild_ChildInputResolver;
	TOptional<UE::TextureDerivedData::FTilingTextureBuildInputResolver> FetchFirst_ChildInputResolver;

	FRWLock Lock;
}; // end DDC2 fetch/build task (FTextureBuildTask)

/* static */ ITextureCompressorModule* FTextureBuildTask::TextureCompressorModule = 0;

FTextureAsyncCacheDerivedDataTask* CreateTextureBuildTask(
	UTexture& Texture,
	FTexturePlatformData& DerivedData,
	const FTextureBuildSettings* SettingsFetch,
	const FTextureBuildSettings& SettingsFetchOrBuild,
	const FTexturePlatformData::FTextureEncodeResultMetadata* FetchMetadata,
	const FTexturePlatformData::FTextureEncodeResultMetadata* FetchOrBuildMetadata,
	EQueuedWorkPriority Priority,
	ETextureCacheFlags Flags)
{
	using namespace UE::DerivedData;

	// If we are tiling, we need to alter the build settings to act as though it's 
	// for the linear base format for the build function - the tiling itself will be
	// a separate build function that will consume the output of that.
	// We have to do this here because if we do it where build settings are created, then
	// the DDC key that is externally visible won't know anything about the tiling and
	// the de-dupe code in BeginCacheForCookedPlatformData will delete the tiling build.
	TOptional<FTextureBuildSettings> BaseSettingsFetch;
	TOptional<FTextureBuildSettings> BaseSettingsFetchOrBuild;
	FUtf8SharedString TilingFunctionName;
	const FTextureBuildSettings* UseSettingsFetch = SettingsFetch;
	const FTextureBuildSettings* UseSettingsFetchOrBuild = &SettingsFetchOrBuild;
	if (SettingsFetchOrBuild.Tiler)
	{
		TilingFunctionName = SettingsFetchOrBuild.Tiler->GetBuildFunctionName();

		if (SettingsFetch)
		{
			BaseSettingsFetch = *SettingsFetch;
			BaseSettingsFetch->TextureFormatName = BaseSettingsFetch->BaseTextureFormatName;
			BaseSettingsFetch->Tiler = nullptr;
			UseSettingsFetch = BaseSettingsFetch.GetPtrOrNull();
		}
		BaseSettingsFetchOrBuild = SettingsFetchOrBuild;
		BaseSettingsFetchOrBuild->TextureFormatName = BaseSettingsFetchOrBuild->BaseTextureFormatName;
		UseSettingsFetchOrBuild = BaseSettingsFetchOrBuild.GetPtrOrNull();
	}
	
	if (FUtf8SharedString FunctionName = FindTextureBuildFunction(UseSettingsFetchOrBuild->TextureFormatName); !FunctionName.IsEmpty())
	{
		return new FTextureBuildTask(Texture, DerivedData, FunctionName, TilingFunctionName, UseSettingsFetch, *UseSettingsFetchOrBuild, FetchMetadata, FetchOrBuildMetadata, Priority, Flags);

	}
	return nullptr;
}

FTexturePlatformData::FStructuredDerivedDataKey CreateTextureDerivedDataKey(
	UTexture& Texture,
	ETextureCacheFlags CacheFlags,
	const FTextureBuildSettings& Settings)
{
	using namespace UE::DerivedData;

	FUtf8SharedString TilingFunctionName;
	if (Settings.Tiler)
	{
		TilingFunctionName = Settings.Tiler->GetBuildFunctionName();
	}
	if (FUtf8SharedString FunctionName = FindTextureBuildFunction(Settings.BaseTextureFormatName); !FunctionName.IsEmpty())
	{
		IBuild& Build = GetBuild();

		TStringBuilder<256> TexturePath;
		Texture.GetPathName(nullptr, TexturePath);

		bool bUseCompositeTexture = false;
		if (FTextureBuildTask::IsTextureValidForBuilding(Texture, CacheFlags, Settings.bCPUAccessible, bUseCompositeTexture))
		{
			// this is just to make DDC Key so I don't need RequiredMemoryEstimate
			// but it goes in the the DDC Key, so I have to compute it
			// how do I pass something to TBF without it going in the DDC Key ? -> currently you can't
			// @todo Oodle : RequiredMemoryEstimate goes in the key for DDC2, not ideal

			check( Texture.Source.GetNumLayers() == 1 ); // no SettingsPerLayer here
			int64 RequiredMemoryEstimate = GetBuildRequiredMemoryEstimate(&Texture,&Settings);

			FBuildDefinition Definition = FTextureBuildTask::CreateDefinition(Build, Texture, TexturePath.ToView(), FunctionName, Settings, bUseCompositeTexture, RequiredMemoryEstimate);
			TOptional<FBuildDefinition> TilingDefinition;
			if (TilingFunctionName.IsEmpty() == false)
			{
				TilingDefinition.Emplace(FTextureBuildTask::CreateTilingDefinition(Build, &Texture, Settings, nullptr, nullptr, Definition, TexturePath.ToView(), TilingFunctionName));
			}

			return FTextureBuildTask::GetKey(Definition, TilingDefinition.GetPtrOrNull(), Texture, bUseCompositeTexture);
		}
	}
	return FTexturePlatformData::FStructuredDerivedDataKey();
}

#endif // WITH_EDITOR

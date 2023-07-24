// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/Texture.h"

#include "EngineLogs.h"
#include "Modules/ModuleManager.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Math/ColorList.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "UObject/UObjectIterator.h"
#include "UObject/ObjectSaveContext.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureMipDataProviderFactory.h"
#include "ContentStreaming.h"
#include "EngineUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Engine/Engine.h"
#include "Interfaces/ITargetPlatform.h"
#include "Engine/TextureLODSettings.h"
#include "RenderUtils.h"
#include "Rendering/StreamableTextureResource.h"
#include "RenderingThread.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "Compression/OodleDataCompression.h"
#include "Engine/TextureCube.h"
#include "Engine/RendererSettings.h"
#include "ColorSpace.h"
#include "ImageCoreUtils.h"
#include "ImageUtils.h"
#include "Algo/Unique.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"

#if WITH_EDITOR
#include "DerivedDataBuildVersion.h"
#include "GuardedInt.h"
#include "TextureCompiler.h"
#include "Misc/ScopeRWLock.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(Texture)

#if WITH_EDITORONLY_DATA
	#include "EditorFramework/AssetImportData.h"
#endif

static TAutoConsoleVariable<int32> CVarVirtualTextures(
	TEXT("r.VirtualTextures"),
	0,
	TEXT("Is virtual texture streaming enabled?"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarMobileVirtualTextures(
	TEXT("r.Mobile.VirtualTextures"),
	0,
	TEXT("Whether virtual texture streaming is enabled on mobile platforms. Requires r.VirtualTextures enabled as well. \n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarVirtualTexturesAutoImport(
	TEXT("r.VT.EnableAutoImport"),
	1,
	TEXT("Enable virtual texture on texture import"),
	ECVF_Default);

// GSkipInvalidDXTDimensions prevents crash with non-4x4 aligned DXT
// if the Texture code is working correctly, this should not be necessary
// turn this bool off when possible ; FORT-515901
int32 GSkipInvalidDXTDimensions = 1;
static FAutoConsoleVariableRef CVarSkipInvalidDXTDimensions(
	TEXT("r.SkipInvalidDXTDimensions"),
	GSkipInvalidDXTDimensions,
	TEXT("If set will skip over creating DXT textures that are smaller than 4x4 or other invalid dimensions.")
);

DEFINE_LOG_CATEGORY(LogTexture);
DEFINE_LOG_CATEGORY(LogTextureUpload);

#if STATS
DECLARE_STATS_GROUP(TEXT("Texture Group"), STATGROUP_TextureGroup, STATCAT_Advanced);

// Declare the stats for each Texture Group.
#define DECLARETEXTUREGROUPSTAT(Group) DECLARE_MEMORY_STAT(TEXT(#Group),STAT_##Group,STATGROUP_TextureGroup);
FOREACH_ENUM_TEXTUREGROUP(DECLARETEXTUREGROUPSTAT)
#undef DECLARETEXTUREGROUPSTAT


// Initialize TextureGroupStatFNames array with the FNames for each stats.
FName FTextureResource::TextureGroupStatFNames[TEXTUREGROUP_MAX] =
	{
		#define ASSIGNTEXTUREGROUPSTATNAME(Group) GET_STATFNAME(STAT_##Group),
		FOREACH_ENUM_TEXTUREGROUP(ASSIGNTEXTUREGROUPSTATNAME)
		#undef ASSIGNTEXTUREGROUPSTATNAME
	};
#endif

// This is used to prevent the PostEditChange to automatically update the material dependencies & material context, in some case we want to manually control this
// to be more efficient.
ENGINE_API bool GDisableAutomaticTextureMaterialUpdateDependencies = false;

UTexture::FOnTextureSaved UTexture::PreSaveEvent;

static FName CachedGetLatestOodleSdkVersion();

UTexture::UTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PrivateResource(nullptr)
	, PrivateResourceRenderThread(nullptr)
#if WITH_TEXTURE_RESOURCE_DEPRECATIONS
	, Resource(
		[this]()-> FTextureResource* { return GetResource(); },
		[this](FTextureResource* InTextureResource) { SetResource(InTextureResource); })
#endif
	, TextureReference(*new FTextureReference())
{
	SRGB = true;
	Filter = TF_Default;
	MipLoadOptions = ETextureMipLoadOptions::Default;
#if WITH_EDITORONLY_DATA
	LoadedMainStreamObjectVersion = FUE5MainStreamObjectVersion::LatestVersion;
	SourceColorSettings = FTextureSourceColorSettings();
	AdjustBrightness = 1.0f;
	AdjustBrightnessCurve = 1.0f;
	AdjustVibrance = 0.0f;
	AdjustSaturation = 1.0f;
	AdjustRGBCurve = 1.0f;
	AdjustHue = 0.0f;
	AdjustMinAlpha = 0.0f;
	AdjustMaxAlpha = 1.0f;
	CompressionNoAlpha = 0;
	CompressionForceAlpha = 0;
	CompressionNone = 0;
	CompressFinal = 0;
	DeferCompression = 0;
	CompressionCacheId = FGuid(0, 0, 0, 0);
	LossyCompressionAmount = TLCA_Default;
	MaxTextureSize = 0; // means no limitation
	MipGenSettings = TMGS_FromTextureGroup;
	CompositeTextureMode = CTM_NormalRoughnessToAlpha;
	// this should have defaulted to CTM_Disabled
	//	but it's hard to change now because of UPROPERTY writing deltas to default
	CompositePower = 1.0f;
	bUseLegacyGamma = false;
	bNormalizeNormals = false;
	bIsImporting = false;
	bCustomPropertiesImported = false;
	bDoScaleMipsForAlphaCoverage = false;
	AlphaCoverageThresholds = FVector4(0, 0, 0, 0);
	bUseNewMipFilter = false;
	PaddingColor = FColor::Black;
	ChromaKeyColor = FColorList::Magenta;
	ChromaKeyThreshold = 1.0f / 255.0f;
	VirtualTextureStreaming = 0;
	CompressionYCoCg = 0;
	Downscale = 0.f;
	DownscaleOptions = ETextureDownscaleOptions::Default;
	CookPlatformTilingSettings = TextureCookPlatformTilingSettings::TCPTS_FromTextureGroup;
	Source.SetOwner(this);
#endif // #if WITH_EDITORONLY_DATA

	if (FApp::CanEverRender() && !IsTemplate())
	{
		TextureReference.BeginInit_GameThread();
	}
}

const FTextureResource* UTexture::GetResource() const
{
	if (IsInParallelGameThread() || IsInGameThread() || IsInSlateThread() || IsInAsyncLoadingThread())
	{
		return PrivateResource;
	}
	else if (IsInParallelRenderingThread() || IsInRHIThread())
	{
		return PrivateResourceRenderThread;
	}

	ensureMsgf(false, TEXT("Attempted to access a texture resource from an unkown thread."));
	return nullptr;
}

FTextureResource* UTexture::GetResource()
{
	if (IsInParallelGameThread() || IsInGameThread() || IsInSlateThread() || IsInAsyncLoadingThread())
	{
		return PrivateResource;
	}
	else if (IsInParallelRenderingThread() || IsInRHIThread())
	{
		return PrivateResourceRenderThread;
	}

	ensureMsgf(false, TEXT("Attempted to access a texture resource from an unkown thread."));
	return nullptr;
}

UTexture::UTexture(FVTableHelper& Helper)
	: TextureReference(*new FTextureReference())
{
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
UTexture::~UTexture()
{
	delete& TextureReference;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UTexture::SetResource(FTextureResource* InResource)
{
	check (!IsInActualRenderingThread() && !IsInRHIThread());

	// Each PrivateResource value must be updated in it's own thread because any
	// rendering code trying to access the Resource from this UTexture will
	// crash if it suddenly sees nullptr or a new resource that has not had it's InitRHI called.

	PrivateResource = InResource;
	ENQUEUE_RENDER_COMMAND(SetResourceRenderThread)([this, InResource](FRHICommandListImmediate& RHICmdList)
	{
		PrivateResourceRenderThread = InResource;
	});
}

void UTexture::ReleaseResource()
{
	if (PrivateResource)
	{
		UnlinkStreaming();

		// When using PlatformData, the resource shouldn't be released before it is initialized to prevent threading issues
		// where the platform data could be updated at the same time InitRHI is reading it on the renderthread.
		if (GetRunningPlatformData() && *GetRunningPlatformData())
		{
			WaitForPendingInitOrStreaming();
		}

		CachedSRRState.Clear();

		FTextureResource* ToDelete = PrivateResource;
		// Free the resource.
		SetResource(nullptr);
		ENQUEUE_RENDER_COMMAND(DeleteResource)([ToDelete](FRHICommandListImmediate& RHICmdList)
		{
			ToDelete->ReleaseResource();
			delete ToDelete;
		});
	}
}

void UTexture::UpdateResource()
{
	// Release the existing texture resource.
	ReleaseResource();

	//Dedicated servers have no texture internals
	if( FApp::CanEverRender() && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Create a new texture resource.
		FTextureResource* NewResource = CreateResource();

		if (GSkipInvalidDXTDimensions != 0)
		{
			if (FStreamableTextureResource* StreamableResource = NewResource ? NewResource->GetStreamableTextureResource() : nullptr)
			{
				uint32 SizeX = StreamableResource->GetSizeX();
				uint32 SizeY = StreamableResource->GetSizeY();
				uint32 SizeZ = StreamableResource->GetSizeZ();

				bool bIsBCN = false;
				switch (StreamableResource->GetPixelFormat())
				{
				case PF_DXT1:
				case PF_DXT3:
				case PF_DXT5:
				case PF_BC4:
				case PF_BC5:
				case PF_BC6H:
				case PF_BC7:
				{
					bIsBCN = true;
					break;
				}
				default:
					break;
				}

				if (bIsBCN && (SizeX < 4 || (SizeX % 4) != 0 || SizeY < 4 || (SizeY % 4) != 0))
				{
					FTexturePlatformData** PtrPlatformData = GetRunningPlatformData();
					const FTexturePlatformData* PlatformData = PtrPlatformData ? *PtrPlatformData : nullptr;
			
					int32 MipsNum=0;
					int32 NumNonStreamingMips=0;
					int32 NumNonOptionalMips=0;
					int32 PDSizeX=0;
					int32 PDSizeY=0;
					if ( PlatformData )
					{
						MipsNum = PlatformData->Mips.Num();
						bool bIsStreamingPossible = IsPossibleToStream();
						NumNonStreamingMips = PlatformData->GetNumNonStreamingMips(bIsStreamingPossible);
						NumNonOptionalMips = PlatformData->GetNumNonOptionalMips();
						PDSizeX = PlatformData->SizeX;
						PDSizeY = PlatformData->SizeY;
					}

					ensureMsgf( (SizeX%4)==0 && (SizeY%4) == 0, TEXT("Skipping init of %s texture %s with non 4x4-aligned size. Resource Size=%ix%ix%i. "
						"Texture PD Size=%dx%d, mips=%d, nonstreaming=%d, nonopt=%d, LODBias=%d,cinematic=%d."), 
						GPixelFormats[StreamableResource->GetPixelFormat()].Name, *GetName(), SizeX, SizeY, SizeZ,
						PDSizeX,PDSizeY,MipsNum,NumNonStreamingMips,NumNonOptionalMips,
						LODBias,NumCinematicMipLevels);

					delete NewResource;
					return;
				}
			}
		}

		SetResource(NewResource);
		if (NewResource)
		{
			LLM_SCOPE(ELLMTag::Textures);
			if (FStreamableTextureResource* StreamableResource = NewResource->GetStreamableTextureResource())
			{
				// State the gamethread coherent resource state.
				CachedSRRState = StreamableResource->GetPostInitState();
				if (CachedSRRState.IsValid())
				{
					// Cache the pending InitRHI flag.
					CachedSRRState.bHasPendingInitHint = true;
				}
			}

			// Init the texture reference, which needs to be set from a render command, since TextureReference.TextureReferenceRHI is gamethread coherent.
			ENQUEUE_RENDER_COMMAND(SetTextureReference)([this, NewResource](FRHICommandListImmediate& RHICmdList)
			{
				NewResource->SetTextureReference(TextureReference.TextureReferenceRHI);
			});

			NewResource->SetOwnerName(FName(GetPathName()));
			BeginInitResource(NewResource);
			// Now that the resource is ready for streaming, bind it to the streamer.
			LinkStreaming();
		}
	}
}

void UTexture::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	// Texture source data export : first, make sure it is ready for export :
	FinishCachePlatformData();

	Source.ExportCustomProperties(Out, Indent);

	Out.Logf(TEXT("\r\n"));
#endif // WITH_EDITOR
}

void UTexture::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
#if WITH_EDITOR
	Source.ImportCustomProperties(SourceText, Warn);

	BeginCachePlatformData();

	bCustomPropertiesImported = true;
#endif // WITH_EDITOR
}

void UTexture::PostEditImport()
{
#if WITH_EDITOR
	bIsImporting = true;

	if (bCustomPropertiesImported)
	{
		FinishCachePlatformData();
	}
#endif // WITH_EDITOR
}

bool UTexture::IsPostLoadThreadSafe() const
{
	return false;
}

#if WITH_EDITOR

bool UTexture::IsDefaultTexture() const
{
	return false;
}

bool UTexture::Modify(bool bAlwaysMarkDirty)
{
	// Before applying any modification to the texture
	// make sure no compilation is still ongoing.
	if (!IsAsyncCacheComplete())
	{
		FinishCachePlatformData();
	}	
	
	if (IsDefaultTexture())
	{
		FTextureCompilingManager::Get().FinishCompilation({this});
	}

	return Super::Modify(bAlwaysMarkDirty);
}

bool UTexture::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FName PropertyName = InProperty->GetFName();
				
		// Only enable chromatic adapation method when the white points differ.
		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FTextureSourceColorSettings, ChromaticAdaptationMethod))
		{
			if (SourceColorSettings.ColorSpace == ETextureColorSpace::TCS_None)
			{
				return false;
			}

			const URendererSettings* Settings = GetDefault<URendererSettings>();
			return !Settings->WhiteChromaticityCoordinate.Equals(SourceColorSettings.WhiteChromaticityCoordinate);
		}

		// Virtual Texturing is only supported for Texture2D 
		static const FName VirtualTextureStreamingName = GET_MEMBER_NAME_CHECKED(UTexture, VirtualTextureStreaming);
		if (PropertyName == VirtualTextureStreamingName)
		{
			return this->IsA<UTexture2D>();
		}
	}

	return true;
}

void UTexture::UpdateOodleTextureSdkVersionToLatest(void)
{
	// OodleTextureSdkVersion = get latest sdk version
	//	this needs to get the actual version number so it will be IO'd frozen (not just "latest")
	OodleTextureSdkVersion = CachedGetLatestOodleSdkVersion();
}


// we're in WITH_EDITOR but not sure that's right, maybe move out?
// Beware: while ValidateSettingsAfterImportOrEdit should have been called on all Textures,
//	 it is not called on load at runtime
//   and it is not always called from dynamically generated textures
//	so you must not rely on the rules it sets up being true!
void UTexture::ValidateSettingsAfterImportOrEdit(bool * pRequiresNotifyMaterials)
{
	bool bRequiresNotifyMaterialsDummy = false;
	bool & bRequiresNotifyMaterials = pRequiresNotifyMaterials ? *pRequiresNotifyMaterials : bRequiresNotifyMaterialsDummy;

	// calling ValidateSettingsAfterImportOrEdit if all settings are already valid should be a nop
	// if you call ValidateSettingsAfterImportOrEdit twice, the second should do nothing

	// this will be called by PostEditChange() with no arg
		
#if WITH_EDITORONLY_DATA

	if (Source.IsValid()) // we can have an empty source if the last source in a texture2d array is removed via the editor.
	{
		if ( MipGenSettings == TMGS_LeaveExistingMips && PowerOfTwoMode != ETexturePowerOfTwoSetting::None )
		{
			// power of 2 pads not allowed with LeaveExistingMips
			UE_LOG(LogTexture, Display, TEXT("Power of 2 padding cannot be used with LeaveExistingMips, disabled. (%s)"), *GetName());

			PowerOfTwoMode = ETexturePowerOfTwoSetting::None;
		}

		// IsPowerOfTwo only checks XY :
		bool bIsPowerOfTwo = Source.IsPowerOfTwo();
		if ( ! FMath::IsPowerOfTwo(Source.GetVolumeSizeZ()) )
		{
			bIsPowerOfTwo = false;
		}
		if ( PowerOfTwoMode != ETexturePowerOfTwoSetting::None )
		{
			bIsPowerOfTwo = true;
		}

		// Downscale can violate IsPow2, but it only acts when NoMipMaps, so it's moot

		if ( ! bIsPowerOfTwo )
		{
			// streaming only supports power of 2 mips
			// due to failure to compensate for the GPU row pitch
			// it only works for mips that naturally have the required 256 pitch
			// so mip levels >= 256 and power of 2 only
			// (this used to be in Texture2D.cpp)
			// see WarnRequiresTightPackedMip
			// there are other issues with streaming non-pow2
			//  all streamable levels must be valid textures, so block-of-4 alignment for BCN
			//  is easiest to guarantee if the source is pow2
			NeverStream = true;	
		}
	
		int32 MaxDimension = FMath::Max( Source.GetSizeX() , Source.GetSizeY() );
		bool bLargeTextureMustBeVT = MaxDimension > GetMaximumDimensionOfNonVT();

		if ( bLargeTextureMustBeVT && ! VirtualTextureStreaming && MaxTextureSize == 0 )
		{
			static const auto CVarVirtualTexturesEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextures"));
			check( CVarVirtualTexturesEnabled != nullptr );

			if ( CVarVirtualTexturesEnabled->GetValueOnAnyThread() )
			{
				if ( GetTextureClass() == ETextureClass::TwoD )
				{
				UE_LOG(LogTexture, Display, TEXT("Large Texture %s Dimension=%d changed to VT; to disable VT set MaxTextureSize first"), *GetName(),MaxDimension);
					VirtualTextureStreaming = true;
					bRequiresNotifyMaterials = true;
				}
				else
				{
					UE_LOG(LogTexture, Warning, TEXT("Large Texture %s Dimension=%d needs to be VT but is not 2d, changing MaxTextureSize"), *GetName(),MaxDimension);
				
					// GetMaximumDimension is the max size for this texture type on the current RHI
					MaxTextureSize = GetMaximumDimension();
				}
			}
			else
			{
				UE_LOG(LogTexture, Warning, TEXT("Large Texture %s Dimension=%d must be VT but VirtualTextures are disabled, changing MaxTextureSize"), *GetName(),MaxDimension);

				// GetMaximumDimension is the max size for this texture type on the current RHI
				MaxTextureSize = GetMaximumDimension();
			}
		}
	
		if (VirtualTextureStreaming)
		{
			if (!bIsPowerOfTwo)
			{
				if ( bLargeTextureMustBeVT )
				{
					UE_LOG(LogTexture, Warning, TEXT("Large VT \"%s\", must be padded to power-of-2 for VT support (%dx%d)"), *GetName(), Source.GetSizeX(),Source.GetSizeY());
					// VT nonpow2 will fail to build
					// force it into a state that will succeed? or just let it fail?
					// you can either pad to pow2 or set MaxTextureSize and turn off VT
					PowerOfTwoMode = ETexturePowerOfTwoSetting::PadToPowerOfTwo;
				}
				else
				{
					UE_LOG(LogTexture, Warning, TEXT("VirtualTextureStreaming not supported for \"%s\", texture size is not a power-of-2"), *GetName());
					VirtualTextureStreaming = false;
					bRequiresNotifyMaterials = true;
				}
			}
		}

		// Make sure settings are correct for LUT textures.
		if(LODGroup == TEXTUREGROUP_ColorLookupTable)
		{
			if ( MipGenSettings != TMGS_NoMipmaps || SRGB != false )
			{
				MipGenSettings = TMGS_NoMipmaps;
				SRGB = false;
				bRequiresNotifyMaterials = true;
			}
		}
	} // end if valid source
#endif // #if WITH_EDITORONLY_DATA

	// check TC_ CompressionSettings that should have SRGB off
	const bool bPreventSRGB = CompressionSettings == TC_Alpha || CompressionSettings == TC_Normalmap || CompressionSettings == TC_Masks || 
		UE::TextureDefines::IsHDR(CompressionSettings);
	if (bPreventSRGB && SRGB == true)
	{
		SRGB = false;
		bRequiresNotifyMaterials = true;
	}

#if WITH_EDITORONLY_DATA
	if ( ! SRGB )
	{
		// bUseLegacyGamma will be ignored if SRGB is off
		// go ahead and switch it off
		bUseLegacyGamma = false;
	}

	if (MaxTextureSize <= 0)
	{
		MaxTextureSize = 0;
	}
	else
	{
		// note : GetMaximumDimension is the max dim for this texture type in the current RHI
		MaxTextureSize = FMath::Min<int32>(FMath::RoundUpToPowerOfTwo(MaxTextureSize), GetMaximumDimension());
	}
#endif
	
	NumCinematicMipLevels = FMath::Max<int32>( NumCinematicMipLevels, 0 );
}

void UTexture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTexture_PostEditChangeProperty);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	ON_SCOPE_EXIT
	{
		// PostEditChange is the last step in the import sequence (PreEditChange/PostEditImport/PostEditChange, called twice : see further details) so reset the import-related flags here:
		bIsImporting = false;
		bCustomPropertiesImported = false;
	};

	// When PostEditChange is called as part of the import process (PostEditImport has just been called), it may be called twice : once for the (sub-)object declaration, and once for the definition, the latter being 
	//  when ImportCustomProperties is called. Because texture bulk data is only being copied to in ImportCustomProperties, it's invalid to do anything the first time so we postpone it to the second call :
	if (bIsImporting && !bCustomPropertiesImported)
	{
		return;
	}

	// assume there was a change that needs a new lighting guid :
	SetLightingGuid();

	// Determine whether any property that requires recompression of the texture, or notification to Materials has changed.
	bool RequiresNotifyMaterials = false;
	bool DeferCompressionWasEnabled = false;
	bool bInvalidatesMaterialShaders = true;	// too conservative, but as to not change the current behavior

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	
	ValidateSettingsAfterImportOrEdit(&RequiresNotifyMaterials);

	if ( PropertyThatChanged == nullptr && RequiresNotifyMaterials )
	{
		// if RequiresNotifyMaterials was turned on by Validate
		// for a PostEditChange() with no Property
		// no need to Notify
		RequiresNotifyMaterials = false;
	}

	if( PropertyThatChanged )
	{
		static const FName CompressionSettingsName = GET_MEMBER_NAME_CHECKED(UTexture, CompressionSettings);
		static const FName LODGroupName = GET_MEMBER_NAME_CHECKED(UTexture, LODGroup);
		static const FName DeferCompressionName = GET_MEMBER_NAME_CHECKED(UTexture, DeferCompression);
		static const FName SrgbName = GET_MEMBER_NAME_CHECKED(UTexture, SRGB);
		static const FName VirtualTextureStreamingName = GET_MEMBER_NAME_CHECKED(UTexture, VirtualTextureStreaming);
		static const FName FilterName = GET_MEMBER_NAME_CHECKED(UTexture, Filter);
#if WITH_EDITORONLY_DATA
		static const FName SourceColorSpaceName = GET_MEMBER_NAME_CHECKED(FTextureSourceColorSettings, ColorSpace);
		static const FName CompressionQualityName = GET_MEMBER_NAME_CHECKED(UTexture, CompressionQuality);
		static const FName OodleTextureSdkVersionName = GET_MEMBER_NAME_CHECKED(UTexture, OodleTextureSdkVersion);
#endif //WITH_EDITORONLY_DATA

		const FName PropertyName = PropertyThatChanged->GetFName();

		if ((PropertyName == CompressionSettingsName) ||
			(PropertyName == FilterName) ||
			(PropertyName == LODGroupName) ||
			(PropertyName == SrgbName))
		{
			RequiresNotifyMaterials = true;
			
			if (PropertyName == LODGroupName)
			{
				// should this be in Validate ? or only when switching to this LODGroup ? (allowing change after)
				if (LODGroup == TEXTUREGROUP_8BitData)
				{
					CompressionSettings = TC_VectorDisplacementmap;
					SRGB = false;
					Filter = TF_Default;
					MipGenSettings = TMGS_FromTextureGroup;
				}
				else if (LODGroup == TEXTUREGROUP_16BitData)
				{
					CompressionSettings = TC_HDR;
					SRGB = false;
					Filter = TF_Default;
					MipGenSettings = TMGS_FromTextureGroup;
				}
			}
		}
		else if (PropertyName == DeferCompressionName)
		{
			DeferCompressionWasEnabled = DeferCompression;
		}
#if WITH_EDITORONLY_DATA
		else if (PropertyName == SourceColorSpaceName)
		{
			// Update the chromaticity coordinates member variables based on the color space choice (unless custom).
			if (SourceColorSettings.ColorSpace != ETextureColorSpace::TCS_Custom)
			{
				UE::Color::FColorSpace ColorSpace(static_cast<UE::Color::EColorSpace>(SourceColorSettings.ColorSpace));
				ColorSpace.GetChromaticities(SourceColorSettings.RedChromaticityCoordinate, SourceColorSettings.GreenChromaticityCoordinate, SourceColorSettings.BlueChromaticityCoordinate, SourceColorSettings.WhiteChromaticityCoordinate);
			}
		}
		else if (PropertyName == CompressionQualityName)
		{
			RequiresNotifyMaterials = true;
			bInvalidatesMaterialShaders = false;
		}
		else if (PropertyName == VirtualTextureStreamingName)
		{
			RequiresNotifyMaterials = true;
		}
		else if (PropertyName == OodleTextureSdkVersionName)
		{
			// if you write "latest" in editor it becomes the number of the latest version
			static const FName NameLatest("latest");
			static const FName NameCurrent("current");
			if ( OodleTextureSdkVersion == NameLatest || OodleTextureSdkVersion == NameCurrent )
			{
				OodleTextureSdkVersion = CachedGetLatestOodleSdkVersion();
			}
		}
#endif //WITH_EDITORONLY_DATA
	}

	// PostEditChange() with no property is called at load time , which goes in here
	if (!PropertyThatChanged && !GDisableAutomaticTextureMaterialUpdateDependencies)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateDependentMaterials);

		// Update any material that uses this texture and must force a recompile of cache resource
		TArray<UMaterial*> MaterialsToUpdate;
		TSet<UMaterial*> BaseMaterialsThatUseThisTexture;
		// this walks all Materials in the world
		for (TObjectIterator<UMaterialInterface> It; It; ++It)
		{
			UMaterialInterface* MaterialInterface = *It;
			if (DoesMaterialUseTexture(MaterialInterface, this))
			{
				UMaterial* Material = MaterialInterface->GetMaterial();
				bool MaterialAlreadyCompute = false;
				BaseMaterialsThatUseThisTexture.Add(Material, &MaterialAlreadyCompute);
				if (!MaterialAlreadyCompute)
				{
					if (Material->IsTextureForceRecompileCacheRessource(this))
					{
						MaterialsToUpdate.Add(Material);
						Material->UpdateMaterialShaderCacheAndTextureReferences();
					}
				}
			}
		}

		if (MaterialsToUpdate.Num())
		{
			FMaterialUpdateContext UpdateContext;

			for (UMaterial* MaterialToUpdate: MaterialsToUpdate)
			{
				UpdateContext.AddMaterial(MaterialToUpdate);
			}
		}
	}

	// If it's a render target, we always need to update the resource, to avoid an assert when rendering to it,
	// due to a mismatch between the render target and scene render.
	if ( (PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive) == 0 || GetTextureClass() == ETextureClass::RenderTarget )
	{
		// Update the texture resource. This will recache derived data if necessary
		// which may involve recompressing the texture.
		UpdateResource();
	}

	// Notify any loaded material instances if changed our compression format
	if (RequiresNotifyMaterials)
	{
		NotifyMaterials(bInvalidatesMaterialShaders ? ENotifyMaterialsEffectOnShaders::Default : ENotifyMaterialsEffectOnShaders::DoesNotInvalidate);
	}
		
#if WITH_EDITORONLY_DATA
	// any texture that is referencing this texture as AssociatedNormalMap needs to be informed
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateDependentTextures);

		TArray<UTexture*> TexturesThatUseThisTexture;

		for (TObjectIterator<UTexture> It; It; ++It) // walk all textures in the world
		{
			UTexture* Tex = *It;
			
			if(Tex != this && Tex->CompositeTexture == this && Tex->CompositeTextureMode != CTM_Disabled)
			{
				TexturesThatUseThisTexture.Add(Tex);
			}
		}
		// there is a potential infinite loop here if two textures depend on each other (or in a ring)
		for (int32 i = 0; i < TexturesThatUseThisTexture.Num(); ++i)
		{
			TexturesThatUseThisTexture[i]->PostEditChange();
		}
	}
#endif

	for (UAssetUserData* Datum : AssetUserData)
	{
		if (Datum != nullptr)
		{
			Datum->PostEditChangeOwner();
		}
	}
}

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
static bool IsEnableLegacyAlphaCoverageThresholdScaling()
{
	static bool bConfigBool = GConfig->GetBoolOrDefault(TEXT("Texture"), TEXT("EnableLegacyAlphaCoverageThresholdScaling"), false, GEditorIni);
	return bConfigBool;
}
#endif

void UTexture::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	Super::Serialize(Ar);
	
	FStripDataFlags StripFlags(Ar);

	/** Legacy serialization. */
#if WITH_EDITORONLY_DATA

	if (Ar.IsLoading())
	{
		LoadedMainStreamObjectVersion = Ar.CustomVer(FUE5MainStreamObjectVersion::GUID);
	}

	// When new mip filter is ready to be enabled:
	// * change "bUseNewMipFilter = false;" to true in UTexture constructor above
	// * change "bool bUseNewMipFilter = false;" member to true in "UTexture" class, in "Texture.h" file
	// * change "bool bUseNewMipFilter = false;" member to true in "UTextureFactory" class, in "TextureFactory.h" file
	// * change "bUseNewMipFilter(false) to true in "FTextureBuildSettings" constructor, in "TextureCompressorModule.h" file
	// * change "ExistingbUseNewMipFilter = false" to true in UTextureFactory::FactoryCreateBinary method, in "EditorFactories.cpp" file
	// * add "TextureUseNewMipFilter" value in "FUE5MainStreamObjectVersion" enum, in "UE5MainStreamObjectVersion.h" file
	// * uncomment if statement below

	//if (Ar.IsLoading() && Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::TextureUseNewMipFilter)
	//{
	//	// Old textures should not use new mip filter for maintaining exacty the same output as before (to not increase patch size)
	//	bUseNewMipFilter = false;
	//}

	if (Ar.IsLoading() && Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::TextureDoScaleMipsForAlphaCoverage)
	{
		// bDoScaleMipsForAlphaCoverage was not transmitted in old versions
		//	and AlphaCoverageThresholds was being incorrectly set to (0,0,0,1)
		check( bDoScaleMipsForAlphaCoverage == false );
	
		if ( AlphaCoverageThresholds != FVector4(0,0,0,0) && AlphaCoverageThresholds != FVector4(0,0,0,1) )
		{
			// AlphaCoverageThresholds is a non-default value, assume that means they wanted it on
			bDoScaleMipsForAlphaCoverage = true;
		}
		else if ( AlphaCoverageThresholds == FVector4(0,0,0,1) )
		{
			// if value is (0,0,0,1)
			//	that was previously incorrectly being set by default and enabling alpha coverage processing
			// we don't want that, but to optionally preserve old behavior you can set a config option
			bDoScaleMipsForAlphaCoverage = IsEnableLegacyAlphaCoverageThresholdScaling();
		}
	}

	if (!StripFlags.IsEditorDataStripped())
	{
#if WITH_EDITOR
		FWriteScopeLock BulkDataExclusiveScope(Source.BulkDataLock.Get());
#endif

		if (Ar.IsLoading() && Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::VirtualizedBulkDataHaveUniqueGuids)
		{
			if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::TextureSourceVirtualization)
			{
				FByteBulkData TempBulkData;
				TempBulkData.Serialize(Ar, this);

				FGuid LegacyPersistentId = Source.GetId();
				Source.BulkData.CreateFromBulkData(TempBulkData, LegacyPersistentId, this);
			}
			else
			{
				Source.BulkData.Serialize(Ar, this, false /* bAllowRegister */);
				Source.BulkData.CreateLegacyUniqueIdentifier(this);
			}

		}
		else
		{
			Source.BulkData.Serialize(Ar, this);
		}
	}

	if (Ar.IsLoading())
	{
		// Could potentially guard this with a new custom version, but overhead of just checking on every load should be very small
		Source.EnsureBlocksAreSorted();

		if (Ar.UEVer() < VER_UE4_TEXTURE_LEGACY_GAMMA)
		{
			bUseLegacyGamma = true;
		}
	}

	if (Ar.IsCooking() && VirtualTextureStreaming)
	{
		if (UseVirtualTexturing(GMaxRHIFeatureLevel, Ar.CookingTarget()) == false)
		{
			UE_LOG(LogTexture, Display, TEXT("%s is marked for virtual streaming but virtual texture streaming is not available."), *GetPathName());
		}
	}
	
	if (Ar.IsLoading())
	{
		// bPNGCompressed is now deprecated and CompressionFormat should be used to detect PNG compression
		// update old assets that did not have CompressionFormat set
		// 
		// - In old versions, CompressionFormat did not exist (so it will load in as None), and bPNGCompressed is used
		//   as the source to set CompressionFormat
		// - In new versions, bPNGCompressed is deprecated, never written (so will load as false), and CompressionFormat
		//   is the authoritative source on whether something is a PNG or not.
		// - In between, for a while after CompressionFormat was introduced, a bug meant that textures that were flagged 
		//   as !bPNGCompressed, had their compression format set to PNG, but did not actually contain compressed data. Fix these up.
		//   this bug only existed up to version TextureSourceVirtualization, but it could be carried forward to later asset versions
		//   until this fixup was added
		//
		// Now, the separate bPNGCompressed is gone (to avoid further desyncs like this) and we make sure
		// that CompressionFormat always matches the contents.	

		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::VolumetricCloudReflectionSampleCountDefaultUpdate
			&& !Source.bPNGCompressed_DEPRECATED
			&& Source.CompressionFormat == TSCF_PNG)
		{
			UE_LOG(LogTexture, Display, TEXT("Texture \"%s\" has CompressionFormat=PNG but not bPNGCompressed, assuming texture is actually uncompressed."), *GetPathName());
			Source.CompressionFormat = TSCF_None;
		}

		if ( Source.bPNGCompressed_DEPRECATED && Source.CompressionFormat != TSCF_PNG )
		{
			// loaded with deprecated "bPNGCompressed" (but not the newer CompressionFormat)
			// change to CompressionFormat PNG
			// this is expected on assets older than the CompressionFormat field
			check( Source.CompressionFormat == TSCF_None );
			Source.CompressionFormat = TSCF_PNG;
		}
		
		// bPNGCompressed_DEPRECATED is not kept in sync with CompressionFormat any more, do not check it after this point

		if ( Source.GetFormat() == TSF_RGBA8_DEPRECATED
			|| Source.GetFormat() == TSF_RGBE8_DEPRECATED )
		{
			// ensure that later code doesn't ever see the _DEPRECATED formats

			// needs RB swap
			// force BulkData to become resident
			// do the swap on the bits
			// change format to swapped version
			// these formats are incredibly rare and old
			// just warn and change the enum but don't swap the bits
			// will appear RB swapped until reimported
			UE_LOG(LogTexture, Warning, TEXT("TextureSource is a deprecated RB swapped format, needs reimport!: %s"), *GetPathName());
		
			for(int i=0;i<Source.LayerFormat.Num();i++)
			{
				if ( Source.LayerFormat[i] == TSF_RGBA8_DEPRECATED )
				{
					Source.LayerFormat[i] = TSF_BGRA8;
				}
				else if ( Source.LayerFormat[i] == TSF_RGBE8_DEPRECATED )
				{
					Source.LayerFormat[i] = TSF_BGRE8;
				}
			}
		}

		// CTM_MAX is mis-named, it's actually one higher than the maximum valid value
		if ( (uint32)CompositeTextureMode >= (uint32)CTM_MAX )
		{
			UE_LOG(LogTexture, Warning, TEXT("CompositeTextureMode was invalid in uasset, disabled.  Fix content and re-save : %s"), *GetPathName());
			
			CompositeTextureMode = CTM_Disabled;
		}
	}
#endif // #if WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
void UTexture::AppendToClassSchema(FAppendToClassSchemaContext& Context)
{
	Super::AppendToClassSchema(Context);

	// IsEnableLegacyAlphaCoverageThresholdScaling affects upgrades executed in Serialize, so include it in the ClassSchema
	uint8 LegacyScalingBool = IsEnableLegacyAlphaCoverageThresholdScaling();
	Context.Update(&LegacyScalingBool, sizeof(LegacyScalingBool));
}
#endif


void UTexture::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	// this was set in the constructor :
	// but it can be stomped from the archetype in duplication
	// re-set it now :
	//check( Source.Owner == this );
	Source.SetOwner(this);

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));

		// this is for textures that are not being loaded
		// eg. created from code, eg. lightmaps
		// we want them to go ahead and use the latest oodle sdk, since their content is new anyway
		UpdateOodleTextureSdkVersionToLatest();
	}
#endif
	Super::PostInitProperties();
}

void UTexture::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (AssetImportData == nullptr)
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}

	if (!SourceFilePath_DEPRECATED.IsEmpty())
	{
		FAssetImportInfo Info;
		Info.Insert(FAssetImportInfo::FSourceFile(SourceFilePath_DEPRECATED));
		AssetImportData->SourceData = MoveTemp(Info);
	}

#endif

	if (IsCookPlatformTilingDisabled(nullptr)) // nullptr TargetPlatform means it will use UDeviceProfileManager::Get().GetActiveProfile() to get the tiling settings
	{
		// The texture was not processed/tiled during cook, so it has to be tiled when uploaded to the GPU if necessary
		bNotOfflineProcessed = true;
	}

	if( !IsTemplate() )
	{
		// The texture will be cached by the cubemap it is contained within on consoles.
		UTextureCube* CubeMap = Cast<UTextureCube>(GetOuter());
		if (CubeMap == NULL)
		{
			// Recreate the texture's resource.
			UpdateResource();
		}
	}
}

void UTexture::BeginFinalReleaseResource()
{
	check(!bAsyncResourceReleaseHasBeenStarted);
	// Send the rendering thread a release message for the texture's resource.
	if (GetResource())
	{
		BeginReleaseResource(GetResource());
	}
	if (TextureReference.IsInitialized_GameThread())
	{
		TextureReference.BeginRelease_GameThread();
	}
	ReleaseFence.BeginFence();
	// Keep track that we already kicked off the async release.
	bAsyncResourceReleaseHasBeenStarted = true;
}


void UTexture::BeginDestroy()
{
	Super::BeginDestroy();

	if (!HasPendingInitOrStreaming())
	{
		BeginFinalReleaseResource();
	}
}

bool UTexture::IsReadyForFinishDestroy()
{
#if WITH_EDITOR
	// We're being garbage collected and might still have async tasks pending
	if (!TryCancelCachePlatformData())
	{
		return false;
	}
#endif

	if (!Super::IsReadyForFinishDestroy())
	{
		return false;
	}
	if (!bAsyncResourceReleaseHasBeenStarted)
	{
		BeginFinalReleaseResource();
	}
	return ReleaseFence.IsFenceComplete();
}

void UTexture::FinishDestroy()
{
	Super::FinishDestroy();

	check(!bAsyncResourceReleaseHasBeenStarted || ReleaseFence.IsFenceComplete());
	check(TextureReference.IsInitialized_GameThread() == false);

	if(PrivateResource)
	{
		// Free the resource.
		delete PrivateResource;
		PrivateResource = NULL;
	}

	CleanupCachedRunningPlatformData();
#if WITH_EDITOR
	if (!GExitPurge)
	{
		ClearAllCachedCookedPlatformData();
	}
#endif
}

void UTexture::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UTexture::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	PreSaveEvent.Broadcast(this);

	Super::PreSave(ObjectSaveContext);

#if WITH_EDITOR
	if (DeferCompression)
	{
		GWarn->StatusUpdate( 0, 0, FText::Format( NSLOCTEXT("UnrealEd", "SavingPackage_CompressingTexture", "Compressing texture:  {0}"), FText::FromString(GetName()) ) );
		DeferCompression = false;
		UpdateResource();
	}

	// Ensure that compilation has finished before saving the package
	// otherwise async compilation might try to read the bulkdata
	// while it's being serialized to the package.
	// This also needs to happen before the source is modified below
	// because it invalidates the texture build due to source hash change
	// and could cause another build to be triggered during PostCompilation
	// causing reentrancy problems.
	FTextureCompilingManager::Get().FinishCompilation({ this });

	if (!GEngine->IsAutosaving() && !ObjectSaveContext.IsProceduralSave())
	{
		GWarn->StatusUpdate(0, 0, FText::Format(NSLOCTEXT("UnrealEd", "SavingPackage_CompressingSourceArt", "Compressing source art for texture:  {0}"), FText::FromString(GetName())));
		Source.Compress();
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITORONLY_DATA
void UTexture::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}

	OutTags.Add(FAssetRegistryTag("SourceCompression", Source.GetSourceCompressionAsString(), FAssetRegistryTag::TT_Alphabetical));

	OutTags.Add(FAssetRegistryTag("SourceFormat", 
		StaticEnum<ETextureSourceFormat>()->GetDisplayNameTextByValue(Source.GetFormat()).ToString(),
		FAssetRegistryTag::TT_Alphabetical));
	
	Super::GetAssetRegistryTags(OutTags);
}
#endif

FIoFilenameHash UTexture::GetMipIoFilenameHash(const int32 MipIndex) const
{
	FTexturePlatformData** PlatformData = const_cast<UTexture*>(this)->GetRunningPlatformData();
	if (PlatformData && *PlatformData)
	{
		const TIndirectArray<struct FTexture2DMipMap>& PlatformMips = (*PlatformData)->Mips;
		if (PlatformMips.IsValidIndex(MipIndex))
		{
			return PlatformMips[MipIndex].BulkData.GetIoFilenameHash();
		}
	}
	return INVALID_IO_FILENAME_HASH;
}

bool UTexture::DoesMipDataExist(const int32 MipIndex) const
{
	FTexturePlatformData** PlatformData = const_cast<UTexture*>(this)->GetRunningPlatformData();
	if (PlatformData && *PlatformData)
	{
		const TIndirectArray<struct FTexture2DMipMap>& PlatformMips = (*PlatformData)->Mips;
		if (PlatformMips.IsValidIndex(MipIndex))
		{
			return PlatformMips[MipIndex].BulkData.DoesExist();
		}
	}
	return false;
}

bool UTexture::HasPendingRenderResourceInitialization() const
{
	return GetResource() && !GetResource()->IsInitialized();
}

bool UTexture::HasPendingLODTransition() const
{
	return GetResource() && GetResource()->MipBiasFade.IsFading();
}

float UTexture::GetLastRenderTimeForStreaming() const
{
	float LastRenderTime = -FLT_MAX;
	if (GetResource())
	{
		// The last render time is the last time the resource was directly bound or the last
		// time the texture reference was cached in a resource table, whichever was later.
		LastRenderTime = FMath::Max<double>(GetResource()->LastRenderTime,TextureReference.GetLastRenderTime());
	}
	return LastRenderTime;
}

void UTexture::InvalidateLastRenderTimeForStreaming()
{
	if (GetResource())
	{
		GetResource()->LastRenderTime = -FLT_MAX;
	}
	TextureReference.InvalidateLastRenderTime();
}


bool UTexture::ShouldMipLevelsBeForcedResident() const
{
	if (LODGroup == TEXTUREGROUP_Skybox || Super::ShouldMipLevelsBeForcedResident())
	{
		return true;
	}
	return false;
}

void UTexture::CancelPendingTextureStreaming()
{
	for( TObjectIterator<UTexture> It; It; ++It )
	{
		UTexture* CurrentTexture = *It;
		CurrentTexture->CancelPendingStreamingRequest();
	}

	// No need to call FlushResourceStreaming(), since calling CancelPendingMipChangeRequest has an immediate effect.
}

float UTexture::GetAverageBrightness(bool bIgnoreTrueBlack, bool bUseGrayscale)
{
	// Indicate the action was not performed...
	return -1.0f;
}

/** Helper functions for text output of texture properties... */
#ifndef CASE_ENUM_TO_TEXT
#define CASE_ENUM_TO_TEXT(txt) case txt: return TEXT(#txt);
#endif

#ifndef TEXT_TO_ENUM
#define TEXT_TO_ENUM(eVal, txt)		if (FCString::Stricmp(TEXT(#eVal), txt) == 0)	return eVal;
#endif

const TCHAR* UTexture::GetTextureGroupString(TextureGroup InGroup)
{
	switch (InGroup)
	{
		FOREACH_ENUM_TEXTUREGROUP(CASE_ENUM_TO_TEXT)
	}

	return TEXT("TEXTUREGROUP_World");
}

const TCHAR* UTexture::GetMipGenSettingsString(TextureMipGenSettings InEnum)
{
	switch(InEnum)
	{
		default:
		FOREACH_ENUM_TEXTUREMIPGENSETTINGS(CASE_ENUM_TO_TEXT)
	}
}

TextureMipGenSettings UTexture::GetMipGenSettingsFromString(const TCHAR* InStr, bool bTextureGroup)
{
#define TEXT_TO_MIPGENSETTINGS(m) TEXT_TO_ENUM(m, InStr);
	FOREACH_ENUM_TEXTUREMIPGENSETTINGS(TEXT_TO_MIPGENSETTINGS)
#undef TEXT_TO_MIPGENSETTINGS

	// default for TextureGroup and Texture is different
	return bTextureGroup ? TMGS_SimpleAverage : TMGS_FromTextureGroup;
}

bool UTexture::IsCookPlatformTilingDisabled(const ITargetPlatform* TargetPlatform) const
{
	if (CookPlatformTilingSettings.GetValue() == TextureCookPlatformTilingSettings::TCPTS_FromTextureGroup)
	{
		const UTextureLODSettings* TextureLODSettings = nullptr;

		if (TargetPlatform)
		{
			TextureLODSettings = &TargetPlatform->GetTextureLODSettings();
		}
		else
		{
			TextureLODSettings = UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings();

			if (!TextureLODSettings)
			{
				return false;
			}
		}

		check(TextureLODSettings);

		checkf(LODGroup < TextureLODSettings->TextureLODGroups.Num(), 
			TEXT("A texture had passed a bad LODGroup to UTexture::IsCookPlatformTilingDisabled (%d, out of %d groups). The texture name is '%s'."), 
			LODGroup, TextureLODSettings->TextureLODGroups.Num(), *GetPathName());

		return TextureLODSettings->TextureLODGroups[LODGroup].CookPlatformTilingDisabled;
	}

	return CookPlatformTilingSettings.GetValue() == TextureCookPlatformTilingSettings::TCPTS_DoNotTile;
}

void UTexture::SetDeterministicLightingGuid()
{
#if WITH_EDITORONLY_DATA
	// Compute a 128-bit hash based on the texture name and use that as a GUID to fix this issue.
	FTCHARToUTF8 Converted(*GetFullName());
	FMD5 MD5Gen;
	MD5Gen.Update((const uint8*)Converted.Get(), Converted.Length());
	uint32 Digest[4];
	MD5Gen.Final((uint8*)Digest);

	// FGuid::NewGuid() creates a version 4 UUID (at least on Windows), which will have the top 4 bits of the
	// second field set to 0100. We'll set the top bit to 1 in the GUID we create, to ensure that we can never
	// have a collision with textures which use implicitly generated GUIDs.
	Digest[1] |= 0x80000000;
	FGuid TextureGUID(Digest[0], Digest[1], Digest[2], Digest[3]);

	LightingGuid = TextureGUID;
#else
	LightingGuid = FGuid(0, 0, 0, 0);
#endif // WITH_EDITORONLY_DATA
}

UEnum* UTexture::GetPixelFormatEnum()
{
	// Lookup the pixel format enum so that the pixel format can be serialized by name.
	static UEnum* PixelFormatEnum = nullptr;
	if (PixelFormatEnum == nullptr)
	{
		FTopLevelAssetPath PixelFormatEnumPath(TEXT("/Script/CoreUObject"), TEXT("EPixelFormat"));
		check(IsInGameThread());
		PixelFormatEnum = FindObject<UEnum>(PixelFormatEnumPath);
		check(PixelFormatEnum);
	}
	return PixelFormatEnum;
}

void UTexture::PostCDOContruct()
{
	GetPixelFormatEnum();
}

bool UTexture::ForceUpdateTextureStreaming()
{
	if (!IStreamingManager::HasShutdown())
	{
/*
		// I'm not sure what the scope of this "ForceUpdate" is supposed to be
		// but if you are trying to account for config changes that can change LODBias in Editor
		// then that means the NumMips in the cached FStreamableRenderResourceState may have changed
		// and they all need to be reset

#if WITH_EDITOR
		for( TObjectIterator<UTexture2D> It; It; ++It )
		{
			UTexture* Texture = *It;

			fill the 
			FStreamableTextureResource::State
			by re-calling
			Texture->GetResourcePostInitState();
		}
#endif // #if WITH_EDITOR
*/

		// Make sure we iterate over all textures by setting it to high value.
		IStreamingManager::Get().SetNumIterationsForNextFrame( 100 );
		// Update resource streaming with updated texture LOD bias/ max texture mip count.
		IStreamingManager::Get().UpdateResourceStreaming( 0 );
		// Block till requests are finished.
		IStreamingManager::Get().BlockTillAllRequestsFinished();
	}

	return true;
}

void UTexture::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UTexture::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void UTexture::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* UTexture::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
}


bool UTexture::IsPossibleToStream() const
{
	if ( NeverStream || LODGroup == TEXTUREGROUP_UI )
	{
		return false;
	}

	#if WITH_EDITORONLY_DATA
	if ( MipGenSettings == TMGS_NoMipmaps )
	{
		return false;
	}

	// VirtualTextureStreaming can be true here and we will still stream if VT is disabled
	
	if ( Source.IsValid() )
	{
		// should have set NeverStream for nonpow2
		// note: this is not the case for all old textures
		// ValidateSettingsAfterImportOrEdit makes sure this is true
		//check( Source.IsPowerOfTwo() || PowerOfTwoMode != ETexturePowerOfTwoSetting::None )
		
		// duplicate the checks done for NeverStream :

		// IsPowerOfTwo only checks XY :
		bool bIsPowerOfTwo = Source.IsPowerOfTwo();
		if ( ! FMath::IsPowerOfTwo(Source.GetVolumeSizeZ()) )
		{
			bIsPowerOfTwo = false;
		}
		if ( PowerOfTwoMode != ETexturePowerOfTwoSetting::None )
		{
			bIsPowerOfTwo = true;
		}
		if ( ! bIsPowerOfTwo )
		{
			// NeverStream should have been set
			return false;
		}
	}
	#endif

	return true;
}

#if WITH_EDITOR
// Based on target platform, returns whether texture is candidate to be streamed.
bool UTexture::IsCandidateForTextureStreamingOnPlatformDuringCook(const ITargetPlatform* InTargetPlatform) const
{
	const bool bIsVirtualTextureStreaming = InTargetPlatform->SupportsFeature(ETargetPlatformFeatures::VirtualTextureStreaming) ? VirtualTextureStreaming : false;
	const bool bIsCandidateForTextureStreaming = InTargetPlatform->SupportsFeature(ETargetPlatformFeatures::TextureStreaming) && !bIsVirtualTextureStreaming;

	if (bIsCandidateForTextureStreaming &&
		IsPossibleToStream())
	{
		return true;
	}
	return false;
}
#endif

FStreamableRenderResourceState UTexture::GetResourcePostInitState(const FTexturePlatformData* PlatformData, bool bAllowStreaming, int32 MinRequestMipCount, int32 MaxMipCount, bool bSkipCanBeLoaded) const
{
	// Async caching of PlatformData must be done before calling this
	//	if you call while async CachePlatformData is in progress, you get garbage out

	// "FullLODBias" is == NumCincematicMipLevels + also maybe drop mip LODBias
	//  the LODBias to drop mips is not added in cooked runs, because those mips have been already dropped
	//  it is added in non-cooked runs because they are still present but we are trying to pretend they are not there
	// "CinematicLODBias" is typically zero in cooked runs, = drop mip count in non-cooked runs

	int32 FullLODBias = CalculateLODBias(true);
	int32 CinematicLODBias = CalculateLODBias(false);
	check( FullLODBias >= CinematicLODBias );

	bool bTextureIsStreamable = IsPossibleToStream();

	int32 FullMipCount = PlatformData->Mips.Num();
	int32 NumOfNonOptionalMips = PlatformData->GetNumNonOptionalMips();
	int32 NumOfNonStreamingMips = PlatformData->GetNumNonStreamingMips(bTextureIsStreamable);
	int32 NumMipsInTail = PlatformData->GetNumMipsInTail();

	// Optional mips must be streaming mips :
	check( NumOfNonOptionalMips >= NumOfNonStreamingMips );
	// Mips in tail must be nonstreaming :
	check( NumOfNonStreamingMips >= NumMipsInTail );

	// Create the resource with a mip count limit taking in consideration the asset LODBias.
	// This ensures that the mip count stays constant when toggling asset streaming at runtime.
	
	const int32 ExpectedAssetLODBias = FMath::Clamp<int32>(CinematicLODBias, 0, FullMipCount - 1);
	// "ExpectedAssetLODBias" is the number of mips that would be dropped in cook
	//		in a cooked run, it is zero

	// in Editor the mips that will be dropped in cook are still present
	//	dropping them is simulated by treating them as streamable
	if ( ExpectedAssetLODBias > 0 && ! bTextureIsStreamable )
	{
		check( ! FPlatformProperties::RequiresCookedData() ); // ExpectedAssetLODBias should have been zero
		NumOfNonStreamingMips = FullMipCount - ExpectedAssetLODBias;
	}

	// GMaxTextureMipCount is for the current running RHI
	//  it may be lower than the number of mips we cooked (eg. on mobile)
	//	we must limit the number of mips to this count.
	const int32 MaxRuntimeMipCount = FMath::Min<int32>(GMaxTextureMipCount, FStreamableRenderResourceState::MAX_LOD_COUNT);

	int32 NumMips = FMath::Min<int32>( FullMipCount - ExpectedAssetLODBias, MaxRuntimeMipCount );
	// "NumMips" is the number of mips after drop LOD Bias
	//	 it should be the same in Editor and Runtime

	if (MaxMipCount > 0 && NumMips > MaxMipCount)
	{
		// MaxMipCount is almost always either 0 or == MaxRuntimeMipCount
		// one exception is :
		//	MobileReduceLoadedMips(NumMips);
		// which can cause an additional reduction of NumMips
		NumMips = MaxMipCount;
	}
	
	// don't allow less than NumOfNonStreamingMips :
	if ( NumMips < NumOfNonStreamingMips )
	{
		// if NumMips went under NumOfNonStreamingMips due to ExpectedAssetLODBias
		//  then force it back up
		// but if it went under due to MaxRuntimeMipCount
		// then that's a problem

		if ( NumOfNonStreamingMips > MaxRuntimeMipCount )
		{
			// this should never happen on a PC platform, only on mobile
			// in that case streaming in the "nonstreaming" may actually be okay
			// in the old code, this was expected behavior, let the MaxRuntimeMipCount trump the NonStreaming constraint
			// in new code (with RequiredBlock4Alignment) we do not expect to see this any more, so warn :
			UE_LOG(LogTexture, Warning, TEXT("NumOfNonStreamingMips > MaxRuntimeMipCount. (%s)"), *GetName());
			NumOfNonStreamingMips = MaxRuntimeMipCount;
		}

		NumMips = NumOfNonStreamingMips;
	}

	check( NumMips >= NumMipsInTail );
	
	if ( NumOfNonStreamingMips == NumMips )
	{
		bTextureIsStreamable = false;
	}

	const int32 AssetMipIdxForResourceFirstMip = FullMipCount - NumMips;

	bool bMakeStreamable = false;
	int32 NumRequestedMips = 0;
	
#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
	if ( bTextureIsStreamable )
	{
		bool bWillProvideMipDataWithoutDisk = false;

		// Check if any of the CustomMipData providers associated with this texture can provide mip data even without DDC or disk,
		// if so, enable streaming for this texture
		for (UAssetUserData* UserData : AssetUserData)
		{
			UTextureMipDataProviderFactory* CustomMipDataProviderFactory = Cast<UTextureMipDataProviderFactory>(UserData);
			if (CustomMipDataProviderFactory)
			{
				bWillProvideMipDataWithoutDisk = CustomMipDataProviderFactory->WillProvideMipDataWithoutDisk();
				if (bWillProvideMipDataWithoutDisk)
				{
					break;
				}
			}
		}

		if (bAllowStreaming &&
			(bSkipCanBeLoaded || PlatformData->CanBeLoaded() || bWillProvideMipDataWithoutDisk))
		{
			bMakeStreamable  = true;
		}
	}
#endif

	if ( ! bTextureIsStreamable )
	{
		// in Editor , NumOfNonStreamingMips may not be all mips
		// but once we cook it will be
		// so check this early to make behavior consistent
		NumRequestedMips = NumMips;
	}
	else if (bMakeStreamable && IStreamingManager::Get().IsRenderAssetStreamingEnabled(EStreamableRenderAssetType::Texture))
	{
		NumRequestedMips = NumOfNonStreamingMips;
	}
	else
	{
		// we are not streaming (bMakeStreamable is false)
		// but this may select a mip below the top mip
		// (due to cinematic lod bias)
		// but only if the texture itself is streamable

		// Adjust CachedLODBias so that it takes into account FStreamableRenderResourceState::AssetLODBias.
		const int32 ResourceLODBias = FMath::Max<int32>(0, FullLODBias - AssetMipIdxForResourceFirstMip);
		//  ResourceLODBias almost always == NumCinematicMipLevels, unless you hit the MaxRuntimeMipCount clamp in NumMips

		// Bias is not allowed to shrink the mip count below NumOfNonStreamingMips.
		NumRequestedMips = FMath::Max<int32>(NumMips - ResourceLODBias, NumOfNonStreamingMips);

		// If trying to load optional mips, check if the first resource mip is available.
		if (NumRequestedMips > NumOfNonOptionalMips && !DoesMipDataExist(AssetMipIdxForResourceFirstMip))
		{
			NumRequestedMips = NumOfNonOptionalMips;
		}
	}

	// @todo Oodle : this looks like a bug; did it mean to be MinRequestMipCount <= NumMips ?
	// typically MinRequestMipCount == 0
	// the only place it's not zero is from UTexture2D::CreateResource from existing resource mem, where MinRequestMipCount is == NumMips
	// but in that case it is ignored here because this branches on < instead of <=
	if ( NumRequestedMips < MinRequestMipCount && MinRequestMipCount < NumMips )
	{
		// as written with < instead of <= this branch is not used

		NumRequestedMips = MinRequestMipCount;
	}

	check( NumOfNonStreamingMips <= NumMips );
	check( NumRequestedMips <= NumMips );
	check( NumRequestedMips >= NumOfNonStreamingMips );

	FStreamableRenderResourceState PostInitState;
	PostInitState.bSupportsStreaming = bMakeStreamable;
	PostInitState.NumNonStreamingLODs = (uint8)NumOfNonStreamingMips;
	PostInitState.NumNonOptionalLODs = (uint8) FMath::Min<int32>(NumOfNonOptionalMips,NumMips);
	PostInitState.MaxNumLODs = (uint8)NumMips;
	PostInitState.AssetLODBias = (uint8)AssetMipIdxForResourceFirstMip;
	PostInitState.NumResidentLODs = (uint8)NumRequestedMips;
	PostInitState.NumRequestedLODs = (uint8)NumRequestedMips;

	return PostInitState;
}

/*------------------------------------------------------------------------------
	Texture source data.
------------------------------------------------------------------------------*/

FTextureSource::FTextureSource()
	: 
#if WITH_EDITOR
	  Owner(nullptr),
	  TornOffTextureClass(ETextureClass::Invalid),
	  TornOffGammaSpace(EGammaSpace::Invalid),
#endif
	  NumLockedMips(0u)
	, LockState(ELockState::None)
#if WITH_EDITOR
	, bHasHadBulkDataCleared(false)
#endif
#if WITH_EDITORONLY_DATA
	, BaseBlockX(0)
	, BaseBlockY(0)
	, SizeX(0)
	, SizeY(0)
	, NumSlices(0)
	, NumMips(0)
	, NumLayers(1) // Default to 1 so old data has the correct value
	, bPNGCompressed_DEPRECATED(false)
	, bLongLatCubemap(false)
	, CompressionFormat(TSCF_None)
	, bGuidIsHash(false)
	, Format(TSF_Invalid)
#endif // WITH_EDITORONLY_DATA
{
}

FTextureSourceBlock::FTextureSourceBlock()
	: BlockX(0)
	, BlockY(0)
	, SizeX(0)
	, SizeY(0)
	, NumSlices(0)
	, NumMips(0)
{
}

int32 FTextureSource::GetBytesPerPixel(ETextureSourceFormat Format)
{
	ERawImageFormat::Type RawFormat = FImageCoreUtils::ConvertToRawImageFormat(Format);
	return ERawImageFormat::GetBytesPerPixel(RawFormat);
}

#if WITH_EDITOR

bool FTextureSource::IsCubeOrCubeArray() const
{
	ETextureClass TextureClass = GetTextureClass();
	return TextureClass == ETextureClass::Cube || TextureClass == ETextureClass::CubeArray;
}

bool FTextureSource::IsVolume() const
{
	ETextureClass TextureClass = GetTextureClass();
	return TextureClass == ETextureClass::Volume;
}

bool FTextureSource::IsLongLatCubemap() const
{
	if ( IsCubeOrCubeArray() )
	{
		check( NumLayers == 1 );

		// bLongLatCubemap is sometimes set for LongLat Cube Arrays but not always
		if ( bLongLatCubemap )
		{
			return true;
		}
		else
		{
			// if NumSlices is not a multiple of 6, must be longlat !?
			return (NumSlices % 6) != 0;
		}
	}
	else
	{
		check( ! bLongLatCubemap );
		return false;
	}
}

// returns volume depth, or 1 if not a volume
int FTextureSource::GetVolumeSizeZ() const
{
	if ( IsVolume() )
	{
		check( NumLayers == 1 );
		return NumSlices;
	}
	else
	{
		return 1;
	}
}

static int GetFullMipCount(int32 SizeX,int32 SizeY,int32 SizeZ = 1)
{
	if ( SizeX == 0 || SizeY == 0 || SizeZ == 0 )
	{
		return 0;
	}
	
	int MaxDim = FMath::Max3(SizeX,SizeY,SizeZ);

	int32 MipCount = FMath::FloorLog2(MaxDim) + 1;

	return MipCount;
}

void FTextureSource::InitBlocked(const ETextureSourceFormat* InLayerFormats,
	const FTextureSourceBlock* InBlocks,
	int32 InNumLayers,
	int32 InNumBlocks,
	const uint8** InDataPerBlock)
{
	InitBlockedImpl(InLayerFormats, InBlocks, InNumLayers, InNumBlocks);

	int64 TotalBytes = CalcTotalSize();

	FUniqueBuffer Buffer = FUniqueBuffer::Alloc(TotalBytes);
	uint8* DataPtr = (uint8*)Buffer.GetData();

	if (InDataPerBlock)
	{
		for (int i = 0; i < InNumBlocks; ++i)
		{
			const int64 BlockSize = CalcBlockSize(InBlocks[i]);
			if (InDataPerBlock[i])
			{
				FMemory::Memcpy(DataPtr, InDataPerBlock[i], BlockSize);
			}
			DataPtr += BlockSize;
		}
	}

	BulkData.UpdatePayload(Buffer.MoveToShared(), Owner);
	BulkData.SetCompressionOptions(UE::Serialization::ECompressionOptions::Default);
}

void FTextureSource::InitBlocked(const ETextureSourceFormat* InLayerFormats,
	const FTextureSourceBlock* InBlocks,
	int32 InNumLayers,
	int32 InNumBlocks,
	UE::Serialization::FEditorBulkData::FSharedBufferWithID NewData)
{
	InitBlockedImpl(InLayerFormats, InBlocks, InNumLayers, InNumBlocks);

	BulkData.UpdatePayload(MoveTemp(NewData), Owner);
	BulkData.SetCompressionOptions(UE::Serialization::ECompressionOptions::Default);
}

void FTextureSource::InitLayered(
	int32 NewSizeX,
	int32 NewSizeY,
	int32 NewNumSlices,
	int32 NewNumLayers,
	int32 NewNumMips,
	const ETextureSourceFormat* NewLayerFormat,
	const uint8* NewData)
{
	InitLayeredImpl(
		NewSizeX,
		NewSizeY,
		NewNumSlices,
		NewNumLayers,
		NewNumMips,
		NewLayerFormat
		);

	int64 TotalBytes = 0;
	for (int i = 0; i < NewNumLayers; ++i)
	{
		TotalBytes += CalcLayerSize(0, i);
	}

	// Init with NewData == null is used to allocate space, which is then filled with LockMip
	if (NewData != nullptr)
	{
		BulkData.UpdatePayload(FSharedBuffer::Clone(NewData, TotalBytes), Owner);
	}
	else
	{
		BulkData.UpdatePayload(FUniqueBuffer::Alloc(TotalBytes).MoveToShared(), Owner);
	}

	BulkData.SetCompressionOptions(UE::Serialization::ECompressionOptions::Default);
}

void FTextureSource::InitLayered(
	int32 NewSizeX,
	int32 NewSizeY,
	int32 NewNumSlices,
	int32 NewNumLayers,
	int32 NewNumMips,
	const ETextureSourceFormat* NewLayerFormat,
	UE::Serialization::FEditorBulkData::FSharedBufferWithID NewData)
{
	InitLayeredImpl(
		NewSizeX,
		NewSizeY,
		NewNumSlices,
		NewNumLayers,
		NewNumMips,
		NewLayerFormat
	);

	BulkData.UpdatePayload(MoveTemp(NewData), Owner);
	BulkData.SetCompressionOptions(UE::Serialization::ECompressionOptions::Default);
}

void FTextureSource::Init(
		int32 NewSizeX,
		int32 NewSizeY,
		int32 NewNumSlices,
		int32 NewNumMips,
		ETextureSourceFormat NewFormat,
		const uint8* NewData
		)
{
	InitLayered(NewSizeX, NewSizeY, NewNumSlices, 1, NewNumMips, &NewFormat, NewData);
}

void FTextureSource::Init(const FImageView & Image)
{
	ETextureSourceFormat SourceFormat = FImageCoreUtils::ConvertToTextureSourceFormat(Image.Format);

	// FImageView has gamma information too that is lost
	// TextureSource does not store gamma information (it's in the owning Texture)

	Init(Image.SizeX,Image.SizeY,Image.NumSlices,1,SourceFormat,(const uint8 *)Image.RawData);
}

void FTextureSource::Init(
	int32 NewSizeX,
	int32 NewSizeY,
	int32 NewNumSlices,
	int32 NewNumMips,
	ETextureSourceFormat NewFormat,
	UE::Serialization::FEditorBulkData::FSharedBufferWithID NewData
)
{
	InitLayered(NewSizeX, NewSizeY, NewNumSlices, 1, NewNumMips, &NewFormat, MoveTemp(NewData));
}

void FTextureSource::Init2DWithMipChain(
	int32 NewSizeX,
	int32 NewSizeY,
	ETextureSourceFormat NewFormat
	)
{
	int32 NewMipCount = GetFullMipCount(NewSizeX,NewSizeY);
	Init(NewSizeX, NewSizeY, 1, NewMipCount, NewFormat);
}

void FTextureSource::InitLayered2DWithMipChain(
	int32 NewSizeX,
	int32 NewSizeY,
	int32 NewNumLayers,
	const ETextureSourceFormat* NewFormat)
{
	int32 NewMipCount = GetFullMipCount(NewSizeX,NewSizeY);
	InitLayered(NewSizeX, NewSizeY, 1, NewNumLayers, NewMipCount, NewFormat);
}

void FTextureSource::InitCubeWithMipChain(
	int32 NewSizeX,
	int32 NewSizeY,
	ETextureSourceFormat NewFormat
	)
{
	int32 NewMipCount = GetFullMipCount(NewSizeX,NewSizeY);
	Init(NewSizeX, NewSizeY, 6, NewMipCount, NewFormat);
}

void FTextureSource::InitWithCompressedSourceData(
	int32 NewSizeX,
	int32 NewSizeY,
	int32 NewNumMips,
	ETextureSourceFormat NewFormat,
	const TArrayView64<uint8> NewData,
	ETextureSourceCompressionFormat NewSourceFormat
)
{
	const int32 NewNumSlice = 1;
	const int32 NewNumLayer = 1;
	InitLayeredImpl(
		NewSizeX,
		NewSizeY,
		NewNumSlice,
		NewNumLayer,
		NewNumMips,
		&NewFormat
		);

	CompressionFormat = NewSourceFormat;

	BulkData.UpdatePayload(FSharedBuffer::Clone(NewData.GetData(), NewData.Num()), Owner);
	// Disable the internal bulkdata compression if the source data is already compressed
	if (CompressionFormat == TSCF_None)
	{
		BulkData.SetCompressionOptions(UE::Serialization::ECompressionOptions::Default);
	}
	else
	{
		BulkData.SetCompressionOptions(UE::Serialization::ECompressionOptions::Disabled);
	}
}

void FTextureSource::InitWithCompressedSourceData(
	int32 NewSizeX,
	int32 NewSizeY,
	int32 NewNumMips,
	ETextureSourceFormat NewFormat,
	UE::Serialization::FEditorBulkData::FSharedBufferWithID NewSourceData,
	ETextureSourceCompressionFormat NewSourceFormat
)
{
	const int32 NewNumSlice = 1;
	const int32 NewNumLayer = 1;
	InitLayeredImpl(
		NewSizeX,
		NewSizeY,
		NewNumSlice,
		NewNumLayer,
		NewNumMips,
		&NewFormat
	);

	CompressionFormat = NewSourceFormat;

	BulkData.UpdatePayload(MoveTemp(NewSourceData), Owner);
	// Disable the internal bulkdata compression if the source data is already compressed
	if (CompressionFormat == TSCF_None)
	{
		BulkData.SetCompressionOptions(UE::Serialization::ECompressionOptions::Default);
	}
	else
	{
		BulkData.SetCompressionOptions(UE::Serialization::ECompressionOptions::Disabled);
	}
}

FTextureSource FTextureSource::CopyTornOff() const
{
	FTextureSource Result;
	// Set the Torn off flag on Result.BulkData so that the copy constructor below will not set it
	Result.BulkData.TearOff();
	// Use the default copy constructor to copy all the fields without having to write them manually
	Result = *this;
	Result.Owner = nullptr; // TornOffs don't count as belonging to the same owner
	// Result can't talk to Owner any more, so save info we need :
	check( Owner != nullptr );
	check( &(Owner->Source) == this );
	Result.TornOffGammaSpace = Owner->GetGammaSpace();
	Result.TornOffTextureClass = Owner->GetTextureClass();
	return Result;
}

void FTextureSource::Compress()
{
	checkf(LockState == ELockState::None, TEXT("Compress shouldn't be called in-between LockMip/UnlockMip"));

#if WITH_EDITOR
	FWriteScopeLock BulkDataExclusiveScope(BulkDataLock.Get());
#endif

	// if bUseOodleOnPNGz0 , do PNG filters but then use Oodle instead of zlib back-end LZ
	//	should be faster to load and also smaller files (than traditional PNG+zlib)
	bool bUseOodleOnPNGz0 = true;

	// may already have "CompressionFormat" set

	if (CanPNGCompress()) // Note that this will return false if the data is already a compressed PNG
	{
		FSharedBuffer Payload = BulkData.GetPayload().Get();

		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
		
		ERGBFormat RGBFormat;
		int BitsPerChannel;
		switch(Format)
		{
		case TSF_G8:
			RGBFormat = ERGBFormat::Gray;
			BitsPerChannel = 8;
			break;
		case TSF_G16:
			RGBFormat = ERGBFormat::Gray;
			BitsPerChannel = 16;
			break;
		case TSF_BGRA8:
		// Legacy bug, must be matched in Compress & Decompress
		// TODO: TSF_BGRA8 is stored as RGBA, so the R and B channels are swapped in the internal png. Should we fix this?
			// should have been ERGBFormat::BGRA
			RGBFormat = ERGBFormat::RGBA;
			BitsPerChannel = 8;
			break;
		case TSF_RGBA16:
			RGBFormat = ERGBFormat::RGBA;
			BitsPerChannel = 16;
			break;
		default:
			check(0); // should not get here because we already checked CanPNGCompress()
			return;
		}

		if ( ImageWrapper.IsValid() && ImageWrapper->SetRaw(Payload.GetData(), Payload.GetSize(), SizeX, SizeY, RGBFormat, BitsPerChannel ) )
		{
			EImageCompressionQuality PngQuality = EImageCompressionQuality::Default; // 0 means default 
			if ( bUseOodleOnPNGz0 )
			{
				PngQuality = EImageCompressionQuality::Uncompressed; // turn off zlib
			}
			TArray64<uint8> CompressedData = ImageWrapper->GetCompressed((int32)PngQuality);
			if ( CompressedData.Num() > 0 )
			{
				BulkData.UpdatePayload(MakeSharedBufferFromArray(MoveTemp(CompressedData)), Owner);

				CompressionFormat = TSCF_PNG;
			}
		}
	}

	if ( ( CompressionFormat == TSCF_PNG && bUseOodleOnPNGz0 ) ||
		CompressionFormat == TSCF_None )
	{
		BulkData.SetCompressionOptions(ECompressedBufferCompressor::Kraken,ECompressedBufferCompressionLevel::Fast);
	}
	else
	{
		BulkData.SetCompressionOptions(UE::Serialization::ECompressionOptions::Disabled);
	}
}

FSharedBuffer FTextureSource::Decompress(IImageWrapperModule* ) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSource::Decompress);
	
	// ImageWrapperModule argument ignored, not drilled through DecompressImage

	int64 ExpectedTotalSize = CalcTotalSize();

	FSharedBuffer Buffer;

	if (CompressionFormat != TSCF_None )
	{
		Buffer = TryDecompressData();
	}
	else
	{
		Buffer = BulkData.GetPayload().Get();
	}
	
	// validate the size of the FSharedBuffer
	if ( Buffer.GetSize() != ExpectedTotalSize )
	{
		UE_LOG(LogTexture,Warning,TEXT("Decompressed buffer does not match expected size : %lld != %lld"),
			Buffer.GetSize(),ExpectedTotalSize);
	}

	return Buffer;
}

// constructor locks the mip (can fail, pointer will be null)
FTextureSource::FMipLock::FMipLock(ELockState InLockState,FTextureSource * InTextureSource,int32 InBlockIndex, int32 InLayerIndex, int32 InMipIndex)
:
	LockState(InLockState),
	TextureSource(InTextureSource),
	BlockIndex(InBlockIndex),
	LayerIndex(InLayerIndex),
	MipIndex(InMipIndex)
{
	void * Locked = TextureSource->LockMipInternal(BlockIndex, LayerIndex, MipIndex, LockState);
	if ( Locked )
	{	
		FTextureSourceBlock Block;
		TextureSource->GetBlock(BlockIndex, Block);
		check(MipIndex < Block.NumMips);

		Image.RawData = Locked;
		Image.SizeX = FMath::Max(Block.SizeX >> MipIndex, 1);
		Image.SizeY = FMath::Max(Block.SizeY >> MipIndex, 1);
		Image.NumSlices = TextureSource->GetMippedNumSlices(Block.NumSlices,MipIndex);
		Image.Format = FImageCoreUtils::ConvertToRawImageFormat(TextureSource->GetFormat(LayerIndex));
		Image.GammaSpace = TextureSource->GetGammaSpace(LayerIndex);
		
		const int64 MipSizeBytes = TextureSource->CalcMipSize(BlockIndex, LayerIndex, MipIndex);

		check( Image.GetImageSizeBytes() == MipSizeBytes );		
		check( IsValid() );
	}
	else
	{
		LockState = ELockState::None;
	}
}

FTextureSource::FMipLock::FMipLock(ELockState InLockState,FTextureSource * InTextureSource,int32 InMipIndex) :
	FMipLock(InLockState,InTextureSource,0,0,InMipIndex)
{
}

// move constructor :
FTextureSource::FMipLock::FMipLock(FMipLock&& RHS) :
	LockState(RHS.LockState),
	TextureSource(RHS.TextureSource),
	BlockIndex(RHS.BlockIndex),
	LayerIndex(RHS.LayerIndex),
	MipIndex(RHS.MipIndex),
	Image(RHS.Image)
{
	// blank out RHS so it doesn't try to unlock :
	RHS.LockState = ELockState::None;
	RHS.Image = FImageView();
	check( ! RHS.IsValid() );
}

FTextureSource::FMipLock::~FMipLock()
{
	if ( IsValid() )
	{
		TextureSource->UnlockMip(BlockIndex,LayerIndex,MipIndex);
		LockState = ELockState::None;
		Image.RawData = nullptr;
	}
}

const uint8* FTextureSource::LockMipReadOnly(int32 BlockIndex, int32 LayerIndex, int32 MipIndex)
{
	return LockMipInternal(BlockIndex, LayerIndex, MipIndex, ELockState::ReadOnly);
}

uint8* FTextureSource::LockMip(int32 BlockIndex, int32 LayerIndex, int32 MipIndex)
{
	return LockMipInternal(BlockIndex, LayerIndex, MipIndex, ELockState::ReadWrite);
}

uint8* FTextureSource::LockMipInternal(int32 BlockIndex, int32 LayerIndex, int32 MipIndex, ELockState RequestedLockState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSource::LockMip);

	checkf(RequestedLockState != ELockState::None, TEXT("Cannot call FTextureSource::LockMipInternal with a RequestedLockState of type ELockState::None"));

	uint8* MipData = nullptr;

	if (BlockIndex < GetNumBlocks() && LayerIndex < NumLayers && MipIndex < NumMips)
	{
		if (LockedMipData.IsNull())
		{
			checkf(NumLockedMips == 0, TEXT("Texture mips are locked but the LockedMipData is missing"));
			LockedMipData = Decompress(nullptr);
		}

		if (RequestedLockState == ELockState::ReadOnly)
		{
			MipData = const_cast<uint8*>(static_cast<const uint8*>(LockedMipData.GetDataReadOnly().GetData()));
		}
		else
		{
			MipData = LockedMipData.GetDataReadWrite();
		}

		if ( MipData == nullptr )
		{
			// no data, you did not get the lock, do not call Unlock
			return nullptr;
		}
		
		int64 MipOffset = CalcMipOffset(BlockIndex, LayerIndex, MipIndex);
		int64 MipSize = CalcMipSize(BlockIndex,LayerIndex,MipIndex);
		if ( MipOffset + MipSize > LockedMipData.GetSize() )
		{
			UE_LOG(LogTexture,Error,TEXT("Mip Data is too small : %lld < %lld+%lld"), LockedMipData.GetSize(),MipOffset,MipSize); 
			LockedMipData.Reset();
			return nullptr;
		}

		MipData += MipOffset;

		if (NumLockedMips == 0)
		{
			LockState = RequestedLockState;
		}
		else
		{
			checkf(LockState == RequestedLockState, TEXT("Cannot change the lock type until UnlockMip is called"));
		}

		++NumLockedMips;
	}

	return MipData;
}

void FTextureSource::UnlockMip(int32 BlockIndex, int32 LayerIndex, int32 MipIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSource::UnlockMip);

	check(BlockIndex < GetNumBlocks());
	check(LayerIndex < NumLayers);
	check(MipIndex < MAX_TEXTURE_MIP_COUNT);
	check(NumLockedMips > 0u);
	check(LockState != ELockState::None);

	--NumLockedMips;
	if (NumLockedMips == 0u)
	{
		// If the lock was for Read/Write then we need to assume that the decompressed copy
		// we returned (LockedMipData) was updated and should update the payload accordingly.
		// This will wipe the compression format that we used to have.
		if (LockState == ELockState::ReadWrite)
		{
			UE_CLOG(CompressionFormat == TSCF_JPEG, LogTexture, Warning, TEXT("Call to FTextureSource::UnlockMip will cause texture source to lose it's jpeg storage format"));

			BulkData.UpdatePayload(LockedMipData.Release(), Owner);
			BulkData.SetCompressionOptions(UE::Serialization::ECompressionOptions::Default);

			CompressionFormat = TSCF_None;

			// Need to unlock before calling UseHashAsGuid
			LockState = ELockState::None;
			UseHashAsGuid();
		}

		LockState = ELockState::None;
		LockedMipData.Reset();
	}
}

bool FTextureSource::GetMipImage(FImage & OutImage, int32 BlockIndex, int32 LayerIndex, int32 MipIndex)
{
	TArray64<uint8> MipData;
	if ( ! GetMipData(MipData,BlockIndex,LayerIndex,MipIndex) )
	{
		return false;
	}
	
	const int64 MipSizeBytes = CalcMipSize(BlockIndex, LayerIndex, MipIndex);
	check( MipData.Num() == MipSizeBytes );
	
	FTextureSourceBlock Block;
	GetBlock(BlockIndex, Block);
	check(MipIndex < Block.NumMips);

	OutImage.RawData = MoveTemp(MipData);
	OutImage.SizeX = FMath::Max(Block.SizeX >> MipIndex, 1);
	OutImage.SizeY = FMath::Max(Block.SizeY >> MipIndex, 1);
	OutImage.NumSlices = GetMippedNumSlices(Block.NumSlices,MipIndex);
	OutImage.Format = FImageCoreUtils::ConvertToRawImageFormat(GetFormat(LayerIndex));
	OutImage.GammaSpace = GetGammaSpace(LayerIndex);

	check( OutImage.GetImageSizeBytes() == MipSizeBytes );
	return true;
}

bool FTextureSource::GetMipData(TArray64<uint8>& OutMipData, int32 BlockIndex, int32 LayerIndex, int32 MipIndex, IImageWrapperModule* )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSource::GetMipData (TArray64));

	checkf(LockState == ELockState::None, TEXT("GetMipData (TArray64) shouldn't be called in-between LockMip/UnlockMip"));

	bool bSuccess = false;

	if (BlockIndex < GetNumBlocks() && LayerIndex < NumLayers && MipIndex < NumMips && HasPayloadData())
	{
#if WITH_EDITOR
		FWriteScopeLock BulkDataExclusiveScope(BulkDataLock.Get());
#endif

		checkf(NumLockedMips == 0, TEXT("Attempting to access a locked FTextureSource"));
		// LockedMipData should only be allocated if NumLockedMips > 0 so the following assert should have been caught
		// by the one above. If it fires then it indicates that there is a lock/unlock mismatch as well as invalid access!
		checkf(LockedMipData.IsNull(), TEXT("Attempting to access mip data while locked mip data is still allocated"));

		FSharedBuffer DecompressedData = Decompress();

		if (!DecompressedData.IsNull())
		{
			const int64 MipOffset = CalcMipOffset(BlockIndex, LayerIndex, MipIndex);
			const int64 MipSize = CalcMipSize(BlockIndex, LayerIndex, MipIndex);

			if ((int64)DecompressedData.GetSize() >= MipOffset + MipSize)
			{
				OutMipData.Empty(MipSize);
				OutMipData.AddUninitialized(MipSize);
				FMemory::Memcpy(
					OutMipData.GetData(),
					(const uint8*)DecompressedData.GetData() + MipOffset,
					MipSize
				);

				bSuccess = true;
			}
		}	
	}
	
	return bSuccess;
}

FTextureSource::FMipData FTextureSource::GetMipData(IImageWrapperModule* )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSource::GetMipData (FMipData));

	checkf(LockState == ELockState::None, TEXT("GetMipData (FMipData) shouldn't be called in-between LockMip/UnlockMip"));

	check(LockedMipData.IsNull());
	check(NumLockedMips == 0);

#if WITH_EDITOR
	FReadScopeLock _(BulkDataLock.Get());
#endif //WITH_EDITOR

	FSharedBuffer DecompressedData = Decompress();
	return FMipData(*this, DecompressedData);
}

int64 FTextureSource::CalcMipSize(int32 BlockIndex, int32 LayerIndex, int32 MipIndex) const
{
	FTextureSourceBlock Block;
	GetBlock(BlockIndex, Block);
	check(MipIndex < Block.NumMips);

	const int64 MipSizeX = FMath::Max(Block.SizeX >> MipIndex, 1);
	const int64 MipSizeY = FMath::Max(Block.SizeY >> MipIndex, 1);
	const int64 MipSlices = GetMippedNumSlices(Block.NumSlices,MipIndex);

	const int64 BytesPerPixel = GetBytesPerPixel(LayerIndex);
	return MipSizeX * MipSizeY * MipSlices * BytesPerPixel;
}

int32 FTextureSource::GetBytesPerPixel(int32 LayerIndex) const
{
	return GetBytesPerPixel(GetFormat(LayerIndex));
}

bool FTextureSource::IsPowerOfTwo(int32 BlockIndex) const
{
	FTextureSourceBlock Block;
	GetBlock(BlockIndex, Block);
	return FMath::IsPowerOfTwo(Block.SizeX) && FMath::IsPowerOfTwo(Block.SizeY);
}

bool FTextureSource::IsValid() const
{
	return SizeX > 0 && SizeY > 0 && NumSlices > 0 && NumLayers > 0 && NumMips > 0 &&
		Format != TSF_Invalid && HasPayloadData();
}

void FTextureSource::GetBlock(int32 Index, FTextureSourceBlock& OutBlock) const
{
	check(Index < GetNumBlocks());
	if (Index == 0)
	{
		OutBlock.BlockX = BaseBlockX;
		OutBlock.BlockY = BaseBlockY;
		OutBlock.SizeX = SizeX;
		OutBlock.SizeY = SizeY;
		OutBlock.NumSlices = NumSlices;
		OutBlock.NumMips = NumMips;
	}
	else
	{
		OutBlock = Blocks[Index - 1];
	}
}

FIntPoint FTextureSource::GetLogicalSize() const
{
	const int32 NumBlocks = GetNumBlocks();
	int32 SizeInBlocksX = 0;
	int32 SizeInBlocksY = 0;
	int32 BlockSizeX = 0;
	int32 BlockSizeY = 0;
	for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
	{
		FTextureSourceBlock SourceBlock;
		GetBlock(BlockIndex, SourceBlock);
		SizeInBlocksX = FMath::Max(SizeInBlocksX, SourceBlock.BlockX + 1);
		SizeInBlocksY = FMath::Max(SizeInBlocksY, SourceBlock.BlockY + 1);
		BlockSizeX = FMath::Max(BlockSizeX, SourceBlock.SizeX);
		BlockSizeY = FMath::Max(BlockSizeY, SourceBlock.SizeY);
	}
	return FIntPoint(SizeInBlocksX * BlockSizeX, SizeInBlocksY * BlockSizeY);
}

FIntPoint FTextureSource::GetSizeInBlocks() const
{
	const int32 NumBlocks = GetNumBlocks();
	int32 SizeInBlocksX = 0;
	int32 SizeInBlocksY = 0;
	for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
	{
		FTextureSourceBlock SourceBlock;
		GetBlock(BlockIndex, SourceBlock);
		SizeInBlocksX = FMath::Max(SizeInBlocksX, SourceBlock.BlockX + 1);
		SizeInBlocksY = FMath::Max(SizeInBlocksY, SourceBlock.BlockY + 1);
	}
	return FIntPoint(SizeInBlocksX, SizeInBlocksY);
}

FString FTextureSource::GetIdString() const
{
	FString GuidString = GetId().ToString();
	if (bGuidIsHash)
	{
		GuidString += TEXT("X");
	}
	return GuidString;
}

int FTextureSource::GetMippedNumSlices(int InNumSlices,int InMipIndex) const
{
	// old behavior was to not mip down NumSlices in TextureSource for volume SizeZ
	//return InNumSlices;

	// what to do with NumSlices on the mip ?
	// if this is an Array, it should stay the same
	// if this is a Volume, it should mip down

	// TextureSource does not know if it's a volume or not
	// need to check type of Owner Texture

	check( InNumSlices > 0 );
	// fast path short cut : 1 slice is always 1 slice
	if ( InNumSlices == 1 )
	{
		return 1;
	}

	if ( IsVolume() )
	{
		return FMath::Max(InNumSlices >> InMipIndex, 1);
	}
	else
	{
		return InNumSlices;
	}	
}

ETextureClass FTextureSource::GetTextureClass() const
{
	// TextureSource does not know its own class, but its owning Texture does :
	if ( Owner != nullptr )
	{
		return Owner->GetTextureClass();
	}
	else
	{
		// torn off, should have saved TornOffTextureClass
		check( TornOffTextureClass != ETextureClass::Invalid );
		return TornOffTextureClass;
	}
}

EGammaSpace FTextureSource::GetGammaSpace(int LayerIndex) const
{
	// Texture->GetGammaSpace does not validate against format, but I do? a bit weird, fix that
	if ( ! ERawImageFormat::GetFormatNeedsGammaSpace( FImageCoreUtils::ConvertToRawImageFormat(GetFormat(LayerIndex)) ) )
	{
		return EGammaSpace::Linear;
	}

	// TextureSource does not know its own gamma, but its owning Texture does :
	if ( Owner != nullptr )
	{
		check( &(Owner->Source) == this );

		// same as Owner->GetGammaSpace , but uses LayerFormatSettings for SRGB flag
		FTextureFormatSettings FormatSettings;
		Owner->GetLayerFormatSettings(LayerIndex, FormatSettings);

		EGammaSpace GammaSpace = FormatSettings.SRGB ? ( Owner->bUseLegacyGamma ? EGammaSpace::Pow22 : EGammaSpace::sRGB ) : EGammaSpace::Linear;
		return GammaSpace;
	}
	else
	{
		// torn off, should have saved TornOffGammaSpace
		check( TornOffGammaSpace != EGammaSpace::Invalid );
		return TornOffGammaSpace;
	}
}

FString FTextureSource::GetSourceCompressionAsString() const
{
	return StaticEnum<ETextureSourceCompressionFormat>()->GetDisplayNameTextByValue(GetSourceCompression()).ToString();
}

FSharedBuffer FTextureSource::TryDecompressData() const
{
	if (NumLayers == 1 && NumSlices == 1 && Blocks.Num() == 0)
	{
		FSharedBuffer Payload = BulkData.GetPayload().Get();

		FImage Image;
		if ( ! FImageUtils::DecompressImage(Payload.GetData(), Payload.GetSize(), Image) )
		{
			int64 LayerSize = CalcLayerSize(0,0);
			int64 PayloadSize = Payload.GetSize();
			
			UE_LOG(LogTexture, Display, TEXT("TryDecompressData failed: LayerSize = %lld PayloadSize = %lld"),LayerSize,PayloadSize);
			UE_LOG(LogTexture, Display, TEXT("TryDecompressData failed: LoadedMainStreamObjectVersion = %d, TextureSourceVirtualization = %d, VolumetricCloudReflectionSampleCountDefaultUpdate = %d"),
				Owner ? Owner->LoadedMainStreamObjectVersion : 0,
				FUE5MainStreamObjectVersion::TextureSourceVirtualization,
				FUE5MainStreamObjectVersion::VolumetricCloudReflectionSampleCountDefaultUpdate);

			if ( LayerSize == PayloadSize )
			{
				// this is most likely from the bug where data is marked TSCF_PNG but is actually uncompressed
				// fix CompressionFormat for the future :
				check( CompressionFormat == TSCF_PNG );
				const_cast<FTextureSource *>(this)->CompressionFormat = TSCF_None;

				UE_LOG(LogTexture, Warning, TEXT("TryDecompressData data marked compressed appears to be uncompressed?"));
				return Payload;
			}
			else
			{
				UE_LOG(LogTexture, Error, TEXT("TryDecompressData failed to return uncompressed data"));
				return FSharedBuffer();
			}
		}

		// we got data in Image.Format
		// we expect data in TSF "Format"
		ERawImageFormat::Type RawFormat = FImageCoreUtils::ConvertToRawImageFormat(Format);

		if ( Image.Format != RawFormat )
		{
			// this shouldn't ever happen currently
			UE_LOG(LogTexture, Warning, TEXT("TryDecompressData unexpected format conversion?"));

			Image.ChangeFormat(RawFormat, GetGammaSpace(0));
		}
			
		if ( CompressionFormat == TSCF_PNG && Format == TSF_BGRA8 )
		{
			// Legacy bug, must be matched in Compress & Decompress
			// see FTextureSource::Compress
			// TODO: TSF_BGRA8 is stored as RGBA, so the R and B channels are swapped in the internal png. Should we fix this?
			FImageCore::TransposeImageRGBABGRA(Image);
		}

		return MakeSharedBufferFromArray(MoveTemp(Image.RawData));
	}
	else
	{
		UE_LOG(LogTexture, Warning, TEXT("Compressed source art is in an invalid format NumLayers:(%d) NumSlices:(%d) NumBlocks:(%d)"),
			NumLayers, NumSlices, Blocks.Num());
		return FSharedBuffer();
	}	
}

void FTextureSource::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	check(LockState == ELockState::None);

	FSharedBuffer Payload = BulkData.GetPayload().Get();
	uint64 PayloadSize = Payload.GetSize();

	Out.Logf(TEXT("%sCustomProperties TextureSourceData "), FCString::Spc(Indent));

	Out.Logf(TEXT("PayloadSize=%llu "), PayloadSize);
	TArrayView<uint8> Buffer((uint8*)Payload.GetData(), PayloadSize);
	for (uint8 Element : Buffer)
	{
		Out.Logf(TEXT("%x "), Element);
	}
}

void FTextureSource::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	check(LockState == ELockState::None);

	if (FParse::Command(&SourceText, TEXT("TextureSourceData")))
	{
		uint64 PayloadSize = 0;
		if (FParse::Value(SourceText, TEXT("PayloadSize="), PayloadSize))
		{
			while (*SourceText && (!FChar::IsWhitespace(*SourceText)))
			{
				++SourceText;
			}
			FParse::Next(&SourceText);
		}

		bool bSuccess = true;
		if (PayloadSize > (uint64)0)
		{
			TCHAR* StopStr;
			FUniqueBuffer Buffer = FUniqueBuffer::Alloc(PayloadSize);
			uint8* DestData = (uint8*)Buffer.GetData();
			if (DestData)
			{
				uint64 Index = 0;
				while (FChar::IsHexDigit(*SourceText))
				{
					if (Index < PayloadSize)
					{
						DestData[Index++] = (uint8)FCString::Strtoi(SourceText, &StopStr, 16);
						while (FChar::IsHexDigit(*SourceText))
						{
							SourceText++;
						}
					}
					FParse::Next(&SourceText);
				}

				if (Index != PayloadSize)
				{
					Warn->Log(*NSLOCTEXT("UnrealEd", "Importing_TextureSource_SyntaxError", "Syntax Error").ToString());
					bSuccess = false;
				}
			}
			else
			{
				Warn->Log(*NSLOCTEXT("UnrealEd", "Importing_TextureSource_BulkDataAllocFailure", "Couldn't allocate bulk data").ToString());
				bSuccess = false;
			}
			
			if (bSuccess)
			{
				BulkData.UpdatePayload(Buffer.MoveToShared(), Owner);
			}
		}

		if (bSuccess)
		{
			if (!bGuidIsHash)
			{
				ForceGenerateGuid();
			}
		}
		else
		{
			BulkData.Reset();
		}
	}
	else
	{
		Warn->Log(*NSLOCTEXT("UnrealEd", "Importing_TextureSource_MissingTextureSourceDataCommand", "Missing TextureSourceData tag from import text.").ToString());
	}
}

bool FTextureSource::CanPNGCompress() const
{
	bool bCanPngCompressFormat = (Format == TSF_G8 || Format == TSF_G16 || Format == TSF_BGRA8 || Format == TSF_RGBA16 );

	if (
		NumLayers == 1 &&
		NumMips == 1 &&
		NumSlices == 1 &&
		Blocks.Num() == 0 &&
		SizeX > 4 &&
		SizeY > 4 &&
		HasPayloadData() &&
		bCanPngCompressFormat &&
		CompressionFormat == TSCF_None)
	{
		return true;
	}
	return false;
}

void FTextureSource::ForceGenerateGuid()
{
	Id = FGuid::NewGuid();
	bGuidIsHash = false;
}

void FTextureSource::ReleaseSourceMemory()
{
	bHasHadBulkDataCleared = true;
	BulkData.UnloadData();
}

void FTextureSource::RemoveSourceData()
{
	SizeX = 0;
	SizeY = 0;
	NumSlices = 0;
	NumLayers = 0;
	NumMips = 0;
	Format = TSF_Invalid;
	LayerFormat.Empty();
	Blocks.Empty();
	BlockDataOffsets.Empty();
	CompressionFormat = TSCF_None;
	LockedMipData.Reset();
	NumLockedMips = 0u;
	LockState = ELockState::None;
	
	BulkData.UnloadData();

	ForceGenerateGuid();
}

int64 FTextureSource::CalcTotalSize() const
{
	int NumBlocks = GetNumBlocks();
	int64 TotalBytes = 0;
	for (int i = 0; i < NumBlocks; ++i)
	{
		TotalBytes += CalcBlockSize(i);
	}
	return TotalBytes;
}

int64 FTextureSource::CalcBlockSize(int32 BlockIndex) const
{
	FTextureSourceBlock Block;
	GetBlock(BlockIndex, Block);
	return CalcBlockSize(Block);
}

int64 FTextureSource::CalcLayerSize(int32 BlockIndex, int32 LayerIndex) const
{
	FTextureSourceBlock Block;
	GetBlock(BlockIndex, Block);
	return CalcLayerSize(Block, LayerIndex);
}

int64 FTextureSource::CalcBlockSize(const FTextureSourceBlock& Block) const
{
	int64 TotalSize = 0;
	for (int32 LayerIndex = 0; LayerIndex < GetNumLayers(); ++LayerIndex)
	{
		TotalSize += CalcLayerSize(Block, LayerIndex);
	}
	return TotalSize;
}

int64 FTextureSource::CalcLayerSize(const FTextureSourceBlock& Block, int32 LayerIndex) const
{
	int64 BytesPerPixel = GetBytesPerPixel(LayerIndex);

	// This is used for memory allocation, so use FCheckedInt to rigorously check against overflow issues.
	FGuardedInt64 TotalSize(0);
	for (int32 MipIndex = 0; MipIndex < Block.NumMips; ++MipIndex)
	{
		int32 MipSizeX = FMath::Max<int32>(Block.SizeX >> MipIndex, 1);
		int32 MipSizeY = FMath::Max<int32>(Block.SizeY >> MipIndex, 1);
		int32 MipSizeZ = GetMippedNumSlices(Block.NumSlices,MipIndex);

		TotalSize += FGuardedInt64(MipSizeX) * MipSizeY * MipSizeZ * BytesPerPixel;
	}

	checkf(TotalSize.IsValid(), TEXT("Invalid (overflowing) mip sizes made it in to FTextureSource::CalcLayerSize! Check import locations for mip size validation"));
	return TotalSize.Get(0);
}

int64 FTextureSource::CalcMipOffset(int32 BlockIndex, int32 LayerIndex, int32 OffsetToMipIndex) const
{
	FTextureSourceBlock Block;
	GetBlock(BlockIndex, Block);
	check(OffsetToMipIndex < Block.NumMips);

	// This is used for memory indexing, so use FCheckedInt to rigorously check against overflow issues.
	FGuardedInt64 MipOffset(BlockDataOffsets[BlockIndex]);

	// Skip over the initial layers within the tile
	for (int i = 0; i < LayerIndex; ++i)
	{
		MipOffset += CalcLayerSize(Block, i);
	}

	int64 BytesPerPixel = GetBytesPerPixel(LayerIndex);

	for (int32 MipIndex = 0; MipIndex < OffsetToMipIndex; ++MipIndex)
	{
		int32 MipSizeX = FMath::Max<int32>(Block.SizeX >> MipIndex, 1);
		int32 MipSizeY = FMath::Max<int32>(Block.SizeY >> MipIndex, 1);
		int32 MipSizeZ = GetMippedNumSlices(Block.NumSlices,MipIndex);

		MipOffset += FGuardedInt64(MipSizeX) * MipSizeY * MipSizeZ * BytesPerPixel;
	}

	checkf(MipOffset.IsValid(), TEXT("Invalid (overflowing) mip sizes made it in to FTextureSource::CalcMipOffset! Check import locations for mip size validation"));
	return MipOffset.Get(0);
}

void FTextureSource::UseHashAsGuid()
{
	if (HasPayloadData())
	{
		checkf(LockState == ELockState::None, TEXT("UseHashAsGuid shouldn't be called in-between LockMip/UnlockMip"));

		bGuidIsHash = true;
		Id = UE::Serialization::IoHashToGuid(BulkData.GetPayloadId());
	}
	else
	{
		Id.Invalidate();
	}
}

template <typename ArrayType>
static SIZE_T ArraySizeBytes(const ArrayType & Array)
{
	return (SIZE_T)( Array.Num() * Array.GetTypeSize() );
}

FGuid FTextureSource::GetId() const
{
	if (!bGuidIsHash)
	{
		return Id;
	}
	
	UE::DerivedData::FBuildVersionBuilder IdBuilder;
	IdBuilder << BaseBlockX;
	IdBuilder << BaseBlockY;
	IdBuilder << SizeX;
	IdBuilder << SizeY;	
	IdBuilder << NumSlices;
	IdBuilder << NumMips;
	IdBuilder << NumLayers;	
	IdBuilder << bLongLatCubemap;
	IdBuilder << CompressionFormat;
	IdBuilder << bGuidIsHash; // always true here
	IdBuilder << static_cast<uint8>(Format.GetValue());
	
	if ( GetNumLayers() == 1 && GetNumBlocks() == 1 )
	{
		// preserve broken code for common case so Ids don't change :
		// was serializing using array Num (element count) instead of byte count
		// the broken serialize here only takes 1 byte from these arrays
		// but that's benign because they don't really need to be hashed anyway (they are redundant in this case)
		
		IdBuilder.Serialize((void *)LayerFormat.GetData(), LayerFormat.Num());
		IdBuilder.Serialize((void *)Blocks.GetData(), Blocks.Num());
		IdBuilder.Serialize((void *)BlockDataOffsets.GetData(), BlockDataOffsets.Num());
	}
	else
	{
		// better version :

		if ( GetNumLayers() > 1 )
		{
			IdBuilder.Serialize((void *)LayerFormat.GetData(), ArraySizeBytes(LayerFormat));
		}
		if ( GetNumBlocks() > 1 )
		{
			IdBuilder.Serialize((void *)Blocks.GetData(), ArraySizeBytes(Blocks));
			IdBuilder.Serialize((void *)BlockDataOffsets.GetData(), ArraySizeBytes(BlockDataOffsets));
		}
	}

	IdBuilder << const_cast<FGuid&>(Id);

	return IdBuilder.Build();
}

void FTextureSource::OperateOnLoadedBulkData(TFunctionRef<void(const FSharedBuffer& BulkDataBuffer)> Operation)
{
	checkf(LockState == ELockState::None, TEXT("OperateOnLoadedBulkData shouldn't be called in-between LockMip/UnlockMip"));

#if WITH_EDITOR
	FReadScopeLock BulkDataExclusiveScope(BulkDataLock.Get());
#endif

	FSharedBuffer Payload = BulkData.GetPayload().Get();
	Operation(Payload);
}

void FTextureSource::SetId(const FGuid& InId, bool bInGuidIsHash)
{
	Id = InId;
	bGuidIsHash = bInGuidIsHash;
}

// GetMaximumDimensionOfNonVT is static
// not for current texture type, not for current RHI

int32 UTexture::GetMaximumDimensionOfNonVT()
{
	// 16384 limit ; larger must be VT
	check( MAX_TEXTURE_MIP_COUNT == 15 );
	// GMaxTextureMipCount is for the current RHI and GMaxTextureMipCount <= MAX_TEXTURE_MIP_COUNT
	return 16384;
}

// GetMaximumDimension is for current texture type (cube/2d/vol)
// and on the current RHI
uint32 UTexture::GetMaximumDimension() const
{
	// just assume anyone who doesn't implement this virtual is 2d
	return GetMax2DTextureDimension();
}

void UTexture::GetDefaultFormatSettings(FTextureFormatSettings& OutSettings) const
{
	OutSettings.CompressionSettings = CompressionSettings;
	OutSettings.CompressionNone = CompressionNone;
	OutSettings.CompressionNoAlpha = CompressionNoAlpha;
	OutSettings.CompressionForceAlpha = CompressionForceAlpha;
	OutSettings.CompressionYCoCg = CompressionYCoCg;
	OutSettings.SRGB = SRGB;
}

void UTexture::GetLayerFormatSettings(int32 LayerIndex, FTextureFormatSettings& OutSettings) const
{
#if WITH_EDITORONLY_DATA
	check( Source.Owner == this );
#endif

	check(LayerIndex >= 0);
	if (LayerIndex < LayerFormatSettings.Num())
	{
		OutSettings = LayerFormatSettings[LayerIndex];
	}
	else
	{
		GetDefaultFormatSettings(OutSettings);
	}
}

void UTexture::SetLayerFormatSettings(int32 LayerIndex, const FTextureFormatSettings& InSettings)
{
	check(LayerIndex >= 0);
	if (LayerIndex == 0 && LayerFormatSettings.Num() == 0)
	{
		// Apply layer0 settings directly to texture properties
		CompressionSettings = InSettings.CompressionSettings;
		CompressionNone = InSettings.CompressionNone;
		CompressionNoAlpha = InSettings.CompressionNoAlpha;
		CompressionForceAlpha = InSettings.CompressionForceAlpha;
		CompressionYCoCg = InSettings.CompressionYCoCg;
		SRGB = InSettings.SRGB;
	}
	else
	{
		if (LayerIndex >= LayerFormatSettings.Num())
		{
			FTextureFormatSettings DefaultSettings;
			GetDefaultFormatSettings(DefaultSettings);
			LayerFormatSettings.Reserve(LayerIndex + 1);
			while (LayerIndex >= LayerFormatSettings.Num())
			{
				LayerFormatSettings.Add(DefaultSettings);
			}
		}
		LayerFormatSettings[LayerIndex] = InSettings;

		// @todo Oodle : inconsistency in SetLayerFormatSettings(0) and possible bug?
		// should SetLayerFormatSettings(0,Settings) always set the base Texture properties?
		// if you call this when you have a LayerFormatSettings[] array, it does not
		// if you query via GetLayerFormatSettings(0) then these settings are seen
		//	but you just get them directly from the texture they are not!
	}
}

int64 UTexture::GetBuildRequiredMemory() const
{
	// DEPRECATED use GetBuildRequiredMemoryEstimate

	return -1; /* Unknown */
}

#endif // #if WITH_EDITOR

static FName GetLatestOodleTextureSdkVersion()
{

#if WITH_EDITOR
	// don't use AlternateTextureCompression pref
	//	just explicitly ask for new Oodle
	// in theory we could look for a "TextureCompressionFormatWithVersion" setting
	//	but to do that we need a target platform, since it could defer by target and not be set for current at all
	// and here we need something global, not per-target
	const TCHAR * TextureCompressionFormat = TEXT("TextureFormatOodle");

	ITextureFormatModule * TextureFormatModule = FModuleManager::LoadModulePtr<ITextureFormatModule>(TextureCompressionFormat);

	// TextureFormatModule can be null if TextureFormatOodle is disabled in this project
	//  then we will return None, which is correct

	if ( TextureFormatModule )
	{
		ITextureFormat* TextureFormat = TextureFormatModule->GetTextureFormat();
					
		if ( TextureFormat )
		{	
		FName LatestSdkVersion = TextureFormat->GetLatestSdkVersion();
			
		return LatestSdkVersion;
	}
	}
#endif

	return NAME_None;
}

static FName CachedGetLatestOodleSdkVersion()
{
	static struct DoOnceGetLatestSdkVersion
	{
		FName Value;

		DoOnceGetLatestSdkVersion() : Value( GetLatestOodleTextureSdkVersion() )
		{
		}
	} Once;

	return Once.Value;
}

static FName ConditionalGetPrefixedFormat(FName TextureFormatName, const ITargetPlatform* TargetPlatform, bool bOodleTextureSdkVersionIsNone)
{
#if WITH_EDITOR

	// "TextureCompressionFormat" specifies the Oodle Texture plugin to use for textures with OodleTextureSdkVersion == None
	//		versioned textures always use TFO
	//		TextureCompressionFormat can specify a pre-TFO plugin if desired
	// 
	// if you want Oodle Texture encoding,
	// TextureCompressionFormat is required, TextureCompressionFormatWithVersion is optional

	FString TextureCompressionFormat;
	bool bHasFormat = TargetPlatform->GetConfigSystem()->GetString(TEXT("AlternateTextureCompression"), TEXT("TextureCompressionFormat"), TextureCompressionFormat, GEngineIni);
	bHasFormat = bHasFormat && ! TextureCompressionFormat.IsEmpty();
	
	if ( bHasFormat )
	{
		if ( ! bOodleTextureSdkVersionIsNone )
		{
			//	new (optional) pref : TextureCompressionFormatWithVersion
			FString TextureCompressionFormatWithVersion;
			bool bHasFormatWithVersion = TargetPlatform->GetConfigSystem()->GetString(TEXT("AlternateTextureCompression"), TEXT("TextureCompressionFormatWithVersion"), TextureCompressionFormatWithVersion, GEngineIni);
			bHasFormatWithVersion = bHasFormatWithVersion && ! TextureCompressionFormatWithVersion.IsEmpty();
			if ( bHasFormatWithVersion )
			{
				TextureCompressionFormat = TextureCompressionFormatWithVersion;
			}
			else
			{
				// if TextureCompressionFormatWithVersion is not set,
				// TextureCompressionFormatWithVersion is automatically set to "TextureFormatOodle"
				// new textures with version field will use TFO (if "TextureCompressionFormat" field exists)

				TextureCompressionFormat = TEXT("TextureFormatOodle");

				UE_CALL_ONCE( [&](){
					UE_LOG(LogTexture, Verbose, TEXT("AlternateTextureCompression/TextureCompressionFormatWithVersion not specified, using %s."), *TextureCompressionFormat);
				} );
			}
		}

		ITextureFormatModule * TextureFormatModule = FModuleManager::LoadModulePtr<ITextureFormatModule>(*TextureCompressionFormat);

		if ( TextureFormatModule )
		{
			ITextureFormat* TextureFormat = TextureFormatModule->GetTextureFormat();
					
			if ( TextureFormat )
			{
			FString FormatPrefix = TextureFormat->GetAlternateTextureFormatPrefix();
			check( ! FormatPrefix.IsEmpty() );
			
			FName NewFormatName(FormatPrefix + TextureFormatName.ToString());

			TArray<FName> SupportedFormats;
			TextureFormat->GetSupportedFormats(SupportedFormats);

			if (SupportedFormats.Contains(NewFormatName))
			{
				return NewFormatName;
			}
		}
		else
		{
			UE_CALL_ONCE( [&](){
					UE_LOG(LogTexture, Warning, TEXT("AlternateTextureCompression specified, Module found, but no TextureFormat : %s."), *TextureCompressionFormat);
				} );
			}
		}
		else
		{
			UE_CALL_ONCE( [&](){
				UE_LOG(LogTexture, Warning, TEXT("AlternateTextureCompression specified but Module not found: %s."), *TextureCompressionFormat);
			} );
		}
	}
#endif

	return TextureFormatName;
}

FName GetDefaultTextureFormatName( const ITargetPlatform* TargetPlatform, const UTexture* Texture, int32 LayerIndex, 
	bool bSupportCompressedVolumeTexture, int32 Unused_BlockSize, bool bSupportFilteredFloat32Textures )
{
	FName TextureFormatName = NAME_None;
	bool bOodleTextureSdkVersionIsNone = true;

	/**
	 * IF you add a format to this function don't forget to update GetAllDefaultTextureFormats 
	 */

#if WITH_EDITOR
	// Supported texture format names.
	static FName NameDXT1(TEXT("DXT1"));
	static FName NameDXT5(TEXT("DXT5"));
	static FName NameDXT5n(TEXT("DXT5n"));
	static FName NameAutoDXT(TEXT("AutoDXT"));
	static FName NameBC4(TEXT("BC4"));
	static FName NameBC5(TEXT("BC5"));
	static FName NameBGRA8(TEXT("BGRA8"));
	static FName NameXGXR8(TEXT("XGXR8"));
	static FName NameG8(TEXT("G8"));
	static FName NameG16(TEXT("G16"));
	static FName NameVU8(TEXT("VU8"));
	static FName NameRGBA16F(TEXT("RGBA16F"));
	static FName NameRGBA32F(TEXT("RGBA32F"));
	static FName NameR16F(TEXT("R16F"));
	static FName NameR32F(TEXT("R32F"));
	static FName NameBC6H(TEXT("BC6H"));
	static FName NameBC7(TEXT("BC7"));
	static FName NameR5G6B5(TEXT("R5G6B5"));
	static FName NameA1RGB555(TEXT("A1RGB555"));
	

	check(TargetPlatform);

	static const auto CVarVirtualTexturesEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextures")); check(CVarVirtualTexturesEnabled);
	const bool bVirtualTextureStreaming = CVarVirtualTexturesEnabled->GetValueOnAnyThread() && TargetPlatform->SupportsFeature(ETargetPlatformFeatures::VirtualTextureStreaming) && Texture->VirtualTextureStreaming;

	FTextureFormatSettings FormatSettings;
	Texture->GetLayerFormatSettings(LayerIndex, FormatSettings);

	const ETextureSourceFormat SourceFormat = Texture->Source.GetFormat(LayerIndex);

	// output format is primarily determined from the CompressionSettings (TC)

	// see if compression needs to be forced off even if requested :
	bool bNoCompression = FormatSettings.CompressionNone				// Code wants the texture uncompressed.
		|| (TargetPlatform->HasEditorOnlyData() && Texture->DeferCompression)	// The user wishes to defer compression, this is ok for the Editor only.
		|| (FormatSettings.CompressionSettings == TC_EditorIcon) // TC_EditorIcon is "UserInterface2D"
		|| (Texture->LODGroup == TEXTUREGROUP_ColorLookupTable)	// Textures in certain LOD groups should remain uncompressed.
		|| (Texture->LODGroup == TEXTUREGROUP_Bokeh)
		|| (Texture->LODGroup == TEXTUREGROUP_IESLightProfile)
		|| ((Texture->GetMaterialType() == MCT_VolumeTexture) && !bSupportCompressedVolumeTexture)
		|| FormatSettings.CompressionSettings == TC_EncodedReflectionCapture;

	if (!bNoCompression)
	{
		int32 SizeX = Texture->Source.GetSizeX();
		int32 SizeY = Texture->Source.GetSizeY();

		if (Texture->PowerOfTwoMode == ETexturePowerOfTwoSetting::PadToPowerOfTwo || Texture->PowerOfTwoMode == ETexturePowerOfTwoSetting::PadToSquarePowerOfTwo)
		{
			SizeX = FMath::RoundUpToPowerOfTwo(SizeX);
			SizeY = FMath::RoundUpToPowerOfTwo(SizeY);

			if (Texture->PowerOfTwoMode == ETexturePowerOfTwoSetting::PadToSquarePowerOfTwo)
			{
				SizeX = SizeY = FMath::Max(SizeX, SizeY);
			}
		}
		else
		{
			checkf(Texture->PowerOfTwoMode == ETexturePowerOfTwoSetting::None, TEXT("Unknown entry in ETexturePowerOfTwoSetting::Type"));
		}

		if (Texture->Source.IsLongLatCubemap())
		{
			// this should be kept in sync with ComputeLongLatCubemapExtents()
			SizeX = SizeY = FMath::Max(1 << FMath::FloorLog2(SizeX / 2), 32);
		}

		//we need to really have the actual top mip size of output platformdata
		//	(hence the LODBias check below)
		// trying to reproduce here exactly what TextureCompressor + serialization will do = brittle

		if ( Texture->MaxTextureSize != 0 )
		{
			while( SizeX > Texture->MaxTextureSize || SizeY > Texture->MaxTextureSize )
			{
				SizeX = FMath::Max(SizeX>>1,1);
				SizeY = FMath::Max(SizeY>>1,1);
			}
		}

#if WITH_EDITORONLY_DATA
		const UTextureLODSettings& LODSettings = TargetPlatform->GetTextureLODSettings();
 		const uint32 LODBiasNoCinematics = FMath::Max<int32>(LODSettings.CalculateLODBias(SizeX, SizeY, Texture->MaxTextureSize, Texture->LODGroup, Texture->LODBias, 0, Texture->MipGenSettings, bVirtualTextureStreaming), 0);
		SizeX = FMath::Max<int32>(SizeX >> LODBiasNoCinematics, 1);
		SizeY = FMath::Max<int32>(SizeY >> LODBiasNoCinematics, 1);
#endif
		// Don't compress textures smaller than the DXT block size.
		// Also force uncompressed if size of top mip is not a multiple of 4
		// note that even if top mip is a multiple of 4, lower may not be
		// we can only choose compression if it's supported by all platforms/RHI's (else check TargetPlatform->SupportsFeature)
		// note: does not use the passed-in "BlockSize" parameter, hard coded to 4
		//	 that is correct because ASTC does not require block alignment, only DXTC does which is always a 4-size block
		if ( (SizeX < 4) || (SizeY < 4) || (SizeX % 4 != 0) || (SizeY % 4 != 0) )
		{
			bNoCompression = true;
		}
	}

	bool bUseDXT5NormalMap = false;

	FString UseDXT5NormalMapsString;

	if (TargetPlatform->GetConfigSystem()->GetString(TEXT("SystemSettings"), TEXT("Compat.UseDXT5NormalMaps"), UseDXT5NormalMapsString, GEngineIni))
	{
		bUseDXT5NormalMap = FCString::ToBool(*UseDXT5NormalMapsString);
	}

	// Determine the pixel format of the (un/)compressed texture

	if (FormatSettings.CompressionSettings == TC_LQ)
	{
		bool bLQCompressionSupported = TargetPlatform->SupportsLQCompressionTextureFormat();
		if(bLQCompressionSupported)
		{
			TextureFormatName = FormatSettings.CompressionNoAlpha ? NameR5G6B5 : NameA1RGB555;
		}
		else
		{
			TextureFormatName = FormatSettings.CompressionNoAlpha ? NameDXT1 : NameDXT5;
		}
	}
	else if (FormatSettings.CompressionSettings == TC_HDR)
	{
		TextureFormatName = NameRGBA16F;
	}
	else if (FormatSettings.CompressionSettings == TC_HDR_F32)
	{
		TextureFormatName = NameRGBA32F;
	}
	else if (FormatSettings.CompressionSettings == TC_Normalmap)
	{
		TextureFormatName = bUseDXT5NormalMap ? NameDXT5n : NameBC5;
	}
	else if (FormatSettings.CompressionSettings == TC_VectorDisplacementmap)
	{
		TextureFormatName = NameBGRA8;
	}
	else if (FormatSettings.CompressionSettings == TC_Grayscale || 
		FormatSettings.CompressionSettings == TC_Displacementmap)
	{
		// Grayscale is G8 output, unless source is specifically G16
		//	(eg. RGBA16 source still uses G8 output, not G16)
		if (SourceFormat == TSF_G16)
		{
			TextureFormatName = NameG16;
		}
		else
		{
			TextureFormatName = NameG8;
		}

		/*
		// @todo Oodle: consider alternatively, use G16 for all 16-bit and floating point sources
		if (SourceFormat == TSF_G8 || SourceFormat == TSF_BGRA8)
		{
			TextureFormatName = NameG8;
		}
		else
		{
			// 16 bit or float sources
			TextureFormatName = NameG16;
		}
		*/
	}
	else if ( FormatSettings.CompressionSettings == TC_Alpha)
	{
		TextureFormatName = NameBC4;
	}
	else if (FormatSettings.CompressionSettings == TC_DistanceFieldFont)
	{
		TextureFormatName = NameG8;
	}
	else if ( FormatSettings.CompressionSettings == TC_HDR_Compressed )
	{
		TextureFormatName = NameBC6H;
	}
	else if ( FormatSettings.CompressionSettings == TC_BC7 )
	{
		TextureFormatName = NameBC7;
	}
	else if (FormatSettings.CompressionSettings == TC_HalfFloat)
	{
		TextureFormatName = NameR16F;
	}
	else if (FormatSettings.CompressionSettings == TC_SingleFloat)
	{
		TextureFormatName = NameR32F;
	}
	else if ( FormatSettings.CompressionSettings == TC_Default ||
			FormatSettings.CompressionSettings == TC_Masks )
	{
		if (FormatSettings.CompressionNoAlpha)
		{
			// CompressionNoAlpha changes AutoDXT to DXT1 early
			//	this is unnecessary/redundant, I believe
			//  the later handling of AutoDXT would make this same mapping
			TextureFormatName = NameDXT1;
		}
		else
		{
			// CompressionForceAlpha is applied later, where the bHasAlpha/DetectAlpha check is done and AutoDXT is resolved
			// alternatively it could be mapped immediately to NameDXT5 here.

			TextureFormatName = NameAutoDXT;
		}
	}
	else
	{
		// un-handled CompressionSettings cases will have TextureFormatName == none and go into the bNoCompression branch below
		// alternatively, should TC_EditorIcon be an explicit branch rather than relying on bNoCompression?
		check( TextureFormatName == NAME_None );
	}

	bool bTextureFormatNameIsCompressed =
		(TextureFormatName == NameDXT1) ||
		(TextureFormatName == NameAutoDXT) ||
		(TextureFormatName == NameDXT5) ||
		(TextureFormatName == NameDXT5n) ||
		(TextureFormatName == NameBC4) ||
		(TextureFormatName == NameBC5) ||
		(TextureFormatName == NameBC6H) ||
		(TextureFormatName == NameBC7);
	
	// if !bTextureFormatNameIsCompressed , we already picked an uncompressed format from TC, leave it alone
	if ( (bNoCompression && bTextureFormatNameIsCompressed) || TextureFormatName == NAME_None )
	{
		// TC_EditorIcon & TC_EncodedReflectionCapture weren't handled in the CompressionSettings branches above
		//	so will have FormatName == None and come in here
		
		if (FormatSettings.CompressionSettings == TC_Normalmap && bUseDXT5NormalMap)
		{
			// move R to A like we do for DXT5 normal maps : (NameDXT5n)
			TextureFormatName = NameXGXR8;
		}
		else if (FormatSettings.CompressionSettings == TC_HDR_Compressed )
		{
			TextureFormatName = NameRGBA16F;
		}
		else if (Texture->HasHDRSource(LayerIndex))
		{
			// note that if user actually selected an HDR TC we do not come in here
			// @todo Oodle : consider removing HasHDRSource ; user did not pick an HDR TC output format
			TextureFormatName = NameRGBA16F;
		}
		else if (SourceFormat == TSF_G16)
		{
			TextureFormatName = NameG16;
		}
		else if (SourceFormat == TSF_G8)
		{
			TextureFormatName = NameG8;
		}
		else
		{
			// note CompressionNoAlpha no longer kills alpha if it's forced to uncompressed (eg. because size is not multiple of 4)
			TextureFormatName = NameBGRA8;
		}
	}

	// fix up stage :

	// Some PC GPUs don't support sRGB read from G8 textures (e.g. AMD DX10 cards on ShaderModel3.0)
	// This solution requires 4x more memory but a lot of PC HW emulate the format anyway
	// note: GrayscaleSRGB is off on all targetplatforms currently
	// someday: I think this could use G16 instead and be half the size
	//	 (that's doing the gamma->linear in the G8->G16 conversion)
	if ((TextureFormatName == NameG8) && FormatSettings.SRGB && !TargetPlatform->SupportsFeature(ETargetPlatformFeatures::GrayscaleSRGB))
	{
		TextureFormatName = NameBGRA8;
	}
	
	// remap 32F to 16F if not supported :
	if ( !bSupportFilteredFloat32Textures &&
		( TextureFormatName == NameR32F || TextureFormatName == NameRGBA32F ) )
	{
		// Texture::Filter can be manually set to TF_Nearest , if it's Default it comes from LOD Group
		//   eg. Nearest for TEXTUREGROUP_ColorLookupTable and TEXTUREGROUP_Pixels2D
		const UTextureLODSettings& LODSettings = TargetPlatform->GetTextureLODSettings();
		ETextureSamplerFilter Filter = LODSettings.GetSamplerFilter(Texture);

		if ( Filter != ETextureSamplerFilter::Point )
		{
			// non-Point filters require remap

			UE_LOG(LogTexture, Display, TEXT("32 bit float texture changed to 16F because Filter is not Nearest and !bSupportFilteredFloat32Textures : %s"), *Texture->GetPathName());

			if (TextureFormatName == NameR32F)
			{
				TextureFormatName = NameR16F;
			}
			else 
			{
				check(TextureFormatName == NameRGBA32F);

				TextureFormatName = NameRGBA16F;
			}			
		}
	}

	bOodleTextureSdkVersionIsNone = Texture->OodleTextureSdkVersion.IsNone();
#endif //WITH_EDITOR

	FName Result = ConditionalGetPrefixedFormat(TextureFormatName, TargetPlatform, bOodleTextureSdkVersionIsNone);

	return Result;
}

void GetDefaultTextureFormatNamePerLayer(TArray<FName>& OutFormatNames, const class ITargetPlatform* TargetPlatform, const class UTexture* Texture, 
	bool bSupportCompressedVolumeTexture, int32 Unused_BlockSize, bool bSupportFilteredFloat32Textures )
{
#if WITH_EDITOR
	OutFormatNames.Reserve(Texture->Source.GetNumLayers());
	for (int32 LayerIndex = 0; LayerIndex < Texture->Source.GetNumLayers(); ++LayerIndex)
	{
		OutFormatNames.Add(GetDefaultTextureFormatName(TargetPlatform, Texture, LayerIndex, bSupportCompressedVolumeTexture, Unused_BlockSize, bSupportFilteredFloat32Textures));
	}
#endif // WITH_EDITOR
}

void GetAllDefaultTextureFormats(const class ITargetPlatform* TargetPlatform, TArray<FName>& OutFormats)
{
	// this is only used by CookOnTheFlyServer, it could be removed entirely

#if WITH_EDITOR
	static FName NameDXT1(TEXT("DXT1"));
	static FName NameDXT5(TEXT("DXT5"));
	static FName NameDXT5n(TEXT("DXT5n"));
	static FName NameAutoDXT(TEXT("AutoDXT"));
	static FName NameBC4(TEXT("BC4"));
	static FName NameBC5(TEXT("BC5"));
	static FName NameBGRA8(TEXT("BGRA8"));
	static FName NameXGXR8(TEXT("XGXR8"));
	static FName NameG8(TEXT("G8"));
	static FName NameG16(TEXT("G16"));
	static FName NameVU8(TEXT("VU8"));
	static FName NameRGBA16F(TEXT("RGBA16F"));
	static FName NameRGBA32F(TEXT("RGBA32F"));
	static FName NameR16F(TEXT("R16F"));
	static FName NameR32F(TEXT("R32F"));
	static FName NameBC6H(TEXT("BC6H"));
	static FName NameBC7(TEXT("BC7"));

	OutFormats.Add(NameDXT1);
	OutFormats.Add(NameDXT5);
	OutFormats.Add(NameDXT5n);
	OutFormats.Add(NameAutoDXT);
	OutFormats.Add(NameBC4);
	OutFormats.Add(NameBC5);
	OutFormats.Add(NameBGRA8);
	OutFormats.Add(NameXGXR8);
	OutFormats.Add(NameG8);
	OutFormats.Add(NameG16);
	OutFormats.Add(NameVU8);
	OutFormats.Add(NameRGBA16F);
	OutFormats.Add(NameRGBA32F);
	OutFormats.Add(NameR16F);
	OutFormats.Add(NameR32F);
	OutFormats.Add(NameBC6H);
	OutFormats.Add(NameBC7);
	// is there any drawback to just adding the 32F textures here even if we don't want them? -> no.
	//	what is this list even used for?
	//  AFAICT it's only used by CookOnTheFlyServer for GetVersionFormatNumbersForIniVersionStrings

	// go over the original base formats only, and possibly add on to the end of the array if there is a prefix needed
	int NumBaseFormats = OutFormats.Num();
	for (int Index = 0; Index < NumBaseFormats; Index++)
	{
		OutFormats.Add(ConditionalGetPrefixedFormat(OutFormats[Index], TargetPlatform, true));
		OutFormats.Add(ConditionalGetPrefixedFormat(OutFormats[Index], TargetPlatform, false));
	}
	
	// make unique:		
	OutFormats.Sort( FNameFastLess() );
	OutFormats.SetNum( Algo::Unique( OutFormats ) );
#endif
}

#if WITH_EDITOR

void UTexture::NotifyMaterials(const ENotifyMaterialsEffectOnShaders EffectOnShaders)
{
	// Create a material update context to safely update materials.
	{
		FMaterialUpdateContext UpdateContext;

		// Notify any material that uses this texture
		TSet<UMaterial*> BaseMaterialsThatUseThisTexture;
		for (TObjectIterator<UMaterialInterface> It; It; ++It)
		{
			UMaterialInterface* MaterialInterface = *It;
			if (DoesMaterialUseTexture(MaterialInterface, this))
			{
				UpdateContext.AddMaterialInterface(MaterialInterface);
				// This is a bit tricky. We want to make sure all materials using this texture are
				// updated. Materials are always updated. Material instances may also have to be
				// updated and if they have static permutations their children must be updated
				// whether they use the texture or not! The safe thing to do is to add the instance's
				// base material to the update context causing all materials in the tree to update.
				BaseMaterialsThatUseThisTexture.Add(MaterialInterface->GetMaterial());
			}
		}

		// Go ahead and update any base materials that need to be.
		if (EffectOnShaders == ENotifyMaterialsEffectOnShaders::Default)
		{
			for (TSet<UMaterial*>::TConstIterator It(BaseMaterialsThatUseThisTexture); It; ++It)
			{
				(*It)->PostEditChange();
			}
		}
		else
		{
			FPropertyChangedEvent EmptyPropertyUpdateStruct(nullptr);
			for (TSet<UMaterial*>::TConstIterator It(BaseMaterialsThatUseThisTexture); It; ++It)
			{
				(*It)->PostEditChangePropertyInternal(EmptyPropertyUpdateStruct, UMaterial::EPostEditChangeEffectOnShaders::DoesNotInvalidate);
			}
		}
	}
}

#endif //WITH_EDITOR

int64 UTexture::Blueprint_GetMemorySize() const
{
	return CalcTextureMemorySizeEnum(TMC_ResidentMips);
}

void UTexture::Blueprint_GetTextureSourceDiskAndMemorySize(int64 & OutDiskSize,int64 & OutMemorySize) const
{
#if WITH_EDITORONLY_DATA
	OutMemorySize = Source.CalcMipSize(0);
	OutDiskSize = Source.GetSizeOnDisk();
#else
	OutDiskSize = OutMemorySize = 0;
	UE_LOG(LogTexture, Error, TEXT("Blueprint_GetTextureSourceDiskAndMemorySize can only be called WITH_EDITORONLY_DATA. (%s)"), *GetName());
#endif
}

bool UTexture::ComputeTextureSourceChannelMinMax(FLinearColor & OutColorMin, FLinearColor & OutColorMax) const
{
	// make sure we fill the outputs if we return failure :
	OutColorMin = FLinearColor(ForceInit);
	OutColorMax = FLinearColor(ForceInit);

#if WITH_EDITORONLY_DATA
	FImage Image;
	if ( ! const_cast<UTexture *>(this)->Source.GetMipImage(Image,0) )
	{
		UE_LOG(LogTexture, Error, TEXT("ComputeTextureSourceChannelMinMax failed to GetMipImage. (%s)"), *GetName());
		return false;
	}

	FImageCore::ComputeChannelLinearMinMax(Image,OutColorMin,OutColorMax);
	return true;
#else
	UE_LOG(LogTexture, Error, TEXT("ComputeTextureSourceChannelMinMax can only be called WITH_EDITORONLY_DATA. (%s)"), *GetName());
	return false;
#endif
}

#if WITH_EDITOR

FTextureSource::FMipData::FMipData(const FTextureSource& InSource, FSharedBuffer InData)
	: TextureSource(InSource)
	, MipData(InData)
{
}

bool FTextureSource::FMipData::GetMipData(TArray64<uint8>& OutMipData, int32 BlockIndex, int32 LayerIndex, int32 MipIndex) const
{
	if (BlockIndex < TextureSource.GetNumBlocks() && LayerIndex < TextureSource.GetNumLayers() && MipIndex < TextureSource.GetNumMips() && !MipData.IsNull())
	{
		const int64 MipOffset = TextureSource.CalcMipOffset(BlockIndex, LayerIndex, MipIndex);
		const int64 MipSize = TextureSource.CalcMipSize(BlockIndex, LayerIndex, MipIndex);

		if ((int64)MipData.GetSize() >= MipOffset + MipSize)
		{
			OutMipData.Empty(MipSize);
			OutMipData.AddUninitialized(MipSize);
			FMemory::Memcpy(
				OutMipData.GetData(),
				(const uint8*)MipData.GetData() + MipOffset,
				MipSize
			);

			return true;
		}
	}

	return false;
}

#endif //WITH_EDITOR

#if WITH_EDITOR

FTextureSource::FMipAllocation::FMipAllocation(FSharedBuffer SrcData)
	: ReadOnlyReference(SrcData)
{
}

FTextureSource::FMipAllocation::FMipAllocation(FTextureSource::FMipAllocation&& Other)
{
	*this = MoveTemp(Other);
}

FTextureSource::FMipAllocation& FTextureSource::FMipAllocation::operator =(FTextureSource::FMipAllocation&& Other)
{
	ReadOnlyReference = MoveTemp(Other.ReadOnlyReference);
	ReadWriteBuffer = MoveTemp(Other.ReadWriteBuffer);

	return *this;
}

void FTextureSource::FMipAllocation::Reset()
{
	ReadOnlyReference.Reset();
	ReadWriteBuffer = nullptr;
}

uint8* FTextureSource::FMipAllocation::GetDataReadWrite()
{
	if (!ReadWriteBuffer.IsValid())
	{
		CreateReadWriteBuffer(ReadOnlyReference.GetData(), ReadOnlyReference.GetSize());
	}

	return ReadWriteBuffer.Get();
}

FSharedBuffer FTextureSource::FMipAllocation::Release()
{
	if (ReadWriteBuffer.IsValid())
	{
		const int64 DataSize = ReadOnlyReference.GetSize();
		ReadOnlyReference.Reset();
		return FSharedBuffer::TakeOwnership(ReadWriteBuffer.Release(), DataSize, FMemory::Free);
	}
	else
	{
		return MoveTemp(ReadOnlyReference);
	}
}

void FTextureSource::FMipAllocation::CreateReadWriteBuffer(const void* SrcData, int64 DataLength)
{
	if (DataLength > 0)
	{
		ReadWriteBuffer = TUniquePtr<uint8, FDeleterFree>((uint8*)FMemory::Malloc(DataLength));
		FMemory::Memcpy(ReadWriteBuffer.Get(), SrcData, DataLength);
	}

	ReadOnlyReference = FSharedBuffer::MakeView(ReadWriteBuffer.Get(), DataLength);
}

void FTextureSource::InitLayeredImpl(
	int32 NewSizeX,
	int32 NewSizeY,
	int32 NewNumSlices,
	int32 NewNumLayers,
	int32 NewNumMips,
	const ETextureSourceFormat* NewLayerFormat)
{
	RemoveSourceData();
	SizeX = NewSizeX;
	SizeY = NewSizeY;
	NumLayers = NewNumLayers;
	NumSlices = NewNumSlices;
	NumMips = NewNumMips;
	
	// VT can exceed the size limit of MAX_TEXTURE_MIP_COUNT
	//  but doesn't have all those mips
	check( NewNumMips <= MAX_TEXTURE_MIP_COUNT );
	// NumSlices could be volume size Z or not
	check( NewNumMips <= GetFullMipCount(SizeX,SizeY,GetVolumeSizeZ()) );

	Format = NewLayerFormat[0];
	LayerFormat.SetNum(NewNumLayers, true);
	for (int i = 0; i < NewNumLayers; ++i)
	{
		LayerFormat[i] = NewLayerFormat[i];
	}

	BlockDataOffsets.Add(0);

	checkf(LockState == ELockState::None, TEXT("InitLayered shouldn't be called in-between LockMip/UnlockMip"));
}

void FTextureSource::InitBlockedImpl(const ETextureSourceFormat* InLayerFormats,
	const FTextureSourceBlock* InBlocks,
	int32 InNumLayers,
	int32 InNumBlocks)
{
	check(InNumBlocks > 0);
	check(InNumLayers > 0);

	RemoveSourceData();

	BaseBlockX = InBlocks[0].BlockX;
	BaseBlockY = InBlocks[0].BlockY;
	SizeX = InBlocks[0].SizeX;
	SizeY = InBlocks[0].SizeY;
	NumSlices = InBlocks[0].NumSlices;
	NumMips = InBlocks[0].NumMips;
	
	check( NumMips <= GetFullMipCount(SizeX,SizeY) );

	NumLayers = InNumLayers;
	Format = InLayerFormats[0];

	// Blocks is of size NumBlocks-1 , and 0th block is in the TextureSource
	Blocks.Reserve(InNumBlocks - 1);
	for (int32 BlockIndex = 1; BlockIndex < InNumBlocks; ++BlockIndex)
	{
		Blocks.Add(InBlocks[BlockIndex]);
	}

	// LayerFormat is of size NumLayers, and Format == LayerFormat[0]
	LayerFormat.SetNum(InNumLayers, true);
	for (int i = 0; i < InNumLayers; ++i)
	{
		LayerFormat[i] = InLayerFormats[i];
	}

	EnsureBlocksAreSorted();

	checkf(LockState == ELockState::None, TEXT("InitBlocked shouldn't be called in-between LockMip/UnlockMip"));
}

namespace
{
struct FSortedTextureSourceBlock
{
	FTextureSourceBlock Block;
	int64 DataOffset;
	int32 SourceBlockIndex;
	int32 SortKey;
};
inline bool operator<(const FSortedTextureSourceBlock& Lhs, const FSortedTextureSourceBlock& Rhs)
{
	return Lhs.SortKey < Rhs.SortKey;
}
} // namespace

bool FTextureSource::EnsureBlocksAreSorted()
{
	// BlockDataOffsets is of size NumBlocks, even when NumBlocks==1
	// and BlockDataOffsets[0] == 0
	const int32 NumBlocks = GetNumBlocks();
	if (BlockDataOffsets.Num() == NumBlocks)
	{
		return false;
	}

	BlockDataOffsets.Empty(NumBlocks);
	if (NumBlocks > 1)
	{
		const FIntPoint SizeInBlocks = GetSizeInBlocks();

		TArray<FSortedTextureSourceBlock> SortedBlocks;
		SortedBlocks.Empty(NumBlocks);

		int64 CurrentDataOffset = 0;
		for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
		{
			FSortedTextureSourceBlock& SortedBlock = SortedBlocks.AddDefaulted_GetRef();
			GetBlock(BlockIndex, SortedBlock.Block);
			SortedBlock.SourceBlockIndex = BlockIndex;
			SortedBlock.DataOffset = CurrentDataOffset;
			SortedBlock.SortKey = SortedBlock.Block.BlockY * SizeInBlocks.X + SortedBlock.Block.BlockX;
			CurrentDataOffset += CalcBlockSize(SortedBlock.Block);
		}
		SortedBlocks.Sort();

		BlockDataOffsets.Add(SortedBlocks[0].DataOffset);
		BaseBlockX = SortedBlocks[0].Block.BlockX;
		BaseBlockY = SortedBlocks[0].Block.BlockY;
		SizeX = SortedBlocks[0].Block.SizeX;
		SizeY = SortedBlocks[0].Block.SizeY;
		NumSlices = SortedBlocks[0].Block.NumSlices;
		NumMips = SortedBlocks[0].Block.NumMips;
		for (int32 BlockIndex = 1; BlockIndex < NumBlocks; ++BlockIndex)
		{
			const FSortedTextureSourceBlock& SortedBlock = SortedBlocks[BlockIndex];
			BlockDataOffsets.Add(SortedBlock.DataOffset);
			Blocks[BlockIndex - 1] = SortedBlock.Block;
		}
	}
	else
	{
		BlockDataOffsets.Add(0);
	}

	return true;
}
#endif //WITH_EDITOR


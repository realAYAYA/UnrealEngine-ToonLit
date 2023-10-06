// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataDrivenShaderPlatformInfo.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Misc/CommandLine.h"
#include "RHI.h"
#include "RHIStrings.h"

#if WITH_EDITOR
#include "Internationalization/Text.h"
#endif

const FName LANGUAGE_D3D("D3D");
const FName LANGUAGE_Metal("Metal");
const FName LANGUAGE_OpenGL("OpenGL");
const FName LANGUAGE_Vulkan("Vulkan");
const FName LANGUAGE_Sony("Sony");
const FName LANGUAGE_Nintendo("Nintendo");

FGenericDataDrivenShaderPlatformInfo FGenericDataDrivenShaderPlatformInfo::Infos[SP_NumPlatforms];
static TMap<FName, EShaderPlatform> PlatformNameToShaderPlatformMap;

#if WITH_EDITOR
struct FDataDrivenShaderPlatformInfoEditorOnly
{
	FText FriendlyName;
	EShaderPlatform PreviewShaderPlatformParent = SP_NumPlatforms;
};
static FDataDrivenShaderPlatformInfoEditorOnly DataDrivenShaderPlatformInfoEditorOnlyInfos[SP_NumPlatforms];

TMap<FString, TFunction<bool(const FStaticShaderPlatform Platform)>> FGenericDataDrivenShaderPlatformInfo::PropertyToShaderPlatformFunctionMap;
#endif

EShaderPlatform ParseShaderPlatform(const TCHAR* String)
{
	for (int i = 0; i < (int)EShaderPlatform::SP_NumPlatforms; ++i)
	{
		if (LexToString((EShaderPlatform)i, false).Equals(String, ESearchCase::IgnoreCase))
		{
			return (EShaderPlatform)i;
		}
	}

	return SP_NumPlatforms;
}

// Gets a string from a section, or empty string if it didn't exist
static inline FString GetSectionString(const FConfigSection& Section, FName Key)
{
	return Section.FindRef(Key).GetValue();
}

// Gets a bool from a section.  It returns the original value if the setting does not exist
static inline bool GetSectionBool(const FConfigSection& Section, FName Key, bool OriginalValue)
{
	const FConfigValue* ConfigValue = Section.Find(Key);
	if (ConfigValue != nullptr)
	{
		return FCString::ToBool(*ConfigValue->GetValue());
	}
	else
	{
		return OriginalValue;
	}
}

// Gets an integer from a section.  It returns the original value if the setting does not exist
static inline uint32 GetSectionUint(const FConfigSection& Section, FName Key, uint32 OriginalValue)
{
	const FConfigValue* ConfigValue = Section.Find(Key);
	if (ConfigValue != nullptr)
	{
		return (uint32)FCString::Atoi(*ConfigValue->GetValue());
	}
	else
	{
		return OriginalValue;
	}
}

// Gets an integer from a section.  It returns the original value if the setting does not exist
static inline uint32 GetSectionFeatureSupport(const FConfigSection& Section, FName Key, uint32 OriginalValue)
{
	const FConfigValue* ConfigValue = Section.Find(Key);
	if (ConfigValue != nullptr)
	{
		FString Value = ConfigValue->GetValue();
		if (Value == TEXT("Unsupported"))
		{
			return uint32(ERHIFeatureSupport::Unsupported);
		}
		if (Value == TEXT("RuntimeDependent"))
		{
			return uint32(ERHIFeatureSupport::RuntimeDependent);
		}
		else if (Value == TEXT("RuntimeGuaranteed"))
		{
			return uint32(ERHIFeatureSupport::RuntimeGuaranteed);
		}
		else
		{
			checkf(false, TEXT("Unknown ERHIFeatureSupport value \"%s\" for %s"), *Value, *Key.ToString());
		}
	}

	return OriginalValue;
}

static inline uint32 GetSectionBindlessSupport(const FConfigSection& Section, FName Key, uint32 OriginalValue)
{
	const FConfigValue* ConfigValue = Section.Find(Key);
	if (ConfigValue != nullptr)
	{
		FString Value = ConfigValue->GetValue();
		if (Value == TEXT("Unsupported"))
		{
			return uint32(ERHIBindlessSupport::Unsupported);
		}
		if (Value == TEXT("RayTracingOnly"))
		{
			return uint32(ERHIBindlessSupport::RayTracingOnly);
		}
		else if (Value == TEXT("AllShaderTypes"))
		{
			return uint32(ERHIBindlessSupport::AllShaderTypes);
		}
		else
		{
			checkf(false, TEXT("Unknown ERHIBindlessSupport value \"%s\" for %s"), *Value, *Key.ToString());
		}
	}

	return OriginalValue;
}

void FGenericDataDrivenShaderPlatformInfo::SetDefaultValues()
{
	MaxFeatureLevel = ERHIFeatureLevel::Num;
	bSupportsMSAA = true;
	bSupportsDOFHybridScattering = true;
	bSupportsHZBOcclusion = true;
	bSupportsWaterIndirectDraw = true;
	bSupportsAsyncPipelineCompilation = true;
	bSupportsManualVertexFetch = true;
	bSupportsVolumeTextureAtomics = true;
	bSupportsClipDistance = true;
	bSupportsShaderPipelines = true;
}

void FGenericDataDrivenShaderPlatformInfo::ParseDataDrivenShaderInfo(const FConfigSection& Section, uint32 Index)
{
	FGenericDataDrivenShaderPlatformInfo& Info = Infos[Index];

	Info.Language = *GetSectionString(Section, "Language");
	Info.ShaderFormat = *GetSectionString(Section, "ShaderFormat");
	checkf(!Info.ShaderFormat.IsNone(), TEXT("Missing ShaderFormat for ShaderPlatform %s  ShaderFormat %s"), *Info.Name.ToString(), *Info.ShaderFormat.ToString());

	GetFeatureLevelFromName(GetSectionString(Section, "MaxFeatureLevel"), Info.MaxFeatureLevel);

	Info.ShaderPropertiesHash = 0;
	FString ShaderPropertiesString = Info.Name.GetPlainNameString();

#define ADD_TO_PROPERTIES_STRING(SettingName, SettingValue) \
	ShaderPropertiesString += TEXT(#SettingName); \
	ShaderPropertiesString += FString::Printf(TEXT("_%d"), SettingValue);

#if WITH_EDITOR
	#define ADD_PROPERTY_TO_SHADERPLATFORM_FUNCTIONMAP(SettingName, FunctionName) \
	FGenericDataDrivenShaderPlatformInfo::PropertyToShaderPlatformFunctionMap.FindOrAdd(#SettingName) = &FunctionName;
#else
	#define ADD_PROPERTY_TO_SHADERPLATFORM_FUNCTIONMAP(SettingName, FunctionName)
#endif

#define GET_SECTION_BOOL_HELPER(SettingName)	\
	Info.SettingName = GetSectionBool(Section, #SettingName, Info.SettingName);	\
	ADD_TO_PROPERTIES_STRING(SettingName, Info.SettingName)

#define GET_SECTION_INT_HELPER(SettingName)	\
	Info.SettingName = GetSectionUint(Section, #SettingName, Info.SettingName); \
	ADD_TO_PROPERTIES_STRING(SettingName, Info.SettingName)

#define GET_SECTION_SUPPORT_HELPER(SettingName)	\
	Info.SettingName = GetSectionFeatureSupport(Section, #SettingName, Info.SettingName); \
	ADD_TO_PROPERTIES_STRING(SettingName, Info.SettingName)

#define GET_SECTION_BINDLESS_SUPPORT_HELPER(SettingName) \
	Info.SettingName = GetSectionBindlessSupport(Section, #SettingName, Info.SettingName); \
	ADD_TO_PROPERTIES_STRING(SettingName, Info.SettingName)

	// These properties will be exposed to the MaterialEditor that use the ShaderPlatformInfo Node
	// If you remove/rename a property be sure to address this in UMaterialExpressionDataDrivenShaderPlatformInfoSwitch serialization
	// If you add a property that you think needs to be exposed to materials, it is enough to just call the macro here with the property name and the function it will call
	ADD_PROPERTY_TO_SHADERPLATFORM_FUNCTIONMAP(IsMobile, GetIsMobile);
	ADD_PROPERTY_TO_SHADERPLATFORM_FUNCTIONMAP(IsPC, GetIsPC);
	ADD_PROPERTY_TO_SHADERPLATFORM_FUNCTIONMAP(IsConsole, GetIsConsole);

	GET_SECTION_BOOL_HELPER(bIsMobile);
	GET_SECTION_BOOL_HELPER(bIsMetalMRT);
	GET_SECTION_BOOL_HELPER(bIsPC);
	GET_SECTION_BOOL_HELPER(bIsConsole);
	GET_SECTION_BOOL_HELPER(bIsAndroidOpenGLES);
	GET_SECTION_BOOL_HELPER(bSupportsDebugViewShaders);
	GET_SECTION_BOOL_HELPER(bSupportsMobileMultiView);
	GET_SECTION_BOOL_HELPER(bSupportsArrayTextureCompression);
	GET_SECTION_BOOL_HELPER(bSupportsDistanceFields);
	GET_SECTION_BOOL_HELPER(bSupportsDiaphragmDOF);
	GET_SECTION_BOOL_HELPER(bSupportsRGBColorBuffer);
	GET_SECTION_BOOL_HELPER(bSupportsCapsuleShadows);
	GET_SECTION_BOOL_HELPER(bSupportsPercentageCloserShadows);
	GET_SECTION_BOOL_HELPER(bSupportsIndexBufferUAVs);
	GET_SECTION_BOOL_HELPER(bSupportsInstancedStereo);
	GET_SECTION_SUPPORT_HELPER(SupportsMultiViewport);
	GET_SECTION_BOOL_HELPER(bSupportsMSAA);
	GET_SECTION_BOOL_HELPER(bSupports4ComponentUAVReadWrite);
	GET_SECTION_BOOL_HELPER(bSupportsRenderTargetWriteMask);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracing);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracingShaders);
	GET_SECTION_BOOL_HELPER(bSupportsInlineRayTracing);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracingCallableShaders);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracingProceduralPrimitive);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracingTraversalStatistics);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracingIndirectInstanceData);
	GET_SECTION_BOOL_HELPER(bSupportsPathTracing);
	GET_SECTION_BOOL_HELPER(bSupportsHighEndRayTracingReflections);
	GET_SECTION_BOOL_HELPER(bSupportsByteBufferComputeShaders);
	GET_SECTION_BOOL_HELPER(bSupportsGPUScene);
	GET_SECTION_BOOL_HELPER(bSupportsPrimitiveShaders);
	GET_SECTION_BOOL_HELPER(bSupportsUInt64ImageAtomics);
	GET_SECTION_BOOL_HELPER(bRequiresVendorExtensionsForAtomics);
	GET_SECTION_BOOL_HELPER(bSupportsNanite);
	GET_SECTION_BOOL_HELPER(bSupportsLumenGI);
	GET_SECTION_BOOL_HELPER(bSupportsSSDIndirect);
	GET_SECTION_BOOL_HELPER(bSupportsTemporalHistoryUpscale);
	GET_SECTION_BOOL_HELPER(bSupportsRTIndexFromVS);
	GET_SECTION_BOOL_HELPER(bSupportsIntrinsicWaveOnce);
	GET_SECTION_BOOL_HELPER(bSupportsConservativeRasterization);
	GET_SECTION_SUPPORT_HELPER(bSupportsWaveOperations);
	GET_SECTION_BOOL_HELPER(bSupportsWavePermute);
	GET_SECTION_INT_HELPER(MinimumWaveSize);
	GET_SECTION_INT_HELPER(MaximumWaveSize);
	GET_SECTION_BOOL_HELPER(bRequiresExplicit128bitRT);
	GET_SECTION_BOOL_HELPER(bSupportsGen5TemporalAA);
	GET_SECTION_BOOL_HELPER(bTargetsTiledGPU);
	GET_SECTION_BOOL_HELPER(bNeedsOfflineCompiler);
	GET_SECTION_BOOL_HELPER(bSupportsComputeFramework);
	GET_SECTION_BOOL_HELPER(bSupportsAnisotropicMaterials);
	GET_SECTION_BOOL_HELPER(bSupportsDualSourceBlending);
	GET_SECTION_BOOL_HELPER(bRequiresGeneratePrevTransformBuffer);
	GET_SECTION_BOOL_HELPER(bRequiresRenderTargetDuringRaster);
	GET_SECTION_BOOL_HELPER(bRequiresDisableForwardLocalLights);
	GET_SECTION_BOOL_HELPER(bCompileSignalProcessingPipeline);
	GET_SECTION_BOOL_HELPER(bSupportsMeshShadersTier0);
	GET_SECTION_BOOL_HELPER(bSupportsMeshShadersTier1);
	GET_SECTION_BOOL_HELPER(bSupportsMeshShadersWithClipDistance);
	GET_SECTION_INT_HELPER(MaxMeshShaderThreadGroupSize);
	GET_SECTION_BOOL_HELPER(bSupportsPerPixelDBufferMask);
	GET_SECTION_BOOL_HELPER(bIsHlslcc);
	GET_SECTION_BOOL_HELPER(bSupportsDxc);
	GET_SECTION_BOOL_HELPER(bSupportsVariableRateShading);
	GET_SECTION_BOOL_HELPER(bIsSPIRV);
	GET_SECTION_INT_HELPER(NumberOfComputeThreads);

	GET_SECTION_BOOL_HELPER(bWaterUsesSimpleForwardShading);
	GET_SECTION_BOOL_HELPER(bSupportsHairStrandGeometry);
	GET_SECTION_BOOL_HELPER(bSupportsDOFHybridScattering);
	GET_SECTION_BOOL_HELPER(bNeedsExtraMobileFrames);
	GET_SECTION_BOOL_HELPER(bSupportsHZBOcclusion);
	GET_SECTION_BOOL_HELPER(bSupportsWaterIndirectDraw);
	GET_SECTION_BOOL_HELPER(bSupportsAsyncPipelineCompilation);
	GET_SECTION_BOOL_HELPER(bSupportsManualVertexFetch);
	GET_SECTION_BOOL_HELPER(bRequiresReverseCullingOnMobile);
	GET_SECTION_BOOL_HELPER(bOverrideFMaterial_NeedsGBufferEnabled);
	GET_SECTION_BOOL_HELPER(bSupportsFFTBloom);
	GET_SECTION_BOOL_HELPER(bSupportsVertexShaderLayer);
	GET_SECTION_BINDLESS_SUPPORT_HELPER(BindlessSupport);
	GET_SECTION_BOOL_HELPER(bSupportsVolumeTextureAtomics);
	GET_SECTION_BOOL_HELPER(bSupportsROV);
	GET_SECTION_BOOL_HELPER(bSupportsOIT);
	GET_SECTION_SUPPORT_HELPER(bSupportsRealTypes);
	GET_SECTION_INT_HELPER(EnablesHLSL2021ByDefault);
	GET_SECTION_BOOL_HELPER(bSupportsSceneDataCompressedTransforms);
	GET_SECTION_BOOL_HELPER(bSupportsSwapchainUAVs);
	GET_SECTION_BOOL_HELPER(bSupportsClipDistance);
	GET_SECTION_BOOL_HELPER(bSupportsNNEShaders);
	GET_SECTION_BOOL_HELPER(bSupportsShaderPipelines);
#undef GET_SECTION_BOOL_HELPER
#undef GET_SECTION_INT_HELPER
#undef GET_SECTION_SUPPORT_HELPER
#undef ADD_TO_PROPERTIES_STRING
#undef ADD_PROPERTY_TO_SHADERPLATFORM_FUNCTIONMAP

	Info.ShaderPropertiesHash = GetTypeHash(ShaderPropertiesString);

#if WITH_EDITOR
	FDataDrivenShaderPlatformInfoEditorOnly& EditorInfo = DataDrivenShaderPlatformInfoEditorOnlyInfos[Index];
	FTextStringHelper::ReadFromBuffer(*GetSectionString(Section, FName("FriendlyName")), EditorInfo.FriendlyName);
#endif
}

void FGenericDataDrivenShaderPlatformInfo::Initialize()
{
	static bool bInitialized = false;
	if (bInitialized)
	{
		return;
	}
	PlatformNameToShaderPlatformMap.Empty();

	// look for the standard DataDriven ini files
	int32 NumDDInfoFiles = FDataDrivenPlatformInfoRegistry::GetNumDataDrivenIniFiles();
	int32 CustomShaderPlatform = EShaderPlatform::SP_CUSTOM_PLATFORM_FIRST;

	struct PlatformInfoAndPlatformEnum
	{
		FGenericDataDrivenShaderPlatformInfo Info;
		EShaderPlatform ShaderPlatform;
	};

	for (int32 Index = 0; Index < NumDDInfoFiles; Index++)
	{
		FConfigFile IniFile;
		FString PlatformName;

		FDataDrivenPlatformInfoRegistry::LoadDataDrivenIniFile(Index, IniFile, PlatformName);

		// now walk over the file, looking for ShaderPlatformInfo sections
		for (auto Section : IniFile)
		{
			if (Section.Key.StartsWith(TEXT("ShaderPlatform ")))
			{
				const FString& SectionName = Section.Key;

				// get enum value for the string name
				const EShaderPlatform ShaderPlatform = ParseShaderPlatform(*SectionName.Mid(15));
				if (ShaderPlatform == SP_NumPlatforms)
				{
#if DDPI_HAS_EXTENDED_PLATFORMINFO_DATA
					const bool bIsEnabled = FDataDrivenPlatformInfoRegistry::GetPlatformInfo(PlatformName).bEnabledForUse;
#else
					const bool bIsEnabled = true;
#endif
					UE_CLOG(bIsEnabled, LogRHI, Warning, TEXT("Found an unknown shader platform %s in a DataDriven ini file"), *SectionName.Mid(15));
					continue;
				}

				// at this point, we can start pulling information out
				Infos[ShaderPlatform].Name = *SectionName.Mid(15);
				PlatformNameToShaderPlatformMap.FindOrAdd(Infos[ShaderPlatform].Name) = ShaderPlatform;
				ParseDataDrivenShaderInfo(Section.Value, ShaderPlatform);
				Infos[ShaderPlatform].bContainsValidPlatformInfo = true;

#if WITH_EDITOR
				if (!FParse::Param(FCommandLine::Get(), TEXT("NoPreviewPlatforms")))
				{
					for (const FPreviewPlatformMenuItem& Item : FDataDrivenPlatformInfoRegistry::GetAllPreviewPlatformMenuItems())
					{
						const FName PreviewPlatformName = *(Infos[ShaderPlatform].Name).ToString();
						if (Item.ShaderPlatformToPreview == PreviewPlatformName)
						{
							const EShaderPlatform PreviewShaderPlatform = EShaderPlatform(CustomShaderPlatform++);
							ParseDataDrivenShaderInfo(Section.Value, PreviewShaderPlatform);

							FGenericDataDrivenShaderPlatformInfo& PreviewInfo = Infos[PreviewShaderPlatform];
							PreviewInfo.Name = Item.PreviewShaderPlatformName;
							PreviewInfo.bIsPreviewPlatform = true;
							PreviewInfo.bContainsValidPlatformInfo = true;

							FDataDrivenShaderPlatformInfoEditorOnly& PreviewEditorInfo = DataDrivenShaderPlatformInfoEditorOnlyInfos[PreviewShaderPlatform];
							PreviewEditorInfo.PreviewShaderPlatformParent = ShaderPlatform;
							if (!Item.OptionalFriendlyNameOverride.IsEmpty())
							{
								PreviewEditorInfo.FriendlyName = Item.OptionalFriendlyNameOverride;
							}

							PlatformNameToShaderPlatformMap.FindOrAdd(PreviewInfo.Name) = PreviewShaderPlatform;
						}
					}
				}
#endif
			}
		}
	}
	bInitialized = true;
}

void FGenericDataDrivenShaderPlatformInfo::UpdatePreviewPlatforms()
{
	for (int i = 0; i < EShaderPlatform::SP_NumPlatforms; ++i)
	{
		EShaderPlatform ShaderPlatform = EShaderPlatform(i);
		if (IsValid(ShaderPlatform))
		{
			ERHIFeatureLevel::Type PreviewSPMaxFeatureLevel = Infos[ShaderPlatform].MaxFeatureLevel;
			EShaderPlatform EditorSPForPreviewMaxFeatureLevel = GShaderPlatformForFeatureLevel[PreviewSPMaxFeatureLevel];
			if (Infos[ShaderPlatform].bIsPreviewPlatform && EditorSPForPreviewMaxFeatureLevel < EShaderPlatform::SP_NumPlatforms)
			{
				Infos[ShaderPlatform].ShaderFormat = Infos[EditorSPForPreviewMaxFeatureLevel].ShaderFormat;
				Infos[ShaderPlatform].Language = Infos[EditorSPForPreviewMaxFeatureLevel].Language;
				Infos[ShaderPlatform].bIsHlslcc = Infos[EditorSPForPreviewMaxFeatureLevel].bIsHlslcc;
				Infos[ShaderPlatform].bSupportsDxc = Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsDxc;
				Infos[ShaderPlatform].bSupportsGPUScene = Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsGPUScene;
				Infos[ShaderPlatform].bIsPC = true;
				Infos[ShaderPlatform].bSupportsDebugViewShaders = true;
				Infos[ShaderPlatform].bIsConsole = false;
				Infos[ShaderPlatform].bSupportsSceneDataCompressedTransforms = Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsSceneDataCompressedTransforms;
				Infos[ShaderPlatform].bSupportsNanite &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsNanite;
				Infos[ShaderPlatform].bSupportsLumenGI &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsLumenGI;
				Infos[ShaderPlatform].bSupportsPrimitiveShaders &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsPrimitiveShaders;
				Infos[ShaderPlatform].bSupportsUInt64ImageAtomics &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsUInt64ImageAtomics;
				Infos[ShaderPlatform].bSupportsGen5TemporalAA &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsGen5TemporalAA;
				Infos[ShaderPlatform].bSupportsInlineRayTracing &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsInlineRayTracing;
				Infos[ShaderPlatform].bSupportsRayTracingShaders &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsRayTracingShaders;
				Infos[ShaderPlatform].bSupportsMeshShadersTier0 &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsMeshShadersTier0;
				Infos[ShaderPlatform].bSupportsMeshShadersTier1 &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsMeshShadersTier1;

				// Support for stereo features requires extra consideration. The editor may not use the same technique as the preview platform,
				// particularly MobileMultiView may be substituted by a fallback path. In order to avoid inundating real mobile platforms
				// with the properties needed for the desktop MMV fallback path, override them here with the editor ones to make MMV preview possible
				if (Infos[ShaderPlatform].bSupportsMobileMultiView && !Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsMobileMultiView)
				{
					Infos[ShaderPlatform].bSupportsInstancedStereo = Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsInstancedStereo;
					Infos[ShaderPlatform].bSupportsVertexShaderLayer = Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsVertexShaderLayer;
				}
				else
				{
					Infos[ShaderPlatform].bSupportsInstancedStereo &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsInstancedStereo;
				}
				Infos[ShaderPlatform].bSupportsMobileMultiView &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsMobileMultiView;
				Infos[ShaderPlatform].bSupportsManualVertexFetch = Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsManualVertexFetch;
				Infos[ShaderPlatform].bSupportsRenderTargetWriteMask = false;
				Infos[ShaderPlatform].bSupportsIntrinsicWaveOnce = false;
				Infos[ShaderPlatform].bSupportsDOFHybridScattering = false;
				Infos[ShaderPlatform].bSupports4ComponentUAVReadWrite = false;
				Infos[ShaderPlatform].bContainsValidPlatformInfo = true;
			}
		}
	}
}

#if WITH_EDITOR
FText FGenericDataDrivenShaderPlatformInfo::GetFriendlyName(const FStaticShaderPlatform Platform)
{
	if (IsRunningCommandlet() || GUsingNullRHI)
	{
		return FText();
	}
	check(IsValid(Platform));
	return DataDrivenShaderPlatformInfoEditorOnlyInfos[Platform].FriendlyName;
}

const EShaderPlatform FGenericDataDrivenShaderPlatformInfo::GetPreviewShaderPlatformParent(const FStaticShaderPlatform Platform)
{
	check(IsValid(Platform));
	return DataDrivenShaderPlatformInfoEditorOnlyInfos[Platform].PreviewShaderPlatformParent;
}
#endif

const EShaderPlatform FGenericDataDrivenShaderPlatformInfo::GetShaderPlatformFromName(const FName ShaderPlatformName)
{
	if (EShaderPlatform* ShaderPlatform = PlatformNameToShaderPlatformMap.Find(ShaderPlatformName))
	{
		return *ShaderPlatform;
	}
	return SP_NumPlatforms;
}

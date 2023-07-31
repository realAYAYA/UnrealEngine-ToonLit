// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHI.cpp: Metal device RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "RenderUtils.h"
#if PLATFORM_IOS
#include "IOS/IOSAppDelegate.h"
#elif PLATFORM_MAC
#include "Mac/MacApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GenericPlatform/GenericPlatformFile.h"
#endif
#include "HAL/FileManager.h"
#include "MetalProfiler.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "MetalShaderResources.h"
#include "MetalLLM.h"
#include "Engine/RendererSettings.h"
#include "MetalTransitionData.h"
#include "EngineGlobals.h"

DEFINE_LOG_CATEGORY(LogMetal)

bool GIsMetalInitialized = false;

FMetalBufferFormat GMetalBufferFormats[PF_MAX];

static TAutoConsoleVariable<int32> CVarUseIOSRHIThread(
													TEXT("r.Metal.IOSRHIThread"),
													0,
													TEXT("Controls RHIThread usage for IOS:\n")
													TEXT("\t0: No RHIThread.\n")
													TEXT("\t1: Use RHIThread.\n")
													TEXT("Default is 0."),
													ECVF_Default | ECVF_RenderThreadSafe
													);


static void ValidateTargetedRHIFeatureLevelExists(EShaderPlatform Platform)
{
	bool bSupportsShaderPlatform = false;
#if PLATFORM_MAC
	TArray<FString> TargetedShaderFormats;
	GConfig->GetArray(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);
	
	for (FString Name : TargetedShaderFormats)
	{
		FName ShaderFormatName(*Name);
		if (ShaderFormatToLegacyShaderPlatform(ShaderFormatName) == Platform)
		{
			bSupportsShaderPlatform = true;
			break;
		}
	}
#else
	if (Platform == SP_METAL || Platform == SP_METAL_TVOS)
	{
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsShaderPlatform, GEngineIni);
	}
	else if (Platform == SP_METAL_MRT || Platform == SP_METAL_MRT_TVOS)
	{
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsShaderPlatform, GEngineIni);
	}
#endif
	
	if (!bSupportsShaderPlatform && !WITH_EDITOR)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ShaderPlatform"), FText::FromString(LegacyShaderPlatformToShaderFormat(Platform).ToString()));
		FText LocalizedMsg = FText::Format(NSLOCTEXT("MetalRHI", "ShaderPlatformUnavailable","Shader platform: {ShaderPlatform} was not cooked! Please enable this shader platform in the project's target settings."),Args);
		
		FText Title = NSLOCTEXT("MetalRHI", "ShaderPlatformUnavailableTitle","Shader Platform Unavailable");
		FMessageDialog::Open(EAppMsgType::Ok, LocalizedMsg, &Title);
		FPlatformMisc::RequestExit(true);
		
		METAL_FATAL_ERROR(TEXT("Shader platform: %s was not cooked! Please enable this shader platform in the project's target settings."), *LegacyShaderPlatformToShaderFormat(Platform).ToString());
	}
}

#if PLATFORM_MAC && WITH_EDITOR
static void VerifyMetalCompiler()
{
	FString OutStdOut;
	FString OutStdErr;
	
	// Using xcrun or xcodebuild will fire xcode-select if xcode or command line tools are not installed
	// This will also issue a popup dialog which will attempt to install command line tools which we don't want from the Editor
	
	// xcode-select --print-path
	// Can print out /Applications/Xcode.app/Contents/Developer OR /Library/Developer/CommandLineTools
	// CommandLineTools is no good for us as the Metal compiler isn't included
	{
		int32 ReturnCode = -1;
		bool bFoundXCode = false;
		
		FPlatformProcess::ExecProcess(TEXT("/usr/bin/xcode-select"), TEXT("--print-path"), &ReturnCode, &OutStdOut, &OutStdErr);
		if(ReturnCode == 0 && OutStdOut.Len() > 0)
		{
			OutStdOut.RemoveAt(OutStdOut.Len() - 1);
			if (IFileManager::Get().DirectoryExists(*OutStdOut))
			{
				FString XcodeAppPath = OutStdOut.Left(OutStdOut.Find(TEXT(".app/")) + 4);
				NSBundle* XcodeBundle = [NSBundle bundleWithPath:XcodeAppPath.GetNSString()];
				if (XcodeBundle)
				{
					bFoundXCode = true;
				}
			}
		}
		
		if(!bFoundXCode)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText(NSLOCTEXT("MetalRHI", "XCodeMissingInstall", "Can't find Xcode install for Metal compiler. Please install Xcode and run Xcode.app to accept license or ensure active developer directory is set to current Xcode installation using xcode-select.")));
			FPlatformMisc::RequestExit(true);
			return;
		}
	}
	
	// xcodebuild -license check
	// -license check :returns 0 for accepted, otherwise 1 for command line tools or non zero for license not accepted
	// -checkFirstLaunchStatus | -runFirstLaunch : returns status and runs first launch not so useful from within the editor as sudo is required
	{
		int ReturnCode = -1;
		FPlatformProcess::ExecProcess(TEXT("/usr/bin/xcodebuild"), TEXT("-license check"), &ReturnCode, &OutStdOut, &OutStdErr);
		if(ReturnCode != 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("MetalRHI", "XCodeLicenseAgreement", "Xcode license agreement error: {0}"), FText::FromString(OutStdErr)));
			FPlatformMisc::RequestExit(true);
			return;
		}
	}
	
	
	// xcrun will return non zero if using command line tools
	// This can fail for license agreement as well or wrong command line tools set i.e set to /Library/Developer/CommandLineTools rather than Applications/Xcode.app/Contents/Developer
	{
		int ReturnCode = -1;
		FPlatformProcess::ExecProcess(TEXT("/usr/bin/xcrun"), TEXT("-sdk macosx metal -v"), &ReturnCode, &OutStdOut, &OutStdErr);
		if(ReturnCode != 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("MetalRHI", "XCodeMetalCompiler", "Xcode Metal Compiler error: {0}"), FText::FromString(OutStdErr)));
			FPlatformMisc::RequestExit(true);
			return;
		}
	}
}
#endif

FMetalDynamicRHI::FMetalDynamicRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
: ImmediateContext(nullptr, FMetalDeviceContext::CreateDeviceContext())
{
	check(Singleton == nullptr);
	Singleton = this;

	@autoreleasepool {
	// This should be called once at the start 
	check( IsInGameThread() );
	check( !GIsThreadedRendering );
	
#if PLATFORM_MAC && WITH_EDITOR
	VerifyMetalCompiler();
#endif
	
	GRHISupportsMultithreading = true;
	GRHISupportsMultithreadedResources = true;
	
	// we cannot render to a volume texture without geometry shader or vertex-shader-layer support, so initialise to false and enable based on platform feature availability
	GSupportsVolumeTextureRendering = false;
	
	// Metal always needs a render target to render with fragment shaders!
	GRHIRequiresRenderTargetForPixelShaderUAVs = true;

	//@todo-rco: Query name from API
	GRHIAdapterName = TEXT("Metal");
	GRHIVendorId = 1; // non-zero to avoid asserts

	bool const bRequestedFeatureLevel = (RequestedFeatureLevel != ERHIFeatureLevel::Num);
	bool bSupportsPointLights = false;
	
	// get the device to ask about capabilities?
	mtlpp::Device Device = ImmediateContext.Context->GetDevice();
		
#if PLATFORM_IOS
    bool bSupportAppleA8 = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportAppleA8"), bSupportAppleA8, GEngineIni);
        
    bool bIsA8FeatureSet = false;
        
#if PLATFORM_TVOS
	GRHISupportsDrawIndirect = Device.SupportsFeatureSet(mtlpp::FeatureSet::tvOS_GPUFamily2_v1);
	GRHISupportsPixelShaderUAVs = Device.SupportsFeatureSet(mtlpp::FeatureSet::tvOS_GPUFamily2_v1);
        
    if (!Device.SupportsFeatureSet(mtlpp::FeatureSet::tvOS_GPUFamily2_v1))
    {
        bIsA8FeatureSet = true;
    }
        
#else
	if (!Device.SupportsFeatureSet(mtlpp::FeatureSet::iOS_GPUFamily3_v1))
	{
        bIsA8FeatureSet = true;
    }
    
	GRHISupportsRWTextureBuffers = Device.SupportsFeatureSet(mtlpp::FeatureSet::iOS_GPUFamily4_v1);
	GRHISupportsDrawIndirect = Device.SupportsFeatureSet(mtlpp::FeatureSet::iOS_GPUFamily3_v1);
	GRHISupportsPixelShaderUAVs = Device.SupportsFeatureSet(mtlpp::FeatureSet::iOS_GPUFamily3_v1);

	const mtlpp::FeatureSet FeatureSets[] = {
		mtlpp::FeatureSet::iOS_GPUFamily1_v1,
		mtlpp::FeatureSet::iOS_GPUFamily2_v1,
		mtlpp::FeatureSet::iOS_GPUFamily3_v1,
		mtlpp::FeatureSet::iOS_GPUFamily4_v1
	};
		
	const uint8 FeatureSetVersions[][3] = {
		{8, 0, 0},
		{8, 3, 0},
		{10, 0, 0},
		{11, 0, 0}
	};
	
	GRHIDeviceId = 0;
	for (uint32 i = 0; i < 4; i++)
	{
		if (FPlatformMisc::IOSVersionCompare(FeatureSetVersions[i][0],FeatureSetVersions[i][1],FeatureSetVersions[i][2]) >= 0 && Device.SupportsFeatureSet(FeatureSets[i]))
		{
			GRHIDeviceId++;
		}
	}
		
	GSupportsVolumeTextureRendering = FMetalCommandQueue::SupportsFeature(EMetalFeaturesLayeredRendering);
	bSupportsPointLights = GSupportsVolumeTextureRendering;
#endif

    if(bIsA8FeatureSet)
    {
        if(!bSupportAppleA8)
        {
            UE_LOG(LogMetal, Fatal, TEXT("This device does not supports the Apple A8x or above feature set which is the minimum for this build. Please check the Support Apple A8 checkbox in the IOS Project Settings."));
        }
        
        static auto* CVarMobileVirtualTextures = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.VirtualTextures"));
        if(CVarMobileVirtualTextures->GetValueOnAnyThread() != 0)
        {
            UE_LOG(LogMetal, Fatal, TEXT("Mobile Virtual Textures require a minimum of the Apple A9 feature set."));
        }
    }
        
    bool bProjectSupportsMRTs = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bProjectSupportsMRTs, GEngineIni);

	bool const bRequestedMetalMRT = ((RequestedFeatureLevel >= ERHIFeatureLevel::SM5) || (!bRequestedFeatureLevel && FParse::Param(FCommandLine::Get(),TEXT("metalmrt"))));

    // only allow GBuffers, etc on A8s (A7s are just not going to cut it)
    if (bProjectSupportsMRTs && bRequestedMetalMRT)
    {
#if PLATFORM_TVOS
		ValidateTargetedRHIFeatureLevelExists(SP_METAL_MRT);
		GMaxRHIShaderPlatform = SP_METAL_MRT_TVOS;
#else
		ValidateTargetedRHIFeatureLevelExists(SP_METAL_MRT);
        GMaxRHIShaderPlatform = SP_METAL_MRT;
#endif
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
    }
    else
	{
		if (bRequestedMetalMRT)
		{
			UE_LOG(LogMetal, Warning, TEXT("Metal MRT support requires an iOS or tvOS device with an A8 processor or later. Falling back to Metal ES 3.1."));
		}
		
#if PLATFORM_TVOS
		ValidateTargetedRHIFeatureLevelExists(SP_METAL_TVOS);
		GMaxRHIShaderPlatform = SP_METAL_TVOS;
#else
		ValidateTargetedRHIFeatureLevelExists(SP_METAL);
		GMaxRHIShaderPlatform = SP_METAL;
#endif
        GMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;
	}
		
	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
		
	MemoryStats.DedicatedVideoMemory = 0;
	MemoryStats.TotalGraphicsMemory = Stats.AvailablePhysical;
	MemoryStats.DedicatedSystemMemory = 0;
	MemoryStats.SharedSystemMemory = Stats.AvailablePhysical;
	
#if PLATFORM_TVOS
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2_REMOVED] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL_TVOS;
#else
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2_REMOVED] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL;
#endif
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4_REMOVED] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5) ? GMaxRHIShaderPlatform : SP_NumPlatforms;

#else // PLATFORM_IOS
                
	uint32 DeviceIndex = ((FMetalDeviceContext*)ImmediateContext.Context)->GetDeviceIndex();
	
	TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
	check(DeviceIndex < GPUs.Num());
	FMacPlatformMisc::FGPUDescriptor const& GPUDesc = GPUs[DeviceIndex];
	
	bool bSupportsD24S8 = false;
	bool bSupportsD16 = false;
	
	GRHIAdapterName = FString(Device.GetName());
	
	// However they don't all support other features depending on the version of the OS.
	bool bSupportsTiledReflections = false;
	bool bSupportsDistanceFields = false;
	
	// Default is SM5 on:
	// 10.11.6 for AMD/Nvidia
	// 10.12.2+ for AMD/Nvidia
	// 10.12.4+ for Intel
	bool bSupportsSM5 = true;
	bool bIsIntelHaswell = false;
	
	GSupportsTimestampRenderQueries = true;
	
	if(GRHIAdapterName.Contains("Nvidia"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = 0x10DE;
		bSupportsTiledReflections = true;
		bSupportsDistanceFields = true;
		GRHISupportsWaveOperations = false;
	}
	else if(GRHIAdapterName.Contains("ATi") || GRHIAdapterName.Contains("AMD"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = 0x1002;
		if(GPUDesc.GPUVendorId == GRHIVendorId)
		{
			GRHIAdapterName = FString(GPUDesc.GPUName);
		}
		bSupportsTiledReflections = true;
		bSupportsDistanceFields = true;
		
		// On AMD can also use completion handler time stamp if macOS < Catalina
		GSupportsTimestampRenderQueries = true;
		
		// Only tested on Vega.
		GRHISupportsWaveOperations = GRHIAdapterName.Contains(TEXT("Vega"));
		if (GRHISupportsWaveOperations)
		{
			GRHIMinimumWaveSize = 32;
			GRHIMaximumWaveSize = 64;
		}
	}
	else if(GRHIAdapterName.Contains("Intel"))
	{
		bSupportsTiledReflections = false;
		bSupportsPointLights = true;
		GRHIVendorId = 0x8086;
		bSupportsDistanceFields = true;
		bIsIntelHaswell = (GRHIAdapterName == TEXT("Intel HD Graphics 5000") || GRHIAdapterName == TEXT("Intel Iris Graphics") || GRHIAdapterName == TEXT("Intel Iris Pro Graphics"));
		GRHISupportsWaveOperations = false;
	}
	else if(GRHIAdapterName.Contains("Apple"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = 0x106B;
		bSupportsTiledReflections = true;
		bSupportsDistanceFields = true;
		GSupportsTimestampRenderQueries = true;
		
		GRHISupportsWaveOperations = true;
		GRHIMinimumWaveSize = 32;
		GRHIMaximumWaveSize = 32;
	}

	bool const bRequestedSM5 = (RequestedFeatureLevel == ERHIFeatureLevel::SM5 || (!bRequestedFeatureLevel && (FParse::Param(FCommandLine::Get(),TEXT("metalsm5")) || FParse::Param(FCommandLine::Get(),TEXT("metalmrt")))));
	if(bSupportsSM5 && bRequestedSM5)
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		if (!FParse::Param(FCommandLine::Get(),TEXT("metalmrt")))
		{
			GMaxRHIShaderPlatform = SP_METAL_SM5;
		}
		else
		{
			GMaxRHIShaderPlatform = SP_METAL_MRT_MAC;
		}
	}
	else
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		GMaxRHIShaderPlatform = SP_METAL_SM5;
	}

	ERHIFeatureLevel::Type PreviewFeatureLevel;
	if (RHIGetPreviewFeatureLevel(PreviewFeatureLevel))
	{
		check(PreviewFeatureLevel == ERHIFeatureLevel::ES3_1);

		// ES3.1 feature level emulation
		GMaxRHIFeatureLevel = PreviewFeatureLevel;
		if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1)
		{
			GMaxRHIShaderPlatform = SP_METAL_MACES3_1;
		}
	}

	ValidateTargetedRHIFeatureLevelExists(GMaxRHIShaderPlatform);
	
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2_REMOVED] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::ES3_1) ? SP_METAL_MACES3_1 : SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4_REMOVED] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5) ? GMaxRHIShaderPlatform : SP_NumPlatforms;
	
	// Mac GPUs support layer indexing.
	GSupportsVolumeTextureRendering = (GMaxRHIShaderPlatform != SP_METAL_MRT_MAC);
	bSupportsPointLights &= (GMaxRHIShaderPlatform != SP_METAL_MRT_MAC);
	
	// Make sure the vendors match - the assumption that order in IORegistry is the order in Metal may not hold up forever.
	if(GPUDesc.GPUVendorId == GRHIVendorId)
	{
		GRHIDeviceId = GPUDesc.GPUDeviceId;
		MemoryStats.DedicatedVideoMemory = (int64)GPUDesc.GPUMemoryMB * 1024 * 1024;
		MemoryStats.TotalGraphicsMemory = (int64)GPUDesc.GPUMemoryMB * 1024 * 1024;
		MemoryStats.DedicatedSystemMemory = 0;
		MemoryStats.SharedSystemMemory = 0;
	}
	
	// Change the support depth format if we can
	bSupportsD24S8 = Device.IsDepth24Stencil8PixelFormatSupported();
	
	// Disable tiled reflections on Mac Metal for some GPU drivers that ignore the lod-level and so render incorrectly.
	if (!bSupportsTiledReflections && !FParse::Param(FCommandLine::Get(),TEXT("metaltiledreflections")))
	{
		static auto CVarDoTiledReflections = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DoTiledReflections"));
		if(CVarDoTiledReflections && CVarDoTiledReflections->GetInt() != 0)
		{
			CVarDoTiledReflections->Set(0);
		}
	}
	
	// Disable the distance field AO & shadowing effects on GPU drivers that don't currently execute the shaders correctly.
	if ((GMaxRHIShaderPlatform == SP_METAL_SM5) && !bSupportsDistanceFields && !FParse::Param(FCommandLine::Get(),TEXT("metaldistancefields")))
	{
		static auto CVarDistanceFieldAO = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldAO"));
		if(CVarDistanceFieldAO && CVarDistanceFieldAO->GetInt() != 0)
		{
			CVarDistanceFieldAO->Set(0);
		}
		
		static auto CVarDistanceFieldShadowing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldShadowing"));
		if(CVarDistanceFieldShadowing && CVarDistanceFieldShadowing->GetInt() != 0)
		{
			CVarDistanceFieldShadowing->Set(0);
		}
	}
	
#endif
		
#if PLATFORM_MAC
    if (Device.SupportsFeatureSet(mtlpp::FeatureSet::macOS_GPUFamily1_v3))
#endif
	{
		GRHISupportsDynamicResolution = true;
		GRHISupportsFrameCyclesBubblesRemoval = true;
	}

	GPoolSizeVRAMPercentage = 0;
	GTexturePoolSize = 0;
	GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSizeVRAMPercentage"), GPoolSizeVRAMPercentage, GEngineIni);
	if ( GPoolSizeVRAMPercentage > 0 && MemoryStats.TotalGraphicsMemory > 0 )
	{
		float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(MemoryStats.TotalGraphicsMemory);
		
		// Truncate GTexturePoolSize to MB (but still counted in bytes)
		GTexturePoolSize = int64(FGenericPlatformMath::TruncToFloat(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;
		
		UE_LOG(LogRHI,Log,TEXT("Texture pool is %llu MB (%d%% of %llu MB)"),
			   GTexturePoolSize / 1024 / 1024,
			   GPoolSizeVRAMPercentage,
			   MemoryStats.TotalGraphicsMemory / 1024 / 1024);
	}
	else
	{
		static const auto CVarStreamingTexturePoolSize = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Streaming.PoolSize"));
		GTexturePoolSize = (int64)CVarStreamingTexturePoolSize->GetValueOnAnyThread() * 1024 * 1024;

		UE_LOG(LogRHI,Log,TEXT("Texture pool is %llu MB (of %llu MB total graphics mem)"),
			   GTexturePoolSize / 1024 / 1024,
			   MemoryStats.TotalGraphicsMemory / 1024 / 1024);
	}

	GRHITransitionPrivateData_SizeInBytes = sizeof(FMetalTransitionData);
	GRHITransitionPrivateData_AlignInBytes = alignof(FMetalTransitionData);

	GRHISupportsRHIThread = false;
	if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
	{
		GRHISupportsRHIThread = true;
		GSupportsParallelOcclusionQueries = GRHISupportsRHIThread;
	}
	else
	{
		GRHISupportsRHIThread = FParse::Param(FCommandLine::Get(),TEXT("rhithread")) || (CVarUseIOSRHIThread.GetValueOnAnyThread() > 0);
		GSupportsParallelOcclusionQueries = false;
	}

	if (FPlatformMisc::IsDebuggerPresent() && UE_BUILD_DEBUG)
	{
#if PLATFORM_IOS // @todo zebra : needs a RENDER_API or whatever
		// Enable debug markers if we're running in Xcode
		extern int32 GEmitMeshDrawEvent;
		GEmitMeshDrawEvent = 1;
#endif
		SetEmitDrawEvents(true);
	}
	
	// Force disable vertex-shader-layer point light rendering on GPUs that don't support it properly yet.
	if(!bSupportsPointLights && !FParse::Param(FCommandLine::Get(),TEXT("metalpointlights")))
	{
		// Disable point light cubemap shadows on Mac Metal as currently they aren't supported.
		static auto CVarCubemapShadows = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowPointLightCubemapShadows"));
		if(CVarCubemapShadows && CVarCubemapShadows->GetInt() != 0)
		{
			CVarCubemapShadows->Set(0);
		}
	}
	
	if (!GSupportsVolumeTextureRendering && !FParse::Param(FCommandLine::Get(),TEXT("metaltlv")))
	{
		// Disable point light cubemap shadows on Mac Metal as currently they aren't supported.
		static auto CVarTranslucentLightingVolume = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TranslucentLightingVolume"));
		if(CVarTranslucentLightingVolume && CVarTranslucentLightingVolume->GetInt() != 0)
		{
			CVarTranslucentLightingVolume->Set(0);
		}
	}

#if PLATFORM_MAC
    if (bIsIntelHaswell)
	{
		static auto CVarForceDisableVideoPlayback = IConsoleManager::Get().FindConsoleVariable((TEXT("Fort.ForceDisableVideoPlayback")));
		if (CVarForceDisableVideoPlayback && CVarForceDisableVideoPlayback->GetInt() != 1)
		{
			CVarForceDisableVideoPlayback->Set(1);
		}
	}
#endif

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	// we don't want to auto-enable draw events in Test
	SetEmitDrawEvents(GetEmitDrawEvents() | ENABLE_METAL_GPUEVENTS);
#endif

	GSupportsShaderFramebufferFetch = !PLATFORM_MAC && GMaxRHIShaderPlatform != SP_METAL_MRT && GMaxRHIShaderPlatform != SP_METAL_MRT_TVOS;
	GSupportsShaderMRTFramebufferFetch = GSupportsShaderFramebufferFetch;
	GHardwareHiddenSurfaceRemoval = true;
	GSupportsRenderTargetFormat_PF_G8 = false;
	GRHISupportsTextureStreaming = true;
	GSupportsWideMRT = true;
	GSupportsSeparateRenderTargetBlendState = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);

	GRHISupportsPipelineFileCache = true;

#if PLATFORM_MAC
	check(Device.SupportsFeatureSet(mtlpp::FeatureSet::macOS_GPUFamily1_v1));
	GRHISupportsBaseVertexIndex = true;
	GRHISupportsFirstInstance = true; // Supported on macOS & iOS but not tvOS.
	GMaxTextureDimensions = 16384;
	GMaxCubeTextureDimensions = 16384;
	GMaxTextureArrayLayers = 2048;
	GMaxShadowDepthBufferSizeX = GMaxTextureDimensions;
	GMaxShadowDepthBufferSizeY = GMaxTextureDimensions;
    bSupportsD16 = !FParse::Param(FCommandLine::Get(),TEXT("nometalv2")) && Device.SupportsFeatureSet(mtlpp::FeatureSet::macOS_GPUFamily1_v2);
    GRHISupportsHDROutput = Device.SupportsFeatureSet(mtlpp::FeatureSet::macOS_GPUFamily1_v2);
	GRHIHDRDisplayOutputFormat = (GRHISupportsHDROutput) ? PF_PLATFORM_HDR_0 : PF_B8G8R8A8;
	// Based on the spec below, the maxTotalThreadsPerThreadgroup is not a fixed number but calculated according to the device current ability, so the available threads could less than the maximum number.
	// For safety and keep the consistency for all platform, reduce the maximum number to half of the device based.
	// https://developer.apple.com/documentation/metal/mtlcomputepipelinedescriptor/2966560-maxtotalthreadsperthreadgroup?language=objc
	GMaxWorkGroupInvocations = 512;
#else
	//@todo investigate gpufam4
	GMaxComputeSharedMemory = 1 << 14;
#if PLATFORM_TVOS
	GRHISupportsBaseVertexIndex = false;
	GRHISupportsFirstInstance = false; // Supported on macOS & iOS but not tvOS.
	GRHISupportsHDROutput = false;
	GRHIHDRDisplayOutputFormat = PF_B8G8R8A8; // must have a default value for non-hdr, just like mac or ios
#else
	// Only A9+ can support this, so for now we need to limit this to the desktop-forward renderer only.
	GRHISupportsBaseVertexIndex = Device.SupportsFeatureSet(mtlpp::FeatureSet::iOS_GPUFamily3_v1) && (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	GRHISupportsFirstInstance = GRHISupportsBaseVertexIndex;
	
	// TODO: Move this into IOSPlatform
    @autoreleasepool {
        UIScreen* mainScreen = [UIScreen mainScreen];
        UIDisplayGamut gamut = mainScreen.traitCollection.displayGamut;
        GRHISupportsHDROutput = FPlatformMisc::IOSVersionCompare(10, 0, 0) && gamut == UIDisplayGamutP3;
    }
	
	GRHIHDRDisplayOutputFormat = (GRHISupportsHDROutput) ? PF_PLATFORM_HDR_0 : PF_B8G8R8A8;
	// Based on the spec below, the maxTotalThreadsPerThreadgroup is not a fixed number but calculated according to the device current ability, so the available threads could less than the maximum number.
	// For safety and keep the consistency for all platform, reduce the maximum number to half of the device based.
	// https://developer.apple.com/documentation/metal/mtlcomputepipelinedescriptor/2966560-maxtotalthreadsperthreadgroup?language=objc
	GMaxWorkGroupInvocations = Device.SupportsFeatureSet(mtlpp::FeatureSet::iOS_GPUFamily4_v1) ? 512 : 256;
#endif
	GMaxTextureDimensions = 8192;
	GMaxCubeTextureDimensions = 8192;
	GMaxTextureArrayLayers = 2048;
	GMaxShadowDepthBufferSizeX = GMaxTextureDimensions;
	GMaxShadowDepthBufferSizeY = GMaxTextureDimensions;
#endif

	GRHIMaxDispatchThreadGroupsPerDimension.X = MAX_uint16;
	GRHIMaxDispatchThreadGroupsPerDimension.Y = MAX_uint16;
	GRHIMaxDispatchThreadGroupsPerDimension.Z = MAX_uint16;

	GMaxTextureMipCount = FPlatformMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
	GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );

	// Initialize the buffer format map - in such a way as to be able to validate it in non-shipping...
#if METAL_DEBUG_OPTIONS
	FMemory::Memset(GMetalBufferFormats, 255);
#endif
	GMetalBufferFormats[PF_Unknown              ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_A32B32G32R32F        ] = { mtlpp::PixelFormat::RGBA32Float, (uint8)EMetalBufferFormat::RGBA32Float };
	GMetalBufferFormats[PF_B8G8R8A8             ] = { mtlpp::PixelFormat::RGBA8Unorm, (uint8)EMetalBufferFormat::RGBA8Unorm }; // mtlpp::PixelFormat::BGRA8Unorm/EMetalBufferFormat::BGRA8Unorm,  < We don't support this as a vertex-format so we have code to swizzle in the shader
	GMetalBufferFormats[PF_G8                   ] = { mtlpp::PixelFormat::R8Unorm, (uint8)EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_G16                  ] = { mtlpp::PixelFormat::R16Unorm, (uint8)EMetalBufferFormat::R16Unorm };
	GMetalBufferFormats[PF_DXT1                 ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_DXT3                 ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_DXT5                 ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_UYVY                 ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_FloatRGB        	] = { mtlpp::PixelFormat::RG11B10Float, (uint8)EMetalBufferFormat::RG11B10Half };
	GMetalBufferFormats[PF_FloatRGBA            ] = { mtlpp::PixelFormat::RGBA16Float, (uint8)EMetalBufferFormat::RGBA16Half };
	GMetalBufferFormats[PF_DepthStencil         ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ShadowDepth          ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R32_FLOAT            ] = { mtlpp::PixelFormat::R32Float, (uint8)EMetalBufferFormat::R32Float };
	GMetalBufferFormats[PF_G16R16               ] = { mtlpp::PixelFormat::RG16Unorm, (uint8)EMetalBufferFormat::RG16Unorm };
	GMetalBufferFormats[PF_G16R16F              ] = { mtlpp::PixelFormat::RG16Float, (uint8)EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_G16R16F_FILTER       ] = { mtlpp::PixelFormat::RG16Float, (uint8)EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_G32R32F              ] = { mtlpp::PixelFormat::RG32Float, (uint8)EMetalBufferFormat::RG32Float };
	GMetalBufferFormats[PF_A2B10G10R10          ] = { mtlpp::PixelFormat::RGB10A2Unorm, (uint8)EMetalBufferFormat::RGB10A2Unorm };
	GMetalBufferFormats[PF_A16B16G16R16         ] = { mtlpp::PixelFormat::RGBA16Unorm, (uint8)EMetalBufferFormat::RGBA16Half };
	GMetalBufferFormats[PF_D24                  ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R16F                 ] = { mtlpp::PixelFormat::R16Float, (uint8)EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_R16F_FILTER          ] = { mtlpp::PixelFormat::R16Float, (uint8)EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_BC5                  ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_V8U8                 ] = { mtlpp::PixelFormat::RG8Snorm, (uint8)EMetalBufferFormat::RG8Unorm };
	GMetalBufferFormats[PF_A1                   ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_FloatR11G11B10       ] = { mtlpp::PixelFormat::RG11B10Float, (uint8)EMetalBufferFormat::RG11B10Half }; // < May not work on tvOS
	GMetalBufferFormats[PF_A8                   ] = { mtlpp::PixelFormat::A8Unorm, (uint8)EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_R32_UINT             ] = { mtlpp::PixelFormat::R32Uint, (uint8)EMetalBufferFormat::R32Uint };
	GMetalBufferFormats[PF_R32_SINT             ] = { mtlpp::PixelFormat::R32Sint, (uint8)EMetalBufferFormat::R32Sint };
	GMetalBufferFormats[PF_PVRTC2               ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_PVRTC4               ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R16_UINT             ] = { mtlpp::PixelFormat::R16Uint, (uint8)EMetalBufferFormat::R16Uint };
	GMetalBufferFormats[PF_R16_SINT             ] = { mtlpp::PixelFormat::R16Sint, (uint8)EMetalBufferFormat::R16Sint };
	GMetalBufferFormats[PF_R16G16B16A16_UINT    ] = { mtlpp::PixelFormat::RGBA16Uint, (uint8)EMetalBufferFormat::RGBA16Uint };
	GMetalBufferFormats[PF_R16G16B16A16_SINT    ] = { mtlpp::PixelFormat::RGBA16Sint, (uint8)EMetalBufferFormat::RGBA16Sint };
	GMetalBufferFormats[PF_R5G6B5_UNORM         ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::R5G6B5Unorm };
	GMetalBufferFormats[PF_B5G5R5A1_UNORM       ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::B5G5R5A1Unorm };
	GMetalBufferFormats[PF_R8G8B8A8             ] = { mtlpp::PixelFormat::RGBA8Unorm, (uint8)EMetalBufferFormat::RGBA8Unorm };
	GMetalBufferFormats[PF_A8R8G8B8				] = { mtlpp::PixelFormat::RGBA8Unorm, (uint8)EMetalBufferFormat::RGBA8Unorm }; // mtlpp::PixelFormat::BGRA8Unorm/EMetalBufferFormat::BGRA8Unorm,  < We don't support this as a vertex-format so we have code to swizzle in the shader
	GMetalBufferFormats[PF_BC4					] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8G8                 ] = { mtlpp::PixelFormat::RG8Unorm, (uint8)EMetalBufferFormat::RG8Unorm };
	GMetalBufferFormats[PF_ATC_RGB				] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ATC_RGBA_E			] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ATC_RGBA_I			] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_X24_G8				] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC1					] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC2_RGB				] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC2_RGBA			] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R32G32B32A32_UINT	] = { mtlpp::PixelFormat::RGBA32Uint, (uint8)EMetalBufferFormat::RGBA32Uint };
	GMetalBufferFormats[PF_R16G16_UINT			] = { mtlpp::PixelFormat::RG16Uint, (uint8)EMetalBufferFormat::RG16Uint };
	GMetalBufferFormats[PF_R32G32_UINT			] = { mtlpp::PixelFormat::RG32Uint, (uint8)EMetalBufferFormat::RG32Uint };
	GMetalBufferFormats[PF_ASTC_4x4             ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_6x6             ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_8x8             ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_10x10           ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_12x12           ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_4x4_HDR         ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_6x6_HDR         ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_8x8_HDR         ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_10x10_HDR       ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_12x12_HDR       ] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_BC6H					] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_BC7					] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8_UINT				] = { mtlpp::PixelFormat::R8Uint, (uint8)EMetalBufferFormat::R8Uint };
	GMetalBufferFormats[PF_R8					] = { mtlpp::PixelFormat::R8Unorm, (uint8)EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_L8					] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_XGXR8				] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8G8B8A8_UINT		] = { mtlpp::PixelFormat::RGBA8Uint, (uint8)EMetalBufferFormat::RGBA8Uint };
	GMetalBufferFormats[PF_R8G8B8A8_SNORM		] = { mtlpp::PixelFormat::RGBA8Snorm, (uint8)EMetalBufferFormat::RGBA8Snorm };
	GMetalBufferFormats[PF_R16G16B16A16_UNORM	] = { mtlpp::PixelFormat::RGBA16Unorm, (uint8)EMetalBufferFormat::RGBA16Unorm };
	GMetalBufferFormats[PF_R16G16B16A16_SNORM	] = { mtlpp::PixelFormat::RGBA16Snorm, (uint8)EMetalBufferFormat::RGBA16Snorm };
	GMetalBufferFormats[PF_PLATFORM_HDR_0		] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_PLATFORM_HDR_1		] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_PLATFORM_HDR_2		] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_NV12					] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	
	GMetalBufferFormats[PF_ETC2_R11_EAC			] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC2_RG11_EAC		] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
		
	GMetalBufferFormats[PF_G16R16_SNORM			] = { mtlpp::PixelFormat::RG16Snorm, (uint8)EMetalBufferFormat::RG16Snorm };
	GMetalBufferFormats[PF_R8G8_UINT			] = { mtlpp::PixelFormat::RG8Uint, (uint8)EMetalBufferFormat::RG8Uint };
	GMetalBufferFormats[PF_R32G32B32_UINT		] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R32G32B32_SINT		] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R32G32B32F			] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8_SINT				] = { mtlpp::PixelFormat::R8Sint, (uint8)EMetalBufferFormat::R8Sint };
	GMetalBufferFormats[PF_R64_UINT				] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R9G9B9EXP5			] = { mtlpp::PixelFormat::Invalid, (uint8)EMetalBufferFormat::Unknown };
	static_assert(PF_MAX == 86, "Please setup GMetalBufferFormats properly for the new pixel format");

	// Initialize the platform pixel format map.
	GPixelFormats[PF_Unknown			].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_A32B32G32R32F		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA32Float;
	GPixelFormats[PF_B8G8R8A8			].PlatformFormat	= (uint32)mtlpp::PixelFormat::BGRA8Unorm;
	GPixelFormats[PF_G8					].PlatformFormat	= (uint32)mtlpp::PixelFormat::R8Unorm;
	GPixelFormats[PF_G16				].PlatformFormat	= (uint32)mtlpp::PixelFormat::R16Unorm;
	GPixelFormats[PF_R32G32B32A32_UINT	].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA32Uint;
	GPixelFormats[PF_R16G16_UINT		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG16Uint;
	GPixelFormats[PF_R32G32_UINT		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG32Uint;

#if PLATFORM_IOS
    GPixelFormats[PF_DXT1				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_DXT1				].Supported			= false;
    GPixelFormats[PF_DXT3				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_DXT3				].Supported			= false;
    GPixelFormats[PF_DXT5				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_DXT5				].Supported			= false;
	GPixelFormats[PF_BC5				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_BC5				].Supported			= false;
	GPixelFormats[PF_PVRTC2				].PlatformFormat	= (uint32)mtlpp::PixelFormat::PVRTC_RGBA_2BPP;
	GPixelFormats[PF_PVRTC2				].Supported			= true;
	GPixelFormats[PF_PVRTC4				].PlatformFormat	= (uint32)mtlpp::PixelFormat::PVRTC_RGBA_4BPP;
	GPixelFormats[PF_PVRTC4				].Supported			= true;
	GPixelFormats[PF_PVRTC4				].PlatformFormat	= (uint32)mtlpp::PixelFormat::PVRTC_RGBA_4BPP;
	GPixelFormats[PF_PVRTC4				].Supported			= true;
	GPixelFormats[PF_ASTC_4x4			].PlatformFormat	= (uint32)mtlpp::PixelFormat::ASTC_4x4_LDR;
	GPixelFormats[PF_ASTC_4x4			].Supported			= true;
	GPixelFormats[PF_ASTC_6x6			].PlatformFormat	= (uint32)mtlpp::PixelFormat::ASTC_6x6_LDR;
	GPixelFormats[PF_ASTC_6x6			].Supported			= true;
	GPixelFormats[PF_ASTC_8x8			].PlatformFormat	= (uint32)mtlpp::PixelFormat::ASTC_8x8_LDR;
	GPixelFormats[PF_ASTC_8x8			].Supported			= true;
	GPixelFormats[PF_ASTC_10x10			].PlatformFormat	= (uint32)mtlpp::PixelFormat::ASTC_10x10_LDR;
	GPixelFormats[PF_ASTC_10x10			].Supported			= true;
	GPixelFormats[PF_ASTC_12x12			].PlatformFormat	= (uint32)mtlpp::PixelFormat::ASTC_12x12_LDR;
	GPixelFormats[PF_ASTC_12x12			].Supported			= true;

#if !PLATFORM_TVOS
	if([Device.GetPtr() supportsFamily:MTLGPUFamilyApple6])
	{
		GPixelFormats[PF_ASTC_4x4_HDR].PlatformFormat = (uint32)mtlpp::PixelFormat::ASTC_4x4_HDR;
		GPixelFormats[PF_ASTC_4x4_HDR].Supported = true;
		GPixelFormats[PF_ASTC_6x6_HDR].PlatformFormat = (uint32)mtlpp::PixelFormat::ASTC_6x6_HDR;
		GPixelFormats[PF_ASTC_6x6_HDR].Supported = true;
		GPixelFormats[PF_ASTC_8x8_HDR].PlatformFormat = (uint32)mtlpp::PixelFormat::ASTC_8x8_HDR;
		GPixelFormats[PF_ASTC_8x8_HDR].Supported = true;
		GPixelFormats[PF_ASTC_10x10_HDR].PlatformFormat = (uint32)mtlpp::PixelFormat::ASTC_10x10_HDR;
		GPixelFormats[PF_ASTC_10x10_HDR].Supported = true;
		GPixelFormats[PF_ASTC_12x12_HDR].PlatformFormat = (uint32)mtlpp::PixelFormat::ASTC_12x12_HDR;
		GPixelFormats[PF_ASTC_12x12_HDR].Supported = true;
	}
#endif
	// used with virtual textures
	GPixelFormats[PF_ETC2_RGB	  		].PlatformFormat	= (uint32)mtlpp::PixelFormat::ETC2_RGB8;
	GPixelFormats[PF_ETC2_RGB			].Supported			= true;
	GPixelFormats[PF_ETC2_RGBA	  		].PlatformFormat	= (uint32)mtlpp::PixelFormat::EAC_RGBA8;
	GPixelFormats[PF_ETC2_RGBA			].Supported			= true;
	GPixelFormats[PF_ETC2_R11_EAC	  	].PlatformFormat	= (uint32)mtlpp::PixelFormat::EAC_R11Unorm;
	GPixelFormats[PF_ETC2_R11_EAC		].Supported			= true;
	GPixelFormats[PF_ETC2_RG11_EAC		].PlatformFormat	= (uint32)mtlpp::PixelFormat::EAC_RG11Unorm;
	GPixelFormats[PF_ETC2_RG11_EAC		].Supported			= true;

	// IOS HDR format is BGR10_XR (32bits, 3 components)
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeX		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeY		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeZ		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockBytes		= 4;
	GPixelFormats[PF_PLATFORM_HDR_0		].NumComponents		= 3;
	GPixelFormats[PF_PLATFORM_HDR_0		].PlatformFormat	= (uint32)mtlpp::PixelFormat::BGR10_XR_sRGB;
	GPixelFormats[PF_PLATFORM_HDR_0		].Supported			= GRHISupportsHDROutput;
		
#if PLATFORM_TVOS
    if (!Device.SupportsFeatureSet(mtlpp::FeatureSet::tvOS_GPUFamily2_v1))
#else
	if (!Device.SupportsFeatureSet(mtlpp::FeatureSet::iOS_GPUFamily3_v2))
#endif
	{
		GPixelFormats[PF_FloatRGB			].PlatformFormat 	= (uint32)mtlpp::PixelFormat::RGBA16Float;
		GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
		GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Float;
		GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 8;
		GPixelFormats[PF_FloatR11G11B10		].Supported			= true;
	}
	else
	{
		GPixelFormats[PF_FloatRGB			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG11B10Float;
		GPixelFormats[PF_FloatRGB			].BlockBytes		= 4;
		GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG11B10Float;
		GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
		GPixelFormats[PF_FloatR11G11B10		].Supported			= true;
	}
	
		GPixelFormats[PF_DepthStencil		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float_Stencil8;
		GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;

	GPixelFormats[PF_DepthStencil		].Supported			= true;
	GPixelFormats[PF_ShadowDepth		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float;
	GPixelFormats[PF_ShadowDepth		].BlockBytes		= 4;
	GPixelFormats[PF_ShadowDepth		].Supported			= true;
		
	GPixelFormats[PF_BC5				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= (uint32)mtlpp::PixelFormat::B5G6R5Unorm;
	GPixelFormats[PF_R5G6B5_UNORM       ].Supported         = true;
	GPixelFormats[PF_B5G5R5A1_UNORM     ].PlatformFormat    = (uint32)mtlpp::PixelFormat::BGR5A1Unorm;
	GPixelFormats[PF_B5G5R5A1_UNORM     ].Supported         = true;
#else
    GPixelFormats[PF_DXT1				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC1_RGBA;
    GPixelFormats[PF_DXT3				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC2_RGBA;
    GPixelFormats[PF_DXT5				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC3_RGBA;
	
    GPixelFormats[PF_FloatRGB		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG11B10Float;
    GPixelFormats[PF_FloatRGB		].BlockBytes		= 4;

	
	GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG11B10Float;
	GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
	GPixelFormats[PF_FloatR11G11B10		].Supported			= true;
	
	// Only one HDR format for OSX.
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeX		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeY		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeZ		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockBytes		= 8;
	GPixelFormats[PF_PLATFORM_HDR_0		].NumComponents		= 4;
	GPixelFormats[PF_PLATFORM_HDR_0		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Float;
	GPixelFormats[PF_PLATFORM_HDR_0		].Supported			= GRHISupportsHDROutput;
		
	// Use Depth28_Stencil8 when it is available for consistency
	if(bSupportsD24S8)
	{
		GPixelFormats[PF_DepthStencil	].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth24Unorm_Stencil8;
		GPixelFormats[PF_DepthStencil	].bIs24BitUnormDepthStencil = true;
	}
	else
	{
		GPixelFormats[PF_DepthStencil	].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float_Stencil8;
		GPixelFormats[PF_DepthStencil	].bIs24BitUnormDepthStencil = false;
	}
	GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
	GPixelFormats[PF_DepthStencil		].Supported			= true;
	if (bSupportsD16)
	{
		GPixelFormats[PF_ShadowDepth		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth16Unorm;
		GPixelFormats[PF_ShadowDepth		].BlockBytes		= 2;
	}
	else
	{
		GPixelFormats[PF_ShadowDepth		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float;
		GPixelFormats[PF_ShadowDepth		].BlockBytes		= 4;
	}
	GPixelFormats[PF_ShadowDepth		].Supported			= true;
	if(bSupportsD24S8)
	{
		GPixelFormats[PF_D24			].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth24Unorm_Stencil8;
	}
	else
	{
		GPixelFormats[PF_D24			].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float;
	}
	GPixelFormats[PF_D24				].Supported			= true;
	GPixelFormats[PF_BC4				].Supported			= true;
	GPixelFormats[PF_BC4				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC4_RUnorm;
	GPixelFormats[PF_BC5				].Supported			= true;
	GPixelFormats[PF_BC5				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC5_RGUnorm;
	GPixelFormats[PF_BC6H				].Supported			= true;
	GPixelFormats[PF_BC6H               ].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC6H_RGBUfloat;
	GPixelFormats[PF_BC7				].Supported			= true;
	GPixelFormats[PF_BC7				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC7_RGBAUnorm;
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_B5G5R5A1_UNORM		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
#endif
	GPixelFormats[PF_UYVY				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_FloatRGBA			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Float;
	GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
    GPixelFormats[PF_X24_G8				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Stencil8;
    GPixelFormats[PF_X24_G8				].BlockBytes		= 1;
	GPixelFormats[PF_R32_FLOAT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R32Float;
	GPixelFormats[PF_G16R16				].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG16Unorm;
	GPixelFormats[PF_G16R16				].Supported			= true;
	GPixelFormats[PF_G16R16F			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG16Float;
	GPixelFormats[PF_G16R16F_FILTER		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG16Float;
	GPixelFormats[PF_G32R32F			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG32Float;
	GPixelFormats[PF_A2B10G10R10		].PlatformFormat    = (uint32)mtlpp::PixelFormat::RGB10A2Unorm;
	GPixelFormats[PF_A16B16G16R16		].PlatformFormat    = (uint32)mtlpp::PixelFormat::RGBA16Unorm;
	GPixelFormats[PF_R16F				].PlatformFormat	= (uint32)mtlpp::PixelFormat::R16Float;
	GPixelFormats[PF_R16F_FILTER		].PlatformFormat	= (uint32)mtlpp::PixelFormat::R16Float;
	GPixelFormats[PF_V8U8				].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG8Snorm;
	GPixelFormats[PF_A1					].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	// A8 does not allow writes in Metal. So we will fake it with R8.
	// If you change this you must also change the swizzle pattern in Platform.ush
	// See Texture2DSample_A8 in Common.ush and A8_SAMPLE_MASK in Platform.ush
	GPixelFormats[PF_A8					].PlatformFormat	= (uint32)mtlpp::PixelFormat::R8Unorm;
	GPixelFormats[PF_R32_UINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R32Uint;
	GPixelFormats[PF_R32_SINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R32Sint;
	GPixelFormats[PF_R16G16B16A16_UINT	].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Uint;
	GPixelFormats[PF_R16G16B16A16_SINT	].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Sint;
	GPixelFormats[PF_R8G8B8A8			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA8Unorm;
    GPixelFormats[PF_A8R8G8B8           ].PlatformFormat    = (uint32)mtlpp::PixelFormat::RGBA8Unorm;
	GPixelFormats[PF_R8G8B8A8_UINT		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA8Uint;
	GPixelFormats[PF_R8G8B8A8_SNORM		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA8Snorm;
	GPixelFormats[PF_R8G8				].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG8Unorm;
	GPixelFormats[PF_R16_SINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R16Sint;
	GPixelFormats[PF_R16_UINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R16Uint;
	GPixelFormats[PF_R8_UINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R8Uint;
	GPixelFormats[PF_R8					].PlatformFormat	= (uint32)mtlpp::PixelFormat::R8Unorm;

	GPixelFormats[PF_R16G16B16A16_UNORM ].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Unorm;
	GPixelFormats[PF_R16G16B16A16_SNORM ].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Snorm;

	GPixelFormats[PF_NV12				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_NV12				].Supported			= false;
	
	GPixelFormats[PF_G16R16_SNORM		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG16Snorm;
	GPixelFormats[PF_R8G8_UINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG8Uint;
	GPixelFormats[PF_R32G32B32_UINT		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_R32G32B32_UINT		].Supported			= false;
	GPixelFormats[PF_R32G32B32_SINT		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_R32G32B32_SINT		].Supported			= false;
	GPixelFormats[PF_R32G32B32F			].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_R32G32B32F			].Supported			= false;
	GPixelFormats[PF_R8_SINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R8Sint;
	GPixelFormats[PF_R64_UINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_R64_UINT			].Supported			= false;
	GPixelFormats[PF_R9G9B9EXP5		    ].PlatformFormat    = (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_R9G9B9EXP5		    ].Supported			= false;

#if METAL_DEBUG_OPTIONS
	for (uint32 i = 0; i < PF_MAX; i++)
	{
		checkf((NSUInteger)GMetalBufferFormats[i].LinearTextureFormat != NSUIntegerMax, TEXT("Metal linear texture format for pixel-format %s (%d) is not configured!"), GPixelFormats[i].Name, i);
		checkf(GMetalBufferFormats[i].DataFormat != 255, TEXT("Metal data buffer format for pixel-format %s (%d) is not configured!"), GPixelFormats[i].Name, i);
	}
#endif

	RHIInitDefaultPixelFormatCapabilities();

	auto AddTypedUAVSupport = [](EPixelFormat InPixelFormat)
	{
		EnumAddFlags(GPixelFormats[InPixelFormat].Capabilities, EPixelFormatCapabilities::TypedUAVLoad | EPixelFormatCapabilities::TypedUAVStore);
	};

	switch (Device.GetReadWriteTextureSupport())
	{
	case mtlpp::ReadWriteTextureTier::Tier2:
		AddTypedUAVSupport(PF_A32B32G32R32F);
		AddTypedUAVSupport(PF_R32G32B32A32_UINT);
		AddTypedUAVSupport(PF_FloatRGBA);
		AddTypedUAVSupport(PF_R16G16B16A16_UINT);
		AddTypedUAVSupport(PF_R16G16B16A16_SINT);
		AddTypedUAVSupport(PF_R8G8B8A8);
		AddTypedUAVSupport(PF_R8G8B8A8_UINT);
		AddTypedUAVSupport(PF_R16F);
		AddTypedUAVSupport(PF_R16_UINT);
		AddTypedUAVSupport(PF_R16_SINT);
		AddTypedUAVSupport(PF_R8);
		AddTypedUAVSupport(PF_R8_UINT);
		// Fall through

	case mtlpp::ReadWriteTextureTier::Tier1:
		AddTypedUAVSupport(PF_R32_FLOAT);
		AddTypedUAVSupport(PF_R32_UINT);
		AddTypedUAVSupport(PF_R32_SINT);
		// Fall through

	case mtlpp::ReadWriteTextureTier::None:
		break;
	};

#if PLATFORM_MAC
	if(GPUDesc.GPUVendorId == GRHIVendorId)
	{
		UE_LOG(LogMetal, Display,  TEXT("      Vendor ID: %d"), GPUDesc.GPUVendorId);
		UE_LOG(LogMetal, Display,  TEXT("      Device ID: %d"), GPUDesc.GPUDeviceId);
		UE_LOG(LogMetal, Display,  TEXT("      VRAM (MB): %d"), GPUDesc.GPUMemoryMB);
	}
	else
	{
		UE_LOG(LogMetal, Warning,  TEXT("GPU descriptor (%s) from IORegistry failed to match Metal (%s)"), *FString(GPUDesc.GPUName), *GRHIAdapterName);
	}
#endif

#if PLATFORM_MAC
	if (!FPlatformProcess::IsSandboxedApplication())
	{
		// Cleanup local BinaryPSOs folder as it's not used anymore.
		const FString BinaryPSOsDir = FPaths::ProjectSavedDir() / TEXT("BinaryPSOs");
		IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*BinaryPSOsDir);
	}
#endif

	((FMetalDeviceContext&)ImmediateContext.GetInternalContext()).Init();
		
	GDynamicRHI = this;
	GIsMetalInitialized = true;

	ImmediateContext.Profiler = nullptr;
#if ENABLE_METAL_GPUPROFILE
	ImmediateContext.Profiler = FMetalProfiler::CreateProfiler(ImmediateContext.Context);
	if (ImmediateContext.Profiler)
		ImmediateContext.Profiler->BeginFrame();
#endif

#if ENABLE_METAL_GPUPROFILE
		if (ImmediateContext.Profiler)
			ImmediateContext.Profiler->EndFrame();
#endif
	}
}

FMetalDynamicRHI::~FMetalDynamicRHI()
{
	check(IsInGameThread() && IsInRenderingThread());
	
	GIsMetalInitialized = false;
	GIsRHIInitialized = false;

	// Ask all initialized FRenderResources to release their RHI resources.
	FRenderResource::ReleaseRHIForAllResources();	
	
#if ENABLE_METAL_GPUPROFILE
	FMetalProfiler::DestroyProfiler();
#endif
}

FDynamicRHI::FRHICalcTextureSizeResult FMetalDynamicRHI::RHICalcTexturePlatformSize(FRHITextureDesc const& Desc, uint32 FirstMipIndex)
{
	FDynamicRHI::FRHICalcTextureSizeResult Result;
	Result.Size = Desc.CalcMemorySizeEstimate(FirstMipIndex);
	Result.Align = 0;
	return Result;
}

uint64 FMetalDynamicRHI::RHIGetMinimumAlignmentForBufferBackedSRV(EPixelFormat Format)
{
	return ImmediateContext.Context->GetDevice().GetMinimumLinearTextureAlignmentForPixelFormat((mtlpp::PixelFormat)GMetalBufferFormats[Format].LinearTextureFormat);
}

void FMetalDynamicRHI::Init()
{
	GRHICommandList.GetImmediateCommandList().InitializeImmediateContexts();

	FRenderResource::InitPreRHIResources();
	GIsRHIInitialized = true;
}

void FMetalRHIImmediateCommandContext::RHIBeginFrame()
{
	@autoreleasepool {
#if ENABLE_METAL_GPUPROFILE
	Profiler->BeginFrame();
#endif
	((FMetalDeviceContext*)Context)->BeginFrame();
	}
}

void FMetalRHICommandContext::RHIBeginFrame()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIEndFrame()
{
	@autoreleasepool {
#if ENABLE_METAL_GPUPROFILE
	Profiler->EndFrame();
#endif
	((FMetalDeviceContext*)Context)->EndFrame();
	}
}

void FMetalRHICommandContext::RHIEndFrame()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIBeginScene()
{
	@autoreleasepool {
	((FMetalDeviceContext*)Context)->BeginScene();
	}
}

void FMetalRHICommandContext::RHIBeginScene()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIEndScene()
{
	@autoreleasepool {
	((FMetalDeviceContext*)Context)->EndScene();
	}
}

void FMetalRHICommandContext::RHIEndScene()
{
	check(false);
}

void FMetalRHICommandContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
#if ENABLE_METAL_GPUEVENTS
	@autoreleasepool
	{
		FPlatformMisc::BeginNamedEvent(Color, Name);
#if ENABLE_METAL_GPUPROFILE
		Profiler->PushEvent(Name, Color);
#endif
		Context->GetCurrentRenderPass().PushDebugGroup([NSString stringWithCString:TCHAR_TO_UTF8(Name) encoding:NSUTF8StringEncoding]);
	}
#endif
}

void FMetalRHICommandContext::RHIPopEvent()
{
#if ENABLE_METAL_GPUEVENTS
	@autoreleasepool {
	FPlatformMisc::EndNamedEvent();
	Context->GetCurrentRenderPass().PopDebugGroup();
#if ENABLE_METAL_GPUPROFILE
	Profiler->PopEvent();
#endif
	}
#endif
}

void FMetalDynamicRHI::RHIGetSupportedResolution( uint32 &Width, uint32 &Height )
{
#if PLATFORM_MAC
	CGDisplayModeRef DisplayMode = FPlatformApplicationMisc::GetSupportedDisplayMode(kCGDirectMainDisplay, Width, Height);
	if (DisplayMode)
	{
		Width = CGDisplayModeGetWidth(DisplayMode);
		Height = CGDisplayModeGetHeight(DisplayMode);
		CGDisplayModeRelease(DisplayMode);
	}
#else
	UE_LOG(LogMetal, Warning,  TEXT("RHIGetSupportedResolution unimplemented!"));
#endif
}

bool FMetalDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
#if PLATFORM_MAC
	const int32 MinAllowableResolutionX = 0;
	const int32 MinAllowableResolutionY = 0;
	const int32 MaxAllowableResolutionX = 10480;
	const int32 MaxAllowableResolutionY = 10480;
	const int32 MinAllowableRefreshRate = 0;
	const int32 MaxAllowableRefreshRate = 10480;
	
	CFArrayRef AllModes = CGDisplayCopyAllDisplayModes(kCGDirectMainDisplay, NULL);
	if (AllModes)
	{
		const int32 NumModes = CFArrayGetCount(AllModes);
		const int32 Scale = (int32)FMacApplication::GetPrimaryScreenBackingScaleFactor();
		
		for (int32 Index = 0; Index < NumModes; Index++)
		{
			const CGDisplayModeRef Mode = (const CGDisplayModeRef)CFArrayGetValueAtIndex(AllModes, Index);
			const int32 Width = (int32)CGDisplayModeGetWidth(Mode) / Scale;
			const int32 Height = (int32)CGDisplayModeGetHeight(Mode) / Scale;
			const int32 RefreshRate = (int32)CGDisplayModeGetRefreshRate(Mode);
			
			if (Width >= MinAllowableResolutionX && Width <= MaxAllowableResolutionX && Height >= MinAllowableResolutionY && Height <= MaxAllowableResolutionY)
			{
				bool bAddIt = true;
				if (bIgnoreRefreshRate == false)
				{
					if (RefreshRate < MinAllowableRefreshRate || RefreshRate > MaxAllowableRefreshRate)
					{
						continue;
					}
				}
				else
				{
					// See if it is in the list already
					for (int32 CheckIndex = 0; CheckIndex < Resolutions.Num(); CheckIndex++)
					{
						FScreenResolutionRHI& CheckResolution = Resolutions[CheckIndex];
						if ((CheckResolution.Width == Width) &&
							(CheckResolution.Height == Height))
						{
							// Already in the list...
							bAddIt = false;
							break;
						}
					}
				}
				
				if (bAddIt)
				{
					// Add the mode to the list
					const int32 Temp2Index = Resolutions.AddZeroed();
					FScreenResolutionRHI& ScreenResolution = Resolutions[Temp2Index];
					
					ScreenResolution.Width = Width;
					ScreenResolution.Height = Height;
					ScreenResolution.RefreshRate = RefreshRate;
				}
			}
		}
		
		CFRelease(AllModes);
	}
	
	return true;
#else
	UE_LOG(LogMetal, Warning,  TEXT("RHIGetAvailableResolutions unimplemented!"));
	return false;
#endif
}

void FMetalDynamicRHI::RHIFlushResources()
{
	@autoreleasepool {
		((FMetalDeviceContext*)ImmediateContext.Context)->FlushFreeList(false);
		ImmediateContext.Context->SubmitCommandBufferAndWait();
		((FMetalDeviceContext*)ImmediateContext.Context)->ClearFreeList();
        ((FMetalDeviceContext*)ImmediateContext.Context)->DrainHeap();
		ImmediateContext.Context->GetCurrentState().Reset();
	}
}

void FMetalDynamicRHI::RHIAcquireThreadOwnership()
{
}

void FMetalDynamicRHI::RHIReleaseThreadOwnership()
{
}

void* FMetalDynamicRHI::RHIGetNativeDevice()
{
	return (void*)ImmediateContext.Context->GetDevice().GetPtr();
}

void* FMetalDynamicRHI::RHIGetNativeGraphicsQueue()
{
	return ImmediateContext.GetInternalContext().GetCommandQueue().GetQueue();
}

void* FMetalDynamicRHI::RHIGetNativeComputeQueue()
{
	return ImmediateContext.GetInternalContext().GetCommandQueue().GetQueue();
}

void* FMetalDynamicRHI::RHIGetNativeInstance()
{
	return nullptr;
}

uint16 FMetalDynamicRHI::RHIGetPlatformTextureMaxSampleCount()
{
	TArray<ECompositingSampleCount::Type> SamplesArray{ ECompositingSampleCount::Type::One, ECompositingSampleCount::Type::Two, ECompositingSampleCount::Type::Four, ECompositingSampleCount::Type::Eight };

	uint16 PlatformMaxSampleCount = ECompositingSampleCount::Type::One;
	for (auto sampleIt = SamplesArray.CreateConstIterator(); sampleIt; ++sampleIt)
	{
		int sample = *sampleIt;

#if PLATFORM_IOS || PLATFORM_MAC
		id<MTLDevice> Device = (id<MTLDevice>)RHIGetNativeDevice();
		check(Device);

		if (![Device supportsTextureSampleCount : sample])
		{
			break;
		}
		PlatformMaxSampleCount = sample;
#endif
	}
	return PlatformMaxSampleCount;
}

void FMetalDynamicRHI::RHIBlockUntilGPUIdle()
{
	@autoreleasepool {
	ImmediateContext.Context->SubmitCommandBufferAndWait();
	}
}

uint32 FMetalDynamicRHI::RHIGetGPUFrameCycles(uint32 GPUIndex)
{
	check(GPUIndex == 0);
	return GGPUFrameTime;
}

void FMetalDynamicRHI::RHIExecuteCommandList(FRHICommandList* RHICmdList)
{
	NOT_SUPPORTED("RHIExecuteCommandList");
}

IRHICommandContext* FMetalDynamicRHI::RHIGetDefaultContext()
{
	return &ImmediateContext;
}

IRHIComputeContext* FMetalDynamicRHI::RHIGetCommandContext(ERHIPipeline Pipeline, FRHIGPUMask GPUMask)
{
	UE_LOG(LogRHI, Fatal, TEXT("FMetalDynamicRHI::RHIGetCommandContext should never be called. Metal RHI does not implement parallel command list execution."));
	return nullptr;
}

IRHIPlatformCommandList* FMetalDynamicRHI::RHIFinalizeContext(IRHIComputeContext* Context)
{
	// "Context" will always be the default context, since we don't implement parallel execution.
	// Metal uses an immediate context, there's nothing to do here. Executed commands will have already reached the driver.

	// Returning nullptr indicates that we don't want RHISubmitCommandLists to be called.
	return nullptr;
}

void FMetalDynamicRHI::RHISubmitCommandLists(TArrayView<IRHIPlatformCommandList*> CommandLists)
{
	UE_LOG(LogRHI, Fatal, TEXT("FMetalDynamicRHI::RHISubmitCommandLists should never be called. Metal RHI does not implement parallel command list execution."));
}

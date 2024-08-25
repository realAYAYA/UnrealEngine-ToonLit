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
#include "MetalBindlessDescriptors.h"
#include "DataDrivenShaderPlatformInfo.h"
 
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
	if (Platform == SP_METAL || Platform == SP_METAL_TVOS || Platform == SP_METAL_SIM)
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
		FMessageDialog::Open(EAppMsgType::Ok, LocalizedMsg, Title);
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
			FMessageDialog::Open(EAppMsgType::Ok, 
				NSLOCTEXT("MetalRHI", "XCodeMissingInstall", "Unreal Engine requires Xcode to compile shaders for Metal. To continue, install Xcode and open it to accept the license agreement. If you install Xcode to any location other than Applications/Xcode, also run the xcode-select command-line tool to specify its location."), 
				NSLOCTEXT("MetalRHI", "XCodeMissingInstallTitle", "Xcode Not Found"));
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

    MTL_SCOPED_AUTORELEASE_POOL;
    
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
	MTL::Device* Device = ImmediateContext.Context->GetDevice();
		
#if PLATFORM_IOS
    bool bSupportAppleA8 = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportAppleA8"), bSupportAppleA8, GEngineIni);
        
    bool bIsA8FeatureSet = false;
        
#if PLATFORM_TVOS
	GRHISupportsDrawIndirect = Device->supportsFeatureSet(MTL::FeatureSet_tvOS_GPUFamily2_v1);
	GRHISupportsPixelShaderUAVs = Device->supportsFeatureSet(MTL::FeatureSet_tvOS_GPUFamily2_v1);
        
    if (!Device->supportsFeatureSet(MTL::FeatureSet_tvOS_GPUFamily2_v1))
    {
        bIsA8FeatureSet = true;
    }
        
#else
	if (!Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily3_v1))
	{
        bIsA8FeatureSet = true;
    }
    
	GRHISupportsRWTextureBuffers = Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily4_v1);
	GRHISupportsDrawIndirect = Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily3_v1);
	GRHISupportsPixelShaderUAVs = Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily3_v1);

	const MTL::FeatureSet FeatureSets[] = {
        MTL::FeatureSet_iOS_GPUFamily1_v1,
        MTL::FeatureSet_iOS_GPUFamily2_v1,
        MTL::FeatureSet_iOS_GPUFamily3_v1,
        MTL::FeatureSet_iOS_GPUFamily4_v1
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
		if (FPlatformMisc::IOSVersionCompare(FeatureSetVersions[i][0],FeatureSetVersions[i][1],FeatureSetVersions[i][2]) >= 0 && Device->supportsFeatureSet(FeatureSets[i]))
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
            UE_LOG(LogMetal, Warning, TEXT("Mobile Virtual Textures require a minimum of the Apple A9 feature set."));
        }
    }
        
    bool bProjectSupportsMRTs = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bProjectSupportsMRTs, GEngineIni);

	bool const bRequestedMetalMRT = ((RequestedFeatureLevel >= ERHIFeatureLevel::SM5) || (!bRequestedFeatureLevel && FParse::Param(FCommandLine::Get(),TEXT("metalmrt"))));

    // Only allow SM5 MRT on A9 or above devices
    if (bProjectSupportsMRTs && bRequestedMetalMRT && !bIsA8FeatureSet)
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
#if WITH_IOS_SIMULATOR
		ValidateTargetedRHIFeatureLevelExists(SP_METAL_SIM);
		GMaxRHIShaderPlatform = SP_METAL_SIM;
#else
		ValidateTargetedRHIFeatureLevelExists(SP_METAL);
		GMaxRHIShaderPlatform = SP_METAL;
#endif	// WITH_IOS_SIMULATOR
#endif	// PLATFORM_TVOS
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
#if WITH_IOS_SIMULATOR
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL_SIM;
#else
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL;
#endif
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
	
	GRHIAdapterName = NSStringToFString(Device->name());
	
	// However they don't all support other features depending on the version of the OS.
	bool bSupportsTiledReflections = false;
	bool bSupportsDistanceFields = false;
	
    bool bSupportsSM6 = false;
	bool bSupportsSM5 = true;
	bool bIsIntelHaswell = false;
	
	GSupportsTimestampRenderQueries = true;
	
    checkf(!GRHIAdapterName.Contains("Nvidia"), TEXT("NVIDIA GPU's are no longer supported in UE 5.4 and above"));
    
	if(GRHIAdapterName.Contains("ATi") || GRHIAdapterName.Contains("AMD"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = (uint32)EGpuVendorId::Amd;
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
		GRHIVendorId = (uint32)EGpuVendorId::Intel;
		bSupportsDistanceFields = true;
		bIsIntelHaswell = (GRHIAdapterName == TEXT("Intel HD Graphics 5000") || GRHIAdapterName == TEXT("Intel Iris Graphics") || GRHIAdapterName == TEXT("Intel Iris Pro Graphics"));
		GRHISupportsWaveOperations = false;
	}
	else if(GRHIAdapterName.Contains("Apple"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = (uint32)EGpuVendorId::Apple;
		bSupportsTiledReflections = true;
		bSupportsDistanceFields = true;
		GSupportsTimestampRenderQueries = true;
		
		GRHISupportsWaveOperations = true;
		GRHIMinimumWaveSize = 32;
		GRHIMaximumWaveSize = 32;

		bSupportsSM6 = !GRHIAdapterName.Contains("M1");
        if(bSupportsSM6)
        {
            // Int64 atomic support was introduced with M2 devices.
            GRHISupportsAtomicUInt64 = bSupportsSM6;
            GRHIPersistentThreadGroupCount = 1024;
            
            // Disable persistent threads on Apple Silicon (as it doesn't support forward progress guarantee).
            IConsoleVariable* NanitePersistentThreadCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.PersistentThreadsCulling"));
            if (NanitePersistentThreadCVar != nullptr && NanitePersistentThreadCVar->GetInt() == 1)
            {
                NanitePersistentThreadCVar->Set(0);
            }
        }
	}

    bool const bRequestedSM6 = RequestedFeatureLevel == ERHIFeatureLevel::SM6 ||
                               (!bRequestedFeatureLevel && (FParse::Param(FCommandLine::Get(),TEXT("metalsm6"))));
        
	bool const bRequestedSM5 = RequestedFeatureLevel == ERHIFeatureLevel::SM5 ||
                               (!bRequestedFeatureLevel && (FParse::Param(FCommandLine::Get(),TEXT("metalsm5")) || FParse::Param(FCommandLine::Get(),TEXT("metalmrt"))));
                                
    if(bSupportsSM6 && bRequestedSM6)
    {
        GMaxRHIFeatureLevel = ERHIFeatureLevel::SM6;
        GMaxRHIShaderPlatform = SP_METAL_SM6;
		
		GRHIGlobals.SupportsNative16BitOps = true;
    }
	else if(bSupportsSM5 && bRequestedSM5)
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

    GRHIBindlessSupport = RHIGetBindlessSupport(GMaxRHIShaderPlatform);
	
	if(GRHIAdapterName.Contains("Apple"))
	{
		if(GRHIBindlessSupport == ERHIBindlessSupport::Unsupported)
		{
			// Switch back to single page allocation for VSM (Metal does not support atomic operations on Texture2DArrays...).
			IConsoleVariable* VSMCacheStaticSeparateCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Cache.StaticSeparate"));
			if (VSMCacheStaticSeparateCVar != nullptr)
			{
				VSMCacheStaticSeparateCVar->Set(0);
			}
		}
	}

#if PLATFORM_SUPPORTS_MESH_SHADERS
    GRHISupportsMeshShadersTier0 = RHISupportsMeshShadersTier0(GMaxRHIShaderPlatform);
    GRHISupportsMeshShadersTier1 = RHISupportsMeshShadersTier1(GMaxRHIShaderPlatform);
#endif

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
    GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM6] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM6) ? GMaxRHIShaderPlatform : SP_NumPlatforms;
	
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
	bSupportsD24S8 = Device->depth24Stencil8PixelFormatSupported();
	
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
	if ((GMaxRHIShaderPlatform == SP_METAL_SM5 || GMaxRHIShaderPlatform == SP_METAL_SM6) && !bSupportsDistanceFields && !FParse::Param(FCommandLine::Get(),TEXT("metaldistancefields")))
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
    if (Device->supportsFeatureSet(MTL::FeatureSet_macOS_GPUFamily1_v3))
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
	}
	else
	{
		GRHISupportsRHIThread = FParse::Param(FCommandLine::Get(),TEXT("rhithread")) || (CVarUseIOSRHIThread.GetValueOnAnyThread() > 0);
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
	
	// Appears to be no queryable value for max texture_buffer size and its not specified in the docs.  However,
	// current testing across Apple Silicon macs, AMD and iPhone all quote a max value of 268435456 (1 << 28)
	// from the Metal validation layer.  For certain pixel formats this appears to be larger than max buffer
	// size on some devices.  Due to the way we are using texture_buffers allocated from a backing buffer,
	// keep this safe by using a step down.  This is the current default value anyway but set here too in case that changes.
	GMaxBufferDimensions = 1 << 27;

#if PLATFORM_MAC
	check(Device->supportsFeatureSet(MTL::FeatureSet_macOS_GPUFamily1_v1));
	GRHISupportsBaseVertexIndex = true;
	GRHISupportsFirstInstance = true; // Supported on macOS & iOS but not tvOS.
	GMaxTextureDimensions = 16384;
	GMaxCubeTextureDimensions = 16384;
	GMaxTextureArrayLayers = 2048;
	GMaxShadowDepthBufferSizeX = GMaxTextureDimensions;
	GMaxShadowDepthBufferSizeY = GMaxTextureDimensions;
    bSupportsD16 = !FParse::Param(FCommandLine::Get(),TEXT("nometalv2")) && Device->supportsFeatureSet(MTL::FeatureSet_macOS_GPUFamily1_v2);
    GRHISupportsHDROutput = Device->supportsFeatureSet(MTL::FeatureSet_macOS_GPUFamily1_v2);
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
#elif PLATFORM_VISIONOS
	GRHISupportsBaseVertexIndex = true;
	GRHISupportsFirstInstance = GRHISupportsBaseVertexIndex;
	GRHISupportsHDROutput = true;
	GRHIHDRDisplayOutputFormat = (GRHISupportsHDROutput) ? PF_PLATFORM_HDR_0 : PF_B8G8R8A8;
	GMaxWorkGroupInvocations = 512;
#else
	// Only A9+ can support this, so for now we need to limit this to the desktop-forward renderer only.
	GRHISupportsBaseVertexIndex = Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily3_v1) && (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	GRHISupportsFirstInstance = GRHISupportsBaseVertexIndex;
	
	// TODO: Move this into IOSPlatform
    {
        MTL_SCOPED_AUTORELEASE_POOL;
        UIScreen* mainScreen = [UIScreen mainScreen];
        UIDisplayGamut gamut = mainScreen.traitCollection.displayGamut;
        GRHISupportsHDROutput = FPlatformMisc::IOSVersionCompare(10, 0, 0) && gamut == UIDisplayGamutP3;
    }
	
	GRHIHDRDisplayOutputFormat = (GRHISupportsHDROutput) ? PF_PLATFORM_HDR_0 : PF_B8G8R8A8;
	// Based on the spec below, the maxTotalThreadsPerThreadgroup is not a fixed number but calculated according to the device current ability, so the available threads could less than the maximum number.
	// For safety and keep the consistency for all platform, reduce the maximum number to half of the device based.
	// https://developer.apple.com/documentation/metal/mtlcomputepipelinedescriptor/2966560-maxtotalthreadsperthreadgroup?language=objc
	GMaxWorkGroupInvocations = Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily4_v1) ? 512 : 256;
#endif
	GMaxTextureDimensions = 8192;
	GMaxCubeTextureDimensions = 8192;
	GMaxTextureArrayLayers = 2048;
	GMaxShadowDepthBufferSizeX = GMaxTextureDimensions;
	GMaxShadowDepthBufferSizeY = GMaxTextureDimensions;
#endif

    if(Device->supportsFamily(MTL::GPUFamilyApple6) ||
       Device->supportsFamily(MTL::GPUFamilyMac2))
    {
        GRHISupportsArrayIndexFromAnyShader = true;
    }
            
	GRHIMaxDispatchThreadGroupsPerDimension.X = MAX_uint16;
	GRHIMaxDispatchThreadGroupsPerDimension.Y = MAX_uint16;
	GRHIMaxDispatchThreadGroupsPerDimension.Z = MAX_uint16;

	GMaxTextureMipCount = FPlatformMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
	GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );

	// Initialize the buffer format map - in such a way as to be able to validate it in non-shipping...
#if METAL_DEBUG_OPTIONS
	FMemory::Memset(GMetalBufferFormats, 255);
#endif
	GMetalBufferFormats[PF_Unknown              ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_A32B32G32R32F        ] = { MTL::PixelFormatRGBA32Float, (uint8)EMetalBufferFormat::RGBA32Float };
	GMetalBufferFormats[PF_B8G8R8A8             ] = { MTL::PixelFormatRGBA8Unorm, (uint8)EMetalBufferFormat::RGBA8Unorm }; // MTL::PixelFormatBGRA8Unorm/EMetalBufferFormat::BGRA8Unorm,  < We don't support this as a vertex-format so we have code to swizzle in the shader
	GMetalBufferFormats[PF_G8                   ] = { MTL::PixelFormatR8Unorm, (uint8)EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_G16                  ] = { MTL::PixelFormatR16Unorm, (uint8)EMetalBufferFormat::R16Unorm };
	GMetalBufferFormats[PF_DXT1                 ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_DXT3                 ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_DXT5                 ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_UYVY                 ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_FloatRGB        	] = { MTL::PixelFormatRG11B10Float, (uint8)EMetalBufferFormat::RG11B10Half };
	GMetalBufferFormats[PF_FloatRGBA            ] = { MTL::PixelFormatRGBA16Float, (uint8)EMetalBufferFormat::RGBA16Half };
	GMetalBufferFormats[PF_DepthStencil         ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ShadowDepth          ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R32_FLOAT            ] = { MTL::PixelFormatR32Float, (uint8)EMetalBufferFormat::R32Float };
	GMetalBufferFormats[PF_G16R16               ] = { MTL::PixelFormatRG16Unorm, (uint8)EMetalBufferFormat::RG16Unorm };
	GMetalBufferFormats[PF_G16R16F              ] = { MTL::PixelFormatRG16Float, (uint8)EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_G16R16F_FILTER       ] = { MTL::PixelFormatRG16Float, (uint8)EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_G32R32F              ] = { MTL::PixelFormatRG32Float, (uint8)EMetalBufferFormat::RG32Float };
	GMetalBufferFormats[PF_A2B10G10R10          ] = { MTL::PixelFormatRGB10A2Unorm, (uint8)EMetalBufferFormat::RGB10A2Unorm };
	GMetalBufferFormats[PF_A16B16G16R16         ] = { MTL::PixelFormatRGBA16Unorm, (uint8)EMetalBufferFormat::RGBA16Half };
	GMetalBufferFormats[PF_D24                  ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R16F                 ] = { MTL::PixelFormatR16Float, (uint8)EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_R16F_FILTER          ] = { MTL::PixelFormatR16Float, (uint8)EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_BC5                  ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_V8U8                 ] = { MTL::PixelFormatRG8Snorm, (uint8)EMetalBufferFormat::RG8Unorm };
	GMetalBufferFormats[PF_A1                   ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_FloatR11G11B10       ] = { MTL::PixelFormatRG11B10Float, (uint8)EMetalBufferFormat::RG11B10Half }; // < May not work on tvOS
	GMetalBufferFormats[PF_A8                   ] = { MTL::PixelFormatA8Unorm, (uint8)EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_R32_UINT             ] = { MTL::PixelFormatR32Uint, (uint8)EMetalBufferFormat::R32Uint };
	GMetalBufferFormats[PF_R32_SINT             ] = { MTL::PixelFormatR32Sint, (uint8)EMetalBufferFormat::R32Sint };
	GMetalBufferFormats[PF_PVRTC2               ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_PVRTC4               ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R16_UINT             ] = { MTL::PixelFormatR16Uint, (uint8)EMetalBufferFormat::R16Uint };
	GMetalBufferFormats[PF_R16_SINT             ] = { MTL::PixelFormatR16Sint, (uint8)EMetalBufferFormat::R16Sint };
	GMetalBufferFormats[PF_R16G16B16A16_UINT    ] = { MTL::PixelFormatRGBA16Uint, (uint8)EMetalBufferFormat::RGBA16Uint };
	GMetalBufferFormats[PF_R16G16B16A16_SINT    ] = { MTL::PixelFormatRGBA16Sint, (uint8)EMetalBufferFormat::RGBA16Sint };
	GMetalBufferFormats[PF_R5G6B5_UNORM         ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::R5G6B5Unorm };
	GMetalBufferFormats[PF_B5G5R5A1_UNORM       ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::B5G5R5A1Unorm };
	GMetalBufferFormats[PF_R8G8B8A8             ] = { MTL::PixelFormatRGBA8Unorm, (uint8)EMetalBufferFormat::RGBA8Unorm };
	GMetalBufferFormats[PF_A8R8G8B8				] = { MTL::PixelFormatRGBA8Unorm, (uint8)EMetalBufferFormat::RGBA8Unorm }; // MTL::PixelFormatBGRA8Unorm/EMetalBufferFormat::BGRA8Unorm,  < We don't support this as a vertex-format so we have code to swizzle in the shader
	GMetalBufferFormats[PF_BC4					] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8G8                 ] = { MTL::PixelFormatRG8Unorm, (uint8)EMetalBufferFormat::RG8Unorm };
	GMetalBufferFormats[PF_ATC_RGB				] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ATC_RGBA_E			] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ATC_RGBA_I			] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_X24_G8				] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC1					] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC2_RGB				] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC2_RGBA			] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R32G32B32A32_UINT	] = { MTL::PixelFormatRGBA32Uint, (uint8)EMetalBufferFormat::RGBA32Uint };
	GMetalBufferFormats[PF_R16G16_UINT			] = { MTL::PixelFormatRG16Uint, (uint8)EMetalBufferFormat::RG16Uint };
	GMetalBufferFormats[PF_R32G32_UINT			] = { MTL::PixelFormatRG32Uint, (uint8)EMetalBufferFormat::RG32Uint };
	GMetalBufferFormats[PF_ASTC_4x4             ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_6x6             ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_8x8             ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_10x10           ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_12x12           ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_4x4_HDR         ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_6x6_HDR         ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_8x8_HDR         ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_10x10_HDR       ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_12x12_HDR       ] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_BC6H					] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_BC7					] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8_UINT				] = { MTL::PixelFormatR8Uint, (uint8)EMetalBufferFormat::R8Uint };
	GMetalBufferFormats[PF_R8					] = { MTL::PixelFormatR8Unorm, (uint8)EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_L8					] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_XGXR8				] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8G8B8A8_UINT		] = { MTL::PixelFormatRGBA8Uint, (uint8)EMetalBufferFormat::RGBA8Uint };
	GMetalBufferFormats[PF_R8G8B8A8_SNORM		] = { MTL::PixelFormatRGBA8Snorm, (uint8)EMetalBufferFormat::RGBA8Snorm };
	GMetalBufferFormats[PF_R16G16B16A16_UNORM	] = { MTL::PixelFormatRGBA16Unorm, (uint8)EMetalBufferFormat::RGBA16Unorm };
	GMetalBufferFormats[PF_R16G16B16A16_SNORM	] = { MTL::PixelFormatRGBA16Snorm, (uint8)EMetalBufferFormat::RGBA16Snorm };
	GMetalBufferFormats[PF_PLATFORM_HDR_0		] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_PLATFORM_HDR_1		] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_PLATFORM_HDR_2		] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_NV12					] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	
	GMetalBufferFormats[PF_ETC2_R11_EAC			] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC2_RG11_EAC		] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
		
	GMetalBufferFormats[PF_G16R16_SNORM			] = { MTL::PixelFormatRG16Snorm, (uint8)EMetalBufferFormat::RG16Snorm };
	GMetalBufferFormats[PF_R8G8_UINT			] = { MTL::PixelFormatRG8Uint, (uint8)EMetalBufferFormat::RG8Uint };
	GMetalBufferFormats[PF_R32G32B32_UINT		] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R32G32B32_SINT		] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R32G32B32F			] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8_SINT				] = { MTL::PixelFormatR8Sint, (uint8)EMetalBufferFormat::R8Sint };
	GMetalBufferFormats[PF_R64_UINT				] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R9G9B9EXP5			] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_P010					] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_4x4_NORM_RG		] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_6x6_NORM_RG		] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_8x8_NORM_RG		] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_10x10_NORM_RG	] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_12x12_NORM_RG	] = { MTL::PixelFormatInvalid, (uint8)EMetalBufferFormat::Unknown };
	static_assert(PF_MAX == 92, "Please setup GMetalBufferFormats properly for the new pixel format");

	// Initialize the platform pixel format map.
	GPixelFormats[PF_Unknown			].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_A32B32G32R32F		].PlatformFormat	= (uint32)MTL::PixelFormatRGBA32Float;
	GPixelFormats[PF_B8G8R8A8			].PlatformFormat	= (uint32)MTL::PixelFormatBGRA8Unorm;
	GPixelFormats[PF_G8					].PlatformFormat	= (uint32)MTL::PixelFormatR8Unorm;
	GPixelFormats[PF_G16				].PlatformFormat	= (uint32)MTL::PixelFormatR16Unorm;
	GPixelFormats[PF_R32G32B32A32_UINT	].PlatformFormat	= (uint32)MTL::PixelFormatRGBA32Uint;
	GPixelFormats[PF_R16G16_UINT		].PlatformFormat	= (uint32)MTL::PixelFormatRG16Uint;
	GPixelFormats[PF_R32G32_UINT		].PlatformFormat	= (uint32)MTL::PixelFormatRG32Uint;

#if PLATFORM_IOS
    GPixelFormats[PF_DXT1				].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_DXT1				].Supported			= false;
    GPixelFormats[PF_DXT3				].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_DXT3				].Supported			= false;
    GPixelFormats[PF_DXT5				].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_DXT5				].Supported			= false;
	GPixelFormats[PF_BC4				].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_BC4				].Supported			= false;
	GPixelFormats[PF_BC5				].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_BC5				].Supported			= false;
	GPixelFormats[PF_BC6H				].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_BC6H				].Supported			= false;
	GPixelFormats[PF_BC7				].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_BC7				].Supported			= false;
	GPixelFormats[PF_PVRTC2				].PlatformFormat	= (uint32)MTL::PixelFormatPVRTC_RGBA_2BPP;
	GPixelFormats[PF_PVRTC2				].Supported			= true;
	GPixelFormats[PF_PVRTC4				].PlatformFormat	= (uint32)MTL::PixelFormatPVRTC_RGBA_4BPP;
	GPixelFormats[PF_PVRTC4				].Supported			= true;
	GPixelFormats[PF_PVRTC4				].PlatformFormat	= (uint32)MTL::PixelFormatPVRTC_RGBA_4BPP;
	GPixelFormats[PF_PVRTC4				].Supported			= true;
	GPixelFormats[PF_ASTC_4x4			].PlatformFormat	= (uint32)MTL::PixelFormatASTC_4x4_LDR;
	GPixelFormats[PF_ASTC_4x4			].Supported			= true;
	GPixelFormats[PF_ASTC_6x6			].PlatformFormat	= (uint32)MTL::PixelFormatASTC_6x6_LDR;
	GPixelFormats[PF_ASTC_6x6			].Supported			= true;
	GPixelFormats[PF_ASTC_8x8			].PlatformFormat	= (uint32)MTL::PixelFormatASTC_8x8_LDR;
	GPixelFormats[PF_ASTC_8x8			].Supported			= true;
	GPixelFormats[PF_ASTC_10x10			].PlatformFormat	= (uint32)MTL::PixelFormatASTC_10x10_LDR;
	GPixelFormats[PF_ASTC_10x10			].Supported			= true;
	GPixelFormats[PF_ASTC_12x12			].PlatformFormat	= (uint32)MTL::PixelFormatASTC_12x12_LDR;
	GPixelFormats[PF_ASTC_12x12			].Supported			= true;

#if !PLATFORM_TVOS
	if(Device->supportsFamily(MTL::GPUFamilyApple6))
	{
		GPixelFormats[PF_ASTC_4x4_HDR].PlatformFormat = (uint32)MTL::PixelFormatASTC_4x4_HDR;
		GPixelFormats[PF_ASTC_4x4_HDR].Supported = true;
		GPixelFormats[PF_ASTC_6x6_HDR].PlatformFormat = (uint32)MTL::PixelFormatASTC_6x6_HDR;
		GPixelFormats[PF_ASTC_6x6_HDR].Supported = true;
		GPixelFormats[PF_ASTC_8x8_HDR].PlatformFormat = (uint32)MTL::PixelFormatASTC_8x8_HDR;
		GPixelFormats[PF_ASTC_8x8_HDR].Supported = true;
		GPixelFormats[PF_ASTC_10x10_HDR].PlatformFormat = (uint32)MTL::PixelFormatASTC_10x10_HDR;
		GPixelFormats[PF_ASTC_10x10_HDR].Supported = true;
		GPixelFormats[PF_ASTC_12x12_HDR].PlatformFormat = (uint32)MTL::PixelFormatASTC_12x12_HDR;
		GPixelFormats[PF_ASTC_12x12_HDR].Supported = true;
	}
#endif
	// used with virtual textures
	GPixelFormats[PF_ETC2_RGB	  		].PlatformFormat	= (uint32)MTL::PixelFormatETC2_RGB8;
	GPixelFormats[PF_ETC2_RGB			].Supported			= true;
	GPixelFormats[PF_ETC2_RGBA	  		].PlatformFormat	= (uint32)MTL::PixelFormatEAC_RGBA8;
	GPixelFormats[PF_ETC2_RGBA			].Supported			= true;
	GPixelFormats[PF_ETC2_R11_EAC	  	].PlatformFormat	= (uint32)MTL::PixelFormatEAC_R11Unorm;
	GPixelFormats[PF_ETC2_R11_EAC		].Supported			= true;
	GPixelFormats[PF_ETC2_RG11_EAC		].PlatformFormat	= (uint32)MTL::PixelFormatEAC_RG11Unorm;
	GPixelFormats[PF_ETC2_RG11_EAC		].Supported			= true;

	// IOS HDR format is BGR10_XR (32bits, 3 components)
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeX		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeY		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeZ		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockBytes		= 4;
	GPixelFormats[PF_PLATFORM_HDR_0		].NumComponents		= 3;
	GPixelFormats[PF_PLATFORM_HDR_0		].PlatformFormat	= (uint32)MTL::PixelFormatBGR10_XR_sRGB;
	GPixelFormats[PF_PLATFORM_HDR_0		].Supported			= GRHISupportsHDROutput;
		
#if PLATFORM_TVOS
    if (!Device->supportsFeatureSet(MTL::FeatureSet_tvOS_GPUFamily2_v1))
#else
	if (!Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily3_v2))
#endif
	{
		GPixelFormats[PF_FloatRGB			].PlatformFormat 	= (uint32)MTL::PixelFormatRGBA16Float;
		GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
		GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= (uint32)MTL::PixelFormatRGBA16Float;
		GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 8;
		GPixelFormats[PF_FloatR11G11B10		].Supported			= true;
	}
	else
	{
		GPixelFormats[PF_FloatRGB			].PlatformFormat	= (uint32)MTL::PixelFormatRG11B10Float;
		GPixelFormats[PF_FloatRGB			].BlockBytes		= 4;
		GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= (uint32)MTL::PixelFormatRG11B10Float;
		GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
		GPixelFormats[PF_FloatR11G11B10		].Supported			= true;
	}
	
		GPixelFormats[PF_DepthStencil		].PlatformFormat	= (uint32)MTL::PixelFormatDepth32Float_Stencil8;
		GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;

	GPixelFormats[PF_DepthStencil		].Supported			= true;
	GPixelFormats[PF_ShadowDepth		].PlatformFormat	= (uint32)MTL::PixelFormatDepth16Unorm;
	GPixelFormats[PF_ShadowDepth		].BlockBytes		= 2;
	GPixelFormats[PF_ShadowDepth		].Supported			= true;
	GPixelFormats[PF_D24				].PlatformFormat	= (uint32)MTL::PixelFormatDepth32Float;
	GPixelFormats[PF_D24				].Supported			= true;
		
	GPixelFormats[PF_BC5				].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= (uint32)MTL::PixelFormatB5G6R5Unorm;
	GPixelFormats[PF_R5G6B5_UNORM       ].Supported         = true;
	GPixelFormats[PF_B5G5R5A1_UNORM     ].PlatformFormat    = (uint32)MTL::PixelFormatBGR5A1Unorm;
	GPixelFormats[PF_B5G5R5A1_UNORM     ].Supported         = true;
#else
    GPixelFormats[PF_DXT1				].PlatformFormat	= (uint32)MTL::PixelFormatBC1_RGBA;
    GPixelFormats[PF_DXT3				].PlatformFormat	= (uint32)MTL::PixelFormatBC2_RGBA;
    GPixelFormats[PF_DXT5				].PlatformFormat	= (uint32)MTL::PixelFormatBC3_RGBA;
	
    GPixelFormats[PF_FloatRGB		].PlatformFormat	= (uint32)MTL::PixelFormatRG11B10Float;
    GPixelFormats[PF_FloatRGB		].BlockBytes		= 4;

	
	GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= (uint32)MTL::PixelFormatRG11B10Float;
	GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
	GPixelFormats[PF_FloatR11G11B10		].Supported			= true;
	
	// Only one HDR format for OSX.
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeX		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeY		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeZ		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockBytes		= 8;
	GPixelFormats[PF_PLATFORM_HDR_0		].NumComponents		= 4;
	GPixelFormats[PF_PLATFORM_HDR_0		].PlatformFormat	= (uint32)MTL::PixelFormatRGBA16Float;
	GPixelFormats[PF_PLATFORM_HDR_0		].Supported			= GRHISupportsHDROutput;
		
	// Use Depth28_Stencil8 when it is available for consistency
	if(bSupportsD24S8)
	{
		GPixelFormats[PF_DepthStencil	].PlatformFormat	= (uint32)MTL::PixelFormatDepth24Unorm_Stencil8;
		GPixelFormats[PF_DepthStencil	].bIs24BitUnormDepthStencil = true;
	}
	else
	{
		GPixelFormats[PF_DepthStencil	].PlatformFormat	= (uint32)MTL::PixelFormatDepth32Float_Stencil8;
		GPixelFormats[PF_DepthStencil	].bIs24BitUnormDepthStencil = false;
	}
	GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
	GPixelFormats[PF_DepthStencil		].Supported			= true;
	if (bSupportsD16)
	{
		GPixelFormats[PF_ShadowDepth		].PlatformFormat	= (uint32)MTL::PixelFormatDepth16Unorm;
		GPixelFormats[PF_ShadowDepth		].BlockBytes		= 2;
	}
	else
	{
		GPixelFormats[PF_ShadowDepth		].PlatformFormat	= (uint32)MTL::PixelFormatDepth32Float;
		GPixelFormats[PF_ShadowDepth		].BlockBytes		= 4;
	}
	GPixelFormats[PF_ShadowDepth		].Supported			= true;
	if(bSupportsD24S8)
	{
		GPixelFormats[PF_D24			].PlatformFormat	= (uint32)MTL::PixelFormatDepth24Unorm_Stencil8;
	}
	else
	{
		GPixelFormats[PF_D24			].PlatformFormat	= (uint32)MTL::PixelFormatDepth32Float;
	}
	GPixelFormats[PF_D24				].Supported			= true;
	GPixelFormats[PF_BC4				].Supported			= true;
	GPixelFormats[PF_BC4				].PlatformFormat	= (uint32)MTL::PixelFormatBC4_RUnorm;
	GPixelFormats[PF_BC5				].Supported			= true;
	GPixelFormats[PF_BC5				].PlatformFormat	= (uint32)MTL::PixelFormatBC5_RGUnorm;
	GPixelFormats[PF_BC6H				].Supported			= true;
	GPixelFormats[PF_BC6H               ].PlatformFormat	= (uint32)MTL::PixelFormatBC6H_RGBUfloat;
	GPixelFormats[PF_BC7				].Supported			= true;
	GPixelFormats[PF_BC7				].PlatformFormat	= (uint32)MTL::PixelFormatBC7_RGBAUnorm;
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_B5G5R5A1_UNORM		].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
#endif
	GPixelFormats[PF_UYVY				].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_FloatRGBA			].PlatformFormat	= (uint32)MTL::PixelFormatRGBA16Float;
	GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
    GPixelFormats[PF_X24_G8				].PlatformFormat	= (uint32)MTL::PixelFormatStencil8;
    GPixelFormats[PF_X24_G8				].BlockBytes		= 1;
	GPixelFormats[PF_X24_G8             ].Supported         = true;
	
    GPixelFormats[PF_R32_FLOAT			].PlatformFormat	= (uint32)MTL::PixelFormatR32Float;
#if PLATFORM_MAC
    if(Device->supportsFeatureSet(MTL::FeatureSet_macOS_GPUFamily2_v1))
    {
        EnumAddFlags(GPixelFormats[PF_R32_FLOAT].Capabilities, EPixelFormatCapabilities::TextureFilterable);
    }
#endif
        
	GPixelFormats[PF_G16R16				].PlatformFormat	= (uint32)MTL::PixelFormatRG16Unorm;
	GPixelFormats[PF_G16R16				].Supported			= true;
#if PLATFORM_MAC
    if(Device->supportsFeatureSet(MTL::FeatureSet_macOS_GPUFamily2_v1))
    {
        EnumAddFlags(GPixelFormats[PF_G16R16].Capabilities, EPixelFormatCapabilities::TextureFilterable);
    }
#endif
        
    GPixelFormats[PF_G16R16F			].PlatformFormat	= (uint32)MTL::PixelFormatRG16Float;
	GPixelFormats[PF_G16R16F_FILTER		].PlatformFormat	= (uint32)MTL::PixelFormatRG16Float;
	GPixelFormats[PF_G32R32F			].PlatformFormat	= (uint32)MTL::PixelFormatRG32Float;
	GPixelFormats[PF_A2B10G10R10		].PlatformFormat    = (uint32)MTL::PixelFormatRGB10A2Unorm;
	GPixelFormats[PF_A16B16G16R16		].PlatformFormat    = (uint32)MTL::PixelFormatRGBA16Unorm;
	GPixelFormats[PF_R16F				].PlatformFormat	= (uint32)MTL::PixelFormatR16Float;
	GPixelFormats[PF_R16F_FILTER		].PlatformFormat	= (uint32)MTL::PixelFormatR16Float;
	GPixelFormats[PF_V8U8				].PlatformFormat	= (uint32)MTL::PixelFormatRG8Snorm;
	GPixelFormats[PF_A1					].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	// A8 does not allow writes in Metal. So we will fake it with R8.
	// If you change this you must also change the swizzle pattern in Platform.ush
	// See Texture2DSample_A8 in Common.ush and A8_SAMPLE_MASK in Platform.ush
	GPixelFormats[PF_A8					].PlatformFormat	= (uint32)MTL::PixelFormatR8Unorm;
	GPixelFormats[PF_R32_UINT			].PlatformFormat	= (uint32)MTL::PixelFormatR32Uint;
	GPixelFormats[PF_R32_SINT			].PlatformFormat	= (uint32)MTL::PixelFormatR32Sint;
	GPixelFormats[PF_R16G16B16A16_UINT	].PlatformFormat	= (uint32)MTL::PixelFormatRGBA16Uint;
	GPixelFormats[PF_R16G16B16A16_SINT	].PlatformFormat	= (uint32)MTL::PixelFormatRGBA16Sint;
	GPixelFormats[PF_R8G8B8A8			].PlatformFormat	= (uint32)MTL::PixelFormatRGBA8Unorm;
    GPixelFormats[PF_A8R8G8B8           ].PlatformFormat    = (uint32)MTL::PixelFormatRGBA8Unorm;
	GPixelFormats[PF_R8G8B8A8_UINT		].PlatformFormat	= (uint32)MTL::PixelFormatRGBA8Uint;
	GPixelFormats[PF_R8G8B8A8_SNORM		].PlatformFormat	= (uint32)MTL::PixelFormatRGBA8Snorm;
	GPixelFormats[PF_R8G8				].PlatformFormat	= (uint32)MTL::PixelFormatRG8Unorm;
	GPixelFormats[PF_R16_SINT			].PlatformFormat	= (uint32)MTL::PixelFormatR16Sint;
	GPixelFormats[PF_R16_UINT			].PlatformFormat	= (uint32)MTL::PixelFormatR16Uint;
	GPixelFormats[PF_R8_UINT			].PlatformFormat	= (uint32)MTL::PixelFormatR8Uint;
	GPixelFormats[PF_R8					].PlatformFormat	= (uint32)MTL::PixelFormatR8Unorm;

	GPixelFormats[PF_R16G16B16A16_UNORM ].PlatformFormat	= (uint32)MTL::PixelFormatRGBA16Unorm;
	GPixelFormats[PF_R16G16B16A16_SNORM ].PlatformFormat	= (uint32)MTL::PixelFormatRGBA16Snorm;

	GPixelFormats[PF_NV12				].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_NV12				].Supported			= false;
	
	GPixelFormats[PF_G16R16_SNORM		].PlatformFormat	= (uint32)MTL::PixelFormatRG16Snorm;
	GPixelFormats[PF_R8G8_UINT			].PlatformFormat	= (uint32)MTL::PixelFormatRG8Uint;
	GPixelFormats[PF_R32G32B32_UINT		].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_R32G32B32_UINT		].Supported			= false;
	GPixelFormats[PF_R32G32B32_SINT		].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_R32G32B32_SINT		].Supported			= false;
	GPixelFormats[PF_R32G32B32F			].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_R32G32B32F			].Supported			= false;
	GPixelFormats[PF_R8_SINT			].PlatformFormat	= (uint32)MTL::PixelFormatR8Sint;
	GPixelFormats[PF_R64_UINT			].PlatformFormat	= (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_R64_UINT			].Supported			= false;
	GPixelFormats[PF_R9G9B9EXP5		    ].PlatformFormat    = (uint32)MTL::PixelFormatInvalid;
	GPixelFormats[PF_R9G9B9EXP5		    ].Supported			= false;

#if METAL_DEBUG_OPTIONS
	for (uint32 i = 0; i < PF_MAX; i++)
	{
		checkf((NS::UInteger)GMetalBufferFormats[i].LinearTextureFormat != NS::UIntegerMax, TEXT("Metal linear texture format for pixel-format %s (%d) is not configured!"), GPixelFormats[i].Name, i);
		checkf(GMetalBufferFormats[i].DataFormat != 255, TEXT("Metal data buffer format for pixel-format %s (%d) is not configured!"), GPixelFormats[i].Name, i);
	}
#endif

	RHIInitDefaultPixelFormatCapabilities();

	auto AddTypedUAVSupport = [](EPixelFormat InPixelFormat)
	{
		EnumAddFlags(GPixelFormats[InPixelFormat].Capabilities, EPixelFormatCapabilities::TypedUAVLoad | EPixelFormatCapabilities::TypedUAVStore);
	};

	switch (Device->readWriteTextureSupport())
	{
	case MTL::ReadWriteTextureTier2:
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

	case MTL::ReadWriteTextureTier1:
		AddTypedUAVSupport(PF_R32_FLOAT);
		AddTypedUAVSupport(PF_R32_UINT);
		AddTypedUAVSupport(PF_R32_SINT);
		// Fall through

	case MTL::ReadWriteTextureTierNone:
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

#if METAL_RHI_RAYTRACING
	if (ImmediateContext.Context->GetDevice().IsRayTracingSupported())
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("noraytracing")))
		{
			GRHISupportsRayTracing = RHISupportsRayTracing(GMaxRHIShaderPlatform);
			GRHISupportsRayTracingShaders = RHISupportsRayTracingShaders(GMaxRHIShaderPlatform);

			GRHISupportsRayTracingPSOAdditions = false;
			GRHISupportsRayTracingAMDHitToken = false;

			GRHISupportsInlineRayTracing = GRHISupportsRayTracing && RHISupportsInlineRayTracing(GMaxRHIShaderPlatform);
		}
		else
		{
			GRHISupportsRayTracing = false;
		}

		GRHISupportsRayTracingDispatchIndirect = true;
		GRHIRayTracingAccelerationStructureAlignment = 16;
		GRHIRayTracingScratchBufferAlignment = 4;
		GRHIRayTracingInstanceDescriptorSize = uint32(sizeof(MTLAccelerationStructureUserIDInstanceDescriptor));
	}
#endif
		
	GDynamicRHI = this;
	GIsMetalInitialized = true;

	ImmediateContext.Profiler = nullptr;
#if ENABLE_METAL_GPUPROFILE
	ImmediateContext.Profiler = FMetalProfiler::CreateProfiler(ImmediateContext.Context);
	if (ImmediateContext.Profiler)
		ImmediateContext.Profiler->BeginFrame();
#endif

#if METAL_USE_METAL_SHADER_CONVERTER
	CompilerInstance = IRCompilerCreate();
#endif

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if(GRHIBindlessSupport != ERHIBindlessSupport::Unsupported)
	{
		FMetalBindlessDescriptorManager* BindlessDescriptorManager = ImmediateContext.Context->GetBindlessDescriptorManager();
		BindlessDescriptorManager->Init();
	}
#endif
	
#if ENABLE_METAL_GPUPROFILE
    if (ImmediateContext.Profiler)
		ImmediateContext.Profiler->EndFrame();
#endif
}

FMetalDynamicRHI::~FMetalDynamicRHI()
{
	check(IsInGameThread() && IsInRenderingThread());
	
	GIsMetalInitialized = false;
	GIsRHIInitialized = false;

	// Ask all initialized FRenderResources to release their RHI resources.
	FRenderResource::ReleaseRHIForAllResources();	
	
#if METAL_USE_METAL_SHADER_CONVERTER
    IRCompilerDestroy(CompilerInstance);
#endif

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
	return ImmediateContext.Context->GetDevice()->minimumLinearTextureAlignmentForPixelFormat((MTL::PixelFormat)GMetalBufferFormats[Format].LinearTextureFormat);
}

void FMetalDynamicRHI::Init()
{
	GRHICommandList.GetImmediateCommandList().InitializeImmediateContexts();

	FRenderResource::InitPreRHIResources();
	GIsRHIInitialized = true;
}

void FMetalRHIImmediateCommandContext::RHIBeginFrame()
{
    MTL_SCOPED_AUTORELEASE_POOL;
#if ENABLE_METAL_GPUPROFILE
	Profiler->BeginFrame();
#endif
	((FMetalDeviceContext*)Context)->BeginFrame();
}

void FMetalRHICommandContext::RHIBeginFrame()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIEndFrame()
{
    MTL_SCOPED_AUTORELEASE_POOL;
#if ENABLE_METAL_GPUPROFILE
	Profiler->EndFrame();
#endif
	((FMetalDeviceContext*)Context)->EndFrame();
}

void FMetalRHICommandContext::RHIEndFrame()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIBeginScene()
{
    MTL_SCOPED_AUTORELEASE_POOL;
	((FMetalDeviceContext*)Context)->BeginScene();
}

void FMetalRHICommandContext::RHIBeginScene()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIEndScene()
{
    MTL_SCOPED_AUTORELEASE_POOL;
	((FMetalDeviceContext*)Context)->EndScene();
}

void FMetalRHICommandContext::RHIEndScene()
{
	check(false);
}

void FMetalRHICommandContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
#if ENABLE_METAL_GPUEVENTS
    MTL_SCOPED_AUTORELEASE_POOL;
    FPlatformMisc::BeginNamedEvent(Color, Name);
#if ENABLE_METAL_GPUPROFILE
    Profiler->PushEvent(Name, Color);
#endif
    Context->GetCurrentRenderPass().PushDebugGroup(NS::String::string(TCHAR_TO_UTF8(Name), NS::UTF8StringEncoding));
#endif
}

void FMetalRHICommandContext::RHIPopEvent()
{
#if ENABLE_METAL_GPUEVENTS
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FPlatformMisc::EndNamedEvent();
	Context->GetCurrentRenderPass().PopDebugGroup();
#if ENABLE_METAL_GPUPROFILE
	Profiler->PopEvent();
#endif
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
    MTL_SCOPED_AUTORELEASE_POOL;
    
    ((FMetalDeviceContext*)ImmediateContext.Context)->FlushFreeList(false);
    ImmediateContext.Context->SubmitCommandBufferAndWait();
    ((FMetalDeviceContext*)ImmediateContext.Context)->ClearFreeList();
    ((FMetalDeviceContext*)ImmediateContext.Context)->DrainHeap();
    ImmediateContext.Context->GetCurrentState().Reset();
}

void FMetalDynamicRHI::RHIAcquireThreadOwnership()
{
}

void FMetalDynamicRHI::RHIReleaseThreadOwnership()
{
}

void* FMetalDynamicRHI::RHIGetNativeDevice()
{
	return (void*)ImmediateContext.Context->GetDevice();
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
    MTL_SCOPED_AUTORELEASE_POOL;
	ImmediateContext.Context->SubmitCommandBufferAndWait();
}

void FMetalDynamicRHI::RHISubmitCommandsAndFlushGPU()
{
    MTL_SCOPED_AUTORELEASE_POOL;
    ImmediateContext.Context->SubmitCommandBufferAndWait();
}

uint32 FMetalDynamicRHI::RHIGetGPUFrameCycles(uint32 GPUIndex)
{
	check(GPUIndex == 0);
	return GGPUFrameTime;
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

void FMetalDynamicRHI::RHISubmitCommandLists(TArrayView<IRHIPlatformCommandList*> CommandLists, bool bFlushResources)
{
}

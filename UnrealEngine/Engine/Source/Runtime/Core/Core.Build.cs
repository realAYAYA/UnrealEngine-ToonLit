// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Core : ModuleRules
{
	public Core(ReadOnlyTargetRules Target) : base(Target)
	{
		NumIncludedBytesPerUnityCPPOverride = 491520; // best unity size found from using UBT ProfileUnitySizes mode

		PrivatePCHHeaderFile = "Private/CorePrivatePCH.h";

		SharedPCHHeaderFile = "Public/CoreSharedPCH.h";

		PrivateDependencyModuleNames.Add("BuildSettings");
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("GoogleGameSDK");

			if (Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				PrivateDependencyModuleNames.Add("HWCPipe");        // Performance counters for ARM CPUs and ARM Mali GPUs
				PrivateDependencyModuleNames.Add("heapprofd");      // Exposes custom allocators to Google's Memory Profiler
			}
		}

		PrivateDependencyModuleNames.Add("AtomicQueue");
		PrivateDependencyModuleNames.Add("BLAKE3");
		PrivateDependencyModuleNames.Add("OodleDataCompression");
		PrivateDependencyModuleNames.Add("xxhash");

		PublicDependencyModuleNames.Add("TraceLog");

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DerivedDataCache",
				"TargetPlatform",
			}
			);

		if (Target.bBuildEditor == true)
		{
			PrivateIncludePathModuleNames.Add("DirectoryWatcher");
			DynamicallyLoadedModuleNames.Add("DirectoryWatcher");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDefinitions.Add("PLATFORM_BUILDS_LIBPAS=1");
			PrivateDependencyModuleNames.Add("libpas");
		}
			
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib"
				);

			if (Target.WindowsPlatform.Architecture != UnrealArch.Arm64)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"IntelTBB",
					"IntelVTune"
					);
			}

			// We do not want the static analyzer to run on thirdparty code
			if (Target.StaticAnalyzer == StaticAnalyzer.None) 
			{
				PrivateDependencyModuleNames.Add("mimalloc");
				PrivateDefinitions.Add("PLATFORM_BUILDS_MIMALLOC=1");
			}
			
			if (Target.WindowsPlatform.bUseBundledDbgHelp)
			{
				PublicDelayLoadDLLs.Add("DBGHELP.DLL");
				PrivateDefinitions.Add("USE_BUNDLED_DBGHELP=1");
				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/DbgHelp/dbghelp.dll");
			}
			else
			{
				PrivateDefinitions.Add("USE_BUNDLED_DBGHELP=0");
			}
			PrivateDefinitions.Add("YIELD_BETWEEN_TASKS=1");

			if (Target.WindowsPlatform.bPixProfilingEnabled && Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				PrivateDependencyModuleNames.Add("WinPixEventRuntime");
			}

			if (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Type != TargetType.Program)
			{
				PublicDefinitions.Add("UE_MEMORY_TAGS_TRACE_ENABLED=1");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"IntelTBB",
				"zlib",
				"PLCrashReporter"
				);
			PublicFrameworks.AddRange(new string[] { "Cocoa", "Carbon", "IOKit", "Security", "UniformTypeIdentifiers" });

			PrivateDependencyModuleNames.Add("mimalloc");
			PrivateDefinitions.Add("PLATFORM_BUILDS_MIMALLOC=1");

			if (Target.bBuildEditor == true)
			{
				string XcodeRoot = Utils.RunLocalProcessAndReturnStdOut("/usr/bin/xcode-select", "--print-path");
				PublicAdditionalLibraries.Add(XcodeRoot + "/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/PrivateFrameworks/MultitouchSupport.framework/Versions/Current/MultitouchSupport.tbd");
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.IOS))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib"
				);
			PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation", "AudioToolbox", "AVFoundation", "GameKit", "StoreKit", "CoreVideo", "CoreMedia", "CoreGraphics", "GameController", "SystemConfiguration", "DeviceCheck", "UserNotifications" });
			if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				PublicFrameworks.AddRange(new string[] { "CoreMotion", "AdSupport", "WebKit" });
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"PLCrashReporter"
					);
			}
			if (Target.Platform == UnrealTargetPlatform.VisionOS)
			{
				PublicFrameworks.Add("CoreMotion");
			}

			PrivateIncludePathModuleNames.Add("ApplicationCore");

			bool bSupportAdvertising = Target.Platform == UnrealTargetPlatform.IOS;
			if (bSupportAdvertising)
			{
				PublicFrameworks.AddRange(new string[] { "iAD" });
			}

			// export Core symbols for embedded Dlls
			ModuleSymbolVisibility = ModuleRules.SymbolVisibility.VisibileForDll;
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"cxademangle",
				"zlib",
				"libunwind"
				);

			if (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Type != TargetType.Program)
			{
				PublicDefinitions.Add("UE_MEMORY_TRACE_AVAILABLE=1");
				PublicDefinitions.Add("UE_MEMORY_TAGS_TRACE_ENABLED=1");
				PublicDefinitions.Add("UE_CALLSTACK_TRACE_ENABLED=1");
				PrivateDefinitions.Add("UE_CALLSTACK_TRACE_ANDROID_USE_STACK_FRAMES_WALKING=1");

				// Support for memory tracing libc.so malloc
				PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Build", "Android", "Prebuilt", "ScudoMemoryTrace"));
				PrivateDefinitions.Add("UE_MEMORY_TRACE_ANDROID_ENABLE_SCUDO_TRACING_SUPPORT=1");
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib",
				"jemalloc"
				);

			// Core uses dlopen()
			PublicSystemLibraries.Add("dl");

			PrivateDependencyModuleNames.Add("mimalloc");
			PrivateDefinitions.Add("PLATFORM_BUILDS_MIMALLOC=1");

			if (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Type != TargetType.Program)
			{
				PublicDefinitions.Add("UE_MEMORY_TRACE_AVAILABLE=1");
				PublicDefinitions.Add("UE_MEMORY_TAGS_TRACE_ENABLED=1");
				PublicDefinitions.Add("UE_CALLSTACK_TRACE_ENABLED=1");
			}
		}

		if (Target.bCompileICU == true)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "ICU");
		}
		PublicDefinitions.Add("UE_ENABLE_ICU=" + (Target.bCompileICU ? "1" : "0")); // Enable/disable (=1/=0) ICU usage in the codebase. NOTE: This flag is for use while integrating ICU and will be removed afterward.

		// If we're compiling with the engine, then add Core's engine dependencies
		if (Target.bCompileAgainstEngine && !Target.bBuildRequiresCookedData)
		{
			DynamicallyLoadedModuleNames.AddRange(new string[] { "DerivedDataCache" });
			DynamicallyLoadedModuleNames.AddRange(new string[] { "Virtualization" });
		}

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop))
        {
			// Enabling more crash information on Desktop platforms.
			PublicDefinitions.Add("WITH_ADDITIONAL_CRASH_CONTEXTS=1");
		}
		
		// On Windows platform, VSPerfExternalProfiler.cpp needs access to "VSPerf.h".  This header is included with Visual Studio, but it's not in a standard include path.
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			bool VSPerfDefined = false;
			string VisualStudioInstallation = Target.WindowsPlatform.IDEDir;
			if (VisualStudioInstallation != null && VisualStudioInstallation != string.Empty && Directory.Exists(VisualStudioInstallation))
			{
				string SubFolderName = "x64/PerfSDK/";
				string PerfIncludeDirectory = Path.Combine(VisualStudioInstallation, String.Format("Team Tools/Performance Tools/{0}", SubFolderName));

				if (File.Exists(Path.Combine(PerfIncludeDirectory, "VSPerf.h"))
					&& Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					PrivateIncludePaths.Add(PerfIncludeDirectory);
					PublicDefinitions.Add("WITH_VS_PERF_PROFILER=1");
					VSPerfDefined = true;
				}
			}

			if (!VSPerfDefined)
			{
				PublicDefinitions.Add("WITH_VS_PERF_PROFILER=0");
			}
		}

		// Superluminal instrumentation support, if one has it installed
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			string SuperluminalInstallDir = OperatingSystem.IsWindows() ? Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Superluminal\Performance", "InstallDir", null) as string : null;
			if (String.IsNullOrEmpty(SuperluminalInstallDir))
			{
				SuperluminalInstallDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Superluminal/Performance");
			}

			string SuperluminalApiDir = Path.Combine(SuperluminalInstallDir, "API/");
			string SubFolderName = "lib/x64/";
			string SuperluminalLibDir = Path.Combine(SuperluminalApiDir, SubFolderName);

			if (Target.Configuration != UnrealTargetConfiguration.Shipping &&
				File.Exists(Path.Combine(SuperluminalApiDir, "include/Superluminal/PerformanceAPI_capi.h")))
			{
				if (Target.bDebugBuildsActuallyUseDebugCRT == true && Target.Configuration == UnrealTargetConfiguration.Debug)
				{
					PublicAdditionalLibraries.Add(Path.Combine(SuperluminalLibDir, "PerformanceAPI_MDd.lib"));
				}
				else
				{
					PublicAdditionalLibraries.Add(Path.Combine(SuperluminalLibDir, "PerformanceAPI_MD.lib"));
				}
				PrivateDefinitions.Add("WITH_SUPERLUMINAL_PROFILER=1");
				PrivateIncludePaths.Add(Path.Combine(SuperluminalApiDir, "include/"));
			}
			else
			{
				PrivateDefinitions.Add("WITH_SUPERLUMINAL_PROFILER=0");
			}
		}

		// Detect if the Concurrency Viewer Extension is installed
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			bool VSCVDefined = false;
			string VisualStudioInstallation = Target.WindowsPlatform.IDEDir;
			if (VisualStudioInstallation != null && VisualStudioInstallation != string.Empty && Directory.Exists(VisualStudioInstallation))
			{
				string SubFolderName = @"Common7\IDE\Extensions\jcn3iwfw.vp2\SDK\Native\Inc";
				string IncludeDirectory = Path.Combine(VisualStudioInstallation, SubFolderName);

				if (File.Exists(Path.Combine(IncludeDirectory, "cvmarkers.h"))
					&& Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					PrivateIncludePaths.Add(IncludeDirectory);
					PublicDefinitions.Add("WITH_CONCURRENCYVIEWER_PROFILER=1");
					VSCVDefined = true;
				}
			}

			if (!VSCVDefined)
			{
				PublicDefinitions.Add("WITH_CONCURRENCYVIEWER_PROFILER=0");
			}
		}

		if (Target.bWithDirectXMath && Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("WITH_DIRECTXMATH=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_DIRECTXMATH=0");
		}

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Apple) ||
			Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicDefinitions.Add("IS_RUNNING_GAMETHREAD_ON_EXTERNAL_THREAD=1");
		}

		// Set a macro to allow FApp::GetBuildTargetType() to detect client targts
		if (Target.Type == TargetRules.TargetType.Client)
		{
			PrivateDefinitions.Add("IS_CLIENT_TARGET=1");
		}
		else
		{
			PrivateDefinitions.Add("IS_CLIENT_TARGET=0");
		}

		// Setup definitions to include / exclude Iris modifications to UObject Note: Only the definition is required as we do not depend on Iris in any way.
		if (Target.bUseIris == true)
		{
			PublicDefinitions.Add("UE_WITH_IRIS=1");
		}
		else
		{
			PublicDefinitions.Add("UE_WITH_IRIS=0");
		}
		
		if (Target.Platform == UnrealTargetPlatform.Win64
			&& Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PublicDefinitions.Add("PLATFORM_SUPPORTS_PLATFORM_EVENTS=1");
			PublicDefinitions.Add("PLATFORM_SUPPORTS_TRACE_WIN32_VIRTUAL_MEMORY_HOOKS=1");
			PublicDefinitions.Add("PLATFORM_SUPPORTS_TRACE_WIN32_MODULE_DIAGNOSTICS=1");
			PublicDefinitions.Add("PLATFORM_SUPPORTS_TRACE_WIN32_CALLSTACK=1");
			PublicDefinitions.Add("UE_MEMORY_TRACE_AVAILABLE=1");
		}

		// temporary thing.
		PrivateDefinitions.Add("PLATFORM_SUPPORTS_BINARYCONFIG=" + (SupportsBinaryConfig(Target) ? "1" : "0"));

		PublicDefinitions.Add("WITH_MALLOC_STOMP=" + (bWithMallocStomp ? "1" : "0"));

		PrivateDefinitions.Add("PLATFORM_COMPILER_OPTIMIZATION_LTCG=" + (Target.bAllowLTCG ? "1" : "0"));
		PrivateDefinitions.Add("PLATFORM_COMPILER_OPTIMIZATION_PG=" + (Target.bPGOOptimize ? "1" : "0"));
		PrivateDefinitions.Add("PLATFORM_COMPILER_OPTIMIZATION_PG_PROFILING=" + (Target.bPGOProfile ? "1" : "0"));

		PrivateDefinitions.Add("UE_DEFINE_LEGACY_MATH_CONSTANT_MACRO_NAMES=0");

		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		IWYUSupport = IWYUSupport.KeepAsIs;

		bAllowAutoRTFMInstrumentation = true;
	}

	protected virtual bool SupportsBinaryConfig(ReadOnlyTargetRules Target)
	{
		return Target.Platform != UnrealTargetPlatform.Android;
	}

	// Decide if validating memory allocator (aka MallocStomp) can be used on the current plVatform.
	// Run-time validation must be enabled through '-stompmalloc' command line argument.
	protected virtual bool bWithMallocStomp
	{
		get => 
			Target.Configuration != UnrealTargetConfiguration.Shipping &&
			(Target.Platform == UnrealTargetPlatform.Mac ||
			 Target.Platform == UnrealTargetPlatform.Linux ||
			 Target.Platform == UnrealTargetPlatform.LinuxArm64 ||
			 Target.Platform == UnrealTargetPlatform.Win64);
	}
}

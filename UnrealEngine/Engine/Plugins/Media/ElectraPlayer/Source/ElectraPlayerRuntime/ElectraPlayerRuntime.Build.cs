// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ElectraPlayerRuntime: ModuleRules
	{
		public ElectraPlayerRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			//
			// Common setup...
			//
			bLegalToDistributeObjectCode = true;
			IWYUSupport = IWYUSupport.None;

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Json",
					"ElectraBase",
					"ElectraCodecFactory",
					"ElectraDecoders",
					"ElectraHTTPStream",
					"ElectraCDM",
					"ElectraSubtitles",
					"XmlParser",
					"SoundTouchZ"
				});
			if (Target.bCompileAgainstEngine)
			{
				// Added to allow debug rendering if used in UE context
				PrivateDependencyModuleNames.Add("Engine");
			}

			PrivateIncludePaths.AddRange(
				new string[] {
					"ElectraPlayerRuntime/Private/Runtime",
				});

			//
			// Common platform setup...
			//
			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				PublicDependencyModuleNames.Add("DirectX");

				PrivateDefinitions.Add("_CRT_SECURE_NO_WARNINGS=1");

				AddEngineThirdPartyPrivateStaticDependencies(Target, "WinHttp");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");

				PrivateDefinitions.Add("ELECTRA_HAVE_DX11");	// video decoding for DX11 enabled (Win8+)

				if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.WindowsPlatform.Architecture != UnrealArch.Arm64)
				{
					PublicAdditionalLibraries.AddRange(new string[] {
						Path.Combine(Target.WindowsPlatform.DirectXLibDir, "dxerr.lib"),
					});
				}

				PublicSystemLibraries.AddRange(new string[] {
					"strmiids.lib",
					"legacy_stdio_definitions.lib",
					"Dxva2.lib",
				});

				// Delay-load all MF DLLs to be able to check Windows version for compatibility in `StartupModule` before loading them manually
				PublicSystemLibraries.Add("mfplat.lib");
				PublicDelayLoadDLLs.Add("mfplat.dll");
				PublicSystemLibraries.Add("mfuuid.lib");

				PublicIncludePaths.Add("$(ModuleDir)/Public/Windows");
				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Runtime/Decoder/Windows");
				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Windows");
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Apple))
			{
				PublicFrameworks.AddRange(
				new string[] {
								"CoreMedia",
								"CoreVideo",
								"AVFoundation",
								"AudioToolbox",
								"VideoToolbox",
								"QuartzCore"
				});

				PublicIncludePaths.Add("$(ModuleDir)/Public/Apple");

				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Runtime/Decoder/Apple");
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android) )
			{
				PublicDefinitions.Add("CURL_ENABLE_DEBUG_CALLBACK=1");

				if (Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					PublicDefinitions.Add("CURL_ENABLE_NO_TIMEOUTS_OPTION=1");
				}

				PublicIncludePaths.Add("$(ModuleDir)/Public/Android");

				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Runtime/Decoder/Android");
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) )
			{
				PublicDefinitions.Add("CURL_ENABLE_DEBUG_CALLBACK=1");

				if (Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					PublicDefinitions.Add("CURL_ENABLE_NO_TIMEOUTS_OPTION=1");
				}

				PublicIncludePaths.Add("$(ModuleDir)/Public/Linux");
				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Runtime/Decoder/Linux");

				AddEngineThirdPartyPrivateStaticDependencies(Target, "libav");
			}
		}
	}
}

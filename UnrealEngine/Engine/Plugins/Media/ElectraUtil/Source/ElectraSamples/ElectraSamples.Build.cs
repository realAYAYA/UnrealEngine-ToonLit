// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ElectraSamples: ModuleRules
	{
		public ElectraSamples(ReadOnlyTargetRules Target) : base(Target)
		{
			bLegalToDistributeObjectCode = true;

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MediaUtils",
					"RenderCore",
					"RHI",
					"ElectraBase",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			if (Target.bCompileAgainstEngine)
			{
				PrivateDependencyModuleNames.Add("Engine");
			}

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				string DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";
				PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

				if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
				{
					DirectXSDKDir += "/Lib/x64/";
				}

				if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
				{
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DX9");

					PublicAdditionalLibraries.AddRange(new string[] {
						DirectXSDKDir + "dxerr.lib",
					});

					PrivateDependencyModuleNames.Add("D3D11RHI");

					PrivateDefinitions.Add("ELECTRA_SUPPORT_PREWIN8");
					PrivateDefinitions.Add("ELECTRA_HAVE_DX11");
				}

				PublicSystemLibraries.AddRange(new string[] {
					"strmiids.lib",
					"legacy_stdio_definitions.lib",
					"Dxva2.lib",
				});

				PublicIncludePaths.Add("$(ModuleDir)/Public/Windows");
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
			{
				PrivateDependencyModuleNames.Add("MetalRHI");
				PublicIncludePaths.Add("$(ModuleDir)/Public/Apple");
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
			{
				PublicIncludePaths.Add("$(ModuleDir)/Public/Android");
				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "ElectraSamples_UPL.xml"));
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				PublicIncludePaths.Add("$(ModuleDir)/Public/Linux");
			}
		}
	}
}

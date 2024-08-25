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
					"ColorManagement",
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
				PublicDependencyModuleNames.Add("DirectX");

				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX9");

				if (Target.WindowsPlatform.Architecture != UnrealArch.Arm64)
				{
					PublicAdditionalLibraries.AddRange(new string[] {
						Path.Combine(Target.WindowsPlatform.DirectXLibDir, "dxerr.lib"),
					});
				}

				PrivateDependencyModuleNames.Add("D3D11RHI");
				PrivateDependencyModuleNames.Add("D3D12RHI");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");

				PrivateDefinitions.Add("ELECTRA_SUPPORT_PREWIN8");
				PrivateDefinitions.Add("ELECTRA_HAVE_DX11");

				PublicSystemLibraries.AddRange(new string[] {
					"strmiids.lib",
					"legacy_stdio_definitions.lib",
					"Dxva2.lib",
				});

				PublicIncludePaths.Add("$(ModuleDir)/Public/Windows");
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Apple))
			{
				PrivateDependencyModuleNames.Add("MetalRHI");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "MetalCPP");
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

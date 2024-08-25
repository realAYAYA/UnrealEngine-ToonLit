// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class ElectraPlayerPlugin: ModuleRules
	{
		public ElectraPlayerPlugin(ReadOnlyTargetRules Target) : base(Target)
		{
			bLegalToDistributeObjectCode = true;

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"MediaUtils",
					"RenderCore",
					"RHI",
					"ElectraPlayerRuntime",
					"ElectraSamples",
					"ElectraBase",
					"ElectraDecoders"
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
				PrivateDependencyModuleNames.Add("D3D12RHI");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			}

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Apple))
			{
				PrivateDependencyModuleNames.Add("MetalRHI");
			}

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.Add("DeveloperSettings");
			}
		}
	}
}

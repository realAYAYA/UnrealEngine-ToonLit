// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class SharedMemoryMedia : ModuleRules
	{
		public SharedMemoryMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"MediaIOCore",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"Media",
					"MediaAssets",
					"RenderCore",
					"Renderer",
					"RHI",
				});

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
				PrivateDependencyModuleNames.Add("D3D12RHI");
			}

		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class BlackmagicMediaOutput : ModuleRules
	{
		public BlackmagicMediaOutput(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"BlackmagicMedia",
					"MediaIOCore",
				});

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"BlackmagicMediaOutput/Private"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"BlackmagicCore",
					"Core",
					"CoreUObject",
					"Engine",
					"GPUTextureTransfer",
					"RenderCore",
					"RHI",
					"Slate"
				}
            );
		}
	}
}

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

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"BlackmagicCore",
					"ColorManagement",
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

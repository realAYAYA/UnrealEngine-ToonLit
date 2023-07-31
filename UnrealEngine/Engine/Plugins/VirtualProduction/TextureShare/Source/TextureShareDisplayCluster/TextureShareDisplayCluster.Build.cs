// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

[SupportedPlatforms("Win64")]
public class TextureShareDisplayCluster : ModuleRules
{
	public TextureShareDisplayCluster(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(EngineDirectory,"Plugins/VirtualProduction/TextureShare/Source/TextureShare/Private"),
				Path.Combine(EngineDirectory,"Plugins/VirtualProduction/TextureShare/Source/TextureShareCore/Private"),
			}
		);

		// List of public dependency module names (no path needed) (automatically does the private/public include). These are modules that are required by our public source files.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",

				"DisplayCluster",
				"DisplayClusterShaders",
				"DisplayClusterConfiguration",
			});

		// List of private dependency module names.  These are modules that our private code depends on but nothing in our public include files depend on.
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"RHI",
				"RHICore",
				"Renderer",
				"RenderCore",
				"TextureShare",
				"TextureShareCore",
			});
	}
}

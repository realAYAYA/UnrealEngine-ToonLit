// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	using System.IO;

	public class AjaMedia : ModuleRules
	{
		public AjaMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AJA",
					"Core",
					"CoreUObject",
					"Engine",
					"MediaIOCore",
					"MediaUtils",
					"Projects",
					"TimeManagement",
					"RenderCore",
					"RHI",
					"SlateCore"
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"AjaMedia/Private",
					"AjaMedia/Private/Aja",
					"AjaMedia/Private/Assets",
					"AjaMedia/Private/Player",
					"AjaMedia/Private/Shared",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"MediaAssets",
				});
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MobileFSR : ModuleRules
{
	public MobileFSR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[] {
				EngineDirectory + "/Source/Runtime/Renderer/Private",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"Projects",
				"RenderCore",
				"Renderer",
				"RHI",
			}
			);


		PrecompileForTargets = PrecompileTargetsType.Any;

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			//string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			//AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "MobileFSR_APL.xml"));
		}
	}
}

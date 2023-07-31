// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class heapprofd : ModuleRules
{
	public heapprofd(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

        string BasePath = Target.UEThirdPartySourceDirectory + "heapprofd";
		PublicSystemIncludePaths.Add(BasePath);

        if (Target.Platform == UnrealTargetPlatform.Android && Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "heapprofd_UPL.xml"));
			PublicDefinitions.Add("ANDROID_HEAP_PROFILING_SUPPORTED=1");
        }
    }
}

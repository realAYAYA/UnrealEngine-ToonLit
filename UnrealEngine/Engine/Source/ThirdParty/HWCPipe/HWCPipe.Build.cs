// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HWCPipe : ModuleRules
{
	public HWCPipe(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

        string BasePath = Target.UEThirdPartySourceDirectory + "HWCPipe/";
		PublicSystemIncludePaths.Add(BasePath + "include");
		PublicSystemIncludePaths.Add(BasePath + "include/third_party");

        if (Target.Platform == UnrealTargetPlatform.Android && Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
			PublicAdditionalLibraries.Add(BasePath + "lib/arm64-v8a/libhwcpipe.so");

			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "HWCPipe_UPL.xml"));
		}
    }
}

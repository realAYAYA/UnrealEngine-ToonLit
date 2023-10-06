// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SPIRVReflect : ModuleRules
{
	public SPIRVReflect(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "SPIRV-Reflect/SPIRV-Reflect"));

		string LibPath = Path.Combine(Target.UEThirdPartySourceDirectory, "SPIRV-Reflect/lib/");
		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibPath, "Mac/libSPIRV-Reflectd.a"));
			}
			else
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibPath, "Mac/libSPIRV-Reflect.a"));
			}
		}
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
			LibPath = Path.Combine(LibPath, "Win64", "VS2017");

            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add(Path.Combine(LibPath, "SPIRV-Reflectd.lib"));
            }
            else
            {
                PublicAdditionalLibraries.Add(Path.Combine(LibPath, "SPIRV-Reflect.lib"));
            }
        }
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "Linux", Target.Architecture.LinuxName, "libSPIRV-Reflect.a"));
		}
		else
		{
			string Err = string.Format("Attempt to build against SPIRV-Reflect on unsupported platform {0}", Target.Platform);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}
	}
}


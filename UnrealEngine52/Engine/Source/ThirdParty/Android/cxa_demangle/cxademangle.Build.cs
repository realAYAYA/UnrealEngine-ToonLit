// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class cxademangle : ModuleRules
{
	public cxademangle(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		string cxademanglepath = Target.UEThirdPartySourceDirectory + "Android/cxa_demangle/";

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// suggestion 2
 
			PublicAdditionalLibraries.AddRange(new string[] {
				Path.Combine(cxademanglepath, "arm64-v8a", "libcxa_demangle.a"),
				Path.Combine(cxademanglepath, "x64", "libcxa_demangle.a"),
			});
		}
	}
}

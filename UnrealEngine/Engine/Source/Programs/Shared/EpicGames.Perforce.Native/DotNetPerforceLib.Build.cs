// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DotNetPerforceLib : ModuleRules
{
	public DotNetPerforceLib(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDefinitions.Add("OS_NT");
		}

		PublicIncludePathModuleNames.Add("Core");

		PrivateDependencyModuleNames.Add("OpenSSL");

		string ApiPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2018.1/";
		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicSystemIncludePaths.Add(ApiPath + "Include/Linux");
			PublicAdditionalLibraries.Add(ApiPath + "Lib/Linux/libclient.a");
			PublicAdditionalLibraries.Add(ApiPath + "Lib/Linux/librpc.a");
			PublicAdditionalLibraries.Add(ApiPath + "Lib/Linux/libsupp.a");

			PublicSystemLibraries.Add("dl");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicIncludePaths.Add(ApiPath + "Include/Mac");
			PublicAdditionalLibraries.Add(ApiPath + "Lib/Mac/libclient.a");
			PublicAdditionalLibraries.Add(ApiPath + "Lib/Mac/librpc.a");
			PublicAdditionalLibraries.Add(ApiPath + "Lib/Mac/libsupp.a");

			PublicFrameworks.Add("Foundation");
			PublicFrameworks.Add("CoreFoundation");
			PublicFrameworks.Add("CoreGraphics");
			PublicFrameworks.Add("CoreServices");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add(ApiPath + "Include/Win64/VS2015");
			PublicAdditionalLibraries.Add(ApiPath + "Lib/Win64/VS2015/Release/libclient.lib");
			PublicAdditionalLibraries.Add(ApiPath + "Lib/Win64/VS2015/Release/librpc.lib");
			PublicAdditionalLibraries.Add(ApiPath + "Lib/Win64/VS2015/Release/libsupp.lib");
		}
	}
}

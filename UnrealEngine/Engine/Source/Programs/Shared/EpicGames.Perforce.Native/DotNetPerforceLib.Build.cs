// Copyright Epic Games, Inc. All Rights Reserved.


// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DotNetPerforceLib : ModuleRules
	{
		public DotNetPerforceLib(ReadOnlyTargetRules Target) : base(Target)
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PrivateDefinitions.Add("OS_NT");
				PrivateDefinitions.Add("_CRT_NONSTDC_NO_DEPRECATE");
			}

			PublicIncludePathModuleNames.Add("Core");

			PrivateDependencyModuleNames.Add("OpenSSL");

			string ApiPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2023.2/";
			if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				PublicSystemIncludePaths.Add(ApiPath + "Linux/include");
				PublicAdditionalLibraries.Add(ApiPath + "Linux/lib/libclient.a");
				PublicAdditionalLibraries.Add(ApiPath + "Linux/lib/librpc.a");
				PublicAdditionalLibraries.Add(ApiPath + "Linux/lib/libsupp.a");
				PublicAdditionalLibraries.Add(ApiPath + "Linux/lib/libp4script_cstub.a");

				PublicSystemLibraries.Add("dl");
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicIncludePaths.Add(ApiPath + "Mac/include");
				PublicAdditionalLibraries.Add(ApiPath + "Mac/lib/libclient.a");
				PublicAdditionalLibraries.Add(ApiPath + "Mac/lib/librpc.a");
				PublicAdditionalLibraries.Add(ApiPath + "Mac/lib/libsupp.a");
				PublicAdditionalLibraries.Add(ApiPath + "Mac/lib/libp4script_cstub.a");

				PublicFrameworks.Add("Foundation");
				PublicFrameworks.Add("CoreFoundation");
				PublicFrameworks.Add("CoreGraphics");
				PublicFrameworks.Add("CoreServices");
				PublicFrameworks.Add("Security");
			}
			else if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicIncludePaths.Add(ApiPath + "Win64/include");
				PublicAdditionalLibraries.Add(ApiPath + "Win64/lib/libclient.lib");
				PublicAdditionalLibraries.Add(ApiPath + "Win64/lib/librpc.lib");
				PublicAdditionalLibraries.Add(ApiPath + "Win64/lib/libsupp.lib");
				PublicAdditionalLibraries.Add(ApiPath + "Win64/lib/libp4script_cstub.lib");
			}
		}
	}
}
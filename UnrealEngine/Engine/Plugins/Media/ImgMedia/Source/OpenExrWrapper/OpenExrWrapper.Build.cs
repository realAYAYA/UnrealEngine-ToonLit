// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenExrWrapper : ModuleRules
	{
		public OpenExrWrapper(ReadOnlyTargetRules Target) : base(Target)
		{
			bEnableExceptions = true;
			bUseRTTI = true;

            PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"TimeManagement",
				});

			bool bLinuxEnabled = Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64");

            if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
                (Target.Platform == UnrealTargetPlatform.Mac) ||
				bLinuxEnabled)
            {
                AddEngineThirdPartyPrivateStaticDependencies(Target, "Imath");
                AddEngineThirdPartyPrivateStaticDependencies(Target, "UEOpenExr");
                AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
            }
            else
            {
                System.Console.WriteLine("OpenExrWrapper does not supported this platform");
            }

			PrivateIncludePaths.Add("OpenExrWrapper/Private");
        }
	}
}

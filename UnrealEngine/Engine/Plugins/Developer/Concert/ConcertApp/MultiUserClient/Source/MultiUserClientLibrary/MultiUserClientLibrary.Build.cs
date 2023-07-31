// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MultiUserClientLibrary : ModuleRules
	{
		public MultiUserClientLibrary(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
				}
			);

			if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
			{
				PrivateDefinitions.Add("WITH_CONCERT=1");

                PrivateDependencyModuleNames.AddRange(
                    new string[]
                    {
                        "Concert"
                    }
                );

                PrivateIncludePathModuleNames.AddRange(
					new string[]
					{
						"ConcertSyncCore",
						"ConcertSyncClient",
						"MultiUserClient",
					}
				);

				DynamicallyLoadedModuleNames.AddRange(
					new string[]
					{
						"MultiUserClient",
					}
				);
			}
			else
			{
				PrivateDefinitions.Add("WITH_CONCERT=0");
			}
		}
	}
}

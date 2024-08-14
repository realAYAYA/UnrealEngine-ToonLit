// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{

	public class MProtocol : ModuleRules
	{
		public MProtocol(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"ZProtobuf",
					"ZFmt",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] { "Engine" }
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
				}
			);

			if (Target.bBuildEditor)
			{
				PublicDependencyModuleNames.AddRange(new string[] { "DirectoryWatcher", });
			}
			
		}
	}

}
// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{

	public class MRpc : ModuleRules
	{
		public MRpc(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					
					"MCommon",
					"ZProtobuf",					
					"MProtocol",
					"NetLib",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
				}
			);

		}
	}

}
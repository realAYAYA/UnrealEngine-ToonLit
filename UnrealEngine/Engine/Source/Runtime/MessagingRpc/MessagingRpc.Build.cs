// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MessagingRpc : ModuleRules
	{
		public MessagingRpc(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					"Messaging",
				});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"MessagingCommon",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"Messaging",
				});
		}
	}
}

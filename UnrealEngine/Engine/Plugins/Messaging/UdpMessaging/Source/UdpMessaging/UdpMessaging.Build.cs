// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UdpMessaging : ModuleRules
	{
		public UdpMessaging(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Networking",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
					"Json",
					"Cbor",
					"Messaging",
					"Projects",
					"Serialization",
					"TraceLog",
					"Sockets"
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"MessagingCommon",
				});

			if (Target.bCompileAgainstEditor)
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[] {
						"Settings",
					});
				
				PrivateIncludePathModuleNames.AddRange(
					new string[] {
						"Settings",
					});

				PrivateDependencyModuleNames.AddRange(
					new string[] {
						"ApplicationCore",
						"EditorStyle",
						"Engine",
						"InputCore",
						"PropertyEditor",
						"Slate",
						"SlateCore",
						"UnrealEd"
					});
			}
		}
	}
}

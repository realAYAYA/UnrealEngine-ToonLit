// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{	public class DatasmithInterchangeScripting : ModuleRules
	{
		public DatasmithInterchangeScripting(ReadOnlyTargetRules Target)
			: base(Target)
		{

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DatasmithInterchange",
					"DatasmithContent",
					"DatasmithCore",
					"DatasmithTranslator",
					"ExternalSource",
					"InterchangeEngine",
					"InterchangeCommonParser",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InterchangeCore",
				}
			);
		}
	}
}
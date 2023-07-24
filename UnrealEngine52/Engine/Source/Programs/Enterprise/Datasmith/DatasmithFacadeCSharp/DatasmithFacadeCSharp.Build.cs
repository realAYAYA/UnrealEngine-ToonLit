// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DatasmithFacadeCSharp : ModuleRules
	{
		public DatasmithFacadeCSharp(ReadOnlyTargetRules Target)
			: base(Target)
		{
			bUseRTTI = true;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"DatasmithCore",
					"DatasmithExporter",
					"DatasmithExporterUI",
					"DatasmithFacade",
					"UdpMessaging", // required for DirectLink networking
				}
			);

			bRequiresImplementModule = false;
		}
	}
}

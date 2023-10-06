// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DatasmithSDK : ModuleRules
	{
		public DatasmithSDK(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PublicIncludePathModuleNames.Add("Launch");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DatasmithCore",
					"DatasmithExporter",
					"DatasmithExporterUI",
					// Network layer
					"UdpMessaging",
				}
			);
		}
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DatasmithSDK : ModuleRules
	{
		public DatasmithSDK(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PublicIncludePaths.Add("Runtime/Launch/Public");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DatasmithCore",
					"DatasmithExporter",
					"UdpMessaging", // required for DirectLink networking
				}
			);
		}
	}
}
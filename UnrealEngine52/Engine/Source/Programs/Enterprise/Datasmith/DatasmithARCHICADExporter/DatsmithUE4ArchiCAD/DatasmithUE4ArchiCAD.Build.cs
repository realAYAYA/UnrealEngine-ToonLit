// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DatasmithUE4ArchiCAD : ModuleRules
	{
		public DatasmithUE4ArchiCAD(ReadOnlyTargetRules Target)
			: base(Target)
		{
			bUseRTTI = true;
			PCHUsage = PCHUsageMode.NoPCHs;

			PublicIncludePaths.Add("Runtime/Launch/Public");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DatasmithCore",
					"DatasmithExporter",
					"DatasmithExporterUI",

					"UEOpenExr",

					// Network layer
					"UdpMessaging",
					"MessagingCommon",
					"Messaging",
                    
                    "DatasmithValidator",
				}
			);
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DatasmithIFCTranslator : ModuleRules
	{
		public DatasmithIFCTranslator(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"CoreUObject",
					"DatasmithCore",
					"Engine",
					"Json",
					"MeshDescription",
					"Projects",
					"RawMesh",
					"Slate",
					"SlateCore",
					"StaticMeshDescription",
					"XmlParser",
				}
			);

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.Add("MessageLog");
			}

			string IfcEngineDir = Path.Combine(EngineDirectory, "Restricted/NotForLicensees/Source/ThirdParty/Enterprise/ifcengine");
			if (Directory.Exists(IfcEngineDir))
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"IFCEngine",
					}
				);
			}

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"DatasmithContent",
					"DatasmithTranslator"
				}
			);

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				RuntimeDependencies.Add("$(EngineDir)/Plugins/Enterprise/DatasmithIFCImporter/Binaries/Win64/ifcengine.dll");
				PublicDelayLoadDLLs.Add("ifcengine.dll");
			}

			RuntimeDependencies.Add(@"$(EngineDir)\Plugins\Enterprise\DatasmithIFCImporter\Resources\IFC\IFC2X3_TC1.exp");
			RuntimeDependencies.Add(@"$(EngineDir)\Plugins\Enterprise\DatasmithIFCImporter\Resources\IFC\IFC2X3-Settings.xml");
			RuntimeDependencies.Add(@"$(EngineDir)\Plugins\Enterprise\DatasmithIFCImporter\Resources\IFC\IFC4.exp");
			RuntimeDependencies.Add(@"$(EngineDir)\Plugins\Enterprise\DatasmithIFCImporter\Resources\IFC\IFC4_ADD2.exp");
			RuntimeDependencies.Add(@"$(EngineDir)\Plugins\Enterprise\DatasmithIFCImporter\Resources\IFC\IFC4-Settings.xml");
			RuntimeDependencies.Add(@"$(EngineDir)\Plugins\Enterprise\DatasmithIFCImporter\Resources\IFC\IFC4x1_FINAL.exp");
		}
	}
}

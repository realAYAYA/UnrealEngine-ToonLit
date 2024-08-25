// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class XmlSerialization : ModuleRules
{
	public XmlSerialization(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
		);

		// PrivateDefinitions.Add("PUGIXML_WCHAR_MODE"); // for pugixml wchar support

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"MaterialX", // for pugixml
				"Serialization",
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "MaterialX");
	}
}

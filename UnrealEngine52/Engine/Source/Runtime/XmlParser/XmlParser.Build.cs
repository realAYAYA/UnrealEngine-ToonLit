// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XmlParser : ModuleRules
{
	public XmlParser( ReadOnlyTargetRules Target ) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
			});
		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}

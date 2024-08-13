// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ZAsio : ModuleRules
{
	public ZAsio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
		);
		
		PublicDefinitions.Add("ASIO_STANDALONE");
		PublicDefinitions.Add("ASIO_SEPARATE_COMPILATION");
		PublicDefinitions.Add("ASIO_DYN_LINK");
		PublicDefinitions.Add("ASIO_HAS_STD_SYSTEM_ERROR");
		PublicDefinitions.Add("ASIO_HAS_STD_ARRAY");
		PublicDefinitions.Add("ASIO_HAS_STD_ATOMIC");
		PublicDefinitions.Add("ASIO_HAS_STD_TYPE_TRAITS");
		PublicDefinitions.Add("ASIO_NO_EXCEPTIONS");
		PublicDefinitions.Add("ASIO_NO_TYPEID");
		
		PublicDefinitions.Add("ASIO_NO_DEPRECATED");
		//PublicDefinitions.Add("ASIO_NO_TS_EXECUTORS");
		PublicDefinitions.Add("ASIO_NO_NOMINMAX");
		
		ShadowVariableWarningLevel = WarningLevel.Off;
		bEnableUndefinedIdentifierWarnings = false;
		bEnableExceptions = false;

	}
}

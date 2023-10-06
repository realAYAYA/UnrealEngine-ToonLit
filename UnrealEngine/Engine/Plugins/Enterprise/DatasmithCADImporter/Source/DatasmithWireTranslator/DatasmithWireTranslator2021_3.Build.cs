// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class DatasmithWireTranslator2021_3 : DatasmithWireTranslatorBase
{
	public DatasmithWireTranslator2021_3(ReadOnlyTargetRules Target) 
		: base(Target)
	{
	}

	public override string GetAliasVersion()
	{
		return "OpenModel2021_3";
	}
	
	public override string GetAliasDefinition()
	{
		return "OPEN_MODEL_2021_3";
	}
}

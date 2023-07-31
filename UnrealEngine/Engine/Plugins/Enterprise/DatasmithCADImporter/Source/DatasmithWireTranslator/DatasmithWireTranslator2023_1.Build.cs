// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class DatasmithWireTranslator2023_1 : DatasmithWireTranslatorBase
{
	public DatasmithWireTranslator2023_1(ReadOnlyTargetRules Target) 
		: base(Target)
	{
	}

	public override string GetAliasVersion()
	{
		return "OpenModel2023_1";
	}

	public override string GetAliasDefinition()
	{
		return "OPEN_MODEL_2023_1";
	}
}
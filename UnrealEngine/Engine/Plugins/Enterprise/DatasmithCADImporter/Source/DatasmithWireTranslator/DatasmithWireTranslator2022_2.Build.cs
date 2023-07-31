// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class DatasmithWireTranslator2022_2 : DatasmithWireTranslatorBase
{
	public DatasmithWireTranslator2022_2(ReadOnlyTargetRules Target) 
		: base(Target)
	{
	}

	public override string GetAliasVersion()
	{
		return "OpenModel2022_2";
	}

	public override string GetAliasDefinition()
	{
		return "OPEN_MODEL_2022_2";
	}
}
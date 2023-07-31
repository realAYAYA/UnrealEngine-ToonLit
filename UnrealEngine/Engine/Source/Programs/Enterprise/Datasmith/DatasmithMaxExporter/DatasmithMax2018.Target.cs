// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DatasmithMax2018Target : DatasmithMaxBaseTarget
{
	public DatasmithMax2018Target(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "DatasmithMax2018";
		ExeBinariesSubFolder = @"3DSMax\2018";

		AddCopyPostBuildStep(Target);
	}
}

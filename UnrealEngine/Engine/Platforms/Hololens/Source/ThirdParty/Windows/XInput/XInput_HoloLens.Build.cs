// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class XInput_HoloLens : XInput
	{
		public XInput_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicSystemLibraries.Add("xinputuap.lib");
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");
		}
	}
}

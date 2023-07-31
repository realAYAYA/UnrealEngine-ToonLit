// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{

	[SupportedPlatforms("Win64", "Mac")]
	public class DatasmithSketchUpRuby2019 : DatasmithSketchUpRubyBase
	{
		public DatasmithSketchUpRuby2019(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PrivateDefinitions.Add("SKP_SDK_2019");
		}

		public override string GetSketchUpSDKFolder()
		{
			return "SDK_WIN_x64_2019-3-253";
		}

		public override string GetSketchUpEnvVar()
		{
			return "SKP_SDK_2019";
		}
		public override string GetRubyLibName()
		{
			return "x64-msvcrt-ruby250.lib";
		}
	}
}

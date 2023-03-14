// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{

	[SupportedPlatforms("Win64", "Mac")]
	public class DatasmithSketchUpRuby2022 : DatasmithSketchUpRubyBase
	{
		public DatasmithSketchUpRuby2022(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PrivateDefinitions.Add("SKP_SDK_2022");
		}

		public override string GetSketchUpSDKFolder()
		{
			return "SDK_WIN_x64_2022-0-316";
		}

		public override string GetSketchUpEnvVar()
		{
			return "SKP_SDK_2022";
		}
		public override string GetRubyLibName()
		{
			return "x64-msvcrt-ruby270.lib";
		}
	}
}

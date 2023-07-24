// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{

	[SupportedPlatforms("Win64", "Mac")]
	public class DatasmithSketchUpRuby2023 : DatasmithSketchUpRubyBase
	{
		public DatasmithSketchUpRuby2023(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PrivateDefinitions.Add("SKP_SDK_2023");
		}

		public override string GetSketchUpSDKFolder()
		{
			return "SDK_WIN_x64_2023-0-367";
		}

		public override string GetSketchUpEnvVar()
		{
			return "SKP_SDK_2023";
		}
		public override string GetRubyLibName()
		{
			return "x64-msvcrt-ruby270.lib";
		}
	}
}

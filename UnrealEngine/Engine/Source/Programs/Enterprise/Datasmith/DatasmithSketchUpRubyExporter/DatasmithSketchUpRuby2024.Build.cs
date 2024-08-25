// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{

	[SupportedPlatforms("Win64", "Mac")]
	public class DatasmithSketchUpRuby2024 : DatasmithSketchUpRubyBase
	{
		public DatasmithSketchUpRuby2024(ReadOnlyTargetRules Target)
			: base(Target)
		{
OptimizeCode = CodeOptimization.Never;
bUseUnity = false;
PCHUsage = PCHUsageMode.NoPCHs;

			PrivateDefinitions.Add("SKP_SDK_2024");
		}

		public override string GetSketchUpSDKFolder()
		{
			return "SDK_WIN_x64_2024-0-484";
		}

		public override string GetSketchUpEnvVar()
		{
			return "SKP_SDK_2024";
		}
		public override string GetRubyLibName()
		{
			return "x64-ucrt-ruby320.lib";
		}
	}
}

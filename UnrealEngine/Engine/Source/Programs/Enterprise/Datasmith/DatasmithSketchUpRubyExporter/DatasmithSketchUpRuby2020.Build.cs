// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using EpicGames.Core;

namespace UnrealBuildTool.Rules
{
	public abstract class DatasmithSketchUpRubyBase : ModuleRules
	{
		public DatasmithSketchUpRubyBase(ReadOnlyTargetRules Target)
			: base(Target)
		{
			bUseRTTI = true;

			//OptimizeCode = CodeOptimization.Never;
			//bUseUnity = false;
			//PCHUsage = PCHUsageMode.NoPCHs;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DatasmithExporter",
					"DatasmithExporterUI",

					"UdpMessaging", // required for DirectLink networking
					"Imath",
				}
			);

			// Set up the SketchUp SDK paths and libraries.
			{
				string SketchUpSDKLocation = System.Environment.GetEnvironmentVariable(GetSketchUpEnvVar());

				if (!Directory.Exists(SketchUpSDKLocation))
				{
					// Try with build machine setup
					string SDKRootEnvVar = System.Environment.GetEnvironmentVariable("UE_SDKS_ROOT");
					if (SDKRootEnvVar != null && SDKRootEnvVar != "")
					{
						SketchUpSDKLocation = Path.Combine(SDKRootEnvVar, "HostWin64", "Win64", "SketchUp", GetSketchUpSDKFolder());
					}
				}

				// Make sure this version of the SketchUp SDK is actually installed.
				if (Directory.Exists(SketchUpSDKLocation))
				{
					PrivateIncludePaths.Add(Path.Combine(SketchUpSDKLocation, "headers"));
					PublicAdditionalLibraries.Add(Path.Combine(SketchUpSDKLocation, "binaries", "sketchup", "x64", "sketchup.lib"));
					PublicDelayLoadDLLs.Add("SketchUpAPI.dll");

					PrivateIncludePaths.Add(Path.Combine(SketchUpSDKLocation, "samples", "common", "ThirdParty", "ruby", "include", "win32_x64"));
					PublicAdditionalLibraries.Add(Path.Combine(SketchUpSDKLocation, "samples", "common", "ThirdParty", "ruby", "lib", "win", "x64", GetRubyLibName()));
				}

				if (!Directory.Exists(SketchUpSDKLocation) && !Target.bGenerateProjectFiles)
				{
					Log.TraceWarningOnce("Unable to find SketchUp SDK directory. SketchUp plugins will not compile");
				}
			}
		}

		public abstract string GetSketchUpSDKFolder();
		public abstract string GetSketchUpEnvVar();
		public abstract string GetRubyLibName();
	}

	[SupportedPlatforms("Win64", "Mac")]
	public class DatasmithSketchUpRuby2020 : DatasmithSketchUpRubyBase
	{
		public DatasmithSketchUpRuby2020(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PrivateDefinitions.Add("SKP_SDK_2020");
		}

		public override string GetSketchUpSDKFolder()
		{
			return "SDK_WIN_x64_2020-2-172";
		}

		public override string GetSketchUpEnvVar()
		{
			return "SKP_SDK_2020";
		}
		public override string GetRubyLibName()
		{
			return "x64-msvcrt-ruby250.lib";
		}
	}
}

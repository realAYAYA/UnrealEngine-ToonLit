// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using Microsoft.Extensions.Logging;
using Microsoft.Win32;
using EpicGames.Core;
using UnrealBuildBase;
using System.Runtime.Versioning;

namespace UnrealBuildTool
{
	static class Extension
	{
		public static void AddFormat(this List<string> list, string formatString, params object[] args)
		{
			list.Add(String.Format(formatString, args));
		}
	}

	[SupportedOSPlatform("windows")]
	class HoloLensToolChain : VCToolChain
	{
		public HoloLensToolChain(ReadOnlyTargetRules Target, ILogger Logger)
			: base(Target, Logger)
		{
			// by default tools chains don't parse arguments, but we want to be able to check the -architectures flag defined above. This is
			// only necessary when AndroidToolChain is used during UAT
			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), this, Logger);
		}

		protected override bool UseWindowsArchitecture(UnrealTargetPlatform Platform)
		{
			return true;
		}

		protected override void AppendCLArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.AppendCLArguments_Global(CompileEnvironment, Arguments);

			// @todo HoloLens: Disable "unreachable code" warning since auto-included vccorlib.h triggers it
			Arguments.Add("/wd4702");

			// @todo HoloLens: Disable "usage of ATL attributes is deprecated" since WRL headers generate this
			Arguments.Add("/wd4467");

			// @todo HoloLens: Disable "declaration of 'identifier' hides class member"
			Arguments.Add("/wd4458");

			// @todo HoloLens: Disable "not defined as a preprocessor macro, replacing with '0'"
			Arguments.Add("/wd4668");

			Arguments.RemoveAll(Argument => Argument.StartsWith("/RTC"));
			if (CompileEnvironment.Configuration == CppConfiguration.Debug && CompileEnvironment.bUseDebugCRT)
			{
				Arguments.Add("/RTC1");
			}

			// C2338: two-phase name lookup is not supported for C++/CLI or C++/CX; use /Zc:twoPhase-
			if (Target.WindowsPlatform.Compiler.IsMSVC() && Target.WindowsPlatform.bStrictConformanceMode)
			{
				Arguments.Add("/Zc:twoPhase-");
			}

			DirectoryReference? PlatformWinMDLocation = HoloLensPlatform.GetCppCXMetadataLocation(EnvVars.Compiler, EnvVars.ToolChainDir);
			if (PlatformWinMDLocation != null)
			{
				Arguments.AddFormat(@"/AI""{0}""", PlatformWinMDLocation);
				Arguments.AddFormat(@"/FU""{0}\platform.winmd""", PlatformWinMDLocation);
			}
		}

		protected override void AppendCLArguments_CPP(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.AppendCLArguments_CPP(CompileEnvironment, Arguments);

			// Enable Windows Runtime extensions.  Do this even for libs (plugins) so that these too can consume WinRT APIs
			Arguments.Add("/ZW");

			// Don't automatically add metadata references.  We'll do that ourselves to avoid referencing windows.winmd directly:
			// we've hit problems where types are somehow in windows.winmd on some installations but not others, leading to either
			// missing or duplicated type references.
			Arguments.Add("/ZW:nostdlib");
		}

		protected override void AppendLinkArguments(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			base.AppendLinkArguments(LinkEnvironment, Arguments);

			// @todo HoloLens: This is logic from before inheriting from VCToolChain. Is not embedding the manifest necessary?
			if (!Arguments.Contains("/MANIFEST:NO"))
			{
				Arguments.Remove("/MANIFEST:EMBED");
				Arguments.RemoveAll((x) => x.StartsWith("/MANIFESTINPUT"));

				// Don't create a side-by-side manifest file for the executable.
				Arguments.Add("/MANIFEST:NO");
			}

			Arguments.Add("/APPCONTAINER");

			// this helps with store API compliance validation tools, adding additional pdb info
			Arguments.Add("/PROFILE");
		}

		protected override void AppendLibArguments(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			base.AppendLibArguments(LinkEnvironment, Arguments);

			// Ignore warning about /ZW in static libraries. It's not relevant since UE modules
			// have no reason to export new WinRT types, and ignoring it quiets noise when using
			// WinRT APIs from plugins.
			Arguments.Add("/ignore:4264");
		}

		protected override void ModifyFinalCompileAction(VCCompileAction CompileAction, CppCompileEnvironment CompileEnvironment, FileItem SourceFile, DirectoryReference OutputDir, string ModuleName)
		{
			base.ModifyFinalCompileAction(CompileAction, CompileEnvironment, SourceFile, OutputDir, ModuleName);

			if (Target.HoloLensPlatform.bRunNativeCodeAnalysis)
			{
				// Add the analysis log to the produced item list.
				FileItem AnalysisLogFile = FileItem.GetItemByFileReference(
					FileReference.Combine(
						OutputDir,
						Path.GetFileName(SourceFile.AbsolutePath) + ".nativecodeanalysis.xml"
						)
					); ;
				CompileAction.AdditionalProducedItems.Add(AnalysisLogFile);
				// Peform code analysis with results in a log file
				CompileAction.Arguments.AddFormat("/analyze:log \"{0}\"", AnalysisLogFile.AbsolutePath);
				// Suppress code analysis output
				CompileAction.Arguments.Add("/analyze:quiet");
				string? rulesetFile = Target.HoloLensPlatform.NativeCodeAnalysisRuleset;
				if (!String.IsNullOrEmpty(rulesetFile))
				{
					if (!Path.IsPathRooted(rulesetFile))
					{
						rulesetFile = FileReference.Combine(Target.ProjectFile!.Directory, rulesetFile).FullName;
					}
					// A non default ruleset was specified
					CompileAction.Arguments.AddFormat("/analyze:ruleset \"{0}\"", rulesetFile);
				}
			}
		}



		private static DirectoryReference? CurrentWindowsSdkBinDir = null;
		private static Version? CurrentWindowsSdkVersion;

		public static bool InitWindowsSdkToolPath(string SdkVersion, ILogger Logger)
		{
			if (string.IsNullOrEmpty(SdkVersion))
			{
				Logger.LogError("WinSDK version is empty");
				return false;
			}

			VersionNumber OutSdkVersion;
			DirectoryReference OutSdkDir;

			if (!WindowsPlatform.TryGetWindowsSdkDir(SdkVersion, Logger, out OutSdkVersion!, out OutSdkDir!))
			{
				Logger.LogError("Failed to find WinSDK {SdkVersion}", SdkVersion);
				return false;
			}

			DirectoryReference WindowsSdkBinDir = DirectoryReference.Combine(OutSdkDir, "bin", OutSdkVersion.ToString(), Environment.Is64BitProcess ? "x64" : "x86");

			if(!DirectoryReference.Exists(WindowsSdkBinDir))
			{
				Logger.LogError("WinSDK {SdkVersion} doesn't exit", SdkVersion);
				return false;
			}

			CurrentWindowsSdkVersion = new Version(OutSdkVersion.ToString());
			CurrentWindowsSdkBinDir = WindowsSdkBinDir;
			return true;
		}

		public static FileReference? GetWindowsSdkToolPath(string ToolName)
		{
			FileReference file = FileReference.Combine(CurrentWindowsSdkBinDir!, ToolName);

			if(!FileReference.Exists(file))
			{
				return null;
			}

			return file;
		}

		public static Version GetCurrentWindowsSdkVersion()
		{
			return CurrentWindowsSdkVersion!;
		}

		public override string GetSDKVersion()
		{
			return CurrentWindowsSdkVersion!.ToString();
		}

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			base.ModifyBuildProducts(Target, Binary, Libraries, BundleResources, BuildProducts);

			DirectoryReference HoloLensBinaryDirectory = Binary.OutputFilePath.Directory;

			AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, "AppxManifest_"+ Target.Architecture + ".xml"), BuildProductType.BuildResource);
			AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, "resources_"+ Target.Architecture + ".pri"), BuildProductType.BuildResource);
			if (Target.Configuration == UnrealTargetConfiguration.Development)
			{
				AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Name + Target.Architecture + ".exe"), BuildProductType.Executable);
				AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Name + Target.Architecture + ".pdb"), BuildProductType.SymbolFile);
			}
			else
			{
				AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Name + "-HoloLens-" + Target.Configuration + Target.Architecture + ".exe"), BuildProductType.Executable);
				AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Name + "-HoloLens-" + Target.Configuration + Target.Architecture + ".pdb"), BuildProductType.SymbolFile);
			}
			AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Name + "-HoloLens-" + Target.Configuration + Target.Architecture + ".target"), BuildProductType.BuildResource);
			AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Architecture + "\\Resources\\Logo.png"), BuildProductType.BuildResource);
			AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Architecture + "\\Resources\\resources.resw"), BuildProductType.BuildResource);
			AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Architecture + "\\Resources\\SmallLogo.png"), BuildProductType.BuildResource);
			AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Architecture + "\\Resources\\SplashScreen.png"), BuildProductType.BuildResource);
			AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Architecture + "\\Resources\\WideLogo.png"), BuildProductType.BuildResource);
			AddBuildProductSafe(BuildProducts, FileReference.Combine(HoloLensBinaryDirectory, Target.Architecture + "\\Resources\\en\\resources.resw"), BuildProductType.BuildResource);
		}

		private void AddBuildProductSafe(Dictionary<FileReference, BuildProductType> BuildProducts, FileReference FileToAdd, BuildProductType ProductType)
		{
			if (!BuildProducts.ContainsKey(FileToAdd))
			{
				BuildProducts.Add(FileToAdd, ProductType);
			}
		}

	};
}

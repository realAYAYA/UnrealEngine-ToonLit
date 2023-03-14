// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Text;
using EpicGames.Core;
using System.Text.RegularExpressions;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	abstract class AppleToolChainSettings
	{
		/// <summary>
		/// Which developer directory to root from? If this is "xcode-select", UBT will query for the currently selected Xcode
		/// </summary>
		public string XcodeDeveloperDir = "xcode-select";

		public AppleToolChainSettings(bool bVerbose, ILogger Logger)
		{
			SelectXcode(ref XcodeDeveloperDir, bVerbose, Logger);
		}

		private static void SelectXcode(ref string DeveloperDir, bool bVerbose, ILogger Logger)
		{
			string Reason = "hardcoded";

			if (DeveloperDir == "xcode-select")
			{
				Reason = "xcode-select";

				// on the Mac, run xcode-select directly
				DeveloperDir = Utils.RunLocalProcessAndReturnStdOut("xcode-select", "--print-path", Logger);

				// make sure we get a full path
				if (Directory.Exists(DeveloperDir) == false)
				{
					throw new BuildException("Selected Xcode ('{0}') doesn't exist, cannot continue.", DeveloperDir);
				}

				if (DeveloperDir.EndsWith("/") == false)
				{
					// we expect this to end with a slash
					DeveloperDir += "/";
				}
			}

			if (bVerbose && !DeveloperDir.StartsWith("/Applications/Xcode.app"))
			{
				Log.TraceInformationOnce("Compiling with non-standard Xcode ({0}): {1}", Reason, DeveloperDir);
			}

			// Installed engine requires Xcode 11
			if (Unreal.IsEngineInstalled())
			{
				string? InstalledSdkVersion = UnrealBuildBase.ApplePlatformSDK.InstalledSDKVersion;
				if (String.IsNullOrEmpty(InstalledSdkVersion))
				{
					throw new BuildException("Unable to get xcode version");
				}
				if (int.Parse(InstalledSdkVersion.Substring(0,2)) < 11)
				{
					throw new BuildException("Building for macOS, iOS and tvOS requires Xcode 11 or newer, Xcode " + InstalledSdkVersion + " detected");
				}
			}
		}

		protected void SelectSDK(string BaseSDKDir, string OSPrefix, ref string PlatformSDKVersion, bool bVerbose, ILogger Logger)
		{
			if (PlatformSDKVersion == "latest")
			{
				PlatformSDKVersion = "";
				try
				{
					// on the Mac, we can just get the directory name
					string[] SubDirs = System.IO.Directory.GetDirectories(BaseSDKDir);

					// loop over the subdirs and parse out the version
					int MaxSDKVersionMajor = 0;
					int MaxSDKVersionMinor = 0;
					string? MaxSDKVersionString = null;
					foreach (string SubDir in SubDirs)
					{
						string SubDirName = Path.GetFileNameWithoutExtension(SubDir);
						if (SubDirName.StartsWith(OSPrefix))
						{
							// get the SDK version from the directory name
							string SDKString = SubDirName.Replace(OSPrefix, "");
							int Major = 0;
							int Minor = 0;

							// parse it into whole and fractional parts (since 10.10 > 10.9 in versions, but not in math)
							try
							{
								string[] Tokens = SDKString.Split(".".ToCharArray());
								if (Tokens.Length == 2)
								{
									Major = int.Parse(Tokens[0]);
									Minor = int.Parse(Tokens[1]);
								}
							}
							catch (Exception)
							{
								// weirdly formatted SDKs
								continue;
							}

							// update largest SDK version number
							if (Major > MaxSDKVersionMajor || (Major == MaxSDKVersionMajor && Minor > MaxSDKVersionMinor))
							{
								MaxSDKVersionString = SDKString;
								MaxSDKVersionMajor = Major;
								MaxSDKVersionMinor = Minor;
							}
						}
					}

					// use the largest version
					if (MaxSDKVersionString != null)
					{
						PlatformSDKVersion = MaxSDKVersionString;
					}
				}
				catch (Exception Ex)
				{
					// on any exception, just use the backup version
					Logger.LogInformation("Triggered an exception while looking for SDK directory in Xcode.app");
					Logger.LogInformation("{Ex}", Ex.ToString());
				}
			}

			// make sure we have a valid SDK directory
			if (!RuntimePlatform.IsWindows && !Directory.Exists(Path.Combine(BaseSDKDir, OSPrefix + PlatformSDKVersion + ".sdk")))
			{
				throw new BuildException("Invalid SDK {0}{1}.sdk, not found in {2}", OSPrefix, PlatformSDKVersion, BaseSDKDir);
			}

			if (bVerbose && !ProjectFileGenerator.bGenerateProjectFiles)
			{
				Logger.LogInformation("Compiling with {Os} SDK {Sdk}", OSPrefix, PlatformSDKVersion);
			}
		}
	}

	abstract class AppleToolChain : ClangToolChain
	{
		protected class AppleToolChainInfo : ClangToolChainInfo
		{
			public AppleToolChainInfo(FileReference Clang, FileReference Archiver, ILogger Logger)
				: base(Clang, Archiver, Logger)
			{
			}

			// libtool doesn't provide version, just use clang's version string
			/// <inheritdoc/>
			protected override string QueryArchiverVersionString() => ClangVersionString;
		}

		protected FileReference? ProjectFile;

		public AppleToolChain(FileReference? InProjectFile, ClangToolChainOptions InOptions, ILogger InLogger) : base(InOptions, InLogger)
		{
			ProjectFile = InProjectFile;
		}

		protected DirectoryReference GetMacDevSrcRoot()
		{
			return Unreal.EngineSourceDirectory;
		}

		protected void StripSymbolsWithXcode(FileReference SourceFile, FileReference TargetFile, string ToolchainDir)
		{
			if (SourceFile != TargetFile)
			{
				// Strip command only works in place so we need to copy original if target is different
				File.Copy(SourceFile.FullName, TargetFile.FullName, true);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = Path.Combine(ToolchainDir, "strip");
			StartInfo.Arguments = String.Format("\"{0}\" -S", TargetFile.FullName);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo, Logger);
		}

		/// <inheritdoc/>
		protected override string EscapePreprocessorDefinition(string Definition)
		{
			return Definition.Contains("\"") ? Definition.Replace("\"", "\\\"") : Definition;
		}

		protected override void GetCompileArguments_CPP(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c++");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
		}

		protected override void GetCompileArguments_MM(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c++");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
		}

		protected override void GetCompileArguments_M(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
		}

		protected override void GetCompileArguments_PCH(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c++-header");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");

			if (CompilerVersionGreaterOrEqual(13, 0, 0)) // Note this is supported for >=11 on other clang platforms
			{
				Arguments.Add("-fpch-instantiate-templates");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_WarningsAndErrors(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_WarningsAndErrors(CompileEnvironment, Arguments);

			// Flags added in Xcode 13.3 (Apple clang 13.1.6)
			if (CompilerVersionGreaterOrEqual(13, 0, 0) && CompilerVersionLessThan(13, 1, 6))
			{
				Arguments.Remove("-Wno-unused-but-set-variable");
				Arguments.Remove("-Wno-unused-but-set-parameter");
				Arguments.Remove("-Wno-ordered-compare-function-pointers");
			}

			// clang 12.00 has a new warning for copies in ranged loops. Instances have all been fixed up (2020/6/26) but
			// are likely to be reintroduced due to no equivalent on other platforms at this time so disable the warning
			if (CompilerVersionGreaterOrEqual(12, 0, 0))
			{
				Arguments.Add("-Wno-range-loop-analysis");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Optimizations(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Optimizations(CompileEnvironment, Arguments);

			bool bStaticAnalysis = false;
			string? StaticAnalysisMode = Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE");
			if (!string.IsNullOrEmpty(StaticAnalysisMode))
			{
				bStaticAnalysis = true;
			}

			// Optimize non- debug builds.
			if (CompileEnvironment.bOptimizeCode && !bStaticAnalysis)
			{
				// Don't over optimise if using AddressSanitizer or you'll get false positive errors due to erroneous optimisation of necessary AddressSanitizer instrumentation.
				if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer))
				{
					Arguments.Add("-O1");
					Arguments.Add("-g");
					Arguments.Add("-fno-optimize-sibling-calls");
					Arguments.Add("-fno-omit-frame-pointer");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
				{
					Arguments.Add("-O1");
					Arguments.Add("-g");
				}
				else if (CompileEnvironment.OptimizationLevel == OptimizationMode.Size)
				{
					Arguments.Add("-Oz");
				}
				else if (CompileEnvironment.OptimizationLevel == OptimizationMode.SizeAndSpeed)
				{
					Arguments.Add("-Os");

					if (CompileEnvironment.Architecture.StartsWith("aarch64"))
					{
						Arguments.Add("-moutline");
					}
				}
				else
				{
					Arguments.Add("-O3");
				}
			}
			else
			{
				Arguments.Add("-O0");
			}
		}


		/// <inheritdoc/>
		protected override void GetCompileArguments_Debugging(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Debugging(CompileEnvironment, Arguments);

			// Create DWARF format debug info if wanted,
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Arguments.Add("-gdwarf-2");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Analyze(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Analyze(CompileEnvironment, Arguments);

			// Disable all clang tidy checks
			Arguments.Add($"-Xclang -analyzer-tidy-checker=-*");
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Global(CompileEnvironment, Arguments);

			Arguments.Add(GetRTTIFlag(CompileEnvironment));

			Arguments.Add("-fmessage-length=0");
			Arguments.Add("-fpascal-strings");
		}

		protected string GetDsymutilPath(ILogger Logger, out string ExtraOptions, bool bIsForLTOBuild=false)
		{
			FileReference DsymutilLocation = new FileReference("/usr/bin/dsymutil");

			// dsymutil before 10.0.1 has a bug that causes issues, it's fixed in autosdks but not everyone has those set up so for the timebeing we have
			// a version in P4 - first determine if we need it
			string DsymutilVersionString = Utils.RunLocalProcessAndReturnStdOut(DsymutilLocation.FullName, "-version", Logger);

			bool bUseInstalledDsymutil = true;
			int Major = 0, Minor = 0, Patch = 0;

			// tease out the version number
			string[] Tokens = DsymutilVersionString.Split(" ".ToCharArray());
			
			// sanity check
			if (Tokens.Length < 4 || Tokens[3].Contains(".") == false)
			{
				Log.TraceInformationOnce("Unable to parse dsymutil version out of: {0}", DsymutilVersionString);
			}
			else
			{
				string[] Versions = Tokens[3].Split(".".ToCharArray());
				if (Versions.Length < 3)
				{
					Log.TraceInformationOnce("Unable to parse version token: {0}", Tokens[3]);
				}
				else
				{
					if (!int.TryParse(Versions[0], out Major) || !int.TryParse(Versions[1], out Minor) || !int.TryParse(Versions[2], out Patch))
					{
						Log.TraceInformationOnce("Unable to parse version tokens: {0}", Tokens[3]);
					}
					else
					{
						if (Major < 12)
						{
							Log.TraceInformationOnce("dsymutil version is {0}.{1}.{2}. Using bundled version.", Major, Minor, Patch);
							bUseInstalledDsymutil = false;
						}
					}
				}
			}

			// if the installed one is too old, use a fixed up one if it can
			if (bUseInstalledDsymutil == false)
			{
				FileReference PatchedDsymutilLocation = FileReference.Combine(Unreal.EngineDirectory, "Restricted/NotForLicensees/Binaries/Mac/LLVM/bin/dsymutil");

				if (File.Exists(PatchedDsymutilLocation.FullName))
				{
					DsymutilLocation = PatchedDsymutilLocation;
				}

				DirectoryReference? AutoSdkDir;
				if (UEBuildPlatformSDK.TryGetHostPlatformAutoSDKDir(out AutoSdkDir))
				{
					FileReference AutoSdkDsymutilLocation = FileReference.Combine(AutoSdkDir, "Mac", "LLVM", "bin", "dsymutil");
					if (FileReference.Exists(AutoSdkDsymutilLocation))
					{
						DsymutilLocation = AutoSdkDsymutilLocation;
					}
				}
			}

			// 10.0.1 has an issue with LTO builds where we need to limit the number of threads
			ExtraOptions = (bIsForLTOBuild && Major == 10 && Minor == 0 && Patch == 1) ? "-j 1" : "";
			return DsymutilLocation.FullName;
		}
	};
}

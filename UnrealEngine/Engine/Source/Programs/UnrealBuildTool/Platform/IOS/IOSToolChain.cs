// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using EpicGames.Core;
using Ionic.Zip;
using Ionic.Zlib;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	class IOSToolChainSettings : AppleToolChainSettings
	{
		public IOSToolChainSettings(ILogger Logger) 
			: this("iPhoneOS", "iPhoneSimulator", "ios", Logger)
		{
		}

		protected IOSToolChainSettings(string DevicePlatformName, string SimulatorPlatformName, string TargetOSName, ILogger Logger) 
			: base(DevicePlatformName, SimulatorPlatformName, TargetOSName, true, Logger)
		{
		}
	}

	class IOSToolChain : AppleToolChain
	{
		private static List<FileItem> BundleDependencies = new List<FileItem>();

		protected IOSProjectSettings ProjectSettings;

		public IOSToolChain(ReadOnlyTargetRules? Target, IOSProjectSettings InProjectSettings, ClangToolChainOptions ToolchainOptions, ILogger InLogger)
			: this(Target, InProjectSettings, () => new IOSToolChainSettings(InLogger), ToolchainOptions, InLogger)
		{
		}

		protected IOSToolChain(ReadOnlyTargetRules? Target, IOSProjectSettings InProjectSettings, Func<AppleToolChainSettings> InCreateSettings, ClangToolChainOptions ToolchainOptions, ILogger InLogger)
			: base(Target, InCreateSettings, ToolchainOptions, InLogger)
		{
			ProjectSettings = InProjectSettings;
		}

		// ***********************************************************************
		// * NOTE:
		// *  Do NOT change the defaults to set your values, instead you should set the environment variables
		// *  properly in your system, as other tools make use of them to work properly!
		// *  The defaults are there simply for examples so you know what to put in your env vars...
		// ***********************************************************************

		// If you are looking for where to change the remote compile server name, look in RemoteToolChain.cs

		/// <summary>
		/// If this is set, then we do not do any post-compile steps -- except moving the executable into the proper spot on Mac.
		/// </summary>
		[XmlConfigFile]
		public static bool bUseDangerouslyFastMode = false;

		/// <summary>
		/// Which compiler\linker frontend to use
		/// </summary>
		private const string IOSCompiler = "clang++";

		/// <summary>
		/// Which library archiver to use
		/// </summary>
		private const string IOSArchiver = "libtool";

		private IOSToolChainSettings Settings => (IOSToolChainSettings)ToolChainSettings.Value;

		protected override ClangToolChainInfo GetToolChainInfo()
		{
			FileReference CompilerPath = FileReference.Combine(Settings.ToolchainDir, IOSCompiler);
			FileReference ArchiverPath = FileReference.Combine(Settings.ToolchainDir, IOSArchiver);
			return new AppleToolChainInfo(CompilerPath, ArchiverPath, Logger);
		}

		public override string GetSDKVersion()
		{
			return Settings.SDKVersionFloat.ToString();
		}

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, IEnumerable<string> Libraries, IEnumerable<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			if (Target.IOSPlatform.bCreateStubIPA && Binary.Type != UEBuildBinaryType.StaticLibrary)
			{
				FileReference StubFile = FileReference.Combine(Binary.OutputFilePath.Directory, Binary.OutputFilePath.GetFileNameWithoutExtension() + ".stub");
				BuildProducts.Add(StubFile, BuildProductType.Package);

				// These Payload assets and .app bundle are no longer needed for Modern Xcode
				if (!bUseModernXcode)
				{
					if (Target.Platform == UnrealTargetPlatform.TVOS)
					{
						FileReference AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "AssetCatalog", "Assets.car");
						BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					}
					else if (Target.Platform == UnrealTargetPlatform.IOS)
					{
						int Index = Binary.OutputFilePath.GetFileNameWithoutExtension().IndexOf("-");
						string OutputFile = Binary.OutputFilePath.GetFileNameWithoutExtension().Substring(0, Index > 0 ? Index : Binary.OutputFilePath.GetFileNameWithoutExtension().Length);
						FileReference AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "Assets.car");
						BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);

						// Default AppIcons need to be copied as is in the Payload's root, even when using an asset catalog.
						AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon60x60@2x.png");
						BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
						AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon76x76@2x~ipad.png");
						BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					}
				}
			}
			if (Target.IOSPlatform.bGeneratedSYM && ProjectSettings.bGenerateCrashReportSymbols && Binary.Type == UEBuildBinaryType.StaticLibrary)
			{
				FileReference DebugFile = FileReference.Combine(Binary.OutputFilePath.Directory, Binary.OutputFilePath.GetFileNameWithoutExtension() + ".udebugsymbols");
				BuildProducts.Add(DebugFile, BuildProductType.SymbolFile);
			}
		}

		/// <summary>
		/// Adds a build product and its associated debug file to a receipt.
		/// </summary>
		/// <param name="OutputFile">Build product to add</param>
		/// <param name="OutputType">Type of build product</param>
		public override bool ShouldAddDebugFileToReceipt(FileReference OutputFile, BuildProductType OutputType)
		{
			return OutputType == BuildProductType.Executable || OutputType == BuildProductType.DynamicLibrary;
		}

		public override FileReference GetDebugFile(FileReference OutputFile, string DebugExtension)
		{
			if (OutputFile.FullName.Contains(".framework"))
			{
				// need to put the debug info outside of the framework
				return FileReference.Combine(OutputFile.Directory.ParentDirectory!, OutputFile.ChangeExtension(DebugExtension).GetFileName());
			}
			//  by default, just change the extension to the debug extension
			return OutputFile.ChangeExtension(DebugExtension);
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_WarningsAndErrors(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_WarningsAndErrors(CompileEnvironment, Arguments);

			// fix for Xcode 8.3 enabling nonportable include checks, but p4 has some invalid cases in it
			if (Settings.SDKVersionFloat >= 10.3)
			{
				Arguments.Add("-Wno-nonportable-include-path");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Optimizations(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Optimizations(CompileEnvironment, Arguments);

			// use LTO if desired (like VCToolchain does)
			if (CompileEnvironment.bAllowLTCG)
			{
				Arguments.Add("-flto");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Debugging(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Debugging(CompileEnvironment, Arguments);

			Arguments.Add("-fvisibility=hidden"); // hides the linker warnings with PhysX
		}

		/// <inheritdoc/>
		protected override void GetCompilerArguments_Sanitizers(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			// TODO: Reconcile with base
			//base.GetCompilerArguments_Sanitizers(CompileEnvironment, Arguments);

			string? SanitizerMode = Environment.GetEnvironmentVariable("ENABLE_ADDRESS_SANITIZER");
			if ((SanitizerMode != null && SanitizerMode == "YES") || (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer)))
			{
				Arguments.Add("-fsanitize=address -fno-omit-frame-pointer -DFORCE_ANSI_ALLOCATOR=1");
			}

			string? UndefSanitizerMode = Environment.GetEnvironmentVariable("ENABLE_UNDEFINED_BEHAVIOR_SANITIZER");
			if ((UndefSanitizerMode != null && UndefSanitizerMode == "YES") || (Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer)))
			{
				Arguments.Add("-fsanitize=undefined -fno-sanitize=bounds,enum,return,float-divide-by-zero");
			}

			if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
			{
				Arguments.Add("-fsanitize=thread");
			}

			if (Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer))
			{
				Arguments.Add("-fsanitize=memory");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Global(CompileEnvironment, Arguments);

			// What architecture(s) to build for
			Arguments.Add(FormatArchitectureArg(CompileEnvironment.Architectures));

			Arguments.Add($"-isysroot \"{Settings.GetSDKPath(CompileEnvironment.Architecture)}\"");

			string MinVersionParam = GetXcodeMinVersionParam(CompileEnvironment.Architecture);
			if (MinVersionParam != "")
			{
				Arguments.Add($"-m{MinVersionParam}={ProjectSettings.RuntimeVersion}");
			}

			// Add additional frameworks so that their headers can be found
			foreach (UEBuildFramework Framework in CompileEnvironment.AdditionalFrameworks)
			{
				if (Framework.bLinkFramework)
				{
					DirectoryReference? FrameworkDirectory = Framework.GetFrameworkDirectory(CompileEnvironment.Platform, CompileEnvironment.Architecture, Logger);
					if (FrameworkDirectory != null)
					{
						string FrameworkDir = FrameworkDirectory.FullName;
						// embedded frameworks have a framework inside of this directory, so we use this directory. regular frameworks need to go one up to point to the 
						// directory containing the framework. -F gives a path to look for the -framework
						if (FrameworkDir.EndsWith(".framework"))
						{
							FrameworkDir = Path.GetDirectoryName(FrameworkDir)!;
						}
						Arguments.Add($"-F\"{FrameworkDir}\"");
					}
				}
			}
		}

		/// <inheritdoc/>
		protected override FileItem GetCompileArguments_FileType(CppCompileEnvironment CompileEnvironment, FileItem SourceFile, DirectoryReference OutputDir, List<string> Arguments, Action CompileAction, CPPOutput CompileResult)
		{
			FileItem TargetFile = base.GetCompileArguments_FileType(CompileEnvironment, SourceFile, OutputDir, Arguments, CompileAction, CompileResult);

			string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();

			if (!Extension.Equals(".C") && !Extension.Equals(".SWIFT"))
			{
				Arguments.Add(GetObjCExceptionsFlag(CompileEnvironment));
			}

			return TargetFile;
		}

		// Conditionally enable (default disabled) Objective-C exceptions
		static string GetObjCExceptionsFlag(CppCompileEnvironment CompileEnvironment)
		{
			string Result = "";

			if (CompileEnvironment.bEnableObjCExceptions)
			{
				Result += "-fobjc-exceptions";
			}
			else
			{
				Result += "-fno-objc-exceptions";
			}

			return Result;
		}

		static void CleanIntermediateDirectory(string Path)
		{
			string ResultsText;

			// Delete the local dest directory if it exists
			if (Directory.Exists(Path))
			{
				// this can deal with linked files
				RunExecutableAndWait("rm", String.Format("-rf \"{0}\"", Path), out ResultsText);
			}

			// Create the intermediate local directory
			RunExecutableAndWait("mkdir", String.Format("-p \"{0}\"", Path), out ResultsText);
		}

		public virtual string GetXcodeMinVersionParam(UnrealArch Architecture)
		{
			return (Architecture != UnrealArch.IOSSimulator) ? "iphoneos-version-min" : "iphonesimulator-version-min";
		}

		public string GetAdditionalLinkerFlags(CppConfiguration InConfiguration)
		{
			if (InConfiguration != CppConfiguration.Shipping)
			{
				return ProjectSettings.AdditionalLinkerFlags;
			}
			else
			{
				return ProjectSettings.AdditionalShippingLinkerFlags;
			}
		}

		protected override void GetLinkArguments_Global(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			base.GetLinkArguments_Global(LinkEnvironment, Arguments);
			Arguments.Add(FormatArchitectureArg(LinkEnvironment.Architectures));

			DirectoryReference SDKPath = Settings.GetSDKPath(LinkEnvironment.Architecture);
			Arguments.Add($" -isysroot \"{SDKPath}\"");

			Arguments.Add("-dead_strip");
			string MinVersionParam = GetXcodeMinVersionParam(LinkEnvironment.Architecture);
			if (MinVersionParam != "")
			{
				Arguments.Add($"-m{MinVersionParam}={ProjectSettings.RuntimeVersion}");
			}
			Arguments.Add("-Wl-no_pie");
			Arguments.Add("-stdlib=libc++");
			Arguments.Add("-ObjC");
			// Arguments.Add("-v");

			// use LTO if desired (like VCToolchain does)
			if (LinkEnvironment.bAllowLTCG)
			{
				Arguments.Add("-flto");
			}

			string? SanitizerMode = Environment.GetEnvironmentVariable("ENABLE_ADDRESS_SANITIZER");
			if ((SanitizerMode != null && SanitizerMode == "YES") || (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer)))
			{
				Arguments.Add("-rpath \"@executable_path/Frameworks\"");
				Arguments.Add("-fsanitize=address");
			}

			string? UndefSanitizerMode = Environment.GetEnvironmentVariable("ENABLE_UNDEFINED_BEHAVIOR_SANITIZER");
			if ((UndefSanitizerMode != null && UndefSanitizerMode == "YES") || (Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer)))
			{
				Arguments.Add("-rpath \"@executable_path/libclang_rt.ubsan_ios_dynamic.dylib\"");
				Arguments.Add("-fsanitize=undefined");
			}

			if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
			{
				Arguments.Add("-rpath \"@executable_path/libclang_rt.tsan_ios_dynamic.dylib\"");
				Arguments.Add("-fsanitize=thread");
			}

			// need to tell where to load Framework dylibs
			Arguments.Add("-rpath @executable_path/Frameworks");

			Arguments.Add(GetAdditionalLinkerFlags(LinkEnvironment.Configuration));

			// link in the frameworks
			foreach (string Framework in LinkEnvironment.Frameworks)
			{
				if (Framework != "ARKit" || Settings.SDKVersionFloat >= 11.0f)
				{
					Arguments.Add("-framework " + Framework);
				}
			}
			foreach (UEBuildFramework Framework in LinkEnvironment.AdditionalFrameworks)
			{
				DirectoryReference? FrameworkDirectory = Framework.GetFrameworkDirectory(LinkEnvironment.Platform, LinkEnvironment.Architecture, Logger);
				if (FrameworkDirectory != null)
				{
					// If this framework has a directory specified, we'll need to setup the path as well
					string FrameworkDir = FrameworkDirectory.FullName;

					// embedded frameworks have a framework inside of this directory, so we use this directory. regular frameworks need to go one up to point to the 
					// directory containing the framework. -F gives a path to look for the -framework
					if (FrameworkDir.EndsWith(".framework"))
					{
						FrameworkDir = Path.GetDirectoryName(FrameworkDir)!;
					}
					Arguments.Add(String.Format("-F\\\"{0}\\\"", FrameworkDir));
				}

				Arguments.Add("-framework " + Framework.Name);
			}
			foreach (string Framework in LinkEnvironment.WeakFrameworks)
			{
				Arguments.Add("-weak_framework " + Framework);
			}
		}

		void GetArchiveArguments_Global(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			Arguments.Add("-static");
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph)
		{
			FileReference LinkerPath = LinkEnvironment.bIsBuildingLibrary ? Info.Archiver : Info.Clang;

			// Create an action that invokes the linker.
			Action LinkAction = Graph.CreateAction(ActionType.Link);

			// RPC utility parameters are in terms of the Mac side
			LinkAction.WorkingDirectory = GetMacDevSrcRoot();

			// build this up over the rest of the function
			List<string> LinkArguments = new();
			if (LinkEnvironment.bIsBuildingLibrary)
			{
				GetArchiveArguments_Global(LinkEnvironment, LinkArguments);
			}
			else
			{
				GetLinkArguments_Global(LinkEnvironment, LinkArguments);
			}
			string LinkCommandArguments = String.Join(' ', LinkArguments);
			if (LinkEnvironment.bIsBuildingDLL)
			{
				// @todo roll this put into GetLinkArguments_Global
				LinkerPath = Info.Clang;
				LinkCommandArguments += " -dynamiclib -Xlinker -export_dynamic -Xlinker -no_deduplicate";

				string InstallName = LinkEnvironment.InstallName ?? String.Format("@executable_path/Frameworks/{0}", LinkEnvironment.OutputFilePath.MakeRelativeTo(LinkEnvironment.OutputFilePath.Directory.ParentDirectory!));
				LinkCommandArguments += String.Format(" -Xlinker -install_name -Xlinker {0}", InstallName);

				LinkCommandArguments += " -Xlinker -rpath -Xlinker @executable_path/Frameworks";
				LinkCommandArguments += " -Xlinker -rpath -Xlinker @loader_path/Frameworks";
			}

			if (!LinkEnvironment.bIsBuildingLibrary)
			{
				// Add the library paths to the argument list.
				foreach (DirectoryReference LibraryPath in LinkEnvironment.SystemLibraryPaths)
				{
					LinkCommandArguments += String.Format(" -L\\\"{0}\\\"", LibraryPath.FullName);
				}

				// Add the additional libraries to the argument list.
				foreach (string AdditionalLibrary in LinkEnvironment.SystemLibraries)
				{
					LinkCommandArguments += String.Format(" -l\\\"{0}\\\"", AdditionalLibrary);
				}

				foreach (FileReference Library in LinkEnvironment.Libraries)
				{
					// for absolute library paths, convert to remote filename
					// add it to the prerequisites to make sure it's built first (this should be the case of non-system libraries)
					FileItem LibFile = FileItem.GetItemByFileReference(Library);
					LinkAction.PrerequisiteItems.Add(LibFile);

					// and add to the commandline
					LinkCommandArguments += String.Format(" \\\"{0}\\\"", Library.FullName);
				}

				// allow for auto-linked swift libs to be found
				// @todo Mac will need this too - a lot of this code should probably be moved up to AppleToolChain and shared
				LinkCommandArguments += $" -L{ToolChainSettings.Value.GetSDKPath(LinkEnvironment.Architecture)}/usr/lib/swift";
			}

			// Handle additional framework assets that might need to be shadowed
			foreach (UEBuildFramework Framework in LinkEnvironment.AdditionalFrameworks)
			{
				if (Framework.ZipFile != null)
				{
					FileItem ExtractedTokenFile = ExtractFramework(Framework, Graph, Logger);
					LinkAction.PrerequisiteItems.Add(ExtractedTokenFile);
				}
			}

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);

			// Add arguments to generate a map file too
			if ((!LinkEnvironment.bIsBuildingLibrary || LinkEnvironment.bIsBuildingDLL) && LinkEnvironment.bCreateMapFile)
			{
				FileItem MapFile = FileItem.GetItemByFileReference(new FileReference(OutputFile.Location.FullName + ".map"));
				LinkCommandArguments += String.Format(" -Wl,-map,\\\"{0}\\\"", MapFile.Location.FullName);
				LinkAction.ProducedItems.Add(MapFile);
			}

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				string FileNamePath = String.Format("\"{0}\"", InputFile.AbsolutePath);
				InputFileNames.Add(FileNamePath);
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			// Write the list of input files to a response file, with a tempfilename, on remote machine
			if (LinkEnvironment.bIsBuildingLibrary)
			{
				foreach (string Filename in InputFileNames)
				{
					LinkCommandArguments += " " + Filename;
				}
				// @todo rocket lib: the -filelist command should take a response file (see else condition), except that it just says it can't
				// find the file that's in there. Rocket.lib may overflow the commandline by putting all files on the commandline, so this 
				// may be needed:
				// LinkCommandArguments += string.Format(" -filelist \"{0}\"", ConvertPath(ResponsePath));
			}
			else
			{
				bool bIsUnrealGame = LinkEnvironment.OutputFilePath.FullName.Contains("UnrealGame");
				FileReference ResponsePath = FileReference.Combine(((!bIsUnrealGame && ProjectFile != null) ? ProjectFile.Directory : Unreal.EngineDirectory), "Intermediate", "Build", LinkEnvironment.Platform.ToString(), "LinkFileList_" + LinkEnvironment.OutputFilePath.GetFileNameWithoutExtension() + ".tmp");
				Graph.CreateIntermediateTextFile(ResponsePath, InputFileNames);
				LinkCommandArguments += String.Format(" \\\"@{0}\\\"", ResponsePath.FullName);
			}

			// if we are making an LTO build, write the lto file next to build so dsymutil can find it
			if (LinkEnvironment.bAllowLTCG)
			{
				string LtoObjectFile;
				if (Target!.bShouldCompileAsDLL)
				{
					// go up a directory so we don't put this big file into the framework
					LtoObjectFile = Path.Combine(Path.GetDirectoryName(Path.GetDirectoryName(OutputFile.AbsolutePath))!, Path.GetFileName(OutputFile.AbsolutePath) + ".lto.o");
				}
				else
				{
					LtoObjectFile = OutputFile.AbsolutePath + ".lto.o";
				}
				LinkCommandArguments += String.Format(" -flto -Xlinker -object_path_lto -Xlinker \\\"{0}\\\"", LtoObjectFile);
			}

			// Add the output file to the command-line.
			LinkCommandArguments += String.Format(" -o \\\"{0}\\\"", OutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkCommandArguments += LinkEnvironment.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			LinkAction.StatusDescription = String.Format("{0}", OutputFile.AbsolutePath);

			LinkAction.CommandVersion = Info.ClangVersionString;

			LinkAction.CommandPath = BuildHostPlatform.Current.Shell;
			if (LinkEnvironment.Configuration == CppConfiguration.Shipping && Path.GetExtension(OutputFile.AbsolutePath) != ".a")
			{
				// When building a shipping package, symbols are stripped from the exe as the last build step. This is a problem
				// when re-packaging and no source files change because the linker skips symbol generation and dsymutil will 
				// recreate a new .dsym file from a symboless exe file. It's just sad. To make things happy we need to delete 
				// the output file to force the linker to recreate it with symbols again.
				string linkCommandArguments = "-c \"";

				linkCommandArguments += String.Format("rm -f \\\"{0}\\\";", OutputFile.AbsolutePath);
				linkCommandArguments += "\\\"" + LinkerPath + "\\\" " + LinkCommandArguments + ";";

				linkCommandArguments += "\"";

				LinkAction.CommandArguments = linkCommandArguments;
			}
			else
			{
				// This is not a shipping build so no need to delete the output file since symbols will not have been stripped from it.
				LinkAction.CommandArguments = String.Format("-c \"\\\"{0}\\\" {1}\"", LinkerPath, LinkCommandArguments);
			}
			return OutputFile;
		}

		static string GetAppBundleName(FileReference Executable)
		{
			// Get the app bundle name
			string AppBundleName = Executable.GetFileNameWithoutExtension();

			// Strip off any platform suffix
			int SuffixIdx = AppBundleName.IndexOf('-');
			if (SuffixIdx != -1)
			{
				AppBundleName = AppBundleName.Substring(0, SuffixIdx);
			}

			// Append the .app suffix
			return AppBundleName + ".app";
		}

		public static FileReference GetAssetCatalogFile(UnrealTargetPlatform Platform, FileReference Executable)
		{
			// Get the output file
			if (Platform == UnrealTargetPlatform.IOS)
			{
				return FileReference.Combine(Executable.Directory, "Payload", GetAppBundleName(Executable), "Assets.car");
			}
			else
			{
				return FileReference.Combine(Executable.Directory, "AssetCatalog", "Assets.car");
			}
		}

		public static string GetAssetCatalogArgs(UnrealTargetPlatform Platform, string InputDir, string OutputDir)
		{
			StringBuilder Arguments = new StringBuilder("actool");
			Arguments.Append(" --output-format human-readable-text");
			Arguments.Append(" --notices");
			Arguments.Append(" --warnings");
			Arguments.AppendFormat(" --output-partial-info-plist \"{0}/assetcatalog_generated_info.plist\"", InputDir);
			if (Platform == UnrealTargetPlatform.TVOS)
			{
				Arguments.Append(" --app-icon \"App Icon & Top Shelf Image\"");
				Arguments.Append(" --launch-image \"Launch Image\"");
				Arguments.Append(" --filter-for-device-model AppleTV5,3");
				Arguments.Append(" --target-device tv");
				Arguments.Append(" --minimum-deployment-target 15.0");
				Arguments.Append(" --platform appletvos");
			}
			else
			{
				Arguments.Append(" --app-icon AppIcon");
				Arguments.Append(" --product-type com.apple.product-type.application");
				Arguments.Append(" --target-device iphone");
				Arguments.Append(" --target-device ipad");
				Arguments.Append(" --minimum-deployment-target 15.0");
				Arguments.Append(" --platform iphoneos");
			}
			Arguments.Append(" --enable-on-demand-resources YES");
			Arguments.AppendFormat(" --compile \"{0}\"", OutputDir);
			Arguments.AppendFormat(" \"{0}/Assets.xcassets\"", InputDir);
			return Arguments.ToString();
		}

		private static bool IsCompiledAsFramework(string ExecutablePath)
		{
			// @todo ios: Get the receipt to here which has the property
			return ExecutablePath.Contains(".framework");
		}

		private static string GetdSYMPath(FileItem Executable)
		{
			string ExecutablePath = Executable.AbsolutePath;
			// for frameworks, we want to put the .dSYM outside of the framework, and the executable is inside the .framework
			if (IsCompiledAsFramework(ExecutablePath))
			{
				return Path.Combine(Path.GetDirectoryName(ExecutablePath)!, "..", Path.GetFileName(ExecutablePath) + ".dSYM");
			}

			// return standard dSYM location
			return Path.Combine(Path.GetDirectoryName(ExecutablePath)!, Path.GetFileName(ExecutablePath) + ".dSYM");
		}

		/// <summary>
		/// Generates debug info for a given executable
		/// </summary>
		/// <param name="Executable">FileItem describing the executable to generate debug info for</param>
		/// <param name="bIsForLTOBuild">Was this build made with LTO enabled?</param>
		/// <param name="Graph">List of actions to be executed. Additional actions will be added to this list.</param>
		/// <param name="Logger">Logger for output</param>
		private List<FileItem> GenerateDebugInfo(FileItem Executable, bool bIsForLTOBuild, IActionGraphBuilder Graph, ILogger Logger)
		{
			// Make a file item for the source and destination files
			string FullDestPathRoot = GetdSYMPath(Executable);

			FileItem OutputFile = FileItem.GetItemByPath(FullDestPathRoot);
			FileItem ZipOutputFile = FileItem.GetItemByPath(FullDestPathRoot + ".zip");

			// Make the compile action
			Action GenDebugAction = Graph.CreateAction(ActionType.GenerateDebugInfo);

			GenDebugAction.WorkingDirectory = GetMacDevSrcRoot();
			GenDebugAction.CommandPath = BuildHostPlatform.Current.Shell;
			string ExtraOptions;
			string DsymutilPath = GetDsymutilPath(Logger, out ExtraOptions, bIsForLTOBuild: bIsForLTOBuild);
			if (ProjectSettings.bGeneratedSYMBundle)
			{
				GenDebugAction.CommandArguments = String.Format("-c \"rm -rf \\\"{2}\\\"; \\\"{0}\\\" \\\"{1}\\\" {4} -o \\\"{2}\\\"; cd \\\"{2}/..\\\"; zip -r -y -1 {3}.zip {3}\"",
					DsymutilPath,
					Executable.AbsolutePath,
					OutputFile.AbsolutePath,
					Path.GetFileName(FullDestPathRoot),
					ExtraOptions);
				GenDebugAction.ProducedItems.Add(ZipOutputFile);
				Logger.LogInformation("Zip file: {File}", ZipOutputFile.AbsolutePath);
			}
			else
			{
				GenDebugAction.CommandArguments = String.Format("-c \"rm -rf \\\"{2}\\\"; \\\"{0}\\\" \\\"{1}\\\" {3} -f -o \\\"{2}\\\"\"",
						DsymutilPath,
						Executable.AbsolutePath,
						OutputFile.AbsolutePath,
						ExtraOptions);
			}
			GenDebugAction.PrerequisiteItems.Add(Executable);
			GenDebugAction.ProducedItems.Add(OutputFile);
			GenDebugAction.StatusDescription = GenDebugAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
			GenDebugAction.bCanExecuteRemotely = false;

			return GenDebugAction.ProducedItems.ToList(); // (ProjectSettings.bGeneratedSYMBundle ? ZipOutputFile : OutputFile);
		}

		/// <summary>
		/// Generates pseudo pdb info for a given executable
		/// </summary>
		/// <param name="Executable">FileItem describing the executable to generate debug info for</param>
		/// <param name="Graph">List of actions to be executed. Additional actions will be added to this list.</param>
		public FileItem GeneratePseudoPDB(FileItem Executable, IActionGraphBuilder Graph)
		{
			// Make a file item for the source and destination files
			string FulldSYMPathRoot = GetdSYMPath(Executable);
			string FullDestPathRoot = Path.ChangeExtension(FulldSYMPathRoot, ".udebugsymbols");
			string PathToDWARF = Path.Combine(FulldSYMPathRoot, "Contents", "Resources", "DWARF", Path.GetFileName(Executable.AbsolutePath));

			FileItem dSYMFile = FileItem.GetItemByPath(FulldSYMPathRoot);

			FileItem DWARFFile = FileItem.GetItemByPath(PathToDWARF);

			FileItem OutputFile = FileItem.GetItemByPath(FullDestPathRoot);

			// Make the compile action
			Action GenDebugAction = Graph.CreateAction(ActionType.GenerateDebugInfo);
			GenDebugAction.WorkingDirectory = DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "Mac");

			GenDebugAction.CommandPath = BuildHostPlatform.Current.Shell;
			GenDebugAction.CommandArguments = String.Format("-c \"rm -rf \\\"{1}\\\"; dwarfdump --uuid \\\"{3}\\\" | cut -d\\  -f2; chmod 777 ./DsymExporter; ./DsymExporter -UUID=$(dwarfdump --uuid \\\"{3}\\\" | cut -d\\  -f2) \\\"{0}\\\" \\\"{2}\\\"\"",
					DWARFFile.AbsolutePath,
					OutputFile.AbsolutePath,
					Path.GetDirectoryName(OutputFile.AbsolutePath),
					dSYMFile.AbsolutePath);
			GenDebugAction.PrerequisiteItems.Add(dSYMFile);
			GenDebugAction.ProducedItems.Add(OutputFile);
			GenDebugAction.StatusDescription = GenDebugAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
			GenDebugAction.bCanExecuteRemotely = false;

			return OutputFile;
		}

		public static void PackageStub(string BinaryPath, string GameName, string ExeName, bool bUseModernXcode, bool bPerformDummySigning=false)
		{
			// create the ipa
			string IPAName = BinaryPath + "/" + ExeName + ".stub";
			// delete the old one
			if (File.Exists(IPAName))
			{
				File.Delete(IPAName);
			}

			// make the subdirectory if needed
			string DestSubdir = Path.GetDirectoryName(IPAName)!;
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}

			// set up the directories (modern has .app named with the config names in them, like UnrealGame-IOS-Shipping, which the ExeName will be)
			string ZipWorkingDir = bUseModernXcode ? $"Payload/{ExeName}.app/" : $"Payload/{GameName}.app/";
			string ZipSourceDir = bUseModernXcode ? $"{BinaryPath}/{ExeName}.app" : $"{BinaryPath}/Payload/{GameName}.app";

			if (bPerformDummySigning)
			{
				// with modern, we don't codesign when building the source .app due to keychain issues over ssh and Xcode wants provision setup, even if using the - identity
				// so, now codesign right at the end with the dummy signature so that there is signature at all, which IPP on windows will replace
				int ExitCode;
				string Output = Utils.RunLocalProcessAndReturnStdOut("/bin/sh", $"-c 'codesign -f -s - \"{ZipSourceDir}\"", null, out ExitCode);
				if (ExitCode != 0)
				{
					throw new BuildException($"Failed to codesign with dummy identity, which should never happen [ExitCode = {ExitCode}, output = {Output}");
				}
				// also codesign the frameworks
				string FrameworksDir = Path.Combine(ZipSourceDir, "Frameworks");
				if (Directory.Exists(FrameworksDir))
				{
					foreach (string FrameworkDir in Directory.EnumerateDirectories(FrameworksDir))
					{
						Output = Utils.RunLocalProcessAndReturnStdOut("/bin/sh", $"-c 'codesign -f -s - \"{FrameworkDir}\"", null, out ExitCode);
						if (ExitCode != 0)
						{
							throw new BuildException($"Failed to codesign with dummy identity, which should never happen [ExitCode = {ExitCode}, output = {Output}");
						}
					}
				}
			}

			// create the file
			using (ZipFile Zip = new ZipFile())
			{
				// add the entire directory
				Zip.AddDirectory(ZipSourceDir, ZipWorkingDir);

				// Update permissions to be UNIX-style
				// Modify the file attributes of any added file to unix format
				foreach (ZipEntry E in Zip.Entries)
				{
					const byte FileAttributePlatform_NTFS = 0x0A;
					const byte FileAttributePlatform_UNIX = 0x03;
					const byte FileAttributePlatform_FAT = 0x00;

					const int UNIX_FILETYPE_NORMAL_FILE = 0x8000;
					//const int UNIX_FILETYPE_SOCKET = 0xC000;
					//const int UNIX_FILETYPE_SYMLINK = 0xA000;
					//const int UNIX_FILETYPE_BLOCKSPECIAL = 0x6000;
					const int UNIX_FILETYPE_DIRECTORY = 0x4000;
					//const int UNIX_FILETYPE_CHARSPECIAL = 0x2000;
					//const int UNIX_FILETYPE_FIFO = 0x1000;

					const int UNIX_EXEC = 1;
					const int UNIX_WRITE = 2;
					const int UNIX_READ = 4;

					int MyPermissions = UNIX_READ | UNIX_WRITE;
					int OtherPermissions = UNIX_READ;

					int PlatformEncodedBy = (E.VersionMadeBy >> 8) & 0xFF;
					int LowerBits = 0;

					// Try to preserve read-only if it was set
					bool bIsDirectory = E.IsDirectory;

					// Check to see if this 
					bool bIsExecutable = false;
					if (Path.GetFileNameWithoutExtension(E.FileName).Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
					{
						bIsExecutable = true;
					}

					if (bIsExecutable)
					{
						// The executable will be encrypted in the final distribution IPA and will compress very poorly, so keeping it
						// uncompressed gives a better indicator of IPA size for our distro builds
						E.CompressionLevel = CompressionLevel.None;
					}

					if ((PlatformEncodedBy == FileAttributePlatform_NTFS) || (PlatformEncodedBy == FileAttributePlatform_FAT))
					{
						FileAttributes OldAttributes = E.Attributes;
						//LowerBits = ((int)E.Attributes) & 0xFFFF;

						if ((OldAttributes & FileAttributes.Directory) != 0)
						{
							bIsDirectory = true;
						}

						// Permissions
						if ((OldAttributes & FileAttributes.ReadOnly) != 0)
						{
							MyPermissions &= ~UNIX_WRITE;
							OtherPermissions &= ~UNIX_WRITE;
						}
					}

					if (bIsDirectory || bIsExecutable)
					{
						MyPermissions |= UNIX_EXEC;
						OtherPermissions |= UNIX_EXEC;
					}

					// Re-jigger the external file attributes to UNIX style if they're not already that way
					if (PlatformEncodedBy != FileAttributePlatform_UNIX)
					{
						int NewAttributes = bIsDirectory ? UNIX_FILETYPE_DIRECTORY : UNIX_FILETYPE_NORMAL_FILE;

						NewAttributes |= (MyPermissions << 6);
						NewAttributes |= (OtherPermissions << 3);
						NewAttributes |= (OtherPermissions << 0);

						// Now modify the properties
						E.AdjustExternalFileAttributes(FileAttributePlatform_UNIX, (NewAttributes << 16) | LowerBits);
					}
				}

				// Save it out
				Zip.Save(IPAName);
			}
		}

		public static DirectoryReference GenerateAssetCatalog(FileReference? ProjectFile, UnrealTargetPlatform Platform, ref bool bUserImagesExist)
		{
			string EngineDir = Unreal.EngineDirectory.ToString();
			string BuildDir = (((ProjectFile != null) ? ProjectFile.Directory.ToString() : (String.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()) ? Unreal.EngineDirectory.ToString() : UnrealBuildTool.GetRemoteIniPath()))) + "/Build/" + (Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS");
			string IntermediateDir = (((ProjectFile != null) ? ProjectFile.Directory.ToString() : Unreal.EngineDirectory.ToString())) + "/Intermediate/" + (Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS");

			bUserImagesExist = false;

			string ResourcesDir = Path.Combine(IntermediateDir, "Resources");
			if (Platform == UnrealTargetPlatform.TVOS)
			{
				// copy the template asset catalog to the appropriate directory
				string Dir = Path.Combine(ResourcesDir, "Assets.xcassets");
				if (!Directory.Exists(Dir))
				{
					Directory.CreateDirectory(Dir);
				}
				// create the directories
				foreach (string directory in Directory.EnumerateDirectories(Path.Combine(EngineDir, "Build", "TVOS", "Resources", "Assets.xcassets"), "*", SearchOption.AllDirectories))
				{
					Dir = directory.Replace(Path.Combine(EngineDir, "Build", "TVOS"), IntermediateDir);
					if (!Directory.Exists(Dir))
					{
						Directory.CreateDirectory(Dir);
					}
				}
				// copy the default files
				foreach (string file in Directory.EnumerateFiles(Path.Combine(EngineDir, "Build", "TVOS", "Resources", "Assets.xcassets"), "*", SearchOption.AllDirectories))
				{
					Dir = file.Replace(Path.Combine(EngineDir, "Build", "TVOS"), IntermediateDir);
					File.Copy(file, Dir, true);
					FileInfo DestFileInfo = new FileInfo(Dir);
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
				}
				// copy the icons from the game directory if it has any
				string[][] Images = {
					new string []{ "Icon_Large_Back-1280x768.png", "App Icon & Top Shelf Image.brandassets/App Icon - Large.imagestack/Back.imagestacklayer/Content.imageset" },
					new string []{ "Icon_Large_Front-1280x768.png", "App Icon & Top Shelf Image.brandassets/App Icon - Large.imagestack/Front.imagestacklayer/Content.imageset" },
					new string []{ "Icon_Large_Middle-1280x768.png", "App Icon & Top Shelf Image.brandassets/App Icon - Large.imagestack/Middle.imagestacklayer/Content.imageset" },
					new string []{ "Icon_Small_Back-400x240.png", "App Icon & Top Shelf Image.brandassets/App Icon.imagestack/Back.imagestacklayer/Content.imageset" },
					new string []{ "Icon_Small_Back-800x480.png", "App Icon & Top Shelf Image.brandassets/App Icon.imagestack/Back.imagestacklayer/Content.imageset" },
					new string []{ "Icon_Small_Front-400x240.png", "App Icon & Top Shelf Image.brandassets/App Icon.imagestack/Front.imagestacklayer/Content.imageset" },
					new string []{ "Icon_Small_Front-800x480.png", "App Icon & Top Shelf Image.brandassets/App Icon.imagestack/Front.imagestacklayer/Content.imageset" },
					new string []{ "Icon_Small_Middle-400x240.png", "App Icon & Top Shelf Image.brandassets/App Icon.imagestack/Middle.imagestacklayer/Content.imageset" },
					new string []{ "Icon_Small_Middle-800x480.png", "App Icon & Top Shelf Image.brandassets/App Icon.imagestack/Middle.imagestacklayer/Content.imageset" },
					new string []{ "TopShelfWide-2320x720@2x.png", "App Icon & Top Shelf Image.brandassets/Top Shelf Image Wide.imageset" },
					new string []{ "TopShelfWide-2320x720.png", "App Icon & Top Shelf Image.brandassets/Top Shelf Image Wide.imageset" },
				};
				Dir = Path.Combine(IntermediateDir, "Resources", "Assets.xcassets");

				string BuildResourcesGraphicsDir = Path.Combine(BuildDir, "Resources", "Assets.xcassets");
				for (int Index = 0; Index < Images.Length; ++Index)
				{
					string SourceDir = Path.Combine((Directory.Exists(BuildResourcesGraphicsDir) ? (BuildDir) : (Path.Combine(EngineDir, "Build", "TVOS"))),
						"Resources",
						"Assets.xcassets");
					string Image = Path.Combine(SourceDir, Images[Index][1], Images[Index][0]);

					if (File.Exists(Image))
					{
						bUserImagesExist |= Image.StartsWith(BuildResourcesGraphicsDir);

						File.Copy(Image, Path.Combine(Dir, Images[Index][1], Images[Index][0]), true);
						FileInfo DestFileInfo = new FileInfo(Path.Combine(Dir, Images[Index][1], Images[Index][0]));
						DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
					}
				}
			}
			else
			{
				// copy the template asset catalog to the appropriate directory
				string Dir = Path.Combine(ResourcesDir, "Assets.xcassets");
				if (!Directory.Exists(Dir))
				{
					Directory.CreateDirectory(Dir);
				}
				// create the directories
				foreach (string directory in Directory.EnumerateDirectories(Path.Combine(EngineDir, "Build", "IOS", "Resources", "Assets.xcassets"), "*", SearchOption.AllDirectories))
				{
					Dir = directory.Replace(Path.Combine(EngineDir, "Build", "IOS"), IntermediateDir);
					if (!Directory.Exists(Dir))
					{
						Directory.CreateDirectory(Dir);
					}
				}
				// copy the default files
				foreach (string file in Directory.EnumerateFiles(Path.Combine(EngineDir, "Build", "IOS", "Resources", "Assets.xcassets"), "*", SearchOption.AllDirectories))
				{
					Dir = file.Replace(Path.Combine(EngineDir, "Build", "IOS"), IntermediateDir);
					File.Copy(file, Dir, true);
					FileInfo DestFileInfo = new FileInfo(Dir);
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
				}
				// copy the icons from the game directory if it has any
				string[][] Images = {
					new string []{ "IPhoneIcon20@2x.png", "Icon20@2x.png" },
					new string []{ "IPhoneIcon20@3x.png", "Icon20@3x.png" },
					new string []{ "IPhoneIcon29@2x.png", "Icon29@2x.png" },
					new string []{ "IPhoneIcon29@3x.png", "Icon29@3x.png" },
					new string []{ "IPhoneIcon40@2x.png", "Icon40@2x.png" },
					new string []{ "IPhoneIcon40@3x.png", "Icon40@3x.png" },
					new string []{ "IPhoneIcon60@2x.png", "Icon60@2x.png" },
					new string []{ "IPhoneIcon60@3x.png", "Icon60@3x.png" },
					new string []{ "IPadIcon20@2x.png", "Icon20@2x.png"},
					new string []{ "IPadIcon29@2x.png", "Icon29@2x.png"},
					new string []{ "IPadIcon40@2x.png", "Icon40@2x.png" },
					new string []{ "IPadIcon76@2x.png", "Icon76@2x.png"},
					new string []{ "IPadIcon83.5@2x.png", "Icon83.5@2x.png"},
					new string []{ "Icon1024.png", "Icon1024.png" },
				};
				Dir = Path.Combine(IntermediateDir, "Resources", "Assets.xcassets", "AppIcon.appiconset");

				string BuildResourcesGraphicsDir = Path.Combine(BuildDir, "Resources", "Graphics");
				for (int Index = 0; Index < Images.Length; ++Index)
				{
					string Image = Path.Combine((Directory.Exists(Path.Combine(BuildDir, "Resources", "Graphics")) ? (BuildDir) : (Path.Combine(EngineDir, "Build", "IOS"))), "Resources", "Graphics", Images[Index][1]);
					if (File.Exists(Image))
					{
						bUserImagesExist |= Image.StartsWith(BuildResourcesGraphicsDir);

						File.Copy(Image, Path.Combine(Dir, Images[Index][0]), true);
						FileInfo DestFileInfo = new FileInfo(Path.Combine(Dir, Images[Index][0]));
						DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
					}
				}

				StringBuilder ContentsJson = new StringBuilder();

				ContentsJson.AppendLine("{");
				ContentsJson.AppendLine("\"images\" : [");
				ContentsJson.AppendLine("{");
				ContentsJson.AppendLine("\"size\" : \"60x60\",");
				ContentsJson.AppendLine("\"idiom\" : \"iphone\",");
				ContentsJson.AppendLine("\"filename\" : \"IPhoneIcon60@2x.png\",");
				ContentsJson.AppendLine("\"scale\" : \"2x\"");
				ContentsJson.AppendLine("},");
				ContentsJson.AppendLine("{");
				ContentsJson.AppendLine("\"size\" : \"76x76\",");
				ContentsJson.AppendLine("\"idiom\" : \"ipad\",");
				ContentsJson.AppendLine("\"filename\" : \"IPadIcon76@2x.png\",");
				ContentsJson.AppendLine("\"scale\" : \"2x\"");
				ContentsJson.AppendLine("},");
				ContentsJson.AppendLine("{");
				ContentsJson.AppendLine("\"size\" : \"83.5x83.5\",");
				ContentsJson.AppendLine("\"idiom\" : \"ipad\",");
				ContentsJson.AppendLine("\"filename\" : \"IPadIcon83.5@2x.png\",");
				ContentsJson.AppendLine("\"scale\" : \"2x\"");
				ContentsJson.AppendLine("},");
				ContentsJson.AppendLine("{");
				ContentsJson.AppendLine("\"size\" : \"1024x1024\",");
				ContentsJson.AppendLine("\"idiom\" : \"ios-marketing\",");
				ContentsJson.AppendLine("\"filename\" : \"Icon1024.png\",");
				ContentsJson.AppendLine("\"scale\" : \"1x\"");
				ContentsJson.AppendLine("},");

				string[][] IconsInfo = {
					new string []{ "IPhoneIcon20@2x.png", "\"20x20\"",  "\"iphone\"", "\"2x\"" },
					new string []{ "IPhoneIcon20@3x.png", "\"20x20\"", "\"iphone\"", "\"3x\"" },
					new string []{ "IPhoneIcon29@2x.png", "\"29x29\"", "\"iphone\"", "\"2x\"" },
					new string []{ "IPhoneIcon29@3x.png", "\"29x29\"", "\"iphone\"", "\"3x\"" },
					new string []{ "IPhoneIcon40@2x.png", "\"40x40\"", "\"iphone\"", "\"2x\"" },
					new string []{ "IPhoneIcon40@3x.png", "\"40x40\"", "\"iphone\"", "\"3x\"" },
					new string []{ "IPhoneIcon60@3x.png", "\"60x60\"", "\"iphone\"", "\"3x\"" },
					new string []{ "IPadIcon20@2x.png", "\"20x20\"", "\"ipad\"", "\"2x\"" },
					new string []{ "IPadIcon29@2x.png", "\"29x29\"", "\"ipad\"", "\"2x\"" },
					new string []{ "IPadIcon40@2x.png", "\"40x40\"", "\"ipad\"", "\"2x\"" },
					};

				for (int Index = 0; Index < IconsInfo.Length; ++Index)
				{
					if (File.Exists(Path.Combine(Dir, IconsInfo[Index][0])))
					{
						ContentsJson.AppendLine("{");
						ContentsJson.AppendLine("\"size\" : " + IconsInfo[Index][1] + ",");
						ContentsJson.AppendLine("\"idiom\" : " + IconsInfo[Index][2] + ",");
						ContentsJson.AppendLine("\"filename\" : \"" + IconsInfo[Index][0] + "\",");
						ContentsJson.AppendLine("\"scale\" : " + IconsInfo[Index][3]);
						ContentsJson.AppendLine("},");
					}
				}

				ContentsJson.AppendLine("],");
				ContentsJson.AppendLine("\"info\" : {");
				ContentsJson.AppendLine("\"version\" : 1,");
				ContentsJson.AppendLine("\"author\" : \"xcode\"");
				ContentsJson.AppendLine("}");
				ContentsJson.AppendLine("}");

				string ContentsFile = Path.Combine(IntermediateDir, "Resources", "Assets.xcassets", "AppIcon.appiconset", "Contents.json");
				File.WriteAllText(ContentsFile, ContentsJson.ToString());

			}
			return new DirectoryReference(ResourcesDir);
		}

		public override ICollection<FileItem> PostBuild(ReadOnlyTargetRules Target, FileItem Executable, LinkEnvironment BinaryLinkEnvironment, IActionGraphBuilder Graph)
		{
			List<FileItem> OutputFiles = new List<FileItem>(base.PostBuild(Target, Executable, BinaryLinkEnvironment, Graph));

			if (BinaryLinkEnvironment.bIsBuildingLibrary)
			{
				return OutputFiles;
			}

			// For IOS/tvOS, generate the dSYM file if needed or requested
			if (Target!.IOSPlatform.bGeneratedSYM)
			{
				List<FileItem> Files = GenerateDebugInfo(Executable, BinaryLinkEnvironment.bAllowLTCG, Graph, Logger);
				foreach (FileItem item in Files)
				{
					OutputFiles.Add(item);
				}
				if (ProjectSettings.bGenerateCrashReportSymbols)
				{
					OutputFiles.Add(GeneratePseudoPDB(Executable, Graph));
				}
			}

			if (bUseModernXcode)
			{
				// base class AppleToolchain makes the shared postbuild sync object, so we are done
				return OutputFiles;
			}

			// strip the debug info from the executable if needed. creates a dummy output file for the action graph to track that it's out of date.
			if (Target.IOSPlatform.bStripSymbols || (Target.Configuration == UnrealTargetConfiguration.Shipping))
			{
				FileItem StripCompleteFile = FileItem.GetItemByFileReference(FileReference.Combine(BinaryLinkEnvironment.IntermediateDirectory!, Executable.Location.GetFileName() + ".stripped"));

				// If building a framework we can only strip local symbols, need to leave global in place
				string StripArguments = BinaryLinkEnvironment.bIsBuildingDLL ? "-x" : "";

				Action StripAction = Graph.CreateAction(ActionType.CreateAppBundle);
				StripAction.WorkingDirectory = GetMacDevSrcRoot();
				StripAction.CommandPath = BuildHostPlatform.Current.Shell;
				StripAction.CommandArguments = String.Format("-c \"\\\"{0}/strip\\\" {1} \\\"{2}\\\" && touch \\\"{3}\\\"\"", Settings.ToolchainDir, StripArguments, Executable.Location, StripCompleteFile);
				StripAction.PrerequisiteItems.Add(Executable);
				StripAction.PrerequisiteItems.UnionWith(OutputFiles);
				StripAction.ProducedItems.Add(StripCompleteFile);
				StripAction.StatusDescription = String.Format("Stripping symbols from {0}", Executable.AbsolutePath);
				StripAction.bCanExecuteRemotely = false;

				OutputFiles.Add(StripCompleteFile);
			}

			if (!AppleExports.UseModernXcode(Target.ProjectFile) && !BinaryLinkEnvironment.bIsBuildingDLL)
			{
				// generate the asset catalog
				bool bUserImagesExist = false;
				DirectoryReference ResourcesDir = GenerateAssetCatalog(ProjectFile, BinaryLinkEnvironment.Platform, ref bUserImagesExist);

				// Get the output location for the asset catalog
				FileItem AssetCatalogFile = FileItem.GetItemByFileReference(GetAssetCatalogFile(BinaryLinkEnvironment.Platform, Executable.Location));

				// Make the compile action
				Action CompileAssetAction = Graph.CreateAction(ActionType.CreateAppBundle);
				CompileAssetAction.WorkingDirectory = GetMacDevSrcRoot();
				CompileAssetAction.CommandPath = new FileReference("/usr/bin/xcrun");
				CompileAssetAction.CommandArguments = GetAssetCatalogArgs(BinaryLinkEnvironment.Platform, ResourcesDir.FullName, Path.GetDirectoryName(AssetCatalogFile.AbsolutePath)!);
				CompileAssetAction.PrerequisiteItems.Add(Executable);
				CompileAssetAction.ProducedItems.Add(AssetCatalogFile);
				CompileAssetAction.DeleteItems.Add(AssetCatalogFile);
				CompileAssetAction.StatusDescription = CompileAssetAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
				CompileAssetAction.bCanExecuteRemotely = false;

				// Add it to the output files so it's always built
				OutputFiles.Add(AssetCatalogFile);
			}

			// Generate the app bundle
			if (!Target.bDisableLinking)
			{
				Logger.LogInformation("Adding PostBuildSync action");

				List<string> UPLScripts = UEDeployIOS.CollectPluginDataPaths(BinaryLinkEnvironment.AdditionalProperties, Logger);
				VersionNumber SdkVersion = VersionNumber.Parse(Settings.SDKVersion);

				Dictionary<string, DirectoryReference> FrameworkNameToSourceDir = new Dictionary<string, DirectoryReference>();

				FileReference StagedExecutablePath = GetStagedExecutablePath(Executable.Location, Target.Name);
				DirectoryReference BundleDirectory = Target.bShouldCompileAsDLL ? Executable.Directory.Location : StagedExecutablePath.Directory;
				foreach (UEBuildFramework Framework in BinaryLinkEnvironment.AdditionalFrameworks)
				{
					DirectoryReference? FrameworkDirectory = Framework.GetFrameworkDirectory(BinaryLinkEnvironment.Platform, BinaryLinkEnvironment.Architecture, Logger);
					if (FrameworkDirectory != null)
					{
						if (!String.IsNullOrEmpty(Framework.CopyBundledAssets))
						{
							// For now, this is hard coded, but we need to loop over all modules, and copy bundled assets that need it
							DirectoryReference LocalSource = DirectoryReference.Combine(FrameworkDirectory, Framework.CopyBundledAssets);
							string BundleName = Framework.CopyBundledAssets.Substring(Framework.CopyBundledAssets.LastIndexOf('/') + 1);
							FrameworkNameToSourceDir[BundleName] = LocalSource;
						}

						if (Framework.bCopyFramework)
						{
							string FrameworkDir = FrameworkDirectory.FullName;
							if (FrameworkDir.EndsWith(".framework"))
							{
								FrameworkDir = Path.GetDirectoryName(FrameworkDir)!;
							}

							OutputFiles.Add(CopyBundleResource(new UEBuildBundleResource(new ModuleRules.BundleResource(Path.Combine(FrameworkDir, Framework.Name + ".framework"), "Frameworks")), Executable, BundleDirectory, Graph));
						}
					}
				}

				foreach (UEBuildBundleResource Resource in BinaryLinkEnvironment.AdditionalBundleResources)
				{
					OutputFiles.Add(CopyBundleResource(Resource, Executable, StagedExecutablePath.Directory, Graph));
				}

				IOSPostBuildSyncTarget PostBuildSyncTarget = new IOSPostBuildSyncTarget(Target, Executable.Location, BinaryLinkEnvironment.IntermediateDirectory, UPLScripts, SdkVersion, FrameworkNameToSourceDir);
				FileReference PostBuildSyncFile = FileReference.Combine(BinaryLinkEnvironment.IntermediateDirectory!, "PostBuildSync.dat");
				BinaryFormatterUtils.Save(PostBuildSyncFile, PostBuildSyncTarget);

				string PostBuildSyncArguments = String.Format("-Input=\"{0}\" -XmlConfigCache=\"{1}\" -remoteini=\"{2}\" -legacyxcode", PostBuildSyncFile, XmlConfig.CacheFile, UnrealBuildTool.GetRemoteIniPath());

				if (Log.OutputFile != null)
				{
					string LogFileName = Log.OutputFile.GetFileNameWithoutExtension() + "_PostBuildSync.txt";
					LogFileName = FileReference.Combine(Log.OutputFile.Directory, LogFileName).ToString();
					PostBuildSyncArguments += " -log=\"" + LogFileName + "\"";
				}

				Action PostBuildSyncAction = Graph.CreateRecursiveAction<IOSPostBuildSyncMode>(ActionType.CreateAppBundle, PostBuildSyncArguments);
				PostBuildSyncAction.WorkingDirectory = Unreal.EngineSourceDirectory;
				PostBuildSyncAction.PrerequisiteItems.Add(Executable);
				PostBuildSyncAction.PrerequisiteItems.UnionWith(OutputFiles);
				PostBuildSyncAction.ProducedItems.Add(FileItem.GetItemByFileReference(GetStagedExecutablePath(Executable.Location, Target.Name)));
				PostBuildSyncAction.DeleteItems.UnionWith(PostBuildSyncAction.ProducedItems);
				PostBuildSyncAction.StatusDescription = "Executing PostBuildSync";

				OutputFiles.AddRange(PostBuildSyncAction.ProducedItems);
			}

			return OutputFiles;
		}

		public static string GetCodesignPlatformName(UnrealTargetPlatform Platform)
		{
			if (Platform == UnrealTargetPlatform.TVOS)
			{
				return "appletvos";
			}
			if (Platform == UnrealTargetPlatform.IOS)
			{
				return "iphoneos";
			}

			throw new BuildException("Invalid platform for GetCodesignPlatformName()");
		}

		class ProcessOutput
		{
			/// <summary>
			/// Substrings that indicate a line contains an error
			/// </summary>
			protected static readonly string[] ErrorMessageTokens =
			{
				"ERROR ",
				"** BUILD FAILED **",
				"[BEROR]",
				"IPP ERROR",
				"System.Net.Sockets.SocketException"
			};

			/// <summary>
			/// Helper function to sync source files to and from the local system and a remote Mac
			/// </summary>
			//This chunk looks to be required to pipe output to VS giving information on the status of a remote build.
			public bool OutputReceivedDataEventHandlerEncounteredError = false;
			public string OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
			public void OutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line, ILogger Logger)
			{
				if ((Line != null) && (Line.Data != null))
				{
					Logger.LogInformation("{Message}", Line.Data);

					foreach (string ErrorToken in ErrorMessageTokens)
					{
						if (Line.Data.Contains(ErrorToken))
						{
							OutputReceivedDataEventHandlerEncounteredError = true;
							OutputReceivedDataEventHandlerEncounteredErrorMessage += Line.Data;
							break;
						}
					}
				}
			}

			public void OutputReceivedDataEventLogger(Object Sender, DataReceivedEventArgs Line, ILogger Logger)
			{
				if ((Line != null) && (Line.Data != null))
				{
					Logger.LogInformation("{Output}", Line.Data);
				}
			}
		}

		private static void GenerateCrashlyticsData(string DsymZip, string ProjectDir, string ProjectName, ILogger Logger)
		{
			Logger.LogInformation("Generating and uploading Crashlytics Data");

			// Clean this folder as it's used for extraction
			string TempPath = Path.Combine(Unreal.EngineDirectory.FullName, "Intermediate", "Unzipped");

			if (Directory.Exists(TempPath))
			{
				Logger.LogInformation("Deleting temp path {TempPath}", TempPath);
				Directory.Delete(TempPath, true);
			}

			string FabricPath = Unreal.EngineDirectory + "/Intermediate/UnzippedFrameworks/Crashlytics/Fabric.embeddedframework";
			if (Directory.Exists(FabricPath) && Unreal.IsBuildMachine())
			{
				//string PlistFile = ProjectDir + "/Intermediate/IOS/" + ProjectName + "-Info.plist";
				Process FabricProcess = new Process();
				FabricProcess.StartInfo.WorkingDirectory = Path.GetDirectoryName(DsymZip);
				FabricProcess.StartInfo.FileName = "/bin/sh";
				FabricProcess.StartInfo.Arguments = String.Format("-c \"chmod 777 \\\"{0}/Fabric.framework/upload-symbols\\\"; \\\"{0}/Fabric.framework/upload-symbols\\\" -a 7a4cebd0324af21696e5e321802c5e26ba541cad -p ios {1}\"",
					FabricPath,
					DsymZip);

				ProcessOutput Output = new ProcessOutput();

				FabricProcess.OutputDataReceived += new DataReceivedEventHandler((s, e) => Output.OutputReceivedDataEventHandler(s, e, Logger));
				FabricProcess.ErrorDataReceived += new DataReceivedEventHandler((s, e) => Output.OutputReceivedDataEventHandler(s, e, Logger));

				Utils.RunLocalProcess(FabricProcess);
				if (Output.OutputReceivedDataEventHandlerEncounteredError)
				{
					throw new Exception(Output.OutputReceivedDataEventHandlerEncounteredErrorMessage);
				}
			}
		}

		public static FileReference GetStagedExecutablePath(FileReference Executable, string TargetName)
		{
			return FileReference.Combine(Executable.Directory, "Payload", TargetName + ".app", TargetName);
		}

		private static void WriteEntitlements(IOSPostBuildSyncTarget Target, ILogger Logger)
		{
			string AppName = Target.TargetName;
			FileReference? MobileProvisionFile;
			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProjectSettings(Target.ProjectFile);

			if (Target.ImportProvision == null)
			{
				IOSProvisioningData ProvisioningData = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProvisioningData(ProjectSettings, Target.bForDistribution);
				MobileProvisionFile = ProvisioningData.MobileProvisionFile;
			}
			else
			{
				MobileProvisionFile = new FileReference(Target.ImportProvision);
			}

			string IntermediateDir = (((Target.ProjectFile != null) ? Target.ProjectFile.Directory.ToString() :
				Unreal.EngineDirectory.ToString())) + "/Intermediate/" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS");
			// get the settings from the ini file
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Target.ProjectFile), UnrealTargetPlatform.IOS);

			IOSExports.WriteEntitlements(Target.Platform, Ini, AppName, MobileProvisionFile, Target.bForDistribution, IntermediateDir);
		}

		private static void WriteMissingEmbeddedPlist(FileReference MissingPlistPath, DirectoryReference ProjectDirectory, FileReference? ProjectFile)
		{
			FileReference PlistTemplatePath = FileReference.Combine(Unreal.EngineDirectory, "Build", "IOS", "Resources", "FrameworkPlist", "Embedded.plist");
			if (!DirectoryReference.Exists(MissingPlistPath.Directory))
			{
				DirectoryReference.CreateDirectory(MissingPlistPath.Directory);
			}

			string BundleID = ProjectFiles.Xcode.XcodeFrameworkWrapperUtils.GetBundleID(ProjectDirectory, ProjectFile);
			string BundleName = ProjectFiles.Xcode.XcodeFrameworkWrapperUtils.GetBundleName(ProjectDirectory, ProjectFile);

			BundleID += ".embedded"; // This is necessary because the wrapper that contains this framework cannot have the same BundleID.

			string Plist = FileReference.ReadAllText(PlistTemplatePath);
			Plist = Plist.Replace("BUNDLE_NAME", BundleName);
			Plist = Plist.Replace("BUNDLE_ID", BundleID);

			FileReference.WriteAllText(MissingPlistPath, Plist);
		}

		private static void GenerateFrameworkWrapperIfNonexistent(IOSPostBuildSyncTarget Target, ILogger Logger)
		{
			DirectoryReference ProjectDirectory = GetActualProjectDirectory(Target.ProjectFile);
			DirectoryReference WrapperDirectory = DirectoryReference.Combine(ProjectDirectory, "Wrapper");

			string ProjectName = ProjectDirectory.GetDirectoryName();
			string FrameworkName = Target.TargetName;
			string BundleId = ProjectFiles.Xcode.XcodeFrameworkWrapperUtils.GetBundleID(Target.ProjectDirectory, Target.ProjectFile);
			string EnginePath = Unreal.EngineDirectory.ToString();
			string SrcFrameworkPath = DirectoryReference.Combine(Target.ProjectDirectory, "Binaries", "IOS", Target.Configuration.ToString()).ToString(); // We use Target.ProjectDirectory because if it is a UnrealGame we want the engine dir and not the actual project dir.
			string CookedDataPath = DirectoryReference.Combine(ProjectDirectory, "Saved", "StagedBuilds", "IOS", "cookeddata").ToString();
			bool GenerateFrameworkWrapperProject = ProjectFiles.Xcode.XcodeFrameworkWrapperUtils.GetGenerateFrameworkWrapperProject(Target.ProjectDirectory);

			string? ProvisionName = String.Empty;
			string? TeamUUID = String.Empty;
			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS)).ReadProjectSettings(Target.ProjectFile);

			IOSProvisioningData Data = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS)).ReadProvisioningData(ProjectSettings, Target.bForDistribution);
			if (Data != null)
			{
				ProvisionName = Data.MobileProvisionName;
				TeamUUID = Data.TeamUUID;
			}

			// Create the framework wrapper project, only if it doesn't exist and only if the user wants to generate the wrapper.
			if (!DirectoryReference.Exists(DirectoryReference.Combine(WrapperDirectory, FrameworkName)) && GenerateFrameworkWrapperProject)
			{
				ProjectFiles.Xcode.XcodeFrameworkWrapperProject.GenerateXcodeFrameworkWrapper(WrapperDirectory.ToString(), ProjectName, FrameworkName, BundleId, SrcFrameworkPath, EnginePath, CookedDataPath, ProvisionName, TeamUUID);
			}
		}

		public static void PostBuildSync(IOSPostBuildSyncTarget Target, ILogger Logger)
		{
			// if xcode is building this, it will also do the Run stuff anyway, so no need to do it here as well
			if (Environment.GetEnvironmentVariable("UE_BUILD_FROM_XCODE") == "1")
			{
				return;
			}

			if (AppleExports.UseModernXcode(Target.ProjectFile))
			{
				throw new BuildException("Modern xcode mode should not reach this, something has broken the PostBuild setup");
			}

			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProjectSettings(Target.ProjectFile);
			string? BundleID = ProjectSettings.BundleIdentifier;

			bool bPerformFullAppCreation = true;
			string PathToDsymZip = Target.OutputPath.FullName + ".dSYM.zip";
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				if (Target.bBuildAsFramework)
				{
					// make sure the framework has a plist
					FileReference PlistSrcLocation = FileReference.Combine(Target.ProjectDirectory, "Build", "IOS", "Embedded.plist");
					if (!FileReference.Exists(PlistSrcLocation))
					{

						WriteMissingEmbeddedPlist(PlistSrcLocation, Target.ProjectDirectory, Target.ProjectFile);
						if (!FileReference.Exists(PlistSrcLocation))
						{
							throw new BuildException("Unable to find plist for output framework ({0})", PlistSrcLocation);
						}
					}

					FileReference PlistDstLocation = FileReference.Combine(Target.OutputPath.Directory, "Info.plist");
					if (FileReference.Exists(PlistDstLocation))
					{
						FileReference.SetAttributes(PlistDstLocation, FileAttributes.Normal);
					}

					FileReference.Copy(PlistSrcLocation, PlistDstLocation, true);

					PathToDsymZip = Path.Combine(Target.OutputPath.Directory.ParentDirectory!.FullName, Target.OutputPath.GetFileName() + ".dSYM.zip");

					GenerateFrameworkWrapperIfNonexistent(Target, Logger);

					// do not perform any of the .app creation below
					bPerformFullAppCreation = false;
				}
			}

			string AppName = Target.TargetName;

			if (!Target.bSkipCrashlytics)
			{
				GenerateCrashlyticsData(PathToDsymZip, Target.ProjectDirectory.FullName, AppName, Logger);
			}

			// only make the app if needed
			if (!bPerformFullAppCreation)
			{
				return;
			}

			// copy the executable
			FileReference StagedExecutablePath = GetStagedExecutablePath(Target.OutputPath, Target.TargetName);
			DirectoryReference.CreateDirectory(StagedExecutablePath.Directory);
			FileReference.Copy(Target.OutputPath, StagedExecutablePath, true);
			string RemoteShadowDirectoryMac = Target.OutputPath.Directory.FullName;

			if (Target.bCreateStubIPA || Target.bBuildAsFramework)
			{
				string FrameworkDerivedDataDir = Path.GetFullPath(Target.ProjectDirectory + "/Intermediate/" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS") + "/DerivedData");
				DirectoryReference FrameworkPayloadDirectory = DirectoryReference.Combine(Target.OutputPath.Directory.ParentDirectory!.ParentDirectory!, "Payload");

				// generate the dummy project so signing works
				DirectoryReference? XcodeWorkspaceDir = null;
				if (!Target.bBuildAsFramework)
				{
					if (AppName == "UnrealGame" || AppName == "UnrealClient" || Target.ProjectFile == null || Target.ProjectFile.IsUnderDirectory(Unreal.EngineDirectory))
					{
						XcodeWorkspaceDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Intermediate/ProjectFiles", String.Format("UE5_{0}.xcworkspace", (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS")));
					}
					else
					{
						XcodeWorkspaceDir = DirectoryReference.Combine(Target.ProjectDirectory, "Intermediate/ProjectFiles", String.Format("{0}_{1}.xcworkspace", Target.ProjectFile.GetFileNameWithoutExtension(), (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS")));
					}
				}

				// Path to the temporary keychain. When -ImportCertificate is specified, we will temporarily add this to the list of keychains to search, and remove it later.
				FileReference TempKeychain = FileReference.Combine(Target.ProjectIntermediateDirectory!, "TempKeychain.keychain");

				FileReference SignProjectScript = FileReference.Combine(Target.ProjectIntermediateDirectory!, "SignProject.sh");
				using (StreamWriter Writer = new StreamWriter(SignProjectScript.FullName))
				{
					// Boilerplate
					Writer.WriteLine("#!/bin/sh");
					Writer.WriteLine("set -e");
					Writer.WriteLine("set -x");

					// Copy the mobile provision into the system store
					if (Target.ImportProvision != null)
					{
						Writer.WriteLine("cp -f {0} ~/Library/MobileDevice/Provisioning\\ Profiles/", Utils.EscapeShellArgument(Target.ImportProvision));
					}

					// Get the signing certificate to use
					string? SigningCertificate;
					if (Target.ImportCertificate == null)
					{
						// Take it from the standard settings
						IOSProvisioningData ProvisioningData = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProvisioningData(Target.ProjectFile, Target.bForDistribution);
						SigningCertificate = ProvisioningData.SigningCertificate;

						// Set the identity on the command line
						if (!ProjectSettings.bAutomaticSigning)
						{
							Writer.WriteLine("CODE_SIGN_IDENTITY='{0}'", String.IsNullOrEmpty(SigningCertificate) ? "IPhoneDeveloper" : SigningCertificate);
						}
					}
					else
					{
						// Read the name from the certificate
						X509Certificate2 Certificate;
						try
						{
							Certificate = new X509Certificate2(Target.ImportCertificate, Target.ImportCertificatePassword ?? "");
						}
						catch (Exception Ex)
						{
							throw new BuildException(Ex, "Unable to read certificate '{0}': {1}", Target.ImportCertificate, Ex.Message);
						}
						SigningCertificate = Certificate.GetNameInfo(X509NameType.SimpleName, false);

						// Install a certificate given on the command line to a temporary keychain
						Writer.WriteLine("security delete-keychain \"{0}\" || true", TempKeychain);
						Writer.WriteLine("security create-keychain -p \"A\" \"{0}\"", TempKeychain);
						Writer.WriteLine("security list-keychains -s \"{0}\"", TempKeychain);
						Writer.WriteLine("security list-keychains");
						Writer.WriteLine("security set-keychain-settings -t 3600 -l  \"{0}\"", TempKeychain);
						Writer.WriteLine("security -v unlock-keychain -p \"A\" \"{0}\"", TempKeychain);
						Writer.WriteLine("security import {0} -P {1} -k \"{2}\" -T /usr/bin/codesign -T /usr/bin/security -t agg", Utils.EscapeShellArgument(Target.ImportCertificate), Utils.EscapeShellArgument(Target.ImportCertificatePassword!), TempKeychain);
						Writer.WriteLine("security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k \"A\" -D '{0}' -t private {1}", SigningCertificate, TempKeychain);

						// Set parameters to make sure it uses the correct identity and keychain
						Writer.WriteLine("CERT_IDENTITY='{0}'", SigningCertificate);
						Writer.WriteLine("CODE_SIGN_IDENTITY='{0}'", SigningCertificate);
						Writer.WriteLine("CODE_SIGN_KEYCHAIN='{0}'", TempKeychain);
					}

					FileReference? MobileProvisionFile;
					string? MobileProvisionUUID;
					string? TeamUUID;
					if (Target.ImportProvision == null)
					{
						IOSProvisioningData ProvisioningData = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProvisioningData(ProjectSettings, Target.bForDistribution);
						MobileProvisionFile = ProvisioningData.MobileProvisionFile;
						MobileProvisionUUID = ProvisioningData.MobileProvisionUUID;
						TeamUUID = ProvisioningData.TeamUUID;
						if (ProvisioningData.BundleIdentifier == null || !ProvisioningData.BundleIdentifier.Contains('*'))
						{
							// If the BundleIndentifer contains a wild card it will not be valid to use in the plist.
							BundleID = ProvisioningData.BundleIdentifier;
						}
					}
					else
					{
						MobileProvisionFile = new FileReference(Target.ImportProvision);

						MobileProvisionContents MobileProvision = MobileProvisionContents.Read(MobileProvisionFile);
						MobileProvisionUUID = MobileProvision.GetUniqueId();
						MobileProvision.TryGetTeamUniqueId(out TeamUUID);
						string BundleIdentifier = MobileProvision.GetBundleIdentifier();
						if (!BundleIdentifier.Contains('*'))
						{
							// If the BundleIndentifer contains a wild card it will not be valid to use in the plist.
							BundleID = BundleIdentifier;
						}
					}

					if (MobileProvisionFile == null)
					{
						throw new BuildException("Unable to find valid certificate/mobile provision pair.");
					}

					string ConfigName = Target.Configuration.ToString();
					if (Target.TargetType != TargetType.Game && Target.TargetType != TargetType.Program)
					{
						ConfigName += " " + Target.TargetType.ToString();
					}

					string SchemeName = Target.TargetName;

					Console.WriteLine("Provisioning: {0}, {1}, {2}, {3}", MobileProvisionFile, MobileProvisionFile.GetFileName(), MobileProvisionUUID, BundleID);

					string CmdLine;
					if (Target.bBuildAsFramework)
					{
						DirectoryReference FrameworkProjectDirectory = GetActualProjectDirectory(Target.ProjectFile);
						DirectoryReference WrapperDirectory = DirectoryReference.Combine(FrameworkProjectDirectory, "Wrapper");
						string FrameworkName = Target.TargetName;
						DirectoryReference WrapperProject = DirectoryReference.Combine(WrapperDirectory, FrameworkName, FrameworkName + ".xcodeproj");

						// Delete everything in payload directory before building
						Writer.WriteLine(String.Format("rm -rf \"{0}\"", FrameworkPayloadDirectory));

						// Build the framework wrapper
						CmdLine = FileReference.Combine(AppleToolChainSettings.XcodeDeveloperDir, "usr/bin/xcodebuild").FullName +
							" -project \"" + WrapperProject + "\"" +
								" -configuration \"" + ConfigName + "\"" +
							" -scheme '" + SchemeName + "'" +
								" -sdk " + GetCodesignPlatformName(Target.Platform) +
							$" -destination generic/platform=\"{AppleExports.GetDestinationPlatform(Target.Platform, Target.Architectures)}\"" +
								" -derivedDataPath \"" + FrameworkDerivedDataDir + "\"" +
							" CONFIGURATION_BUILD_DIR=\"" + FrameworkPayloadDirectory + "\"" +
								(!String.IsNullOrEmpty(TeamUUID) ? " DEVELOPMENT_TEAM=" + TeamUUID : "");
					}
					else
					{
						// code sign the project
						CmdLine = FileReference.Combine(AppleToolChainSettings.XcodeDeveloperDir, "usr/bin/xcodebuild").FullName +
							" -workspace \"" + XcodeWorkspaceDir + "\"" +
								" -configuration \"" + ConfigName + "\"" +
							" -scheme '" + SchemeName + "'" +
								" -sdk " + GetCodesignPlatformName(Target.Platform) +
							$" -destination generic/platform=\"{AppleExports.GetDestinationPlatform(Target.Platform, Target.Architectures)}\"" +
								(!String.IsNullOrEmpty(TeamUUID) ? " DEVELOPMENT_TEAM=" + TeamUUID : "");
					}

					CmdLine += String.Format(" CODE_SIGN_IDENTITY='{0}'", SigningCertificate);

					// <projectname>.entitlements file can get copied over during a RemoteMac build which confuses Xcode, so just tell it to ignore any 
					// entitlement file changes.  See UE-181206.
					CmdLine += String.Format(" CODE_SIGN_ALLOW_ENTITLEMENTS_MODIFICATION=YES");

					if (!ProjectSettings.bAutomaticSigning)
					{
						CmdLine += (!String.IsNullOrEmpty(MobileProvisionUUID) ? (" PROVISIONING_PROFILE_SPECIFIER=" + MobileProvisionUUID) : "");
					}
					Writer.WriteLine("/usr/bin/xcrun {0}", CmdLine);
				}

				if (!Target.bBuildAsFramework)
				{
					Logger.LogInformation("Generating project...");
					DirectoryReference? GeneratedProjectFile;
					if (AppName == "UnrealGame" || AppName == "UnrealClient" || Target.ProjectFile == null || Target.ProjectFile.IsUnderDirectory(Unreal.EngineDirectory))
					{
						GenerateProjectFiles(Target.ProjectFile, new string[] { "-platforms=" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS"), "-NoIntellIsense", (Target.Platform == UnrealTargetPlatform.IOS ? "-iosdeployonly" : "-tvosdeployonly"), "-ignorejunk", (Target.bForDistribution ? "-distribution" : "-development"), "-bundleID=" + BundleID, "-includetemptargets", "-appname=" + AppName }, null, Logger, out GeneratedProjectFile);
					}
					else
					{
						GenerateProjectFiles(Target.ProjectFile, new string[] { "-platforms=" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS"), "-NoIntellIsense", (Target.Platform == UnrealTargetPlatform.IOS ? "-iosdeployonly" : "-tvosdeployonly"), "-ignorejunk", (Target.bForDistribution ? "-distribution" : "-development"), String.Format("-project={0}", Target.ProjectFile), "-game", "-bundleID=" + BundleID, "-includetemptargets" }, null, Logger, out GeneratedProjectFile);
					}
					Logger.LogInformation("Generated {0}!", GeneratedProjectFile?.FullName ?? "NONE");
					// Make sure it exists
					if (!DirectoryReference.Exists(XcodeWorkspaceDir!) || (GeneratedProjectFile != null && GeneratedProjectFile != XcodeWorkspaceDir))
					{
						throw new BuildException("Unable to create stub IPA; Xcode workspace not found at {0}", XcodeWorkspaceDir);
					}
				}

				// ensure the plist, entitlements, and provision files are properly copied
				UEDeployIOS DeployHandler = (Target.Platform == UnrealTargetPlatform.IOS ? new UEDeployIOS(Logger) : new UEDeployTVOS(Logger));
				DeployHandler.ForDistribution = Target.bForDistribution;
				DeployHandler.PrepTargetForDeployment(Target.ProjectFile, Target.TargetName, Target.OutputPath, Target.Platform, Target.Configuration, Target.UPLScripts, Target.bCreateStubIPA, BundleID, Target.bBuildAsFramework);

				Logger.LogInformation("Executing {Script}", SignProjectScript);

				// write the entitlements file (building remotely)
				WriteEntitlements(Target, Logger);

				Process SignProcess = new Process();
				SignProcess.StartInfo.WorkingDirectory = RemoteShadowDirectoryMac;
				SignProcess.StartInfo.FileName = "/bin/sh";
				string Arguments = String.Format("\"{0}\"", SignProjectScript.FullName);
				SignProcess.StartInfo.Arguments = Arguments;

				ProcessOutput Output = new ProcessOutput();

				SignProcess.OutputDataReceived += new DataReceivedEventHandler((s, e) => Output.OutputReceivedDataEventHandler(s, e, Logger));
				SignProcess.ErrorDataReceived += new DataReceivedEventHandler((s, e) => Output.OutputReceivedDataEventHandler(s, e, Logger));

				Output.OutputReceivedDataEventHandlerEncounteredError = false;
				Output.OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
				Utils.RunLocalProcess(SignProcess);

				// cleanup
				if (Target.ImportCertificate != null)
				{
					FileReference CleanProjectScript = FileReference.Combine(Target.ProjectIntermediateDirectory!, "CleanProject.sh");
					using (StreamWriter CleanWriter = new StreamWriter(CleanProjectScript.FullName))
					{
						// Remove the temporary keychain from the search list
						CleanWriter.WriteLine("security delete-keychain \"{0}\" || true", TempKeychain);
						CleanWriter.WriteLine("security list-keychain -s login.keychain");
					}

					Logger.LogInformation("Executing {Script}", CleanProjectScript);

					Process CleanProcess = new Process();
					CleanProcess.StartInfo.WorkingDirectory = RemoteShadowDirectoryMac;
					CleanProcess.StartInfo.FileName = "/bin/sh";
					CleanProcess.StartInfo.Arguments = CleanProjectScript.FullName;

					ProcessOutput CleanOutput = new ProcessOutput();

					SignProcess.OutputDataReceived += new DataReceivedEventHandler((s, e) => CleanOutput.OutputReceivedDataEventLogger(s, e, Logger));
					SignProcess.ErrorDataReceived += new DataReceivedEventHandler((s, e) => CleanOutput.OutputReceivedDataEventLogger(s, e, Logger));

					Utils.RunLocalProcess(CleanProcess);
				}

				if (Target.bBuildAsFramework)
				{
					CleanIntermediateDirectory(FrameworkDerivedDataDir);
				}

				if (XcodeWorkspaceDir != null)
				{
					// delete the temp project
					DirectoryReference.Delete(XcodeWorkspaceDir, true);

					if (Output.OutputReceivedDataEventHandlerEncounteredError)
					{
						throw new Exception(Output.OutputReceivedDataEventHandlerEncounteredErrorMessage);
					}
				}

				// Package the stub
				if (Target.bBuildAsFramework)
				{
					PackageStub(FrameworkPayloadDirectory.ParentDirectory!.ToString(), AppName, Target.OutputPath.GetFileNameWithoutExtension(), AppleExports.UseModernXcode(Target.ProjectFile));
				}
				else
				{
					PackageStub(RemoteShadowDirectoryMac, AppName, Target.OutputPath.GetFileNameWithoutExtension(), AppleExports.UseModernXcode(Target.ProjectFile));
				}
			}
			else
			{
				// ensure the plist, entitlements, and provision files are properly copied
				UEDeployIOS DeployHandler = (Target.Platform == UnrealTargetPlatform.IOS ? new UEDeployIOS(Logger) : new UEDeployTVOS(Logger));
				DeployHandler.ForDistribution = Target.bForDistribution;
				DeployHandler.PrepTargetForDeployment(Target.ProjectFile, Target.TargetName, Target.OutputPath, Target.Platform, Target.Configuration, Target.UPLScripts, Target.bCreateStubIPA, BundleID, Target.bBuildAsFramework);

				// write the entitlements file (building on Mac)
				WriteEntitlements(Target, Logger);
			}

			{
				// Copy bundled assets from additional frameworks to the intermediate assets directory (so they can get picked up during staging)
				String LocalFrameworkAssets = Path.GetFullPath(Target.ProjectDirectory + "/Intermediate/" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS") + "/FrameworkAssets");

				// Clean the local dest directory if it exists
				CleanIntermediateDirectory(LocalFrameworkAssets);

				foreach (KeyValuePair<string, DirectoryReference> Pair in Target.FrameworkNameToSourceDir)
				{
					//string UnpackedZipPath = Pair.Value.FullName;

					// For now, this is hard coded, but we need to loop over all modules, and copy bundled assets that need it
					string LocalDest = LocalFrameworkAssets + "/" + Pair.Key;

					Logger.LogInformation("Copying bundled asset... LocalSource: {Source}, LocalDest: {Target}", Pair.Value, LocalDest);

					string ResultsText;
					RunExecutableAndWait("cp", String.Format("-R -L \"{0}\" \"{1}\"", Pair.Value, LocalDest), out ResultsText);
				}
			}
		}

		public static int RunExecutableAndWait(string ExeName, string ArgumentList, out string StdOutResults)
		{
			// Create the process
			ProcessStartInfo PSI = new ProcessStartInfo(ExeName, ArgumentList);
			PSI.RedirectStandardOutput = true;
			PSI.UseShellExecute = false;
			PSI.CreateNoWindow = true;
			Process NewProcess = Process.Start(PSI)!;

			// Wait for the process to exit and grab it's output
			StdOutResults = NewProcess.StandardOutput.ReadToEnd();
			NewProcess.WaitForExit();
			return NewProcess.ExitCode;
		}

		public void StripSymbols(FileReference SourceFile, FileReference TargetFile)
		{
			StripSymbolsWithXcode(SourceFile, TargetFile, Settings.ToolchainDir);
		}

		FileItem CopyBundleResource(UEBuildBundleResource Resource, FileItem Executable, DirectoryReference BundleDirectory, IActionGraphBuilder Graph)
		{
			Action CopyAction = Graph.CreateAction(ActionType.CreateAppBundle);
			CopyAction.WorkingDirectory = GetMacDevSrcRoot(); // Path.GetFullPath(".");
			CopyAction.CommandPath = BuildHostPlatform.Current.Shell;
			CopyAction.CommandDescription = "";

			string BundlePath = BundleDirectory.FullName;
			string SourcePath = Path.Combine(Path.GetFullPath("."), Resource.ResourcePath!);
			string TargetPath;
			if (Resource.BundleContentsSubdir == "Resources")
			{
				TargetPath = Path.Combine(BundlePath, Path.GetFileName(Resource.ResourcePath)!);
			}
			else
			{
				TargetPath = Path.Combine(BundlePath, Resource.BundleContentsSubdir!, Path.GetFileName(Resource.ResourcePath)!);
			}

			FileItem TargetItem = FileItem.GetItemByPath(TargetPath);

			CopyAction.CommandArguments = String.Format("-c \"cp -f -R \\\"{0}\\\" \\\"{1}\\\"; touch -c \\\"{2}\\\"\"", SourcePath, Path.GetDirectoryName(TargetPath)!.Replace('\\', '/') + "/", TargetPath.Replace('\\', '/'));
			CopyAction.PrerequisiteItems.Add(Executable);
			CopyAction.ProducedItems.Add(TargetItem);
			CopyAction.bShouldOutputStatusDescription = Resource.bShouldLog;
			CopyAction.StatusDescription = String.Format("Copying {0} to app bundle", Path.GetFileName(Resource.ResourcePath));
			CopyAction.bCanExecuteRemotely = false;

			return TargetItem;
		}

		public override void SetupBundleDependencies(ReadOnlyTargetRules Target, IEnumerable<UEBuildBinary> Binaries, string GameName)
		{
			base.SetupBundleDependencies(Target, Binaries, GameName);

			foreach (UEBuildBinary Binary in Binaries)
			{
				BundleDependencies.Add(FileItem.GetItemByFileReference(Binary.OutputFilePath));
			}
		}
	};
}

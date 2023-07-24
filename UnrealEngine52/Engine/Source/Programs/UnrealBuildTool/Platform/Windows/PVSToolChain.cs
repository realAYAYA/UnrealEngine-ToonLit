// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Serialization;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Flags for the PVS analyzer mode
	/// </summary>
	public enum PVSAnalysisModeFlags : uint
	{
		/// <summary>
		/// Check for 64-bit portability issues
		/// </summary>
		Check64BitPortability = 1,

		/// <summary>
		/// Enable general analysis
		/// </summary>
		GeneralAnalysis = 4,

		/// <summary>
		/// Check for optimizations
		/// </summary>
		Optimizations = 8,

		/// <summary>
		/// Enable customer-specific rules
		/// </summary>
		CustomerSpecific = 16,

		/// <summary>
		/// Enable MISRA analysis
		/// </summary>
		MISRA = 32,
	}

  	/// <summary>
	/// Flags for the PVS analyzer timeout
	/// </summary>
	public enum AnalysisTimeoutFlags
	{
		/// <summary>
		/// Analisys timeout for file 10 minutes (600 seconds)
		/// </summary>
		After_10_minutes = 600,
		/// <summary>
		/// Analisys timeout for file 30 minutes (1800 seconds)
		/// </summary>
		After_30_minutes = 1800,
		/// <summary>
		/// Analisys timeout for file 60 minutes (3600 seconds)
		/// </summary>
		After_60_minutes = 3600,
		/// <summary>
		/// Analisys timeout when not set (a lot of seconds)
		/// </summary>
		No_timeout = 999999
	}

	/// <summary>
	/// Partial representation of PVS-Studio main settings file
	/// </summary>
	[XmlRoot("ApplicationSettings")]
	public class PVSApplicationSettings
	{
		/// <summary>
		/// Masks for paths excluded for analysis
		/// </summary>
		public string[]? PathMasks;

		/// <summary>
		/// Registered username
		/// </summary>
		public string? UserName;

		/// <summary>
		/// Registered serial number
		/// </summary>
		public string? SerialNumber;

		/// <summary>
		/// Disable the 64-bit Analysis
		/// </summary>
		public bool Disable64BitAnalysis;

		/// <summary>
		/// Disable the General Analysis
		/// </summary>
		public bool DisableGAAnalysis;

		/// <summary>
		/// Disable the Optimization Analysis
		/// </summary>
		public bool DisableOPAnalysis;

		/// <summary>
		/// Disable the Customer's Specific diagnostic rules
		/// </summary>
		public bool DisableCSAnalysis;

		/// <summary>
		/// Disable the MISRA Analysis
		/// </summary>
		public bool DisableMISRAAnalysis;

    		/// <summary>
		/// File analysis timeout
		/// </summary>
		public AnalysisTimeoutFlags AnalysisTimeout;

		/// <summary>
		/// Disable analyzer Level 3 (Low) messages
		/// </summary>
		public bool NoNoise;

		/// <summary>
		/// Enable the display of analyzer rules exceptions which can be specified by comments and .pvsconfig files.
		/// </summary>
		public bool ReportDisabledRules;
    
		/// <summary>
		/// Gets the analysis mode flags from the settings
		/// </summary>
		/// <returns>Mode flags</returns>
		public PVSAnalysisModeFlags GetModeFlags()
		{
			PVSAnalysisModeFlags Flags = 0;
			if (!Disable64BitAnalysis)
			{
				Flags |= PVSAnalysisModeFlags.Check64BitPortability;
			}
			if (!DisableGAAnalysis)
			{
				Flags |= PVSAnalysisModeFlags.GeneralAnalysis;
			}
			if (!DisableOPAnalysis)
			{
				Flags |= PVSAnalysisModeFlags.Optimizations;
			}
			if (!DisableCSAnalysis)
			{
				Flags |= PVSAnalysisModeFlags.CustomerSpecific;
			}
			if (!DisableMISRAAnalysis)
			{
				Flags |= PVSAnalysisModeFlags.MISRA;
			}
			return Flags;
		}

		/// <summary>
		/// Attempts to read the application settings from the default location
		/// </summary>
		/// <returns>Application settings instance, or null if no file was present</returns>
		internal static PVSApplicationSettings? Read()
		{
			FileReference SettingsPath = FileReference.Combine(new DirectoryReference(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData)), "PVS-Studio", "Settings.xml");
			if (FileReference.Exists(SettingsPath))
			{
				try
				{
					XmlSerializer Serializer = new XmlSerializer(typeof(PVSApplicationSettings));
					using (FileStream Stream = new FileStream(SettingsPath.FullName, FileMode.Open, FileAccess.Read, FileShare.Read))
					{
						return (PVSApplicationSettings?)Serializer.Deserialize(Stream);
					}
				}
				catch (Exception Ex)
				{
					throw new BuildException(Ex, "Unable to read PVS-Studio settings file from {0}", SettingsPath);
				}
			}
			return null;
		}
	}

	/// <summary>
	/// Settings for the PVS Studio analyzer
	/// </summary>
	public class PVSTargetSettings
	{
		/// <summary>
		/// Returns the application settings
		/// </summary>
		internal Lazy<PVSApplicationSettings?> ApplicationSettings { get; } = new Lazy<PVSApplicationSettings?>(() => PVSApplicationSettings.Read());

		/// <summary>
		/// Whether to use application settings to determine the analysis mode
		/// </summary>
		public bool UseApplicationSettings { get; set; }

		/// <summary>
		/// Override for the analysis mode to use
		/// </summary>
		public PVSAnalysisModeFlags ModeFlags
		{
			get
 			{
				if (ModePrivate.HasValue)
				{
					return ModePrivate.Value;
				}
				else if (UseApplicationSettings && ApplicationSettings.Value != null)
				{
					return ApplicationSettings.Value.GetModeFlags();
				}
				else
				{
					return PVSAnalysisModeFlags.GeneralAnalysis;
				}
			}
			set
			{
				ModePrivate = value;
			}
		}

		/// <summary>
		/// Private storage for the mode flags
		/// </summary>
		PVSAnalysisModeFlags? ModePrivate;
    
    		/// <summary>
		/// Override for the analysis timeoutFlag to use
		/// </summary>
		public AnalysisTimeoutFlags AnalysisTimeoutFlag
		{
			get
			{
				if (TimeoutPrivate.HasValue)
				{
					return TimeoutPrivate.Value;
				}
				else if (UseApplicationSettings && ApplicationSettings.Value != null)
				{
					return ApplicationSettings.Value.AnalysisTimeout;
				}
				else
				{
					return AnalysisTimeoutFlags.After_30_minutes;
				}
			}
			set
			{
				TimeoutPrivate = value;
			}
		}

		/// <summary>
		/// Private storage for the analysis timeout
		/// </summary>
		AnalysisTimeoutFlags? TimeoutPrivate;

		/// <summary>
		/// Override for the disable Level 3 (Low) analyzer messages
		/// </summary>
		public bool EnableNoNoise
		{
			get
			{
				if (EnableNoNoisePrivate.HasValue)
				{
					return EnableNoNoisePrivate.Value;
				}
				else if (UseApplicationSettings && ApplicationSettings.Value != null)
				{
					return ApplicationSettings.Value.NoNoise;
				}
				else
				{
					return false;
				}
			}
			set
			{
				EnableNoNoisePrivate = value;
			}
		}

		/// <summary>
		/// Private storage for the NoNoise analyzer setting
		/// </summary>
		bool? EnableNoNoisePrivate;

		/// <summary>
		/// Override for the enable the display of analyzer rules exceptions which can be specified by comments and .pvsconfig files.
		/// </summary>
		public bool EnableReportDisabledRules
		{
			get
			{
				if (EnableReportDisabledRulesPrivate.HasValue)
				{
					return EnableReportDisabledRulesPrivate.Value;
				}
				else if (UseApplicationSettings && ApplicationSettings.Value != null)
				{
					return ApplicationSettings.Value.ReportDisabledRules;
				}
				else
				{
					return false;
				}
			}
			set
			{
				EnableReportDisabledRulesPrivate = value;
			}
		}

		/// <summary>
		/// Private storage for the ReportDisabledRules analyzer setting
		/// </summary>
		bool? EnableReportDisabledRulesPrivate;
	}

	/// <summary>
	/// Read-only version of the PVS toolchain settings
	/// </summary>
	public class ReadOnlyPVSTargetSettings
	{
		/// <summary>
		/// Inner settings
		/// </summary>
		PVSTargetSettings Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The inner object</param>
		public ReadOnlyPVSTargetSettings(PVSTargetSettings Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Accessor for the Application settings
		/// </summary>
		internal PVSApplicationSettings? ApplicationSettings
		{
			get { return Inner.ApplicationSettings.Value; }
		}

		/// <summary>
		/// Whether to use the application settings for the mode
		/// </summary>
		public bool UseApplicationSettings
		{
			get { return Inner.UseApplicationSettings; }
		}

		/// <summary>
		/// Override for the analysis mode to use
		/// </summary>
		public PVSAnalysisModeFlags ModeFlags
		{
			get { return Inner.ModeFlags; }
		}
    
		/// <summary>
		/// Override for the analysis timeout to use
		/// </summary>
		public AnalysisTimeoutFlags AnalysisTimeoutFlag
		{
			get { return Inner.AnalysisTimeoutFlag; }
		}

		/// <summary>
		/// Override NoNoise analysis setting to use
		/// </summary>
		public bool EnableNoNoise
		{
			get { return Inner.EnableNoNoise; }
		}

		/// <summary>
		/// Override EnableReportDisabledRules analysis setting to use
		/// </summary>
		public bool EnableReportDisabledRules
		{
			get { return Inner.EnableReportDisabledRules; }
		}
	}

	/// <summary>
	/// Special mode for gathering all the messages into a single output file
	/// </summary>
	[ToolMode("PVSGather", ToolModeOptions.None)]
	class PVSGatherMode : ToolMode
	{
		/// <summary>
		/// Path to the input file list
		/// </summary>
		[CommandLine("-Input", Required = true)]
		FileReference? InputFileList = null;

		/// <summary>
		/// Output file to generate
		/// </summary>
		[CommandLine("-Output", Required = true)]
		FileReference? OutputFile = null;

		/// <summary>
		/// Execute the command
		/// </summary>
		/// <param name="Arguments">List of command line arguments</param>
		/// <returns>Always zero, or throws an exception</returns>
		/// <param name="Logger"></param>
		public override int Execute(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);
			Arguments.CheckAllArgumentsUsed();

			Logger.LogInformation("{File}", OutputFile!.GetFileName());

			// Read the input files
			string[] InputFileLines = FileReference.ReadAllLines(InputFileList!);
			FileReference[] InputFiles = InputFileLines.Select(x => x.Trim()).Where(x => x.Length > 0).Select(x => new FileReference(x)).ToArray();

			// Create the combined output file, and print the diagnostics to the log
			HashSet<string> UniqueItems = new HashSet<string>();
			List<string> OutputLines = new List<string>();

			using (StreamWriter RawWriter = new StreamWriter(OutputFile.FullName))
			{
				foreach (FileReference InputFile in InputFiles)
				{
					string[] Lines = File.ReadAllLines(InputFile.FullName);
					for(int LineIdx = 0; LineIdx < Lines.Length; LineIdx++)
					{
						string Line = Lines[LineIdx];
						if (!String.IsNullOrWhiteSpace(Line) && UniqueItems.Add(Line))
						{
							bool bCanParse = false;

							string[] Tokens = Line.Split(new string[] { "<#~>" }, StringSplitOptions.None);
							if(Tokens.Length >= 9)
							{
								//string Trial = Tokens[1];
								string LineNumberStr = Tokens[2];
								string FileName = Tokens[3];
								string WarningCode = Tokens[5];
								string WarningMessage = Tokens[6];
								string FalseAlarmStr = Tokens[7];
								string LevelStr = Tokens[8];

								int LineNumber;
								bool bFalseAlarm;
								int Level;
								if(int.TryParse(LineNumberStr, out LineNumber) && bool.TryParse(FalseAlarmStr, out bFalseAlarm) && int.TryParse(LevelStr, out Level))
								{
									bCanParse = true;

									// Ignore anything in ThirdParty folders
									if(FileName.Replace('/', '\\').IndexOf("\\ThirdParty\\", StringComparison.InvariantCultureIgnoreCase) == -1)
									{
										// Output the line to the raw output file
										RawWriter.WriteLine(Line);

										// Output the line to the log
										if (!bFalseAlarm && Level == 1)
										{
											Log.WriteLine(LogEventType.Warning, LogFormatOptions.NoSeverityPrefix, "{0}({1}): warning {2}: {3}", FileName, LineNumber, WarningCode, WarningMessage);
										}
									}
								}
							}

							if(!bCanParse)
							{
								Log.WriteLine(LogEventType.Warning, LogFormatOptions.NoSeverityPrefix, "{0}({1}): warning: Unable to parse PVS output line '{2}' (tokens=|{3}|)", InputFile, LineIdx + 1, Line, String.Join("|", Tokens));
							}
						}
					}
				}
			}
			Logger.LogInformation("Written {NumItems} {Noun} to {File}.", UniqueItems.Count, (UniqueItems.Count == 1)? "diagnostic" : "diagnostics", OutputFile.FullName);
			return 0;
		}
	}

	class PVSToolChain : ISPCToolChain
	{
		ReadOnlyTargetRules Target;
		ReadOnlyPVSTargetSettings Settings;
		PVSApplicationSettings? ApplicationSettings;
		VCToolChain InnerToolChain;
		FileReference AnalyzerFile;
		FileReference? LicenseFile;
		UnrealTargetPlatform Platform;
		Version AnalyzerVersion;

		public PVSToolChain(ReadOnlyTargetRules Target, ILogger Logger)
			: base(Logger)
		{
			this.Target = Target;
			Platform = Target.Platform;
			InnerToolChain = new VCToolChain(Target, Logger);

			AnalyzerFile = FileReference.Combine(Unreal.RootDirectory, "Engine", "Restricted", "NoRedist", "Extras", "ThirdPartyNotUE", "PVS-Studio", "PVS-Studio.exe");
			if (!FileReference.Exists(AnalyzerFile))
			{
				FileReference InstalledAnalyzerFile = FileReference.Combine(new DirectoryReference(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86)), "PVS-Studio", "x64", "PVS-Studio.exe");
				if (FileReference.Exists(InstalledAnalyzerFile))
				{
					AnalyzerFile = InstalledAnalyzerFile;
				}
				else
				{
					throw new BuildException("Unable to find PVS-Studio at {0} or {1}", AnalyzerFile, InstalledAnalyzerFile);
				}
			}

			AnalyzerVersion = GetAnalyzerVersion(AnalyzerFile);
			Settings = Target.WindowsPlatform.PVS;
			ApplicationSettings = Settings.ApplicationSettings;

			if(ApplicationSettings != null)
			{
				if (Settings.ModeFlags == 0)
				{
					throw new BuildException("All PVS-Studio analysis modes are disabled.");
				}

				if (!String.IsNullOrEmpty(ApplicationSettings.UserName) && !String.IsNullOrEmpty(ApplicationSettings.SerialNumber))
				{
					LicenseFile = FileReference.Combine(Unreal.EngineDirectory, "Intermediate", "PVS", "PVS-Studio.lic");
					Utils.WriteFileIfChanged(LicenseFile, String.Format("{0}\n{1}\n", ApplicationSettings.UserName, ApplicationSettings.SerialNumber), Logger);
				}
			}
			else
			{
				FileReference DefaultLicenseFile = AnalyzerFile.ChangeExtension(".lic");
				if(FileReference.Exists(DefaultLicenseFile))
				{
					LicenseFile = DefaultLicenseFile;
				}
			}
		}

		public override void GetVersionInfo(List<string> Lines)
		{
			base.GetVersionInfo(Lines);

			ReadOnlyPVSTargetSettings Settings = Target.WindowsPlatform.PVS;
			Lines.Add(String.Format("Using PVS-Studio installation at {0} with analysis mode {1} ({2})", AnalyzerFile, (uint)Settings.ModeFlags, Settings.ModeFlags.ToString()));
		}

		static Version GetAnalyzerVersion(FileReference AnalyzerPath)
		{
			String Output = String.Empty;
			Version? AnalyzerVersion = new Version(0, 0);

			try
			{
				using (Process PvsProc = new Process())
				{
					PvsProc.StartInfo.FileName = AnalyzerPath.FullName;
					PvsProc.StartInfo.Arguments = "--version";
					PvsProc.StartInfo.UseShellExecute = false;
					PvsProc.StartInfo.CreateNoWindow = true;
					PvsProc.StartInfo.RedirectStandardOutput = true;

					PvsProc.Start();
					Output = PvsProc.StandardOutput.ReadToEnd();
					PvsProc.WaitForExit();
				}

				const String VersionPattern = @"\d+(?:\.\d+)+";
				Match Match = Regex.Match(Output, VersionPattern);

				if (Match.Success)
				{
					string VersionStr = Match.Value;
					if (!Version.TryParse(VersionStr, out AnalyzerVersion))
					{
						throw new BuildException(String.Format("Failed to parse PVS-Studio version: {0}", VersionStr));
					}
				}
			}
			catch (Exception Ex)
			{
				if (Ex is BuildException)
					throw;

				throw new BuildException(Ex, "Failed to obtain PVS-Studio version.");
			}

			return AnalyzerVersion;
		}

		class ActionGraphCapture : ForwardingActionGraphBuilder
		{
			List<IExternalAction> Actions;

			public ActionGraphCapture(IActionGraphBuilder Inner, List<IExternalAction> Actions)
				: base(Inner)
			{
				this.Actions = Actions;
			}

			public override void AddAction(IExternalAction Action)
			{
				base.AddAction(Action);

				Actions.Add(Action);
			}
		}

		public static readonly VersionNumber CLVerWithCPP20Support = new VersionNumber(14, 23);

		const string CPP_20 = "c++20";
		const string CPP_17 = "c++17";

		public static string GetLangStandForCfgFile(CppStandardVersion cppStandard, VersionNumber compilerVersion)
		{
			string cppCfgStandard;

			switch (cppStandard)
			{
				case CppStandardVersion.Cpp17:
					cppCfgStandard = CPP_17;
					break;
        			case CppStandardVersion.Cpp20:
					cppCfgStandard = CPP_20;
					break;
				case CppStandardVersion.Latest:
					cppCfgStandard = VersionNumber.Compare(compilerVersion, CLVerWithCPP20Support) >= 0 ? CPP_20 : CPP_17;
					break;
				default:
					cppCfgStandard = "c++14";
					break;
			}

			return cppCfgStandard;
		}

		public static bool ShouldCompileAsC(String compilerCommandLine, String sourceFileName)
		{
			int CFlagLastPosition = Math.Max(Math.Max(compilerCommandLine.LastIndexOf("/TC "), compilerCommandLine.LastIndexOf("/Tc ")),
											 Math.Max(compilerCommandLine.LastIndexOf("-TC "), compilerCommandLine.LastIndexOf("-Tc ")));

			int CppFlagLastPosition = Math.Max(Math.Max(compilerCommandLine.LastIndexOf("/TP "), compilerCommandLine.LastIndexOf("/Tp ")),
											   Math.Max(compilerCommandLine.LastIndexOf("-TP "), compilerCommandLine.LastIndexOf("-Tp ")));

			bool compileAsCCode;
			if (CFlagLastPosition == CppFlagLastPosition)
				//ни один флаг, определяющий язык, не задан. Определяем по расширению файла
				compileAsCCode = Path.GetExtension(sourceFileName).Equals(".c", StringComparison.InvariantCultureIgnoreCase);
			else
				compileAsCCode = CFlagLastPosition > CppFlagLastPosition;

			return compileAsCCode;
		}

		protected override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph)
		{
			if (CompileEnvironment.bDisableStaticAnalysis)
			{
				return new CPPOutput();
			}

			// Ignore generated files
			if (InputFiles.All(x => x.Location.GetFileName().EndsWith(".gen.cpp")))
			{
				return new CPPOutput();
			}

			// Use a subdirectory for PVS output, to avoid clobbering regular build artifacts
			OutputDir = DirectoryReference.Combine(OutputDir, "PVS");

			// Preprocess the source files with the regular toolchain
			CppCompileEnvironment PreprocessCompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
			PreprocessCompileEnvironment.bPreprocessOnly = true;
			PreprocessCompileEnvironment.bEnableUndefinedIdentifierWarnings = false; // Not sure why THIRD_PARTY_INCLUDES_START doesn't pick this up; the _Pragma appears in the preprocessed output. Perhaps in preprocess-only mode the compiler doesn't respect these?
			PreprocessCompileEnvironment.AdditionalArguments += " /wd4005 /wd4828";
			PreprocessCompileEnvironment.Definitions.Add("PVS_STUDIO");

			List<IExternalAction> PreprocessActions = new List<IExternalAction>();
			CPPOutput Result = InnerToolChain.CompileAllCPPFiles(PreprocessCompileEnvironment, InputFiles, OutputDir, ModuleName, new ActionGraphCapture(Graph, PreprocessActions));

			// Run the source files through PVS-Studio
			for(int Idx = 0; Idx < PreprocessActions.Count; Idx++)
			{
				VCCompileAction? PreprocessAction = PreprocessActions[Idx] as VCCompileAction;
				if (PreprocessAction == null)
				{
					continue;
				}

				FileItem? SourceFileItem = PreprocessAction.SourceFile;
				if (SourceFileItem == null)
				{
					Logger.LogWarning("Unable to find source file from command producing: {File}", String.Join(", ", PreprocessActions[Idx].ProducedItems.Select(x => x.Location.GetFileName())));
					continue;
				}

				FileItem? PreprocessedFileItem = PreprocessAction.PreprocessedFile;
				if (PreprocessedFileItem == null)
				{
					Logger.LogWarning("Unable to find preprocessed output file from {File}", SourceFileItem.Location.GetFileName());
					continue;
				}

				// Write the PVS studio config file
				StringBuilder ConfigFileContents = new StringBuilder();
				foreach(DirectoryReference IncludePath in Target.WindowsPlatform.Environment!.IncludePaths)
				{
					ConfigFileContents.AppendFormat("exclude-path={0}\n", IncludePath.FullName);
				}
				if(ApplicationSettings != null && ApplicationSettings.PathMasks != null)
				{
					foreach(string PathMask in ApplicationSettings.PathMasks)
					{
						if (PathMask.Contains(":") || PathMask.Contains("\\") || PathMask.Contains("/"))
						{
							if(Path.IsPathRooted(PathMask) && !PathMask.Contains(":"))
							{
								ConfigFileContents.AppendFormat("exclude-path=*{0}*\n", PathMask);
							}
							else
							{
								ConfigFileContents.AppendFormat("exclude-path={0}\n", PathMask);
							}
						}
					}
				}
        			if (Platform == UnrealTargetPlatform.Win64)
				{
					ConfigFileContents.Append("platform=x64\n");
				}
				else
				{
					throw new BuildException("PVS-Studio does not support this platform");
				}
        		ConfigFileContents.Append("preprocessor=visualcpp\n");

				bool shouldCompileAsC = ShouldCompileAsC(String.Join(" ", PreprocessAction.Arguments), SourceFileItem.AbsolutePath);
				ConfigFileContents.AppendFormat("language={0}\n", shouldCompileAsC ? "C" : "C++");

				ConfigFileContents.Append("skip-cl-exe=yes\n");

				WindowsCompiler WindowsCompiler = Target.WindowsPlatform.Compiler;
				bool isVisualCppCompiler = WindowsCompiler == WindowsCompiler.VisualStudio2022 || WindowsCompiler == WindowsCompiler.VisualStudio2019;
				if (AnalyzerVersion.CompareTo(new Version("7.07")) >= 0 && !shouldCompileAsC)
				{
					VersionNumber compilerVersion = Target.WindowsPlatform.Environment.CompilerVersion;
					string languageStandardForCfg = GetLangStandForCfgFile(PreprocessCompileEnvironment.CppStandard, compilerVersion);

					ConfigFileContents.AppendFormat("std={0}\n", languageStandardForCfg);
          
          			bool disableMsExtentinsFromArgs = PreprocessAction.Arguments.Any(arg => arg.Equals("/Za") || arg.Equals("-Za") || arg.Equals("/permissive-"));
					bool disableMsExtentions = isVisualCppCompiler && (languageStandardForCfg == CPP_20 || disableMsExtentinsFromArgs);
					ConfigFileContents.AppendFormat("disable-ms-extensions={0}\n", disableMsExtentions ? "yes" : "no");
				}

				if (isVisualCppCompiler && PreprocessAction.Arguments.Any(arg => arg.StartsWith("/await")))
				{
					ConfigFileContents.Append("msvc-await=yes\n");
				}

				if (Settings.EnableNoNoise)
				{
					ConfigFileContents.Append("no-noise=yes\n");
				}

				if (Settings.EnableReportDisabledRules)
				{
					ConfigFileContents.Append("report-disabled-rules=yes\n");
				}

        		int Timeout = (int)(Settings.AnalysisTimeoutFlag == AnalysisTimeoutFlags.No_timeout ? 0 : Settings.AnalysisTimeoutFlag);
				ConfigFileContents.AppendFormat("timeout={0}\n", Timeout);

				string BaseFileName = PreprocessedFileItem.Location.GetFileName();

				FileReference ConfigFileLocation = FileReference.Combine(OutputDir, BaseFileName + ".cfg");
				FileItem ConfigFileItem = Graph.CreateIntermediateTextFile(ConfigFileLocation, ConfigFileContents.ToString());

				// Run the analzyer on the preprocessed source file
				FileReference OutputFileLocation = FileReference.Combine(OutputDir, BaseFileName + ".pvslog");
				FileItem OutputFileItem = FileItem.GetItemByFileReference(OutputFileLocation);

				Action AnalyzeAction = Graph.CreateAction(ActionType.Compile);
				AnalyzeAction.CommandDescription = "Analyzing";
				AnalyzeAction.StatusDescription = BaseFileName;
				AnalyzeAction.WorkingDirectory = Unreal.EngineSourceDirectory;
				AnalyzeAction.CommandPath = AnalyzerFile;

				StringBuilder Arguments = new StringBuilder();
				Arguments.Append($"--source-file \"{SourceFileItem.AbsolutePath}\" --output-file \"{OutputFileLocation}\" --cfg \"{ConfigFileItem.AbsolutePath}\" --i-file=\"{PreprocessedFileItem.AbsolutePath}\" --analysis-mode {(uint)Settings.ModeFlags}");
				if (LicenseFile != null)
				{
					Arguments.Append($" --lic-file \"{LicenseFile}\"");
					AnalyzeAction.PrerequisiteItems.Add(FileItem.GetItemByFileReference(LicenseFile));
				}
				AnalyzeAction.CommandArguments = Arguments.ToString();

				AnalyzeAction.PrerequisiteItems.Add(ConfigFileItem);
				AnalyzeAction.PrerequisiteItems.Add(PreprocessedFileItem);
				AnalyzeAction.PrerequisiteItems.UnionWith(InputFiles); // Add the InputFiles as PrerequisiteItems so that in SingleFileCompile mode the PVSAnalyze step is not filtered out
				AnalyzeAction.ProducedItems.Add(OutputFileItem);
				AnalyzeAction.DeleteItems.Add(OutputFileItem); // PVS Studio will append by default, so need to delete produced items
				AnalyzeAction.bCanExecuteRemotely = false;

				Result.ObjectFiles.AddRange(AnalyzeAction.ProducedItems);
			}
			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph)
		{
			throw new BuildException("Unable to link with PVS toolchain.");
		}

		public override void FinalizeOutput(ReadOnlyTargetRules Target, TargetMakefileBuilder MakefileBuilder)
		{
			FileReference OutputFile;
			if (Target.ProjectFile == null)
			{
				OutputFile = FileReference.Combine(Unreal.EngineDirectory, "Saved", "PVS-Studio", String.Format("{0}.pvslog", Target.Name));
			}
			else
			{
				OutputFile = FileReference.Combine(Target.ProjectFile.Directory, "Saved", "PVS-Studio", String.Format("{0}.pvslog", Target.Name));
			}

			TargetMakefile Makefile = MakefileBuilder.Makefile;
			List<FileReference> InputFiles = Makefile.OutputItems.Select(x => x.Location).Where(x => x.HasExtension(".pvslog")).ToList();

			// Collect the sourcefile items off of the Compile action added in CompileCPPFiles so that in SingleFileCompile mode the PVSGather step is also not filtered out
			List<FileItem> CompileSourceFiles = Makefile.Actions.OfType<VCCompileAction>().Select(x => x.SourceFile!).ToList();

			FileItem InputFileListItem = MakefileBuilder.CreateIntermediateTextFile(OutputFile.ChangeExtension(".input"), InputFiles.Select(x => x.FullName));

			Action AnalyzeAction = MakefileBuilder.CreateAction(ActionType.Compile);
			AnalyzeAction.ActionType = ActionType.PostBuildStep;
			AnalyzeAction.CommandDescription = "Process PVS-Studio Results";
			AnalyzeAction.CommandPath = Unreal.DotnetPath;
			AnalyzeAction.CommandArguments = $"\"{Unreal.UnrealBuildToolDllPath}\" -Mode=PVSGather -Input=\"{InputFileListItem.Location}\" -Output=\"{OutputFile}\" ";
			AnalyzeAction.WorkingDirectory = Unreal.EngineSourceDirectory;
			AnalyzeAction.PrerequisiteItems.Add(InputFileListItem);
			AnalyzeAction.PrerequisiteItems.UnionWith(Makefile.OutputItems);
			AnalyzeAction.PrerequisiteItems.UnionWith(CompileSourceFiles);
			AnalyzeAction.ProducedItems.Add(FileItem.GetItemByFileReference(OutputFile));
			AnalyzeAction.ProducedItems.Add(FileItem.GetItemByPath(OutputFile.FullName + "_does_not_exist")); // Force the gather step to always execute
			AnalyzeAction.DeleteItems.UnionWith(AnalyzeAction.ProducedItems);

			Makefile.OutputItems.AddRange(AnalyzeAction.ProducedItems);
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.BuildGraph;
using EpicGames.BuildGraph.Expressions;
using System;
using UnrealBuildTool;
using AutomationTool.Tasks;
using System.Collections.Generic;
using System.Linq;
using UnrealBuildBase;
using System.Threading.Tasks;

using static AutomationTool.Tasks.StandardTasks;
using Microsoft.Extensions.Logging;

#nullable enable

namespace AutomationTool
{
	class InstalledBuild : BgGraphBuilder
	{
		static FileSet Workspace { get; } = FileSet.FromDirectory(Unreal.RootDirectory);
		static DirectoryReference RootDir { get; } = new DirectoryReference(CommandUtils.CmdEnv.LocalRoot);
		static DirectoryReference IntermediateDir => DirectoryReference.Combine(RootDir, "Engine", "Intermediate", "Installed");
		static DirectoryReference TempMiscDir => DirectoryReference.Combine(IntermediateDir, "General");
		static DirectoryReference TempCsToolsDir => DirectoryReference.Combine(IntermediateDir, "CsTools");
		static DirectoryReference TempDdcDir = DirectoryReference.Combine(IntermediateDir, "DDC");

		static string[] PluginsExceptions =
		{
			"Engine/Plugins/Enterprise/DatasmithCADImporter/...",
			"Engine/Plugins/Enterprise/DatasmithC4DImporter/...",
			"Engine/Plugins/Enterprise/AxFImporter/...",
			"Engine/Plugins/Enterprise/MDLImporter/..."
		};

		static string[] WinSignFilter =
		{
			"*.exe",
			"*.dll"
		};

		static string[] WinStripFilter =
		{
			"*.pdb",
			"-/Engine/Binaries/Win64/UnrealEditor*.pdb",
			"-/Engine/Plugins/.../Binaries/Win64/UnrealEditor*.pdb",
		};

		static string[] ProjectsToBuildDdc =
		{
		};

		static List<string> GetDdcProjects(UnrealTargetPlatform Platform)
		{
			List<string> Projects = new List<string>();
			Projects.Add("Templates/TP_AEC_ArchvisBP/TP_AEC_ArchvisBP.uproject");
			Projects.Add("Templates/TP_AEC_BlankBP/TP_AEC_BlankBP.uproject");
			Projects.Add("Templates/TP_AEC_CollabBP/TP_AEC_CollabBP.uproject");
			Projects.Add("Templates/TP_AEC_ProdConfigBP/TP_AEC_ProdConfigBP.uproject");
			Projects.Add("Templates/TP_FirstPersonBP/TP_FirstPersonBP.uproject");
			Projects.Add("Templates/TP_HandheldARBP/TP_HandheldARBP.uproject");
			Projects.Add("Templates/TP_AEC_HandheldARBP/TP_AEC_HandheldARBP.uproject");
			Projects.Add("Templates/TP_MFG_HandheldARBP/TP_MFG_HandheldARBP.uproject");
			Projects.Add("Templates/TP_MFG_CollabBP/TP_MFG_CollabBP.uproject");
			Projects.Add("Templates/TP_MFG_ProdConfigBP/TP_MFG_ProdConfigBP.uproject");
			Projects.Add("Templates/TP_PhotoStudioBP/TP_PhotoStudioBP.uproject");
			Projects.Add("Templates/TP_PuzzleBP/TP_PuzzleBP.uproject");
			Projects.Add("Templates/TP_ThirdPersonBP/TP_ThirdPersonBP.uproject");
			Projects.Add("Templates/TP_TopDownBP/TP_TopDownBP.uproject");
			Projects.Add("Templates/TP_VehicleAdvBP/TP_VehicleAdvBP.uproject");
			Projects.Add("Templates/TP_VirtualRealityBP/TP_VirtualRealityBP.uproject");
//			Projects.Add("Samples/StarterContent/StarterContent.uproject");

			if (Platform == UnrealTargetPlatform.Win64)
			{
				Projects.Add("Templates/TP_InCamVFXBP/TP_InCamVFXBP.uproject");
				Projects.Add("Templates/TP_DMXBP/TP_DMXBP.uproject");
			}
			return Projects;
		}

		public override BgGraph CreateGraph(BgEnvironment Context)
		{
			string RootDir = CommandUtils.CmdEnv.LocalRoot;

			BgBoolOption HostPlatformOnly = new BgBoolOption("HostPlatformOnly", "A helper option to make an installed build for your host platform only, so that you don't have to disable each platform individually", false);
			BgBoolOption HostPlatformEditorOnly = new BgBoolOption("HostPlatformEditorOnly", "A helper option to make an installed build for your host platform only, so that you don't have to disable each platform individually", false);
			BgBoolOption AllPlatforms = new BgBoolOption("AllPlatforms", "Include all target platforms by default", false);
			BgBoolOption CompileDatasmithPlugins = new BgBoolOption("CompileDatasmithPlugins", "If Datasmith plugins should be compiled on a separate node.", false);

			UnrealTargetPlatform CurrentHostPlatform = HostPlatform.Current.HostEditorPlatform;

			BgBool DefaultWithWin64 = !(HostPlatformEditorOnly | (HostPlatformOnly & (CurrentHostPlatform != UnrealTargetPlatform.Win64)));
			BgBool DefaultWithMac = !(HostPlatformEditorOnly | (HostPlatformOnly & (CurrentHostPlatform != UnrealTargetPlatform.Mac)));
			BgBool DefaultWithLinux = !(HostPlatformEditorOnly | (HostPlatformOnly & (CurrentHostPlatform != UnrealTargetPlatform.Linux)));
			BgBool DefaultWithLinuxArm64 = !(HostPlatformEditorOnly | (HostPlatformOnly & (CurrentHostPlatform != UnrealTargetPlatform.Linux)));
			BgBool DefaultWithPlatform = !(HostPlatformEditorOnly | HostPlatformOnly);
			BgBool DefaultWithIOS = !((CurrentHostPlatform != UnrealTargetPlatform.Mac) & !AllPlatforms);

			BgBoolOption WithWin64 = new BgBoolOption("WithWin64", "Include the Win64 target platform", DefaultWithWin64);
			BgBoolOption WithMac = new BgBoolOption("WithMac", "Include the Mac target platform", DefaultWithMac);
			BgBoolOption WithAndroid = new BgBoolOption("WithAndroid", "Include the Android target platform", DefaultWithPlatform);
			BgBoolOption WithIOS = new BgBoolOption("WithIOS", "Include the iOS target platform", DefaultWithIOS);
			BgBoolOption WithTVOS = new BgBoolOption("WithTVOS", "Include the tvOS target platform", DefaultWithIOS);
			BgBoolOption WithLinux = new BgBoolOption("WithLinux", "Include the Linux target platform", DefaultWithLinux);
			BgBoolOption WithLinuxArm64 = new BgBoolOption("WithLinuxArm64", "Include the Linux AArch64 target platform", DefaultWithLinuxArm64);

			BgBoolOption WithClient = new BgBoolOption("WithClient", "Include precompiled client targets", false);
			BgBoolOption WithServer = new BgBoolOption("WithServer", "Include precompiled server targets", false);
			BgBoolOption WithDDC = new BgBoolOption("WithDDC", "Build a standalone derived-data cache for the engine content and templates", true);
			BgBoolOption HostPlatformDDCOnly = new BgBoolOption("HostPlatformDDCOnly", "Whether to include DDC for the host platform only", true);
			BgBoolOption SignExecutables = new BgBoolOption("SignExecutables", "Sign the executables produced where signing is available", false);

			BgStringOption AnalyticsTypeOverride = new BgStringOption("AnalyticsTypeOverride", "Identifier for analytic events to send", "");
			BgBool EmbedSrcSrvInfo = new BgBoolOption("EmbedSrcSrvInfo", "Whether to add Source indexing to Windows game apps so they can be added to a symbol server", false);

			BgList<BgString> DefaultGameConfigurations = BgList<BgString>.Create(nameof(UnrealTargetConfiguration.DebugGame), nameof(UnrealTargetConfiguration.Development), nameof(UnrealTargetConfiguration.Shipping));
			BgList<BgString> GameConfigurationStrings = new BgListOption("GameConfigurations", description: "Which game configurations to include for packaged applications", style: BgListOptionStyle.CheckList, values: DefaultGameConfigurations);
			BgList<BgEnum<UnrealTargetConfiguration>> GameConfigurations = GameConfigurationStrings.Select(x => BgEnum<UnrealTargetConfiguration>.Parse(x));

			BgBoolOption WithFullDebugInfo = new BgBoolOption("WithFullDebugInfo", "Generate full debug info for binary editor and packaged application builds", false);

			BgStringOption BuiltDirectory = new BgStringOption("BuiltDirectory", "Directory for outputting the built engine", RootDir + "/LocalBuilds/Engine");

			BgStringOption CrashReporterAPIURL = new BgStringOption("CrashReporterAPIURL", "The URL to use to talk to the CrashReporterClient API.", "");
			BgStringOption CrashReporterAPIKey = new BgStringOption("CrashReporterAPIKey", "The API key to use to talk to the CrashReporterClient API.", "");
			BgStringOption BuildId = new BgStringOption("BuildId", "The unique build identifier to associate with this installed build", "");

			BgString CrashReporterCompileArgs = "";
			CrashReporterCompileArgs = CrashReporterCompileArgs.If(CrashReporterAPIURL != "" & CrashReporterAPIKey != "", BgString.Format("-define:CRC_TELEMETRY_URL=\"{0}\" -define:CRC_TELEMETRY_KEY_DEV=\"{1}\" -define:CRC_TELEMETRY_KEY_RELEASE=\"{1}\" -OverrideBuildEnvironment", CrashReporterAPIURL, CrashReporterAPIKey));

			List<BgAggregate> Aggregates = new List<BgAggregate>();

			/////// EDITORS ////////////////////////////////////////////////

			//// Windows ////
			BgAgent EditorWin64 = new BgAgent("Editor Win64", "Win64_Licensee");

			BgNode VersionFilesNode = EditorWin64
				.AddNode(x => UpdateVersionFilesAsync(x));

			BgNode<BgFileSet> WinEditorNode = EditorWin64
				.AddNode(x => CompileUnrealEditorWin64Async(x, CrashReporterCompileArgs, EmbedSrcSrvInfo, CompileDatasmithPlugins, WithFullDebugInfo, SignExecutables))
				.Requires(VersionFilesNode);

			Aggregates.Add(new BgAggregate("Win64 Editor", WinEditorNode, label: "Editors/Win64"));


			/////// TARGET PLATFORMS ////////////////////////////////////////////////

			//// Win64 ////

			BgAgent TargetWin64 = new BgAgent("Target Win64", "Win64_Licensee");

			BgNode<BgFileSet> WinGame = TargetWin64
				.AddNode(x => CompileUnrealGameWin64(x, GameConfigurations, EmbedSrcSrvInfo, WithFullDebugInfo, SignExecutables))
				.Requires(VersionFilesNode);

			Aggregates.Add(new BgAggregate("TargetPlatforms_Win64", WinGame));

			/////// TOOLS //////////////////////////////////////////////////////////

			//// Build Rules ////

			BgAgent BuildRules = new BgAgent("BuildRules", "Win64_Licensee");

			BgNode RulesAssemblies = BuildRules
				.AddNode(x => CompileRulesAssemblies(x));

			//// Win Tools ////

			BgAgent ToolsGroupWin64 = new BgAgent("Tools Group Win64", "Win64_Licensee");

			BgNode<BgFileSet> WinTools = ToolsGroupWin64
				.AddNode(x => BuildToolsWin64Async(x, CrashReporterCompileArgs))
				.Requires(VersionFilesNode);

			BgNode<BgFileSet> CsTools = ToolsGroupWin64
				.AddNode(x => BuildToolsCSAsync(x, SignExecutables))
				.Requires(VersionFilesNode);


			/////// DDC //////////////////////////////////////////////////////////

			BgAgent DDCGroupWin64 = new BgAgent("DDC Group Win64", "Win64_Licensee");

			BgList<BgString> DDCPlatformsWin64 = BgList<BgString>.Create("WindowsEditor");
			DDCPlatformsWin64 = DDCPlatformsWin64.If(WithWin64, x => x.Add("Windows"));

			BgNode DdcNode = DDCGroupWin64
				.AddNode(x => BuildDDCWin64Async(x, DDCPlatformsWin64, BgList<BgFileSet>.Create(WinEditorNode.Output, WinTools.Output)))
				.Requires(WinEditorNode, WinTools);


			/////// STAGING ///////

			// Windows 
			BgAgent WinStageAgent = new BgAgent("Installed Build Group Win64", "Win64_Licensee");

			BgList<BgFileSet> WinInstalledFiles = BgList<BgFileSet>.Empty;
			WinInstalledFiles = WinInstalledFiles.Add(WinEditorNode.Output);
			WinInstalledFiles = WinInstalledFiles.Add(WinTools.Output);
			WinInstalledFiles = WinInstalledFiles.Add(CsTools.Output);
			WinInstalledFiles = WinInstalledFiles.If(WithWin64, x => x.Add(WinGame.Output));

			BgList<BgString> WinPlatforms = BgList<BgString>.Empty;
			WinPlatforms = WinPlatforms.If(WithWin64, x => x.Add("Win64"));

			BgList<BgString> WinContentOnlyPlatforms = BgList<BgString>.Empty;

			BgString WinOutputDir = "LocalBuilds/Engine/Windows";
			BgString WinFinalizeArgs = BgString.Format("-OutputDir=\"{0}\" -Platforms={1} -ContentOnlyPlatforms={2}", WinOutputDir, BgString.Join(";", WinPlatforms), BgString.Join(";", WinContentOnlyPlatforms));

			BgNode WinInstalledNode = WinStageAgent
				.AddNode(x => MakeInstalledBuildWin64Async(x, WinInstalledFiles, WinFinalizeArgs, WinOutputDir))
				.Requires(WinInstalledFiles);

			Aggregates.Add(new BgAggregate("HostPlatforms_Win64", WinInstalledNode, label: "Builds/Win64"));

			return new BgGraph(BgList<BgNode>.Empty, Aggregates);
		}

		/// <summary>
		/// Update the build version
		/// </summary>
		static async Task UpdateVersionFilesAsync(BgContext State)
		{
			if (State.IsBuildMachine)
			{
				await SetVersionAsync(State.Change, State.Stream.Replace('/', '+'));
			}
		}

		/// <summary>
		/// Builds the Windows editor
		/// </summary>
		[BgNodeName("Compile UnrealEditor Win64")]
		static async Task<BgFileSet> CompileUnrealEditorWin64Async(BgContext State, BgString CrashReporterCompileArgs, BgBool EmbedSrcSrvInfo, BgBool CompileDatasmithPlugins, BgBool WithFullDebugInfo, BgBool SignExecutables)
		{
			FileSet OutputFiles = FileSet.Empty;
			OutputFiles += await CompileAsync("UnrealEditor", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.DebugGame, Arguments: $"-precompile -allmodules {State.Get(CrashReporterCompileArgs)}");
			OutputFiles += await CompileAsync("UnrealEditor", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development, Arguments: $"-precompile -allmodules {State.Get(CrashReporterCompileArgs)}");

			if (State.Get(EmbedSrcSrvInfo))
			{
				// Embed source info into the PDB files. Should be done from this machine to ensure that paths are correct.
				Logger.LogInformation("Embedding source file information into PDB files...");
				FileSet SourceFiles = Workspace.Filter("Engine/Source/...;Engine/Plugins/...").Except("Engine/Source/ThirdParty/...").Filter("*.c;*.h;*.cpp;*.hpp;*.inl");
//				State.SrcSrv(BinaryFiles: Full, SourceFiles: SourceFiles);
			}

			if (!State.Get(WithFullDebugInfo))
			{
				FileSet UnstrippedFiles = OutputFiles.Filter(WinStripFilter);
				OutputFiles += await StripAsync(UnstrippedFiles, UnrealTargetPlatform.Win64, BaseDir: RootDir, OutputDir: TempMiscDir);
				OutputFiles -= UnstrippedFiles;
			}

			if(State.Get(SignExecutables))
			{
//				FileSet UnsignedFiles = OutputFiles.Filter(WinSignFilter);
//				FileSet FilesToCopy = UnsignedFiles.Except((new HashSet<FileReference>(UnsignedFiles.Where(x => !x.IsUnderDirectory(TempMiscDir)));
//				UnsignedFiles += State.Copy(FilesToCopy.Flatten(TempMiscDir));
//				UnsignedFiles.Sign();
//				OutputFiles -= FilesToCopy;
//				OutputFiles += UnsignedFiles;
			}

			return OutputFiles;
		}

		/// <summary>
		/// Builds the game target
		/// </summary>
		[BgNodeName("Compile UnrealGame Win64")]
		static async Task<BgFileSet> CompileUnrealGameWin64(BgContext State, BgList<BgEnum<UnrealTargetConfiguration>> Configurations, BgBool EmbedSrcSrvInfo, BgBool WithFullDebugInfo, BgBool SignExecutables)
		{
			FileSet Files = FileSet.Empty;

			List<UnrealTargetConfiguration> ConfigurationsValue = State.Get(Configurations);
			foreach (UnrealTargetConfiguration Configuration in ConfigurationsValue)
			{
				Files += await CompileAsync("UnrealGame", UnrealTargetPlatform.Win64, Configuration, Arguments: "-precompile -allmodules -nolink");
				Files += await CompileAsync("UnrealGame", UnrealTargetPlatform.Win64, Configuration, Arguments: "-precompile", Clean: false);
			}

			return Files;
		}

		static async Task<BgFileSet> CompileRulesAssemblies(BgContext State)
		{
			FileReference UnrealBuildToolDll = FileReference.Combine(RootDir, "Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll");

			await SpawnAsync(Unreal.DotnetPath.FullName, Arguments: $"\"{UnrealBuildToolDll}\" -Mode=QueryTargets");

			return Workspace.Filter("Engine/Intermediate/Build/BuildRules/...");
		}

		static async Task<BgFileSet> BuildToolsWin64Async(BgContext State, BgString CrashReporterCompileArgs)
		{
			string CrashReportClientArgs = State.Get(CrashReporterCompileArgs);

			// State.Tag(Files: "#NotForLicensee Build Tools Win64", With: Files);

			FileSet Files = FileSet.Empty;
			Files += await CompileAsync("CrashReportClient", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Shipping, Arguments: CrashReportClientArgs);
			Files += await CompileAsync("CrashReportClientEditor", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Shipping, Arguments: CrashReportClientArgs);
			Files += await CompileAsync("ShaderCompileWorker", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += await CompileAsync("EpicWebHelper", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += await CompileAsync("UnrealInsights", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += await CompileAsync("UnrealFrontend", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += await CompileAsync("UnrealLightmass", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += await CompileAsync("InterchangeWorker", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += await CompileAsync("UnrealPak", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += await CompileAsync("UnrealMultiUserServer", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += await CompileAsync("UnrealRecoverySvc", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += await CompileAsync("LiveCodingConsole", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += await CompileAsync("BootstrapPackagedGame", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Shipping);
			Files += await CompileAsync("BuildPatchTool", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Shipping);
			Files += await CompileAsync("SwitchboardListener", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
			Files += FileSet.FromFile(RootDir, "Engine/Binaries/Win64/XGEControlWorker.exe");
			Files += FileSet.FromFile(RootDir, "Engine/Saved/UnrealBuildTool/BuildConfiguration.Schema.xsd");

			return Files;
		}

		[BgNodeName("Build Tools CS")]
		static async Task<BgFileSet> BuildToolsCSAsync(BgContext State, BgBool SignExecutables)
		{
			FileUtils.ForceDeleteDirectory(TempCsToolsDir);

			// Copy Source and referenced libraries to a new location with Confidential folders removed
			FileSet UatProjects = Workspace.Filter("Engine/Source/Programs/AutomationTool/....csproj");

			FileSet RedistUatSource = Workspace.Filter("Engine/Binaries/DotNET/...;Engine/Binaries/ThirdParty/Newtonsoft/...;Engine/Binaries/ThirdParty/IOS/...;Engine/Binaries/ThirdParty/VisualStudio/...;Engine/Source/Programs/...;Engine/Platforms/*/Source/Programs/...;Engine/Source/Editor/SwarmInterface/...");
			await RedistUatSource.CopyToAsync(Unreal.RootDirectory);

			// Compile all the tools
			CsCompileOutput Output = CsCompileOutput.Empty;
			if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
			{
				Output += await CsCompileAsync(FileReference.Combine(TempCsToolsDir, "Engine/Source/Programs/UnrealSwarm/SwarmCoordinator/SwarmCoordinator.csproj"), Platform: "AnyCPU", Configuration: "Development");
				Output += await CsCompileAsync(FileReference.Combine(TempCsToolsDir, "Engine/Source/Programs/UnrealSwarm/Agent/Agent.csproj"), Platform: "AnyCPU", Configuration: "Development");
				Output += await CsCompileAsync(FileReference.Combine(TempCsToolsDir, "Engine/Source/Editor/SwarmInterface/DotNET/SwarmInterface.csproj"), Platform: "AnyCPU", Configuration: "Development");
				Output += await CsCompileAsync(FileReference.Combine(TempCsToolsDir, "Engine/Source/Programs/UnrealControls/UnrealControls.csproj"), Platform: "AnyCPU", Configuration: "Development");
				Output += await CsCompileAsync(FileReference.Combine(TempCsToolsDir, "Engine/Source/Programs/IOS/iPhonePackager/iPhonePackager.csproj"), Platform: "AnyCPU", Configuration: "Development", Arguments: "/verbosity:minimal /target:Rebuild");
				Output += await CsCompileAsync(FileReference.Combine(TempCsToolsDir, "Engine/Source/Programs/NetworkProfiler/NetworkProfiler/NetworkProfiler.csproj"), Platform: "AnyCPU", Configuration: "Development");
			}

			FileSet Binaries = Output.Binaries + Output.References;

			// Tag AutomationTool and UnrealBuildTool folders recursively as NET Core dependencies are not currently handled by CsCompile
			Binaries += FileSet.FromDirectory(TempCsToolsDir).Filter("Engine/Binaries/DotNET/AutomationTool/...");
			Binaries += FileSet.FromDirectory(TempCsToolsDir).Filter("Engine/Binaries/DotNET/UnrealBuildTool/...");

			// Tag AutomationTool Script module build records, so that prebuilt modules may be discovered in the absence of source code
			Binaries += FileSet.FromDirectory(TempCsToolsDir).Filter("Engine/Intermediate/ScriptModules/...");

			if(State.Get(SignExecutables))
			{
//				Binaries.Sign();
			}

			return Binaries;
		}

		/// <summary>
		/// Creates a DDC pack file for the supported platforms
		/// </summary>
		[BgNodeName("Build DDC Win64")]
		static async Task<BgFileSet> BuildDDCWin64Async(BgContext State, BgList<BgString> Platforms, BgList<BgFileSet> Dependencies)
		{
			// Build up a list of files needed to build DDC
			FileSet ToCopy = State.Get(Dependencies);
			ToCopy += await CsCompileAsync(FileReference.Combine(RootDir, "Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj"), Platform: "AnyCPU", Configuration: "Development", EnumerateOnly: true).MergeAsync();
			ToCopy += await ToCopy.Filter("*.target").TagReceiptsAsync(RuntimeDependencies: true);
			ToCopy += FileSet.FromFile(RootDir, "Engine/Binaries/DotNET/Ionic.Zip.Reduced.dll");
			ToCopy += FileSet.FromFile(RootDir, "Engine/Binaries/DotNET/OneSky.dll");
			ToCopy += FileSet.FromDirectory(RootDir).Filter("Templates/TemplateResources/...");

			foreach (DirectoryReference ExtensionDir in Unreal.GetExtensionDirs(Unreal.EngineDirectory))
			{
				ToCopy += FileSet.FromDirectory(ExtensionDir).Filter("Content/...").Except("....psd;....pdn;....fbx;....po");
				ToCopy += FileSet.FromDirectory(ExtensionDir).Filter("Config/...").Except("....vdf");
				ToCopy += FileSet.FromDirectory(ExtensionDir).Filter("Shaders/...");

				DirectoryReference ExtensionPluginsDir = DirectoryReference.Combine(ExtensionDir, "Plugins");
				ToCopy += FileSet.FromDirectory(ExtensionPluginsDir).Filter("....uplugin;.../Config/...;.../Content/...;.../Resources/...;.../Shaders/...;.../Templates/...").Except(".../TwitchLiveStreaming/...");
			}

			// Filter out the files not needed to build DDC. Removing confidential folders can affect DDC keys, so we want to be sure that we're making DDC with a build that can use it.
			FileSet FilteredCopyList = ToCopy.Except(".../Source/...;.../Intermediate/...");

			Dictionary<FileReference, FileReference> TargetFileToSourceFile = new Dictionary<FileReference, FileReference>();
			MapFilesToOutputDir(FilteredCopyList, TempDdcDir, TargetFileToSourceFile);

			FileUtils.ForceDeleteDirectoryContents(TempDdcDir);
			await FilteredCopyList.CopyToAsync(TempDdcDir);

			// Run the DDC commandlet
			List<string> Arguments = new List<string>();
			Arguments.Add($"-TempDir=\"{TempDdcDir}\"");
			Arguments.Add($"-FeaturePacks=\"{String.Join(";", GetDdcProjects(UnrealTargetPlatform.Win64))}\"");
			Arguments.Add($"-TargetPlatforms={String.Join("+", State.Get(Platforms))}");
			Arguments.Add($"-HostPlatform=Win64");
			await State.CommandAsync("BuildDerivedDataCache", Arguments: String.Join(" ", Arguments));

			// Return a tag for the output file
			return FileSet.FromFile(TempDdcDir, "Engine/DerivedDataCache/Compressed.ddp");
		}

		/// <summary>
		/// Copy all the build artifacts to the output folder
		/// </summary>
		[BgNodeName("Make Installed Build Win64")]
		static async Task<BgFileSet> MakeInstalledBuildWin64Async(BgContext State, BgList<BgFileSet> InputFiles, BgString FinalizeArgs, BgString OutputDir)
		{
			// Find all the input files, and add any runtime dependencies from the receipts into the list
			FileSet SourceFiles = State.Get(InputFiles);
			SourceFiles += await SourceFiles.Filter("*.target").TagReceiptsAsync(RuntimeDependencies: true);

			// Include any files referenced by dependency lists
			FileSet DependencyListFiles = SourceFiles.Filter(".../DependencyList.txt;.../DependencyList-AllModules.txt");
			foreach (FileReference DependencyListFile in DependencyListFiles)
			{
				string[] Lines = FileReference.ReadAllLines(DependencyListFile);
				foreach (string Line in Lines)
				{
					string TrimLine = Line.Trim();
					if (TrimLine.Length > 0)
					{
						SourceFiles += FileSet.FromFile(Unreal.RootDirectory, TrimLine);
					}
				}
			}
			SourceFiles -= DependencyListFiles;

			// Clear the output directory
			DirectoryReference OutputDirRef = new DirectoryReference(State.Get(OutputDir));
			FileUtils.ForceDeleteDirectoryContents(OutputDirRef);
			FileSet InstalledFiles = await SourceFiles.CopyToAsync(OutputDirRef);

			// Run the finalize command 
			await State.CommandAsync("FinalizeInstalledBuild", Arguments: State.Get(FinalizeArgs));

			// Get the final list of files
			InstalledFiles += FileSet.FromFile(OutputDirRef, "Engine/Build/InstalledEngine.txt");

			// Sanitize any receipts in the output directory
			await InstalledFiles.Filter("*.target").SanitizeReceiptsAsync();

			return InstalledFiles;
		}

		static void MapFilesToOutputDir(IEnumerable<FileReference> SourceFiles, DirectoryReference TargetDir, Dictionary<FileReference, FileReference> TargetFileToSourceFile)
		{
			foreach (FileReference SourceFile in SourceFiles)
			{
				DirectoryReference BaseDir;
				if (SourceFile.IsUnderDirectory(TempCsToolsDir))
				{
					BaseDir = TempCsToolsDir;
				}
				else if (SourceFile.IsUnderDirectory(TempMiscDir))
				{
					BaseDir = TempMiscDir;
				}
				else if (SourceFile.IsUnderDirectory(TempDdcDir))
				{
					BaseDir = TempDdcDir;
				}
				else
				{
					BaseDir = Unreal.RootDirectory;
				}

				FileReference TargetFile = FileReference.Combine(TargetDir, SourceFile.MakeRelativeTo(BaseDir));
				TargetFileToSourceFile[TargetFile] = SourceFile;
			}
		}
	}
}

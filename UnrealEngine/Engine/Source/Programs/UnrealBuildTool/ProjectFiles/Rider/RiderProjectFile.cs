// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	internal class TargetEntry
	{
		public TargetEntry(FileReference OutputFile, UEBuildTarget BuildTarget, bool bBuildByDefault)
		{
			this.OutputFile = OutputFile;
			this.BuildTarget = BuildTarget;
			this.bBuildByDefault = bBuildByDefault;
		}
		
		public readonly FileReference OutputFile;
		public readonly UEBuildTarget BuildTarget;
		public readonly bool bBuildByDefault;
	}

	internal class RiderProjectFile : ProjectFile
	{
		private static readonly XcrunRunner AppleHelper = new XcrunRunner();

		private readonly DirectoryReference RootPath;
		private readonly HashSet<TargetType> TargetTypes;
		private readonly CommandLineArguments Arguments;

		private ToolchainInfo RootToolchainInfo = new ToolchainInfo();
		private UEBuildTarget? CurrentTarget;

		public RiderProjectFile(FileReference InProjectFilePath, DirectoryReference BaseDir, 
			DirectoryReference RootPath, HashSet<TargetType> TargetTypes, CommandLineArguments Arguments)
			: base(InProjectFilePath, BaseDir)
		{
			this.RootPath = RootPath;
			this.TargetTypes = TargetTypes;
			this.Arguments = Arguments;
		}

		/// <summary>
		/// Write project file info in JSON file.
		/// For every combination of <c>UnrealTargetPlatform</c>, <c>UnrealTargetConfiguration</c> and <c>TargetType</c>
		/// will be generated separate JSON file.
		/// Project file will be stored:
		/// For UE:  {UnrealRoot}/Engine/Intermediate/ProjectFiles/.Rider/{Platform}/{Configuration}/{TargetType}/{ProjectName}.json
		/// For game: {GameRoot}/Intermediate/ProjectFiles/.Rider/{Platform}/{Configuration}/{TargetType}/{ProjectName}.json
		/// </summary>
		/// <remarks>
		/// * <c>TargetType.Editor</c> will be generated for current platform only and will ignore <c>UnrealTargetConfiguration.Test</c> and <c>UnrealTargetConfiguration.Shipping</c> configurations
		/// * <c>TargetType.Program</c>  will be generated for current platform only and <c>UnrealTargetConfiguration.Development</c> configuration only 
		/// </remarks>
		/// <param name="InPlatforms"></param>
		/// <param name="InConfigurations"></param>
		/// <param name="PlatformProjectGenerators"></param>
		/// <param name="Minimize"></param>
		/// <param name="Logger"></param>
		/// <returns></returns>
		public bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms,
			List<UnrealTargetConfiguration> InConfigurations,
			PlatformProjectGeneratorCollection PlatformProjectGenerators, JsonWriterStyle Minimize, ILogger Logger)
		{
			string ProjectName = ProjectFilePath.GetFileNameWithoutAnyExtensions();
			DirectoryReference ProjectRootFolder = RootPath;
			List<TargetEntry> FileToTarget = new List<TargetEntry>();
			foreach (UnrealTargetPlatform Platform in InPlatforms)
			{
				if (!IsPlatformInHostGroup(Platform))
				{
					continue;
				}
				
				foreach (UnrealTargetConfiguration Configuration in InConfigurations)
				{
					foreach (ProjectTarget ProjectTarget in ProjectTargets.OfType<ProjectTarget>())
					{
						if (TargetTypes.Any() && !TargetTypes.Contains(ProjectTarget.TargetRules!.Type)) continue;

						// Skip Programs for all configs except for current platform + Development & Debug configurations
						if (ProjectTarget.TargetRules!.Type == TargetType.Program &&
						    (BuildHostPlatform.Current.Platform != Platform ||
						     !(Configuration == UnrealTargetConfiguration.Development || Configuration == UnrealTargetConfiguration.Debug)))
						{
							continue;
						}

						// Skip Editor for all platforms except for current platform
						if (ProjectTarget.TargetRules.Type == TargetType.Editor && (BuildHostPlatform.Current.Platform != Platform || (Configuration == UnrealTargetConfiguration.Test || Configuration == UnrealTargetConfiguration.Shipping)))
						{
							continue;
						}
						
						bool bBuildByDefault = ShouldBuildByDefaultForSolutionTargets && ProjectTarget.SupportedPlatforms.Contains(Platform);
						
						DirectoryReference ConfigurationFolder = DirectoryReference.Combine(ProjectRootFolder, Platform.ToString(), Configuration.ToString());

						DirectoryReference TargetFolder =
							DirectoryReference.Combine(ConfigurationFolder, ProjectTarget.TargetRules.Type.ToString());

						string DefaultArchitecture = UEBuildPlatform
							.GetBuildPlatform(Platform)
							.GetDefaultArchitecture(ProjectTarget.UnrealProjectFilePath);
						TargetDescriptor TargetDesc = new TargetDescriptor(ProjectTarget.UnrealProjectFilePath, ProjectTarget.Name,
							Platform, Configuration, DefaultArchitecture, Arguments);
						try
						{
							UEBuildTarget BuildTarget = UEBuildTarget.Create(TargetDesc, false, false, false, Logger);
						
							FileReference OutputFile = FileReference.Combine(TargetFolder, $"{ProjectName}.json");
							FileToTarget.Add(new TargetEntry(OutputFile, BuildTarget, bBuildByDefault));
						}
						catch(Exception Ex)
						{
							Logger.LogWarning("Exception while generating include data for Target:{Target}, Platform: {Platform}, Configuration: {Configuration}", TargetDesc.Name, Platform.ToString(), Configuration.ToString());
							Logger.LogWarning("{Ex}", Ex.ToString());
						}
					}
				}
			}
			foreach (TargetEntry TargetEntry in FileToTarget)
			{
				try
				{
					CurrentTarget = TargetEntry.BuildTarget;
					CurrentTarget.PreBuildSetup(Logger);
					SerializeTarget(TargetEntry.OutputFile, CurrentTarget, PlatformProjectGenerators, Minimize, TargetEntry.bBuildByDefault, Logger);
				}
				catch (Exception Ex)
				{
					Logger.LogWarning("Exception while generating include data for Target:{Target}, Platform: {Platform}, Configuration: {Configuration}",
						TargetEntry.BuildTarget.AppName, TargetEntry.BuildTarget.Platform.ToString(), TargetEntry.BuildTarget.Configuration.ToString());
					Logger.LogWarning("{Ex}", Ex.ToString());
				}
			}
			
			return true;
		}

		private bool IsPlatformInHostGroup(UnrealTargetPlatform Platform)
		{
			var Groups = UEBuildPlatform.GetPlatformGroups(BuildHostPlatform.Current.Platform);
			foreach(UnrealPlatformGroup Group in Groups)
			{
				// Desktop includes Linux, Mac and Windows.
				if (UEBuildPlatform.IsPlatformInGroup(Platform, Group) && Group != UnrealPlatformGroup.Desktop) 
				{
					return true;
				}
			}
			
			return false;
		}

		private void SerializeTarget(FileReference OutputFile, UEBuildTarget BuildTarget, PlatformProjectGeneratorCollection PlatformProjectGenerators, JsonWriterStyle Minimize, bool bBuildByDefault, ILogger Logger)
		{
			DirectoryReference.CreateDirectory(OutputFile.Directory);
			using (JsonWriter Writer = new JsonWriter(OutputFile, Minimize))
			{
				ExportTarget(BuildTarget, Writer, PlatformProjectGenerators, bBuildByDefault, Logger);
			}
		}

		/// <summary>
		/// Write a Target to a JSON writer. Is array is empty, don't write anything
		/// </summary>
		/// <param name="Target"></param>
		/// <param name="Writer">Writer for the array data</param>
		/// <param name="PlatformProjectGenerators"></param>
		/// <param name="bBuildByDefault"></param>
		/// <param name="Logger">Logger for output</param>
		private void ExportTarget(UEBuildTarget Target, JsonWriter Writer, PlatformProjectGeneratorCollection PlatformProjectGenerators, bool bBuildByDefault, ILogger Logger)
		{
			Writer.WriteObjectStart();

			Writer.WriteValue("Name", Target.TargetName);
			Writer.WriteValue("Configuration", Target.Configuration.ToString());
			Writer.WriteValue("Platform", Target.Platform.ToString());
			Writer.WriteValue("TargetFile", Target.TargetRulesFile.FullName);
			if (Target.ProjectFile != null)
			{
				Writer.WriteValue("ProjectFile", Target.ProjectFile.FullName);
			}
			
			ExportEnvironmentToJson(Target, Writer, PlatformProjectGenerators, bBuildByDefault, Logger);
			
			if(Target.Binaries.Any())
			{
				Writer.WriteArrayStart("Binaries");
				foreach (UEBuildBinary Binary in Target.Binaries)
				{
					Writer.WriteObjectStart();
					ExportBinary(Binary, Writer);
					Writer.WriteObjectEnd();
				}
				Writer.WriteArrayEnd();
			}
			
			CppCompileEnvironment GlobalCompileEnvironment = Target.CreateCompileEnvironmentForProjectFiles(Logger);
			HashSet<string> ModuleNames = new HashSet<string>();
			Writer.WriteObjectStart("Modules");
			foreach (UEBuildBinary Binary in Target.Binaries)
			{
				CppCompileEnvironment BinaryCompileEnvironment = Binary.CreateBinaryCompileEnvironment(GlobalCompileEnvironment);
				foreach (UEBuildModule Module in Binary.Modules)
				{
					if(ModuleNames.Add(Module.Name))
					{
						Writer.WriteObjectStart(Module.Name);
						UEBuildModuleCPP? ModuleCpp = Module as UEBuildModuleCPP;
						if (ModuleCpp != null)
						{
							CppCompileEnvironment ModuleCompileEnvironment = ModuleCpp.CreateCompileEnvironmentForIntellisense(Target.Rules, BinaryCompileEnvironment, Logger);
							ExportModuleCpp(ModuleCpp, ModuleCompileEnvironment, Writer, Logger);
							Module.PrivateIncludePaths.UnionWith(ModuleCompileEnvironment.UserIncludePaths);
						}
						ExportModule(Module, Binary.OutputDir, Target.GetExecutableDir(), Writer, Logger);
						Writer.WriteObjectEnd();
					}
				}
			}
			Writer.WriteObjectEnd();
			
			ExportPluginsFromTarget(Target, Writer, Logger);
			
			Writer.WriteObjectEnd();
		}

		private void ExportModuleCpp(UEBuildModuleCPP ModuleCPP, CppCompileEnvironment ModuleCompileEnvironment, JsonWriter Writer, ILogger Logger)
		{
			Writer.WriteValue("GeneratedCodeDirectory", ModuleCPP.GeneratedCodeDirectory != null ? ModuleCPP.GeneratedCodeDirectory.FullName  : string.Empty);
			
			ToolchainInfo ModuleToolchainInfo = GenerateToolchainInfo(ModuleCompileEnvironment);
			if (!ModuleToolchainInfo.Equals(RootToolchainInfo))
			{
				Writer.WriteObjectStart("ToolchainInfo");
				foreach (Tuple<string,object?> Field in ModuleToolchainInfo.GetDiff(RootToolchainInfo))
				{
					WriteField(ModuleCPP.Name, Writer, Field, Logger);
				}
				Writer.WriteObjectEnd();
			}
			
			if (ModuleCompileEnvironment.PrecompiledHeaderIncludeFilename != null)
			{
				string CorrectFilePathPch;
				if(ExtractWrappedIncludeFile(ModuleCompileEnvironment.PrecompiledHeaderIncludeFilename, Logger, out CorrectFilePathPch))
					Writer.WriteValue("SharedPCHFilePath", CorrectFilePathPch);
			}
		}

		private static bool ExtractWrappedIncludeFile(FileSystemReference FileRef, ILogger Logger, out string CorrectFilePathPch)
		{
			CorrectFilePathPch = "";
			try
			{
				using (StreamReader Reader = new StreamReader(FileRef.FullName))
				{
					string? Line = Reader.ReadLine();
					if (Line != null)
					{
						CorrectFilePathPch = Line.Substring("// PCH for ".Length).Trim();
						return true;
					}
				}
			}
			finally
			{
				Logger.LogDebug("Couldn't extract path to PCH from {FileRef}", FileRef);
			}
			return false;
		}

		/// <summary>
		/// Write a Module to a JSON writer. If array is empty, don't write anything
		/// </summary>
		/// <param name="BinaryOutputDir"></param>
		/// <param name="TargetOutputDir"></param>
		/// <param name="Writer">Writer for the array data</param>
		/// <param name="Module"></param>
		/// <param name="Logger"></param>
		private static void ExportModule(UEBuildModule Module, DirectoryReference BinaryOutputDir, DirectoryReference TargetOutputDir, JsonWriter Writer, ILogger Logger)
		{
			Writer.WriteValue("Name", Module.Name);
			Writer.WriteValue("Directory", Module.ModuleDirectory.FullName );
			Writer.WriteValue("Rules", Module.RulesFile.FullName );
			Writer.WriteValue("PCHUsage", Module.Rules.PCHUsage.ToString());

			if (Module.Rules.PrivatePCHHeaderFile != null)
			{
				Writer.WriteValue("PrivatePCH", FileReference.Combine(Module.ModuleDirectory, Module.Rules.PrivatePCHHeaderFile).FullName);
			}

			if (Module.Rules.SharedPCHHeaderFile != null)
			{
				Writer.WriteValue("SharedPCH", FileReference.Combine(Module.ModuleDirectory, Module.Rules.SharedPCHHeaderFile).FullName);
			}

			ExportJsonModuleArray(Writer, "PublicDependencyModules", Module.PublicDependencyModules);
			ExportJsonModuleArray(Writer, "PublicIncludePathModules", Module.PublicIncludePathModules);
			ExportJsonModuleArray(Writer, "PrivateDependencyModules", Module.PrivateDependencyModules);
			ExportJsonModuleArray(Writer, "PrivateIncludePathModules", Module.PrivateIncludePathModules);
			ExportJsonModuleArray(Writer, "DynamicallyLoadedModules", Module.DynamicallyLoadedModules);

			ExportJsonStringArray(Writer, "PublicSystemIncludePaths", Module.PublicSystemIncludePaths.Select(x => x.FullName ));
			ExportJsonStringArray(Writer, "PublicIncludePaths", Module.PublicIncludePaths.Select(x => x.FullName ));
			ExportJsonStringArray(Writer, "InternalIncludePaths", Module.InternalIncludePaths.Select(x => x.FullName));

			ExportJsonStringArray(Writer, "LegacyPublicIncludePaths", Module.LegacyPublicIncludePaths.Select(x => x.FullName ));
			
			ExportJsonStringArray(Writer, "PrivateIncludePaths", Module.PrivateIncludePaths.Select(x => x.FullName));
			ExportJsonStringArray(Writer, "PublicLibraryPaths", Module.PublicSystemLibraryPaths.Select(x => x.FullName));
			ExportJsonStringArray(Writer, "PublicAdditionalLibraries", Module.PublicSystemLibraries.Concat(Module.PublicLibraries.Select(x => x.FullName)));
			ExportJsonStringArray(Writer, "PublicFrameworks", Module.PublicFrameworks);
			ExportJsonStringArray(Writer, "PublicWeakFrameworks", Module.PublicWeakFrameworks);
			ExportJsonStringArray(Writer, "PublicDelayLoadDLLs", Module.PublicDelayLoadDLLs);
			ExportJsonStringArray(Writer, "PublicDefinitions", Module.PublicDefinitions);
			ExportJsonStringArray(Writer, "PrivateDefinitions", Module.Rules.PrivateDefinitions);
			ExportJsonStringArray(Writer, "ProjectDefinitions", /* TODO: Add method ShouldAddProjectDefinitions */ !Module.Rules.bTreatAsEngineModule ? Module.Rules.Target.ProjectDefinitions : new string[0]);
			ExportJsonStringArray(Writer, "ApiDefinitions", Module.GetEmptyApiMacros());
			Writer.WriteValue("ShouldAddLegacyPublicIncludePaths", Module.Rules.bLegacyPublicIncludePaths);

			if(Module.Rules.CircularlyReferencedDependentModules.Any())
			{
				Writer.WriteArrayStart("CircularlyReferencedModules");
				foreach (string ModuleName in Module.Rules.CircularlyReferencedDependentModules)
				{
					Writer.WriteValue(ModuleName);
				}
				Writer.WriteArrayEnd();
			}
			
			if(Module.Rules.RuntimeDependencies.Inner.Any())
			{
				// We don't use info from RuntimeDependencies for code analyzes (at the moment)
				// So we're OK with skipping some values if they are not presented
				Writer.WriteArrayStart("RuntimeDependencies");
				foreach (ModuleRules.RuntimeDependency RuntimeDependency in Module.Rules.RuntimeDependencies.Inner)
				{
					Writer.WriteObjectStart();

					try
					{
						Writer.WriteValue("Path",
							Module.ExpandPathVariables(RuntimeDependency.Path, BinaryOutputDir, TargetOutputDir));
					}
					catch(BuildException buildException)
					{
						Logger.LogDebug("Value {Value} for module {ModuleName} will not be stored. Reason: {Ex}", "Path", Module.Name, buildException);	
					}
					
					if (RuntimeDependency.SourcePath != null)
					{
						try
						{
							Writer.WriteValue("SourcePath",
								Module.ExpandPathVariables(RuntimeDependency.SourcePath, BinaryOutputDir,
									TargetOutputDir));
						}
						catch(BuildException buildException)
						{
							Logger.LogDebug("Value {Value} for module {ModuleName} will not be stored. Reason: {Ex}", "SourcePath", Module.Name, buildException);	
						}
					}

					Writer.WriteValue("Type", RuntimeDependency.Type.ToString());
					
					Writer.WriteObjectEnd();
				}
				Writer.WriteArrayEnd();
			}
		}
		
		/// <summary>
		/// Write an array of Modules to a JSON writer. If array is empty, don't write anything
		/// </summary>
		/// <param name="Writer">Writer for the array data</param>
		/// <param name="ArrayName">Name of the array property</param>
		/// <param name="Modules">Sequence of Modules to write. May be null.</param>
		private static void ExportJsonModuleArray(JsonWriter Writer, string ArrayName, IEnumerable<UEBuildModule>? Modules)
		{
			if (Modules == null || !Modules.Any()) return;
			
			Writer.WriteArrayStart(ArrayName);
			foreach (UEBuildModule Module in Modules)
			{
				Writer.WriteValue(Module.Name);
			}
			Writer.WriteArrayEnd();
		}
		
		/// <summary>
		/// Write an array of strings to a JSON writer. Ifl array is empty, don't write anything
		/// </summary>
		/// <param name="Writer">Writer for the array data</param>
		/// <param name="ArrayName">Name of the array property</param>
		/// <param name="Strings">Sequence of strings to write. May be null.</param>
		private static void ExportJsonStringArray(JsonWriter Writer, string ArrayName, IEnumerable<string> Strings)
		{
			if (Strings == null || !Strings.Any()) return;
			
			Writer.WriteArrayStart(ArrayName);
			foreach (string String in Strings)
			{
				Writer.WriteValue(String);
			}
			Writer.WriteArrayEnd();
		}
		
		/// <summary>
		/// Write uplugin content to a JSON writer
		/// </summary>
		/// <param name="Plugin">Uplugin description</param>
		/// <param name="Writer">JSON writer</param>
		private static void ExportPlugin(UEBuildPlugin Plugin, JsonWriter Writer)
		{
			Writer.WriteObjectStart(Plugin.Name);
			
			Writer.WriteValue("File", Plugin.File.FullName );
			Writer.WriteValue("Type", Plugin.Type.ToString());
			if(Plugin.Dependencies != null && Plugin.Dependencies.Any())
			{
				Writer.WriteStringArrayField("Dependencies", Plugin.Dependencies.Select(it => it.Name));
			}
			if(Plugin.Modules.Any())
			{
				Writer.WriteStringArrayField("Modules", Plugin.Modules.Select(it => it.Name));
			}
			
			Writer.WriteObjectEnd();
		}
		
		/// <summary>
		/// Setup plugins for Target and write plugins to JSON writer. Don't write anything if there are no plugins 
		/// </summary>
		/// <param name="Target"></param>
		/// <param name="Writer"></param>
		/// <param name="Logger"></param>
		private static void ExportPluginsFromTarget(UEBuildTarget Target, JsonWriter Writer, ILogger Logger)
		{
			Target.SetupPlugins(Logger);
			if (Target.BuildPlugins == null || !Target.BuildPlugins.Any()) return;
			
			Writer.WriteObjectStart("Plugins");
			foreach (UEBuildPlugin plugin in Target.BuildPlugins!)
			{
				ExportPlugin(plugin, Writer);
			}
			Writer.WriteObjectEnd();
		}

		/// <summary>
		/// Write information about this binary to a JSON file
		/// </summary>
		/// <param name="Binary"></param>
		/// <param name="Writer">Writer for this binary's data</param>
		private static void ExportBinary(UEBuildBinary Binary, JsonWriter Writer)
		{
			Writer.WriteValue("File", Binary.OutputFilePath.FullName );
			Writer.WriteValue("Type", Binary.Type.ToString());

			Writer.WriteArrayStart("Modules");
			foreach(UEBuildModule Module in Binary.Modules)
			{
				Writer.WriteValue(Module.Name);
			}
			Writer.WriteArrayEnd();
		}
		
		/// <summary>
		/// Write C++ toolchain information to JSON writer
		/// </summary>
		/// <param name="Target"></param>
		/// <param name="Writer"></param>
		/// <param name="PlatformProjectGenerators"></param>
		/// <param name="bBuildByDefault"></param>
		/// <param name="Logger"></param>
		private void ExportEnvironmentToJson(UEBuildTarget Target, JsonWriter Writer, PlatformProjectGeneratorCollection PlatformProjectGenerators, bool bBuildByDefault, ILogger Logger)
		{
			CppCompileEnvironment GlobalCompileEnvironment = Target.CreateCompileEnvironmentForProjectFiles(Logger);
			
			RootToolchainInfo = GenerateToolchainInfo(GlobalCompileEnvironment);
			
			Writer.WriteObjectStart("ToolchainInfo");
			foreach (Tuple<string, object?> Field in RootToolchainInfo.GetFields())
			{
				WriteField(Target.TargetName, Writer, Field, Logger);
			}
			Writer.WriteObjectEnd();
			
			ExportBuildInfo(Writer, Target, PlatformProjectGenerators, bBuildByDefault);
			
			Writer.WriteArrayStart("EnvironmentIncludePaths");
			foreach (DirectoryReference Path in GlobalCompileEnvironment.UserIncludePaths)
			{
				Writer.WriteValue(Path.FullName );
			}
			foreach (DirectoryReference Path in GlobalCompileEnvironment.SystemIncludePaths)
			{
				Writer.WriteValue(Path.FullName );
			}
			
			if (UEBuildPlatform.IsPlatformInGroup(Target.Platform, UnrealPlatformGroup.Windows))
			{
				foreach (DirectoryReference Path in Target.Rules.WindowsPlatform.Environment!.IncludePaths)
				{
					Writer.WriteValue(Path.FullName);
				}
			}
			else if (UEBuildPlatform.IsPlatformInGroup(Target.Platform, UnrealPlatformGroup.Apple) &&
			         UEBuildPlatform.IsPlatformInGroup(BuildHostPlatform.Current.Platform, UnrealPlatformGroup.Apple))
			{
				// Only generate Apple system include paths when host platform is Apple OS
				// TODO: Fix case when working with MacOS on Windows host platform  
				foreach (string Path in AppleHelper.GetAppleSystemIncludePaths(GlobalCompileEnvironment.Architecture, Target.Platform, Logger))
				{
					Writer.WriteValue(Path);
				}
			}
			else if(UEBuildPlatform.IsPlatformInGroup(Target.Platform, UnrealPlatformGroup.Linux) ||
			        UEBuildPlatform.IsPlatformInGroup(Target.Platform, UnrealPlatformGroup.Unix))
			{
				var EngineDirectory = Unreal.EngineDirectory.ToString();

				string? UseLibcxxEnvVarOverride = Environment.GetEnvironmentVariable("UE_LINUX_USE_LIBCXX");
				if (string.IsNullOrEmpty(UseLibcxxEnvVarOverride) || UseLibcxxEnvVarOverride == "1")
				{
					if (Target.Architecture.StartsWith("x86_64") ||
					    Target.Architecture.StartsWith("aarch64"))
					{
						// libc++ include directories
						Writer.WriteValue(Path.Combine(EngineDirectory, "Source/ThirdParty/Unix/LibCxx/include/"));
						Writer.WriteValue(Path.Combine(EngineDirectory, "Source/ThirdParty/Unix/LibCxx/include/c++/v1"));
					}
				}

				UEBuildPlatform BuildPlatform;

				if (Target.Architecture.StartsWith("x86_64"))
				{
					BuildPlatform = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Linux);
				}
				else if (Target.Architecture.StartsWith("aarch64"))
				{
					BuildPlatform = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.LinuxArm64);
				}
				else
				{
					throw new ArgumentException("Wrong Target.Architecture: {0}", Target.Architecture);
				}

				string PlatformSdkVersionString = UEBuildPlatformSDK.GetSDKForPlatform(BuildPlatform.GetPlatformName())!.GetInstalledVersion()!;
				var Version = GetLinuxToolchainVersionFromFullString(PlatformSdkVersionString);

				string? InternalSdkPath = UEBuildPlatform.GetSDK(UnrealTargetPlatform.Linux)!.GetInternalSDKPath();
				if (InternalSdkPath != null)
				{
					Writer.WriteValue(Path.Combine(InternalSdkPath, "include"));
					Writer.WriteValue(Path.Combine(InternalSdkPath, "usr/include"));
					Writer.WriteValue(Path.Combine(InternalSdkPath, "lib/clang/" + Version + "/include/"));
				}
			}
			
			Writer.WriteArrayEnd();
	
			Writer.WriteArrayStart("EnvironmentDefinitions");
			foreach (string Definition in GlobalCompileEnvironment.Definitions)
			{
				Writer.WriteValue(Definition);
			}
			Writer.WriteArrayEnd();
		}

		private void ExportBuildInfo(JsonWriter Writer, UEBuildTarget Target, PlatformProjectGeneratorCollection PlatformProjectGenerators, bool bBuildByDefault)
		{
			if (IsStubProject) return;
			
			Writer.WriteObjectStart("BuildInfo");
			UnrealTargetPlatform HostPlatform = BuildHostPlatform.Current.Platform;
			
			ProjectTarget ProjectTarget = ProjectTargets.OfType<ProjectTarget>().Single(It => Target.TargetRulesFile == It.TargetFilePath);
			UnrealTargetPlatform Platform = Target.Platform;
			UnrealTargetConfiguration Configuration = Target.Configuration;

			string UProjectPath = "";
			if (IsForeignProject)
			{
				UProjectPath = String.Format("\"{0}\"", ProjectTarget.UnrealProjectFilePath!.FullName);
			}
			
			Writer.WriteValue("bBuildByDefault", bBuildByDefault);
			
			if (HostPlatform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				PlatformProjectGenerator? ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(Platform, true);
				VCProjectFile.BuildCommandBuilder BuildCommandBuilder = new VCProjectFile.BuildCommandBuilder(Configuration, Platform, ProjectTarget, UProjectPath)
				{
					ProjectGenerator = ProjGenerator,
					bIsForeignProject = IsForeignProject
				};
				
				string BuildArguments = BuildCommandBuilder.GetBuildArguments();
				WriteCommand(Writer, "BuildCmd", EscapePath(BuildCommandBuilder.BuildScript.FullName), BuildArguments);
				WriteCommand(Writer, "RebuildCmd", EscapePath(BuildCommandBuilder.RebuildScript.FullName), BuildArguments);
				WriteCommand(Writer, "CleanCmd", EscapePath(BuildCommandBuilder.CleanScript.FullName), BuildArguments);
			}
			else
			{
				string BuildScript = GetBuildScript(HostPlatform);
				string BuildArguments = GetBuildArguments(HostPlatform, ProjectTarget, Target, UProjectPath, false);
				string CleanArguments = GetBuildArguments(HostPlatform, ProjectTarget, Target, UProjectPath, true);
				WriteCommand(Writer, "BuildCmd", BuildScript, BuildArguments);
				WriteCommand(Writer, "RebuildCmd", "", "");
				WriteCommand(Writer, "CleanCmd", BuildScript, CleanArguments);
			}
			
			UEBuildBinary MainBinary = Target.Binaries[0];
			Writer.WriteValue("Output", MainBinary.OutputFilePath.FullName);
			Writer.WriteObjectEnd();
		}

		private string GetBuildScript(UnrealTargetPlatform HostPlatform)
		{
			DirectoryReference BatchFilesDirectory = DirectoryReference.Combine(Unreal.EngineDirectory, "Build", "BatchFiles");
			string ScriptExtension = HostPlatform.IsInGroup(UnrealPlatformGroup.Windows) ? ".bat" : ".sh";
			if (HostPlatform.IsInGroup(UnrealPlatformGroup.Linux))
			{
				BatchFilesDirectory = DirectoryReference.Combine(BatchFilesDirectory, "Linux");
			}
			else if (HostPlatform == UnrealTargetPlatform.Mac)
			{
				BatchFilesDirectory = DirectoryReference.Combine(BatchFilesDirectory, "Mac");
			}
			
			return EscapePath(FileReference.Combine(BatchFilesDirectory, "Build" + ScriptExtension).FullName);
		}

		private string GetBuildArguments(UnrealTargetPlatform HostPlatform, ProjectTarget ProjectTarget, UEBuildTarget Target, string UProjectPath, bool bIsClean)
		{
			UnrealTargetConfiguration Configuration = Target.Configuration;
			UnrealTargetPlatform Platform = Target.Platform;
			string TargetName = ProjectTarget.TargetFilePath.GetFileNameWithoutAnyExtensions();
			StringBuilder BuildArguments = new StringBuilder();
			BuildArguments.AppendFormat("{0} {1} {2}", TargetName, Platform.ToString(), Configuration.ToString());

			if (IsForeignProject)
			{
				BuildArguments.AppendFormat(" -Project={0}", UProjectPath);
			}

			if (Target.TargetType == TargetType.Editor)
			{
				BuildArguments.Append(" -buildscw");
			}

			if (bIsClean)
			{
				BuildArguments.Append(" -clean");
			}
			else if (HostPlatform.IsInGroup(UnrealPlatformGroup.Apple) &&
			         (Platform == UnrealTargetPlatform.TVOS || Platform == UnrealTargetPlatform.IOS))
			{
				BuildArguments.Append(" -deploy");
			}

			return BuildArguments.ToString();
		}

		private static void WriteCommand(JsonWriter Writer, string CommandName, string Command, string Arguments)
		{
			Writer.WriteObjectStart(CommandName);
			Writer.WriteValue("Command", Command);
			Writer.WriteValue("Args", Arguments);
			Writer.WriteObjectEnd();
		}

		private static void WriteField(string ModuleOrTargetName, JsonWriter Writer, Tuple<string, object?> Field, ILogger Logger)
		{
			if (Field.Item2 == null) return;
			string Name = Field.Item1;
			if (Field.Item2 is bool)
			{
				Writer.WriteValue(Name, (bool) Field.Item2);
			}
			else if (Field.Item2 is string)
			{
				string FieldValue = (string) Field.Item2;
				if(FieldValue != "")
					Writer.WriteValue(Name, (string) Field.Item2);
			}
			else if (Field.Item2 is int)
			{
				Writer.WriteValue(Name, (int) Field.Item2);
			}
			else if (Field.Item2 is double)
			{
				Writer.WriteValue(Name, (double) Field.Item2);
			}
			else if (Field.Item2 is CppStandardVersion version)
			{
				// Do not use version.ToString(). See: https://youtrack.jetbrains.com/issue/RIDER-68030
				switch (version)
				{
					case CppStandardVersion.Cpp14:
						Writer.WriteValue(Name, "Cpp14");
						break;
					case CppStandardVersion.Cpp17:
						Writer.WriteValue(Name, "Cpp17");
						break;
					case CppStandardVersion.Cpp20:
						Writer.WriteValue(Name, "Cpp20");
						break;
					case CppStandardVersion.Latest:
						Writer.WriteValue(Name, "Latest");
						break;
					default:
						Logger.LogError("Unsupported C++ standard type: {Type}", version);
						break;
				}
			}
			else if (Field.Item2 is Enum)
			{
				Writer.WriteValue(Name, Field.Item2.ToString());
			}
			else if (Field.Item2 is IEnumerable<string>)
			{
				IEnumerable<string> FieldValue = (IEnumerable<string>)Field.Item2;
				if(FieldValue.Any())
					Writer.WriteStringArrayField(Name, FieldValue);
			}
			else
			{
				Logger.LogWarning("Dumping incompatible ToolchainInfo field: {Name} with type: {Field} for: {ModuleOrTarget}",
					Name, Field.Item2, ModuleOrTargetName);
			}
		}

		private ToolchainInfo GenerateToolchainInfo(CppCompileEnvironment CompileEnvironment)
		{
			ToolchainInfo ToolchainInfo = new ToolchainInfo
			{
				CppStandard = CompileEnvironment.CppStandard,
				Configuration = CompileEnvironment.Configuration.ToString(),
				bEnableExceptions = CompileEnvironment.bEnableExceptions,
				bOptimizeCode = CompileEnvironment.bOptimizeCode,
				bUseInlining = CompileEnvironment.bUseInlining,
				bUseUnity = CompileEnvironment.bUseUnity,
				bCreateDebugInfo = CompileEnvironment.bCreateDebugInfo,
				bIsBuildingLibrary = CompileEnvironment.bIsBuildingLibrary,
				bUseAVX = CompileEnvironment.bUseAVX,
				bIsBuildingDLL = CompileEnvironment.bIsBuildingDLL,
				bUseDebugCRT = CompileEnvironment.bUseDebugCRT,
				bUseRTTI = CompileEnvironment.bUseRTTI,
				bUseStaticCRT = CompileEnvironment.bUseStaticCRT,
				PrecompiledHeaderAction = CompileEnvironment.PrecompiledHeaderAction.ToString(),
				PrecompiledHeaderFile = CompileEnvironment.PrecompiledHeaderFile?.ToString(),
				ForceIncludeFiles = CompileEnvironment.ForceIncludeFiles.Select(Item => Item.ToString()).ToList()
			};

			if (CurrentTarget!.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				ToolchainInfo.Architecture = WindowsExports.GetArchitectureSubpath(CurrentTarget.Rules.WindowsPlatform.Architecture);
				
				WindowsCompiler WindowsPlatformCompiler = CurrentTarget.Rules.WindowsPlatform.Compiler;
				ToolchainInfo.bStrictConformanceMode = WindowsPlatformCompiler.IsMSVC() && CurrentTarget.Rules.WindowsPlatform.bStrictConformanceMode;
				ToolchainInfo.Compiler = WindowsPlatformCompiler.ToString();
			}
			else
			{
				string PlatformName = $"{CurrentTarget.Platform}Platform";
				object? Value = typeof(ReadOnlyTargetRules).GetProperty(PlatformName)?.GetValue(CurrentTarget.Rules);
				object? CompilerField = Value?.GetType().GetProperty("Compiler")?.GetValue(Value);
				if (CompilerField != null)
					ToolchainInfo.Compiler = CompilerField.ToString()!;
			}
				
			return ToolchainInfo; 
		}

		/// <summary>
		/// Get clang toolchain version from full version string
		/// v17_clang-10.0.1-centos7 -> 10.0.1
		/// </summary>
		/// <param name="FullVersion">Full clang toolchain version string. example: "v17_clang-10.0.1-centos7"</param>
		/// <returns>clang toolchain version. example: 10.0.1</returns>
		private string GetLinuxToolchainVersionFromFullString(string FullVersion)
		{
			string FullVersionPattern = @"^v[0-9]+_.*-([0-9]+\.[0-9]+\.[0-9]+)-.*$";
			Regex Regex = new Regex(FullVersionPattern);
			Match m = Regex.Match(FullVersion);
			if (!m.Success)
			{
				throw new ArgumentException("Wrong full version string: {0}", FullVersion);
			}

			Group g = m.Groups[1]; // first and the last capture group 
			CaptureCollection c = g.Captures;
			if (c.Count != 1)
			{
				throw new ArgumentException("Multiple regex capture in full version string: {0}", FullVersion);
			}

			return c[0].Value;
		}

		private class XcrunRunner
		{
			private readonly Dictionary<string, IList<string>> CachedIncludePaths =
				new Dictionary<string, IList<string>>();

			private string CurrentlyProcessedSDK = string.Empty;
			private Process? XcrunProcess;
			private bool IsReadingIncludesSection;

			public IList<string> GetAppleSystemIncludePaths(string Architecture, UnrealTargetPlatform Platform, ILogger Logger)
			{
				if (!UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Apple))
				{
					throw new InvalidOperationException("xcrun can be run only for Apple's platforms");
				}

				string SDKPath = GetSDKPath(Architecture, Platform, Logger);
				if (!CachedIncludePaths.ContainsKey(SDKPath))
				{
					CalculateSystemIncludePaths(SDKPath);
				}

				return CachedIncludePaths[SDKPath];
			}

			private void CalculateSystemIncludePaths(string SDKPath)
			{
				if (CurrentlyProcessedSDK != string.Empty)
				{
					throw new InvalidOperationException("Cannot calculate include paths for several platforms at once");
				}

				CurrentlyProcessedSDK = SDKPath;
				CachedIncludePaths[SDKPath] = new List<string>();
				using (XcrunProcess = new Process())
				{
					string AppName = "xcrun";
					string Arguments = "clang++ -Wp,-v -x c++ - -fsyntax-only" +
					                   (string.IsNullOrEmpty(SDKPath) ? string.Empty : (" -isysroot " + SDKPath));
					XcrunProcess.StartInfo.FileName = AppName;
					XcrunProcess.StartInfo.Arguments = Arguments;
					XcrunProcess.StartInfo.UseShellExecute = false;
					XcrunProcess.StartInfo.CreateNoWindow = true;
					// For some weird reason output of this command is written to error channel so we're redirecting both channels
					XcrunProcess.StartInfo.RedirectStandardOutput = true;
					XcrunProcess.StartInfo.RedirectStandardError = true;
					XcrunProcess.OutputDataReceived += OnOutputDataReceived;
					XcrunProcess.ErrorDataReceived += OnOutputDataReceived;
					XcrunProcess.Start();
					XcrunProcess.BeginOutputReadLine();
					XcrunProcess.BeginErrorReadLine();
					// xcrun is not finished on it's own. It should be killed by OnOutputDataReceived when reading is finished. But we'll add timeout as a safeguard
					XcrunProcess.WaitForExit(3000);
				}

				XcrunProcess = null;
				IsReadingIncludesSection = false;
				CurrentlyProcessedSDK = string.Empty;
			}

			private void OnOutputDataReceived(object Sender, DataReceivedEventArgs Args)
			{
				if (Args.Data != null)
				{
					if (IsReadingIncludesSection)
					{
						if (Args.Data.StartsWith("End of search"))
						{
							IsReadingIncludesSection = false;
							XcrunProcess!.Kill();
						}
						else
						{
							if (!Args.Data.EndsWith("(framework directory)"))
							{
								CachedIncludePaths[CurrentlyProcessedSDK].Add(Args.Data.Trim(' ', '"'));
							}
						}
					}

					if (Args.Data.StartsWith("#include <...>"))
					{
						IsReadingIncludesSection = true;
					}
				}
			}

			private string GetSDKPath(string Architecture, UnrealTargetPlatform Platform, ILogger Logger)
			{
				if (Platform == UnrealTargetPlatform.Mac)
				{
					return MacToolChain.SDKPath;
				}

				if (Platform == UnrealTargetPlatform.IOS)
				{
					return new IOSToolChainSettings(Logger).GetSDKPath(Architecture);
				}

				if (Platform == UnrealTargetPlatform.TVOS)
				{
					return new TVOSToolChainSettings(Logger).GetSDKPath(Architecture);
				}

				throw new NotImplementedException("Path to SDK has to be specified for each Apple's platform");
			}
		}
	}
}

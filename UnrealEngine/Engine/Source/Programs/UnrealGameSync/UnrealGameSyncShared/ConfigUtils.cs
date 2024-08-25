// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;

namespace UnrealGameSync
{
	public class TargetReceipt
	{
		public string? Configuration { get; set; }
		public string? Launch { get; set; }
		public string? LaunchCmd { get; set; }

		public static bool TryRead(FileReference location, DirectoryReference? engineDir, DirectoryReference? projectDir, [NotNullWhen(true)] out TargetReceipt? receipt)
		{
			if (Utility.TryLoadJson(location, out receipt))
			{
				receipt.Launch = ExpandReceiptVariables(receipt.Launch, engineDir, projectDir);
				receipt.LaunchCmd = ExpandReceiptVariables(receipt.LaunchCmd, engineDir, projectDir);
				return true;
			}
			return false;
		}

		[return: NotNullIfNotNull("line")]
		private static string? ExpandReceiptVariables(string? line, DirectoryReference? engineDir, DirectoryReference? projectDir)
		{
			string? expandedLine = line;
			if (expandedLine != null)
			{
				if (engineDir != null)
				{
					expandedLine = expandedLine.Replace("$(EngineDir)", engineDir.FullName, StringComparison.OrdinalIgnoreCase);
				}
				if (projectDir != null)
				{
					expandedLine = expandedLine.Replace("$(ProjectDir)", projectDir.FullName, StringComparison.OrdinalIgnoreCase);
				}
			}
			return expandedLine;
		}
	}

	public static class ConfigUtils
	{
		public static string HostPlatform { get; } = GetHostPlatform();

		public static string HostArchitectureSuffix { get; } = String.Empty;

		static string GetHostPlatform()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return "Win64";
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				return "Mac";
			}
			else
			{
				return "Linux";
			}
		}

		public static Task<ConfigFile> ReadProjectConfigFileAsync(IPerforceConnection perforce, ProjectInfo projectInfo, ILogger logger, CancellationToken cancellationToken)
		{
			return ReadProjectConfigFileAsync(perforce, projectInfo, new List<KeyValuePair<FileReference, DateTime>>(), logger, cancellationToken);
		}

		public static Task<ConfigFile> ReadProjectConfigFileAsync(IPerforceConnection perforce, ProjectInfo projectInfo, List<KeyValuePair<FileReference, DateTime>> localConfigFiles, ILogger logger, CancellationToken cancellationToken)
		{
			return ReadProjectConfigFileAsync(perforce, projectInfo.ClientRootPath, projectInfo.ClientFileName, projectInfo.CacheFolder, localConfigFiles, logger, cancellationToken);
		}

		public static async Task<ConfigFile> ReadProjectConfigFileAsync(IPerforceConnection perforce, string branchClientPath, string selectedClientFileName, DirectoryReference cacheFolder, List<KeyValuePair<FileReference, DateTime>> localConfigFiles, ILogger logger, CancellationToken cancellationToken)
		{
			List<string> configFilePaths = Utility.GetDepotConfigPaths(branchClientPath + "/Engine", selectedClientFileName);

			ConfigFile projectConfig = new ConfigFile();

			List<PerforceResponse<FStatRecord>> responses = await perforce.TryFStatAsync(FStatOptions.IncludeFileSizes, configFilePaths, cancellationToken).ToListAsync(cancellationToken);
			foreach (PerforceResponse<FStatRecord> response in responses)
			{
				if (response.Succeeded)
				{
					string[]? lines = null;

					// Skip file records which are still in the workspace, but were synced from a different branch. For these files, the action seems to be empty, so filter against that.
					FStatRecord fileRecord = response.Data;
					if (fileRecord.HeadAction == FileAction.None)
					{
						continue;
					}

					// If this file is open for edit, read the local version
					string? localFileName = fileRecord.ClientFile;
					if (localFileName != null && File.Exists(localFileName) && (File.GetAttributes(localFileName) & FileAttributes.ReadOnly) == 0)
					{
						try
						{
							DateTime lastModifiedTime = File.GetLastWriteTimeUtc(localFileName);
							localConfigFiles.Add(new KeyValuePair<FileReference, DateTime>(new FileReference(localFileName), lastModifiedTime));
							lines = await File.ReadAllLinesAsync(localFileName, cancellationToken);
						}
						catch (Exception ex)
						{
							logger.LogInformation(ex, "Failed to read local config file for {Path}", localFileName);
						}
					}

					// Otherwise try to get it from perforce
					if (lines == null && fileRecord.DepotFile != null)
					{
						lines = await Utility.TryPrintFileUsingCacheAsync(perforce, fileRecord.DepotFile, cacheFolder, fileRecord.Digest, logger, cancellationToken);
					}

					// Merge the text with the config file
					if (lines != null)
					{
						try
						{
							projectConfig.Parse(lines.ToArray());
							logger.LogDebug("Read config file from {DepotFile}", fileRecord.DepotFile);
						}
						catch (Exception ex)
						{
							logger.LogInformation(ex, "Failed to read config file from {DepotFile}", fileRecord.DepotFile);
						}
					}
				}
			}
			return projectConfig;
		}

		public static async Task<List<string[]>> ReadConfigFiles(IPerforceConnection perforce, IEnumerable<string> depotPaths, List<KeyValuePair<FileReference, DateTime>> localFiles, DirectoryReference cacheFolder, ILogger logger, CancellationToken cancellationToken)
		{
			List<string[]> contents = new List<string[]>();

			List<PerforceResponse<FStatRecord>> responses = await perforce.TryFStatAsync(FStatOptions.IncludeFileSizes, depotPaths.ToArray(), cancellationToken).ToListAsync(cancellationToken);
			foreach (PerforceResponse<FStatRecord> response in responses)
			{
				if (response.Succeeded)
				{
					string[]? lines = null;

					// Skip file records which are still in the workspace, but were synced from a different branch. For these files, the action seems to be empty, so filter against that.
					FStatRecord fileRecord = response.Data;
					if (fileRecord.HeadAction == FileAction.None)
					{
						continue;
					}

					// If this file is open for edit, read the local version
					string? localFileName = fileRecord.ClientFile;
					if (localFileName != null && File.Exists(localFileName) && (File.GetAttributes(localFileName) & FileAttributes.ReadOnly) == 0)
					{
						try
						{
							DateTime lastModifiedTime = File.GetLastWriteTimeUtc(localFileName);
							localFiles.Add(new KeyValuePair<FileReference, DateTime>(new FileReference(localFileName), lastModifiedTime));
							lines = await File.ReadAllLinesAsync(localFileName, cancellationToken);
						}
						catch (Exception ex)
						{
							logger.LogInformation(ex, "Failed to read local config file for {Path}", localFileName);
						}
					}

					// Otherwise try to get it from perforce
					if (lines == null && fileRecord.DepotFile != null)
					{
						lines = await Utility.TryPrintFileUsingCacheAsync(perforce, fileRecord.DepotFile, cacheFolder, fileRecord.Digest, logger, cancellationToken);
					}

					// Merge the text with the config file
					if (lines != null)
					{
						contents.Add(lines);
					}
				}
			}

			return contents;
		}

		public static FileReference GetEditorTargetFile(ProjectInfo projectInfo, ConfigFile projectConfig)
		{
			if (projectInfo.ProjectPath.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase))
			{
				List<FileReference> targetFiles = FindTargets(projectInfo.LocalFileName.Directory);

				FileReference? targetFile = targetFiles.OrderBy(x => x.FullName, StringComparer.OrdinalIgnoreCase).FirstOrDefault(x => x.FullName.EndsWith("Editor.target.cs", StringComparison.OrdinalIgnoreCase));
				if (targetFile != null)
				{
					return targetFile;
				}
			}

			string defaultEditorTargetName = GetDefaultEditorTargetName(projectInfo, projectConfig);
			return FileReference.Combine(projectInfo.LocalRootPath, "Engine", "Source", $"{defaultEditorTargetName}.Target.cs");
		}

		public static FileReference GetEditorReceiptFile(ProjectInfo projectInfo, ConfigFile projectConfig, BuildConfig config)
		{
			FileReference targetFile = GetEditorTargetFile(projectInfo, projectConfig);
			return GetReceiptFile(projectInfo, projectConfig, targetFile, config.ToString());
		}

		private static List<FileReference> FindTargets(DirectoryReference engineOrProjectDir)
		{
			List<FileReference> targets = new List<FileReference>();

			DirectoryReference sourceDir = DirectoryReference.Combine(engineOrProjectDir, "Source");
			if (DirectoryReference.Exists(sourceDir))
			{
				foreach (FileReference targetFile in DirectoryReference.EnumerateFiles(sourceDir))
				{
					const string extension = ".target.cs";
					if (targetFile.FullName.EndsWith(extension, StringComparison.OrdinalIgnoreCase))
					{
						targets.Add(targetFile);
					}
				}
			}

			return targets;
		}

		public static string GetDefaultEditorTargetName(ProjectInfo projectInfo, ConfigFile projectConfigFile)
		{
			string? editorTarget;
			if (!TryGetProjectSetting(projectConfigFile, projectInfo.ProjectIdentifier, "EditorTarget", out editorTarget))
			{
				if (projectInfo.IsEnterpriseProject)
				{
					editorTarget = "StudioEditor";
				}
				else
				{
					editorTarget = "UE4Editor";
				}
			}
			return editorTarget;
		}

		public static bool TryReadEditorReceipt(ProjectInfo projectInfo, FileReference receiptFile, [NotNullWhen(true)] out TargetReceipt? receipt)
		{
			DirectoryReference engineDir = DirectoryReference.Combine(projectInfo.LocalRootPath, "Engine");
			DirectoryReference projectDir = projectInfo.LocalFileName.Directory;

			if (receiptFile.IsUnderDirectory(projectDir))
			{
				return TargetReceipt.TryRead(receiptFile, engineDir, projectDir, out receipt);
			}
			else
			{
				return TargetReceipt.TryRead(receiptFile, engineDir, null, out receipt);
			}
		}

		public static TargetReceipt CreateDefaultEditorReceipt(ProjectInfo projectInfo, ConfigFile projectConfigFile, BuildConfig configuration)
		{
			string baseName = GetDefaultEditorTargetName(projectInfo, projectConfigFile);
			if (configuration != BuildConfig.Development || !String.IsNullOrEmpty(HostArchitectureSuffix))
			{
				if (configuration != BuildConfig.DebugGame || projectConfigFile.GetValue("Options.DebugGameHasSeparateExecutable", false))
				{
					baseName += $"-{HostPlatform}-{configuration}{HostArchitectureSuffix}";
				}
			}

			string extension = String.Empty;
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				extension = ".exe";
			}

			TargetReceipt receipt = new TargetReceipt();
			receipt.Configuration = configuration.ToString();
			receipt.Launch = FileReference.Combine(projectInfo.LocalRootPath, "Engine", "Binaries", HostPlatform, $"{baseName}{extension}").FullName;
			receipt.LaunchCmd = FileReference.Combine(projectInfo.LocalRootPath, "Engine", "Binaries", HostPlatform, $"{baseName}-Cmd{extension}").FullName;
			return receipt;
		}

		private static bool UseSharedEditorReceipt(ProjectInfo projectInfo, ConfigFile projectConfig)
		{
			string? setting;
			if (TryGetProjectSetting(projectConfig, projectInfo.ProjectIdentifier, "UseSharedEditor", out setting))
			{
				bool value;
				if (Boolean.TryParse(setting, out value))
				{
					return value;
				}
			}
			return false;
		}

		public static FileReference GetReceiptFile(ProjectInfo projectInfo, ConfigFile projectConfig, FileReference targetFile, string configuration)
		{
			string targetName = targetFile.GetFileNameWithoutAnyExtensions();

			DirectoryReference? projectDir = projectInfo.ProjectDir;
			if (projectDir != null && (targetFile.IsUnderDirectory(projectDir) || !UseSharedEditorReceipt(projectInfo, projectConfig)))
			{
				return GetReceiptFile(projectDir, targetName, configuration);
			}
			else
			{
				return GetReceiptFile(projectInfo.EngineDir, targetName, configuration);
			}
		}

		public static FileReference GetReceiptFile(DirectoryReference baseDir, string targetName, string configuration)
		{
			return GetReceiptFile(baseDir, targetName, HostPlatform, configuration, HostArchitectureSuffix);
		}

		public static FileReference GetReceiptFile(DirectoryReference baseDir, string targetName, string platform, string configuration, string architectureSuffix)
		{
			if (String.IsNullOrEmpty(architectureSuffix) && configuration.Equals("Development", StringComparison.OrdinalIgnoreCase))
			{
				return FileReference.Combine(baseDir, "Binaries", platform, $"{targetName}.target");
			}
			else
			{
				return FileReference.Combine(baseDir, "Binaries", platform, $"{targetName}-{platform}-{configuration}{architectureSuffix}.target");
			}
		}

		public static Dictionary<Guid, ConfigObject> GetDefaultBuildStepObjects(ProjectInfo projectInfo, string editorTarget, BuildConfig editorConfig, ConfigFile latestProjectConfigFile, bool shouldSyncPrecompiledEditor)
		{
			string projectArgument = "";
			if (projectInfo.LocalFileName.HasExtension(".uproject"))
			{
				projectArgument = String.Format("\"{0}\"", projectInfo.LocalFileName);
			}

			bool useCrashReportClientEditor = latestProjectConfigFile.GetValue("Options.UseCrashReportClientEditor", false);

			string hostPlatform;
			if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				hostPlatform = "Mac";
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				hostPlatform = "Linux";
			}
			else
			{
				hostPlatform = "Win64";
			}

			List<BuildStep> defaultBuildSteps = new List<BuildStep>();
			if (latestProjectConfigFile.GetValue("Options.BuildUnrealHeaderTool", true))
			{
				defaultBuildSteps.Add(new BuildStep(new Guid("{01F66060-73FA-4CC8-9CB3-E217FBBA954E}"), 0, "Compile UnrealHeaderTool", "Compiling UnrealHeaderTool...", 1, "UnrealHeaderTool", hostPlatform, "Development", "", !shouldSyncPrecompiledEditor));
			}
			defaultBuildSteps.Add(new BuildStep(new Guid("{F097FF61-C916-4058-8391-35B46C3173D5}"), 1, $"Compile {editorTarget}", $"Compiling {editorTarget}...", 10, editorTarget, hostPlatform, editorConfig.ToString(), projectArgument, !shouldSyncPrecompiledEditor));
			defaultBuildSteps.Add(new BuildStep(new Guid("{C6E633A1-956F-4AD3-BC95-6D06D131E7B4}"), 2, "Compile ShaderCompileWorker", "Compiling ShaderCompileWorker...", 1, "ShaderCompileWorker", hostPlatform, "Development", "", !shouldSyncPrecompiledEditor));
			defaultBuildSteps.Add(new BuildStep(new Guid("{24FFD88C-7901-4899-9696-AE1066B4B6E8}"), 3, "Compile UnrealLightmass", "Compiling UnrealLightmass...", 1, "UnrealLightmass", hostPlatform, "Development", "", !shouldSyncPrecompiledEditor));
			defaultBuildSteps.Add(new BuildStep(new Guid("{FFF20379-06BF-4205-8A3E-C53427736688}"), 4, "Compile CrashReportClient", "Compiling CrashReportClient...", 1, "CrashReportClient", hostPlatform, "Shipping", "", !shouldSyncPrecompiledEditor && !useCrashReportClientEditor));
			defaultBuildSteps.Add(new BuildStep(new Guid("{7143D861-58D3-4F83-BADC-BC5DCB2079F6}"), 5, "Compile CrashReportClientEditor", "Compiling CrashReportClientEditor...", 1, "CrashReportClientEditor", hostPlatform, "Shipping", "", !shouldSyncPrecompiledEditor && useCrashReportClientEditor));

			return defaultBuildSteps.ToDictionary(x => x.UniqueId, x => x.ToConfigObject());
		}

		public static Dictionary<string, string> GetWorkspaceVariables(ProjectInfo projectInfo, int changeNumber, int codeChangeNumber, TargetReceipt? editorTarget, ConfigFile? projectConfigFile, IPerforceSettings perforceSettings)
		{
			Dictionary<string, string> variables = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

			if (projectInfo.StreamName != null)
			{
				variables.Add("Stream", projectInfo.StreamName);
			}

			variables.Add("Change", changeNumber.ToString());
			variables.Add("CodeChange", codeChangeNumber.ToString());

			variables.Add("ClientName", projectInfo.ClientName);
			variables.Add("BranchDir", projectInfo.LocalRootPath.FullName);
			variables.Add("ProjectDir", projectInfo.LocalFileName.Directory.FullName);
			variables.Add("ProjectFile", projectInfo.LocalFileName.FullName);
			variables.Add("UseIncrementalBuilds", "1");

			string editorConfig = editorTarget?.Configuration ?? String.Empty;
			variables.Add("EditorConfig", editorConfig);

			string editorLaunch = editorTarget?.Launch ?? String.Empty;
			variables.Add("EditorExe", editorLaunch);

			string editorLaunchCmd = editorTarget?.LaunchCmd ?? editorLaunch.Replace(".exe", "-Cmd.exe", StringComparison.OrdinalIgnoreCase);
			variables.Add("EditorCmdExe", editorLaunchCmd);

			// Legacy
			variables.Add("UE4EditorConfig", editorConfig);
			variables.Add("UE4EditorDebugArg", (editorConfig.Equals("Debug", StringComparison.Ordinal) || editorConfig.Equals("DebugGame", StringComparison.Ordinal)) ? " -debug" : "");
			variables.Add("UE4EditorExe", editorLaunch);
			variables.Add("UE4EditorCmdExe", editorLaunchCmd);

			if (projectConfigFile != null)
			{
				if (TryGetProjectSetting(projectConfigFile, projectInfo.ProjectIdentifier, "SdkInstallerDir", out string? sdkInstallerDir))
				{
					variables.Add("SdkInstallerDir", sdkInstallerDir);
				}
			}

			variables.Add("PerforceServerAndPort", perforceSettings.ServerAndPort);
			variables.Add("PerforceUserName", perforceSettings.UserName);
			if (perforceSettings.ClientName != null)
			{
				variables.Add("PerforceClientName", perforceSettings.ClientName);
			}

			return variables;
		}

		public static Dictionary<string, string> GetWorkspaceVariables(ProjectInfo projectInfo, int changeNumber, int codeChangeNumber, TargetReceipt? editorTarget, ConfigFile? projectConfigFile, IPerforceSettings perforceSettings, IEnumerable<KeyValuePair<string, string>> additionalVariables)
		{
			Dictionary<string, string> variables = GetWorkspaceVariables(projectInfo, changeNumber, codeChangeNumber, editorTarget, projectConfigFile, perforceSettings);
			foreach ((string key, string value) in additionalVariables)
			{
				variables[key] = value;
			}
			return variables;
		}

		public static bool TryGetProjectSetting(ConfigFile projectConfigFile, string selectedProjectIdentifier, string name, [NotNullWhen(true)] out string? value)
		{
			string path = selectedProjectIdentifier;
			for (; ; )
			{
				ConfigSection? projectSection = projectConfigFile.FindSection(path);
				if (projectSection != null)
				{
					string? newValue = projectSection.GetValue(name, null);
					if (newValue != null)
					{
						value = newValue;
						return true;
					}
				}

				int lastSlash = path.LastIndexOf('/');
				if (lastSlash < 2)
				{
					break;
				}

				path = path.Substring(0, lastSlash);
			}

			ConfigSection? defaultSection = projectConfigFile.FindSection("Default");
			if (defaultSection != null)
			{
				string? newValue = defaultSection.GetValue(name, null);
				if (newValue != null)
				{
					value = newValue;
					return true;
				}
			}

			value = null;
			return false;
		}

		public static void GetProjectSettings(ConfigFile projectConfigFile, string selectedProjectIdentifier, string name, List<string> values)
		{
			string path = selectedProjectIdentifier;
			for (; ; )
			{
				ConfigSection? projectSection = projectConfigFile.FindSection(path);
				if (projectSection != null)
				{
					values.AddRange(projectSection.GetValues(name, Array.Empty<string>()));
				}

				int lastSlash = path.LastIndexOf('/');
				if (lastSlash < 2)
				{
					break;
				}

				path = path.Substring(0, lastSlash);
			}

			ConfigSection? defaultSection = projectConfigFile.FindSection("Default");
			if (defaultSection != null)
			{
				values.AddRange(defaultSection.GetValues(name, Array.Empty<string>()));
			}
		}

		public static Dictionary<Guid, WorkspaceSyncCategory> GetSyncCategories(ConfigFile projectConfigFile)
		{
			Dictionary<Guid, WorkspaceSyncCategory> uniqueIdToCategory = new Dictionary<Guid, WorkspaceSyncCategory>();
			if (projectConfigFile != null)
			{
				string[] categoryLines = projectConfigFile.GetValues("Options.SyncCategory", Array.Empty<string>());
				foreach (string categoryLine in categoryLines)
				{
					ConfigObject obj = new ConfigObject(categoryLine);

					Guid uniqueId;
					if (Guid.TryParse(obj.GetValue("UniqueId", ""), out uniqueId))
					{
						WorkspaceSyncCategory? category;
						if (!uniqueIdToCategory.TryGetValue(uniqueId, out category))
						{
							category = new WorkspaceSyncCategory(uniqueId);
							uniqueIdToCategory.Add(uniqueId, category);
						}

						if (obj.GetValue("Clear", false))
						{
							category.Paths.Clear();
							category.Requires.Clear();
						}

						category.Name = obj.GetValue("Name", category.Name);
						category.Enable = obj.GetValue("Enable", category.Enable);

						string[] paths = Enumerable.Concat(category.Paths, obj.GetValue("Paths", "").Split(';').Select(x => x.Trim())).Where(x => x.Length > 0).Distinct().OrderBy(x => x).ToArray();
						category.Paths.Clear();
						category.Paths.AddRange(paths);

						category.Hidden = obj.GetValue("Hidden", category.Hidden);

						Guid[] requires = Enumerable.Concat(category.Requires, ParseGuids(obj.GetValue("Requires", "").Split(';'))).Distinct().OrderBy(x => x).ToArray();
						category.Requires.Clear();
						category.Requires.AddRange(requires);
					}
				}
			}
			return uniqueIdToCategory;
		}

		static IEnumerable<Guid> ParseGuids(IEnumerable<string> values)
		{
			foreach (string value in values)
			{
				Guid guid;
				if (Guid.TryParse(value, out guid))
				{
					yield return guid;
				}
			}
		}
	}
}

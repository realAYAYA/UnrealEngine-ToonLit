// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

#nullable enable

namespace UnrealGameSync
{
	class ToolLink
	{
		public string Label;
		public string FileName;
		public string? Arguments;
		public string? WorkingDir;

		public ToolLink(string label, string fileName)
		{
			this.Label = label;
			this.FileName = fileName;
		}
	}

	class ToolDefinition
	{
		public Guid Id;
		public string Name;
		public string Description;
		public bool Enabled;
		public Func<ILogger, CancellationToken, Task>? InstallAction;
		public Func<ILogger, CancellationToken, Task>? UninstallAction;
		public List<ToolLink> StatusPanelLinks = new List<ToolLink>();
		public string ZipPath;
		public int ZipChange;
		public string ConfigPath;
		public int ConfigChange;

		public ToolDefinition(Guid id, string name, string description, string zipPath, int zipChange, string configPath, int configChange)
		{
			this.Id = id;
			this.Name = name;
			this.Description = description;
			this.ZipPath = zipPath;
			this.ZipChange = zipChange;
			this.ConfigPath = configPath;
			this.ConfigChange = configChange;
		}
	}

	class ToolUpdateMonitor : IDisposable
	{
		CancellationTokenSource _cancellationSource;
		SynchronizationContext _synchronizationContext;
		Task? _workerTask;
		AsyncEvent _wakeEvent;
		ILogger _logger;
		public List<ToolDefinition> Tools { get; private set; } = new List<ToolDefinition>();
		int _lastChange = -1;
		IAsyncDisposer _asyncDisposer;

		IPerforceSettings PerforceSettings { get; }
		DirectoryReference ToolsDir { get; }
		UserSettings Settings { get; }

		public Action? OnChange;

		public ToolUpdateMonitor(IPerforceSettings perforceSettings, DirectoryReference dataDir, UserSettings settings, IServiceProvider serviceProvider)
		{
			_cancellationSource = new CancellationTokenSource();
			_synchronizationContext = SynchronizationContext.Current!;
			this.ToolsDir = DirectoryReference.Combine(dataDir, "Tools");
			this.PerforceSettings = perforceSettings;
			this.Settings = settings;
			this._logger = serviceProvider.GetRequiredService<ILogger<ToolUpdateMonitor>>();
			this._asyncDisposer = serviceProvider.GetRequiredService<IAsyncDisposer>();

			DirectoryReference.CreateDirectory(ToolsDir);

			_wakeEvent = new AsyncEvent();
		}

		public void Start()
		{
			if (DeploymentSettings.ToolsDepotPath != null)
			{
				_workerTask = Task.Run(() => PollForUpdatesAsync(_cancellationSource.Token));
			}
		}

		public void Dispose()
		{
			OnChange = null;

			if (_workerTask != null)
			{
				_cancellationSource.Cancel();
				_asyncDisposer.Add(_workerTask.ContinueWith(_ => _cancellationSource.Dispose()));
				_workerTask = null;
			}
		}

		DirectoryReference GetToolPathInternal(string toolName)
		{
			return DirectoryReference.Combine(ToolsDir, toolName, "Current");
		}

		public string? GetToolName(Guid toolId)
		{
			foreach (ToolDefinition tool in Tools)
			{
				if(tool.Id == toolId)
				{
					return tool.Name;
				}
			}
			return null;
		}

		public DirectoryReference? GetToolPath(string toolName)
		{
			if (GetToolChange(toolName) != 0)
			{
				return GetToolPathInternal(toolName);
			}
			else
			{
				return null;
			}
		}

		public void UpdateNow()
		{
			_wakeEvent.Set();
		}

		async Task PollForUpdatesAsync(CancellationToken cancellationToken)
		{
			while (!cancellationToken.IsCancellationRequested)
			{
				Task wakeTask = _wakeEvent.Task;

				try
				{
					await PollForUpdatesOnce(_logger, cancellationToken);
				}
				catch(Exception ex)
				{
					_logger.LogError(ex, "Exception while checking for tool updates");
				}

				Task delayTask = Task.Delay(TimeSpan.FromMinutes(60.0), cancellationToken);
				await Task.WhenAny(delayTask, wakeTask);
			}
		}

		async Task PollForUpdatesOnce(ILogger logger, CancellationToken cancellationToken)
		{
			using IPerforceConnection perforce = await PerforceConnection.CreateAsync(PerforceSettings, logger);

			List<ChangesRecord> changes = await perforce.GetChangesAsync(ChangesOptions.None, 1, ChangeStatus.Submitted, $"{DeploymentSettings.ToolsDepotPath}/...", cancellationToken);
			if (changes.Count == 0 || changes[0].Number == _lastChange)
			{
				return;
			}

			List<FStatRecord> fileRecords = await perforce.FStatAsync($"{DeploymentSettings.ToolsDepotPath}/...", cancellationToken).ToListAsync(cancellationToken);

			// Update the tools list
			List<ToolDefinition> newTools = new List<ToolDefinition>();
			foreach (FStatRecord fileRecord in fileRecords)
			{
				if (fileRecord.DepotFile != null && fileRecord.DepotFile.EndsWith(".ini"))
				{
					ToolDefinition? tool = Tools.FirstOrDefault(x => x.ConfigPath.Equals(fileRecord.DepotFile, StringComparison.Ordinal));
					if (tool == null || tool.ConfigChange != fileRecord.HeadChange)
					{
						tool = await ReadToolDefinitionAsync(perforce, fileRecord.DepotFile, fileRecord.HeadChange, logger, cancellationToken);
					}
					if (tool != null)
					{
						newTools.Add(tool);
					}
				}
			}
			Tools = newTools;

			foreach (ToolDefinition tool in Tools)
			{
				tool.Enabled = Settings.EnabledTools.Contains(tool.Id);

				if(!tool.Enabled)
				{
					continue;
				}

				List<FStatRecord> toolFileRecords = fileRecords.Where(x => String.Equals(x.DepotFile, tool.ZipPath, StringComparison.OrdinalIgnoreCase)).ToList();
				if (toolFileRecords.Count == 0)
				{
					continue;
				}

				int headChange = toolFileRecords.Max(x => x.HeadChange);
				if (headChange == GetToolChange(tool.Name))
				{
					continue;
				}

				List<FStatRecord> syncFileRecords = toolFileRecords.Where(x => x.Action != FileAction.Delete && x.Action != FileAction.MoveDelete).ToList();
				try
				{
					await UpdateToolAsync(perforce, tool.Name, headChange, syncFileRecords, tool.InstallAction, logger, cancellationToken);
				}
				catch (Exception ex)
				{
					logger.LogError(ex, "Exception while updating tool");
				}
			}

			foreach (ToolDefinition tool in Tools)
			{
				if (!tool.Enabled && GetToolChange(tool.Name) != 0)
				{
					try
					{
						await RemoveToolAsync(tool.Name, tool.UninstallAction, logger, cancellationToken);
					}
					catch (Exception ex)
					{
						logger.LogError(ex, "Exception while removing tool");
					}
				}
			}

			_synchronizationContext.Post(_ => OnChange?.Invoke(), null);
		}

		async Task<ToolDefinition?> ReadToolDefinitionAsync(IPerforceConnection perforce, string depotPath, int change, ILogger logger, CancellationToken cancellationToken)
		{
			PerforceResponse<PrintRecord<string[]>> response = await perforce.TryPrintLinesAsync($"{depotPath}@{change}", cancellationToken);
			if (!response.Succeeded || response.Data.Contents == null)
			{
				return null;
			}

			int nameIdx = depotPath.LastIndexOf('/') + 1;
			int extensionIdx = depotPath.LastIndexOf('.');

			ConfigFile configFile = new ConfigFile();
			configFile.Parse(response.Data.Contents);

			string? id = configFile.GetValue("Settings.Id", null);
			if (id == null || !Guid.TryParse(id, out Guid toolId))
			{
				return null;
			}

			string toolName = configFile.GetValue("Settings.Name", depotPath.Substring(nameIdx, extensionIdx - nameIdx));
			string toolDescription = configFile.GetValue("Settings.Description", toolName);
			string toolZipPath = depotPath.Substring(0, extensionIdx) + ".zip";
			int toolZipChange = GetToolChange(toolName);
			string toolConfigPath = depotPath;
			int toolConfigChange = change;

			ToolDefinition tool = new ToolDefinition(toolId, toolName, toolDescription, toolZipPath, toolZipChange, toolConfigPath, toolConfigChange);

			string? installCommand = configFile.GetValue("Settings.InstallCommand", null);
			if (!String.IsNullOrEmpty(installCommand))
			{
				tool.InstallAction = async (logger, cancellationToken) =>
				{
					logger.LogInformation("Running install action: {Command}", installCommand);
					await RunCommandAsync(tool.Name, installCommand, logger, cancellationToken);
				};
			}

			string? uninstallCommand = configFile.GetValue("Settings.UninstallCommand", null);
			if (!String.IsNullOrEmpty(uninstallCommand))
			{
				tool.UninstallAction = async (logger, cancellationToken) =>
				{
					logger.LogInformation("Running unininstall action: {Command}", uninstallCommand);
					await RunCommandAsync(tool.Name, uninstallCommand, logger, cancellationToken);
				};
			}

			string[] statusPanelLinks = configFile.GetValues("Settings.StatusPanelLinks", new string[0]);
			foreach (string statusPanelLink in statusPanelLinks)
			{
				ConfigObject obj = new ConfigObject(statusPanelLink);

				string? label = obj.GetValue("Label", null);
				string? fileName = obj.GetValue("FileName", null);

				if (label != null && fileName != null)
				{
					ToolLink link = new ToolLink(label, fileName);
					link.Arguments = obj.GetValue("Arguments", null);
					link.WorkingDir = obj.GetValue("WorkingDir", null);
					tool.StatusPanelLinks.Add(link);
				}
			}

			return tool;
		}

		async Task RunCommandAsync(string toolName, string command, ILogger logger, CancellationToken cancellationToken)
		{
			DirectoryReference toolPath = GetToolPathInternal(toolName);

			string commandExe = command;
			string commandArgs = string.Empty;

			int spaceIdx = command.IndexOf(' ');
			if (spaceIdx != -1)
			{
				commandExe = command.Substring(0, spaceIdx);
				commandArgs = command.Substring(spaceIdx + 1);
			}

			int exitCode = await Utility.ExecuteProcessAsync(FileReference.Combine(toolPath, commandExe).FullName, toolPath.FullName, commandArgs, line => logger.LogInformation("{ToolName}> {Line}", toolName, line), cancellationToken);
			logger.LogInformation("{ToolName}> Exit code {ExitCode})", toolName, exitCode);
		}

		async Task RemoveToolAsync(string toolName, Func<ILogger, CancellationToken, Task>? uninstallAction, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("Removing {0}", toolName);
			DirectoryReference? toolPath = GetToolPath(toolName);

			if (uninstallAction != null)
			{
				logger.LogInformation("Running uninstall...");
				await uninstallAction(logger, cancellationToken);
			}

			SetToolChange(toolName, null);

			if (toolPath != null)
			{
				logger.LogInformation("Removing {ToolPath}", toolPath);
				TryDeleteDirectory(toolPath, logger);
			}

			logger.LogInformation("{ToolName} has been removed successfully", toolName);
		}

		static void ForceDeleteDirectory(DirectoryReference directoryName)
		{
			DirectoryInfo baseDir = directoryName.ToDirectoryInfo();
			if (baseDir.Exists)
			{
				foreach (FileInfo file in baseDir.EnumerateFiles("*", SearchOption.AllDirectories))
				{
					file.Attributes = FileAttributes.Normal;
				}
				baseDir.Delete(true);
			}
		}

		static bool TryDeleteDirectory(DirectoryReference directoryName, ILogger logger)
		{
			try
			{
				ForceDeleteDirectory(directoryName);
				return true;
			}
			catch(Exception ex)
			{
				logger.LogWarning(ex, "Unable to delete directory {DirectoryName}", directoryName);
				return false;
			}
		}

		async Task<bool> UpdateToolAsync(IPerforceConnection perforce, string toolName, int change, List<FStatRecord> records, Func<ILogger, CancellationToken, Task>? installAction, ILogger logger, CancellationToken cancellationToken)
		{
			DirectoryReference toolDir = DirectoryReference.Combine(ToolsDir, toolName);
			DirectoryReference.CreateDirectory(toolDir);

			foreach (DirectoryReference existingDir in DirectoryReference.EnumerateDirectories(toolDir, "Prev-*"))
			{
				TryDeleteDirectory(existingDir, logger);
			}

			DirectoryReference nextToolDir = DirectoryReference.Combine(toolDir, "Next");
			ForceDeleteDirectory(nextToolDir);
			DirectoryReference.CreateDirectory(nextToolDir);

			DirectoryReference nextToolZipsDir = DirectoryReference.Combine(nextToolDir, ".zips");
			DirectoryReference.CreateDirectory(nextToolZipsDir);

			for (int idx = 0; idx < records.Count; idx++)
			{
				FileReference zipFile = FileReference.Combine(nextToolZipsDir, String.Format("{0}.{1}.zip", toolName, idx));

				PerforceResponseList<PrintRecord> response = await perforce.TryPrintAsync(zipFile.FullName, $"{records[idx].DepotFile}#{records[idx].HeadRevision}", cancellationToken);
				if(!response.Succeeded || !FileReference.Exists(zipFile))
				{
					logger.LogError("Unable to print {0}", records[idx].DepotFile);
					return false;
				}

				ArchiveUtils.ExtractFiles(zipFile, nextToolDir, null, new ProgressValue(), logger);
			}

			SetToolChange(toolName, null);

			DirectoryReference currentToolDir = DirectoryReference.Combine(toolDir, "Current");
			if (DirectoryReference.Exists(currentToolDir))
			{
				DirectoryReference prevDirectoryName = DirectoryReference.Combine(toolDir, String.Format("Prev-{0:X16}", Stopwatch.GetTimestamp()));
				Directory.Move(currentToolDir.FullName, prevDirectoryName.FullName);
				TryDeleteDirectory(prevDirectoryName, logger);
			}

			Directory.Move(nextToolDir.FullName, currentToolDir.FullName);

			if (installAction != null)
			{
				logger.LogInformation("Running installer...");
				await installAction.Invoke(logger, cancellationToken);
			}

			SetToolChange(toolName, change);
			logger.LogInformation("Updated {ToolName} to change {Change}", toolName, change);
			return true;
		}

		FileReference GetConfigFilePath(string toolName)
		{
			return FileReference.Combine(ToolsDir, toolName, toolName + ".ini");
		}

		int GetToolChange(string toolName)
		{
			try
			{
				FileReference configFilePath = GetConfigFilePath(toolName);
				if (FileReference.Exists(configFilePath))
				{
					ConfigFile configFile = new ConfigFile();
					configFile.Load(configFilePath);
					return configFile.GetValue("Settings.Change", 0);
				}
			}
			catch
			{
			}
			return 0;
		}

		void SetToolChange(string toolName, int? change)
		{
			FileReference configFilePath = GetConfigFilePath(toolName);
			if (change.HasValue)
			{
				ConfigFile configFile = new ConfigFile();
				configFile.SetValue("Settings.Change", change.Value);
				configFile.Save(configFilePath);
			}
			else
			{
				FileReference.Delete(configFilePath);
			}
		}
	}
}

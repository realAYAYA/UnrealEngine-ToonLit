// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using UnrealGameSync;

namespace UnrealGameSync
{
	partial class ApplicationSettingsWindow : Form
	{
		static class PerforceTestConnectionTask
		{
			public static async Task RunAsync(IPerforceConnection perforce, string depotPath, CancellationToken cancellationToken)
			{
				string checkFilePath = String.Format("{0}/Release/UnrealGameSync.exe", depotPath);

				List<FStatRecord> fileRecords = await perforce.FStatAsync(checkFilePath, cancellationToken).ToListAsync(cancellationToken);
				if(fileRecords.Count == 0)
				{
					throw new UserErrorException($"Unable to find {checkFilePath}");
				}
			}
		}

		string _originalExecutableFileName;
		IPerforceSettings _defaultPerforceSettings;
		UserSettings _settings;
		ILogger _logger;

		string? _initialServerAndPort;
		string? _initialUserName;
		string? _initialDepotPath;
		bool _initialPreview;
		int _initialAutomationPortNumber;
		ProtocolHandlerState _initialProtocolHandlerState;

		bool? _restartPreview;

		ToolUpdateMonitor _toolUpdateMonitor;

		class ToolItem
		{
			public ToolDefinition Definition { get; }

			public ToolItem(ToolDefinition definition)
			{
				this.Definition = definition;
			}

			public override string ToString()
			{
				return Definition.Description;
			}
		}

		private ApplicationSettingsWindow(IPerforceSettings defaultPerforceSettings, bool preview, string originalExecutableFileName, UserSettings settings, ToolUpdateMonitor toolUpdateMonitor, ILogger<ApplicationSettingsWindow> logger)
		{
			InitializeComponent();

			this._originalExecutableFileName = originalExecutableFileName;
			this._defaultPerforceSettings = defaultPerforceSettings;
			this._settings = settings;
			this._toolUpdateMonitor = toolUpdateMonitor;
			this._logger = logger;

			GlobalPerforceSettings.ReadGlobalPerforceSettings(ref _initialServerAndPort, ref _initialUserName, ref _initialDepotPath, ref preview);
			_initialPreview = preview;

			_initialAutomationPortNumber = AutomationServer.GetPortNumber();
			_initialProtocolHandlerState = ProtocolHandlerUtils.GetState();

			this.AutomaticallyRunAtStartupCheckBox.Checked = IsAutomaticallyRunAtStartup();
			this.KeepInTrayCheckBox.Checked = settings.KeepInTray;
						
			this.ServerTextBox.Text = _initialServerAndPort;
			this.ServerTextBox.Select(ServerTextBox.TextLength, 0);
			this.ServerTextBox.CueBanner = $"Default ({defaultPerforceSettings.ServerAndPort})";

			this.UserNameTextBox.Text = _initialUserName;
			this.UserNameTextBox.Select(UserNameTextBox.TextLength, 0);
			this.UserNameTextBox.CueBanner = $"Default ({defaultPerforceSettings.UserName})";

			this.ParallelSyncThreadsSpinner.Value = Math.Max(Math.Min(settings.SyncOptions.NumThreads, ParallelSyncThreadsSpinner.Maximum), ParallelSyncThreadsSpinner.Minimum);

			this.DepotPathTextBox.Text = _initialDepotPath;
			this.DepotPathTextBox.Select(DepotPathTextBox.TextLength, 0);
			this.DepotPathTextBox.CueBanner = DeploymentSettings.DefaultDepotPath ?? String.Empty;

			this.UsePreviewBuildCheckBox.Checked = preview;

			if(_initialAutomationPortNumber > 0)
			{
				this.EnableAutomationCheckBox.Checked = true;
				this.AutomationPortTextBox.Enabled = true;
				this.AutomationPortTextBox.Text = _initialAutomationPortNumber.ToString();
			}
			else
			{
				this.EnableAutomationCheckBox.Checked = false;
				this.AutomationPortTextBox.Enabled = false;
				this.AutomationPortTextBox.Text = AutomationServer.DefaultPortNumber.ToString();
			}

			if(_initialProtocolHandlerState == ProtocolHandlerState.Installed)
			{
				this.EnableProtocolHandlerCheckBox.CheckState = CheckState.Checked;
			}
			else if (_initialProtocolHandlerState == ProtocolHandlerState.NotInstalled)
			{
				this.EnableProtocolHandlerCheckBox.CheckState = CheckState.Unchecked;
			}
			else
			{
				this.EnableProtocolHandlerCheckBox.CheckState = CheckState.Indeterminate;
			}

			List<ToolDefinition> tools = toolUpdateMonitor.Tools;
			foreach (ToolDefinition tool in tools)
			{
				this.CustomToolsListBox.Items.Add(new ToolItem(tool), settings.EnabledTools.Contains(tool.Id));
			}
		}

		public static bool? ShowModal(IWin32Window owner, IPerforceSettings defaultPerforceSettings, bool preview, string originalExecutableFileName, UserSettings settings, ToolUpdateMonitor toolUpdateMonitor, ILogger<ApplicationSettingsWindow> logger)
		{
			ApplicationSettingsWindow applicationSettings = new ApplicationSettingsWindow(defaultPerforceSettings, preview, originalExecutableFileName, settings, toolUpdateMonitor, logger);
			if(applicationSettings.ShowDialog() == DialogResult.OK)
			{
				return applicationSettings._restartPreview;
			}
			else
			{
				return null;
			}
		}

		private bool IsAutomaticallyRunAtStartup()
		{
			RegistryKey? key = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
			return (key?.GetValue("UnrealGameSync") != null);
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			// Update the settings
			string? serverAndPort = ServerTextBox.Text.Trim();
			if(serverAndPort.Length == 0)
			{
				serverAndPort = null;
			}

			string? userName = UserNameTextBox.Text.Trim();
			if(userName.Length == 0)
			{
				userName = null;
			}

			string? depotPath = DepotPathTextBox.Text.Trim();
			if(depotPath.Length == 0 || depotPath == DeploymentSettings.DefaultDepotPath)
			{
				depotPath = null;
			}

			bool preview = UsePreviewBuildCheckBox.Checked;


			int automationPortNumber;
			if(!EnableAutomationCheckBox.Checked || !int.TryParse(AutomationPortTextBox.Text, out automationPortNumber))
			{
				automationPortNumber = -1;
			}
			
			if(serverAndPort != _initialServerAndPort || userName != _initialUserName || depotPath != _initialDepotPath || preview != _initialPreview || automationPortNumber != _initialAutomationPortNumber)
			{
				// Try to log in to the new server, and check the application is there
				if(serverAndPort != _initialServerAndPort || userName != _initialUserName || depotPath != _initialDepotPath)
				{
					PerforceSettings settings = Utility.OverridePerforceSettings(_defaultPerforceSettings, serverAndPort, userName);

					string? testDepotPath = depotPath ?? DeploymentSettings.DefaultDepotPath;
					if (testDepotPath != null)
					{
						ModalTask? task = PerforceModalTask.Execute(this, "Checking connection", "Checking connection, please wait...", settings, (p, c) => PerforceTestConnectionTask.RunAsync(p, testDepotPath, c), _logger);
						if (task == null || !task.Succeeded)
						{
							return;
						}
					}
				}

				if(MessageBox.Show("UnrealGameSync must be restarted to apply these settings.\n\nWould you like to restart now?", "Restart Required", MessageBoxButtons.OKCancel) != DialogResult.OK)
				{
					return;
				}

				_restartPreview = UsePreviewBuildCheckBox.Checked;
				GlobalPerforceSettings.SaveGlobalPerforceSettings(serverAndPort, userName, depotPath, preview);
				AutomationServer.SetPortNumber(automationPortNumber);
			}

			RegistryKey key = Registry.CurrentUser.CreateSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
			if (AutomaticallyRunAtStartupCheckBox.Checked)
			{
				key.SetValue("UnrealGameSync", String.Format("\"{0}\" -RestoreState", _originalExecutableFileName));
			}
			else
			{
				key.DeleteValue("UnrealGameSync", false);
			}

			if (_settings.KeepInTray != KeepInTrayCheckBox.Checked || _settings.SyncOptions.NumThreads != ParallelSyncThreadsSpinner.Value)
			{
				_settings.SyncOptions.NumThreads = (int)ParallelSyncThreadsSpinner.Value;
				_settings.KeepInTray = KeepInTrayCheckBox.Checked;
				_settings.Save(_logger);
			}

			List<Guid> newEnabledTools = new List<Guid>();
			foreach (ToolItem? item in CustomToolsListBox.CheckedItems)
			{
				if (item != null)
				{
					newEnabledTools.Add(item.Definition.Id);
				}
			}
			if (!newEnabledTools.SequenceEqual(_settings.EnabledTools))
			{
				_settings.EnabledTools = newEnabledTools.ToArray();
				_settings.Save(_logger);
				_toolUpdateMonitor.UpdateNow();
			}

			if (EnableProtocolHandlerCheckBox.CheckState == CheckState.Checked)
			{
				if (_initialProtocolHandlerState != ProtocolHandlerState.Installed)
				{
					ProtocolHandlerUtils.Install();
				}
			}
			else if (EnableProtocolHandlerCheckBox.CheckState == CheckState.Unchecked)
			{
				if (_initialProtocolHandlerState != ProtocolHandlerState.NotInstalled)
				{
					ProtocolHandlerUtils.Uninstall();
				}
			}

			DialogResult = DialogResult.OK;
			Close();
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void EnableAutomationCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			AutomationPortTextBox.Enabled = EnableAutomationCheckBox.Checked;
		}

		private void AdvancedBtn_Click(object sender, EventArgs e)
		{
			PerforceSyncSettingsWindow window = new PerforceSyncSettingsWindow(_settings, _logger);
			window.ShowDialog();
		}
	}
}

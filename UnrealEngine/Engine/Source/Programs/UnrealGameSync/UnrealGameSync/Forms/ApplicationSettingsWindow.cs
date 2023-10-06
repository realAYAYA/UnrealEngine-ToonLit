// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

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

		readonly string _originalExecutableFileName;
		readonly IPerforceSettings _defaultPerforceSettings;
		readonly UserSettings _settings;
		readonly ILogger _logger;

		readonly string? _initialServerAndPort;
		readonly string? _initialUserName;
		readonly string? _initialDepotPath;
		readonly bool _initialPreview;
		readonly int _initialAutomationPortNumber;
		readonly ProtocolHandlerState _initialProtocolHandlerState;

		bool? _restartPreview;

		readonly ToolUpdateMonitor _toolUpdateMonitor;

		class ToolItem
		{
			public ToolDefinition Definition { get; }

			public ToolItem(ToolDefinition definition)
			{
				Definition = definition;
			}

			public override string ToString()
			{
				return Definition.Description;
			}
		}

		private ApplicationSettingsWindow(IPerforceSettings defaultPerforceSettings, bool preview, string originalExecutableFileName, UserSettings settings, ToolUpdateMonitor toolUpdateMonitor, ILogger<ApplicationSettingsWindow> logger)
		{
			InitializeComponent();
			Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));

			_originalExecutableFileName = originalExecutableFileName;
			_defaultPerforceSettings = defaultPerforceSettings;
			_settings = settings;
			_toolUpdateMonitor = toolUpdateMonitor;
			_logger = logger;

			GlobalPerforceSettings.ReadGlobalPerforceSettings(ref _initialServerAndPort, ref _initialUserName, ref _initialDepotPath, ref preview);
			_initialPreview = preview;

			_initialAutomationPortNumber = AutomationServer.GetPortNumber();
			_initialProtocolHandlerState = ProtocolHandlerUtils.GetState();

			AutomaticallyRunAtStartupCheckBox.Checked = IsAutomaticallyRunAtStartup();
			KeepInTrayCheckBox.Checked = settings.KeepInTray;
					
			ServerTextBox.Text = _initialServerAndPort;
			ServerTextBox.Select(ServerTextBox.TextLength, 0);
			ServerTextBox.CueBanner = $"Default ({defaultPerforceSettings.ServerAndPort})";

			UserNameTextBox.Text = _initialUserName;
			UserNameTextBox.Select(UserNameTextBox.TextLength, 0);
			UserNameTextBox.CueBanner = $"Default ({defaultPerforceSettings.UserName})";

			ParallelSyncThreadsSpinner.Value = Math.Max(Math.Min(settings.SyncOptions.NumThreads ?? PerforceSyncOptions.DefaultNumThreads, ParallelSyncThreadsSpinner.Maximum), ParallelSyncThreadsSpinner.Minimum);

			DepotPathTextBox.Text = _initialDepotPath;
			DepotPathTextBox.Select(DepotPathTextBox.TextLength, 0);
			DepotPathTextBox.CueBanner = DeploymentSettings.Instance.DefaultDepotPath ?? String.Empty;

			UsePreviewBuildCheckBox.Checked = preview;

			if(_initialAutomationPortNumber > 0)
			{
				EnableAutomationCheckBox.Checked = true;
				AutomationPortTextBox.Enabled = true;
				AutomationPortTextBox.Text = _initialAutomationPortNumber.ToString();
			}
			else
			{
				EnableAutomationCheckBox.Checked = false;
				AutomationPortTextBox.Enabled = false;
				AutomationPortTextBox.Text = AutomationServer.DefaultPortNumber.ToString();
			}

			if(_initialProtocolHandlerState == ProtocolHandlerState.Installed)
			{
				EnableProtocolHandlerCheckBox.CheckState = CheckState.Checked;
			}
			else if (_initialProtocolHandlerState == ProtocolHandlerState.NotInstalled)
			{
				EnableProtocolHandlerCheckBox.CheckState = CheckState.Unchecked;
			}
			else
			{
				EnableProtocolHandlerCheckBox.CheckState = CheckState.Indeterminate;
			}

			List<ToolDefinition> tools = toolUpdateMonitor.Tools;
			foreach (ToolDefinition tool in tools)
			{
				CustomToolsListBox.Items.Add(new ToolItem(tool), settings.EnabledTools.Contains(tool.Id));
			}
		}

		public static bool? ShowModal(IWin32Window owner, IPerforceSettings defaultPerforceSettings, bool preview, string originalExecutableFileName, UserSettings settings, ToolUpdateMonitor toolUpdateMonitor, ILogger<ApplicationSettingsWindow> logger)
		{
			using ApplicationSettingsWindow applicationSettings = new ApplicationSettingsWindow(defaultPerforceSettings, preview, originalExecutableFileName, settings, toolUpdateMonitor, logger);
			if(applicationSettings.ShowDialog(owner) == DialogResult.OK)
			{
				return applicationSettings._restartPreview;
			}
			else
			{
				return null;
			}
		}

		private static bool IsAutomaticallyRunAtStartup()
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
			if(depotPath.Length == 0 || depotPath == DeploymentSettings.Instance.DefaultDepotPath)
			{
				depotPath = null;
			}

			bool preview = UsePreviewBuildCheckBox.Checked;

			int automationPortNumber;
			if(!EnableAutomationCheckBox.Checked || !Int32.TryParse(AutomationPortTextBox.Text, out automationPortNumber))
			{
				automationPortNumber = -1;
			}
			
			if(serverAndPort != _initialServerAndPort || userName != _initialUserName || depotPath != _initialDepotPath || preview != _initialPreview || automationPortNumber != _initialAutomationPortNumber)
			{
				// Try to log in to the new server, and check the application is there
				if(serverAndPort != _initialServerAndPort || userName != _initialUserName || depotPath != _initialDepotPath)
				{
					PerforceSettings settings = Utility.OverridePerforceSettings(_defaultPerforceSettings, serverAndPort, userName);

					string? testDepotPath = depotPath ?? DeploymentSettings.Instance.DefaultDepotPath;
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
				int numThreads = (int)ParallelSyncThreadsSpinner.Value;
				_settings.SyncOptions.NumThreads = (numThreads != PerforceSyncOptions.DefaultNumThreads)? (int?)numThreads : null;
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
				_settings.EnabledTools.Clear();
				_settings.EnabledTools.UnionWith(newEnabledTools);
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
			using PerforceSyncSettingsWindow window = new PerforceSyncSettingsWindow(_settings, _logger);
			window.ShowDialog();
		}
	}
}

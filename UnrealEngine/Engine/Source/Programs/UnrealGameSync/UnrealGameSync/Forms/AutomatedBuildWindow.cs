// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics.CodeAnalysis;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using UnrealGameSync.Properties;

namespace UnrealGameSync.Forms
{
	partial class AutomatedBuildWindow : Form
	{
		public class BuildInfo
		{
			public AutomatedSyncWindow.WorkspaceInfo SelectedWorkspaceInfo;
			public string ProjectPath;
			public bool Sync;
			public string ExecCommand;

			public BuildInfo(AutomatedSyncWindow.WorkspaceInfo selectedWorkspaceInfo, string projectPath, bool sync, string execCommand)
			{
				this.SelectedWorkspaceInfo = selectedWorkspaceInfo;
				this.ProjectPath = projectPath;
				this.Sync = sync;
				this.ExecCommand = execCommand;
			}
		}

		string _streamName;
		IServiceProvider _serviceProvider;

		string? _serverAndPortOverride;
		string? _userNameOverride;
		IPerforceSettings _defaultPerforceSettings;

		BuildInfo? _result;

		private AutomatedBuildWindow(string streamName, int changelist, string command, IPerforceSettings defaultPerforceSettings, string? defaultWorkspaceName, string? defaultProjectPath, IServiceProvider serviceProvider)
		{
			this._streamName = streamName;
			this._defaultPerforceSettings = defaultPerforceSettings;
			this._serviceProvider = serviceProvider;

			InitializeComponent();

			ActiveControl = WorkspaceNameTextBox;

			MinimumSize = Size;
			MaximumSize = new Size(32768, Size.Height);

			SyncToChangeCheckBox.Text = String.Format("Sync to changelist {0}", changelist);
			ExecCommandTextBox.Text = command;

			if (defaultWorkspaceName != null)
			{
				WorkspaceNameTextBox.Text = defaultWorkspaceName;
				WorkspaceNameTextBox.Select(WorkspaceNameTextBox.Text.Length, 0);
			}

			if (defaultProjectPath != null)
			{
				WorkspacePathTextBox.Text = defaultProjectPath;
				WorkspacePathTextBox.Select(WorkspacePathTextBox.Text.Length, 0);
			}

			UpdateServerLabel();
			UpdateOkButton();
			UpdateWorkspacePathBrowseButton();
		}

		private IPerforceSettings Perforce
		{
			get => Utility.OverridePerforceSettings(_defaultPerforceSettings, _serverAndPortOverride, _userNameOverride);
		}

		public static bool ShowModal(IWin32Window owner, IPerforceSettings defaultPerforceSettings, string streamName, string projectPath, int changelist, string command, UserSettings settings, IServiceProvider loggerFactory, [NotNullWhen(true)] out BuildInfo? buildInfo)
		{
			string? defaultWorkspaceName = AutomatedSyncWindow.FindDefaultWorkspace(owner, defaultPerforceSettings, streamName, loggerFactory);

			string? defaultProjectPath = null;
			if(!String.IsNullOrEmpty(projectPath))
			{
				defaultProjectPath = projectPath;
			}
			else if(defaultWorkspaceName != null)
			{
				string clientPrefix = String.Format("//{0}/", defaultWorkspaceName);
				foreach (UserSelectedProjectSettings projectSettings in settings.RecentProjects)
				{
					if (projectSettings.ClientPath != null && projectSettings.ClientPath.StartsWith(clientPrefix, StringComparison.OrdinalIgnoreCase))
					{
						defaultProjectPath = projectSettings.ClientPath.Substring(clientPrefix.Length - 1);
						break;
					}
				}
			}

			AutomatedBuildWindow window = new AutomatedBuildWindow(streamName, changelist, command, defaultPerforceSettings, defaultWorkspaceName, defaultProjectPath, loggerFactory);
			if (window.ShowDialog() == DialogResult.OK)
			{
				buildInfo = window._result!;
				return true;
			}
			else
			{
				buildInfo = null;
				return false;
			}
		}

		private void ChangeLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
		{
			if (ConnectWindow.ShowModal(this, _defaultPerforceSettings, ref _serverAndPortOverride, ref _userNameOverride, _serviceProvider))
			{
				UpdateServerLabel();
			}
		}

		private void UpdateServerLabel()
		{
			ServerLabel.Text = OpenProjectWindow.GetServerLabelText(_defaultPerforceSettings, _serverAndPortOverride, _userNameOverride);
		}

		private void WorkspaceNameNewBtn_Click(object sender, EventArgs e)
		{
			string? workspaceName;
			if (NewWorkspaceWindow.ShowModal(this, Perforce, _streamName, WorkspaceNameTextBox.Text, _serviceProvider, out workspaceName))
			{
				WorkspaceNameTextBox.Text = workspaceName;
				UpdateOkButton();
			}
		}

		private void WorkspaceNameBrowseBtn_Click(object sender, EventArgs e)
		{
			string? workspaceName = WorkspaceNameTextBox.Text;
			if (SelectWorkspaceWindow.ShowModal(this, Perforce, workspaceName, _serviceProvider, out workspaceName))
			{
				WorkspaceNameTextBox.Text = workspaceName;
				UpdateOkButton();
			}
		}

		private void UpdateOkButton()
		{
			OkBtn.Enabled = (WorkspaceNameTextBox.Text.Length > 0);
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			AutomatedSyncWindow.WorkspaceInfo? selectedWorkspaceInfo;
			if (AutomatedSyncWindow.ValidateWorkspace(this, Perforce, WorkspaceNameTextBox.Text, _streamName, _serviceProvider, out selectedWorkspaceInfo))
			{
				_result = new BuildInfo(selectedWorkspaceInfo, WorkspacePathTextBox.Text, SyncToChangeCheckBox.Checked, ExecCommandTextBox.Text);
				DialogResult = DialogResult.OK;
				Close();
			}
		}

		private void ExecCommandCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			ExecCommandTextBox.Enabled = ExecCommandCheckBox.Checked;
		}

		private void UpdateWorkspacePathBrowseButton()
		{
			string? workspaceName;
			WorkspacePathBrowseBtn.Enabled = TryGetWorkspaceName(out workspaceName);
		}

		private void WorkspacePathBrowseBtn_Click(object sender, EventArgs e)
		{
			string? workspaceName;
			if (TryGetWorkspaceName(out workspaceName))
			{
				string? workspacePath = WorkspacePathTextBox.Text.Trim();
				if (SelectProjectFromWorkspaceWindow.ShowModal(this, Perforce, workspaceName, workspacePath, _serviceProvider, out workspacePath))
				{
					WorkspacePathTextBox.Text = workspacePath;
					UpdateOkButton();
				}
			}
		}

		private bool TryGetWorkspaceName([NotNullWhen(true)] out string? workspaceName)
		{
			string text = WorkspaceNameTextBox.Text.Trim();
			if (text.Length == 0)
			{
				workspaceName = null;
				return false;
			}

			workspaceName = text;
			return true;
		}

		private void WorkspaceNameTextBox_TextChanged(object sender, EventArgs e)
		{
			UpdateWorkspacePathBrowseButton();
		}
	}
}

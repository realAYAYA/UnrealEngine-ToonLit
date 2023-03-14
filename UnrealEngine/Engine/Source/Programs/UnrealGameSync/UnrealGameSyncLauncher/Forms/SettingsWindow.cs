// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
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

namespace UnrealGameSyncLauncher
{
	partial class SettingsWindow : Form
	{
		[DllImport("user32.dll")]
		private static extern IntPtr SendMessage(IntPtr hWnd, int msg, int wParam, [MarshalAs(UnmanagedType.LPWStr)] string lParam);

		public delegate Task SyncAndRunDelegate(IPerforceConnection perforce, string? depotPath, bool preview, ILogger logWriter, CancellationToken cancellationToken);

		const int EmSetcuebanner = 0x1501;

		string? _logText;
		SyncAndRunDelegate _syncAndRun;

		public SettingsWindow(string? prompt, string? logText, string? serverAndPort, string? userName, string? depotPath, bool preview, SyncAndRunDelegate syncAndRun)
		{
			InitializeComponent();

			if(prompt != null)
			{
				this.PromptLabel.Text = prompt;
			}

			this._logText = logText;
			this.ServerTextBox.Text = serverAndPort ?? String.Empty;
			this.UserNameTextBox.Text = userName ?? String.Empty;
			this.DepotPathTextBox.Text = depotPath ?? String.Empty;
			this.UsePreviewBuildCheckBox.Checked = preview;
			this._syncAndRun = syncAndRun;

			ViewLogBtn.Visible = logText != null;
		}

		protected override void OnLoad(EventArgs e)
		{
			base.OnLoad(e);

			SendMessage(ServerTextBox.Handle, EmSetcuebanner, 1, "Default Server");
			SendMessage(UserNameTextBox.Handle, EmSetcuebanner, 1, "Default User");
		}

		private void ViewLogBtn_Click(object sender, EventArgs e)
		{
			LogWindow log = new LogWindow(_logText ?? String.Empty);
			log.ShowDialog(this);
		}

		private void ConnectBtn_Click(object sender, EventArgs e)
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
			if(depotPath.Length == 0)
			{
				depotPath = null;
			}

			bool preview = UsePreviewBuildCheckBox.Checked;

			GlobalPerforceSettings.SaveGlobalPerforceSettings(serverAndPort, userName, depotPath, preview);

			PerforceSettings perforceSettings = new PerforceSettings(PerforceSettings.Default);
			if (!String.IsNullOrEmpty(serverAndPort))
			{
				perforceSettings.ServerAndPort = serverAndPort;
			}
			if (!String.IsNullOrEmpty(userName))
			{
				perforceSettings.UserName = userName;
			}
			perforceSettings.PreferNativeClient = true;

			// Create the P4 connection
			CaptureLogger logger = new CaptureLogger();

			// Create the task for connecting to this server
			ModalTask? task = PerforceModalTask.Execute(this, "Updating", "Checking for updates, please wait...", perforceSettings, (p, c) => _syncAndRun(p, depotPath, preview, logger, c), logger);
			if (task != null)
			{
				if(task.Succeeded)
				{
					GlobalPerforceSettings.SaveGlobalPerforceSettings(serverAndPort, userName, depotPath, preview);
					DialogResult = DialogResult.OK;
					Close();
				}
				PromptLabel.Text = task.Error;
			}

			_logText = logger.Render(Environment.NewLine);
			ViewLogBtn.Visible = true;
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}
	}
}

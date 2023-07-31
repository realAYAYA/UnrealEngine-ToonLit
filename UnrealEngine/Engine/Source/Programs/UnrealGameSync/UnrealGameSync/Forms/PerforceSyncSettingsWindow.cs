// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class PerforceSyncSettingsWindow : Form
	{
		UserSettings _settings;
		ILogger _logger;

		public PerforceSyncSettingsWindow(UserSettings settings, ILogger logger)
		{
			this._settings = settings;
			this._logger = logger;

			InitializeComponent();
		}

		private void PerforceSettingsWindow_Load(object sender, EventArgs e)
		{
			PerforceSyncOptions syncOptions = _settings.SyncOptions;
			numericUpDownNumRetries.Value = (syncOptions.NumRetries > 0) ? syncOptions.NumRetries : PerforceSyncOptions.DefaultNumRetries;
			numericUpDownTcpBufferSize.Value = (syncOptions.TcpBufferSize > 0) ? syncOptions.TcpBufferSize / 1024 : PerforceSyncOptions.DefaultTcpBufferSize / 1024;
			numericUpDownFileBufferSize.Value = (syncOptions.FileBufferSize > 0) ? syncOptions.FileBufferSize / 1024 : PerforceSyncOptions.DefaultFileBufferSize / 1024;
			numericUpDownMaxCommandsPerBatch.Value = (syncOptions.MaxCommandsPerBatch > 0) ? syncOptions.MaxCommandsPerBatch : PerforceSyncOptions.DefaultMaxCommandsPerBatch;
			numericUpDownMaxSizePerBatch.Value = (syncOptions.MaxSizePerBatch > 0) ? syncOptions.MaxSizePerBatch / 1024 / 1024 : PerforceSyncOptions.DefaultMaxSizePerBatch / 1024 / 1024;
			numericUpDownRetriesOnSyncError.Value = (syncOptions.NumSyncErrorRetries > 0) ? syncOptions.NumSyncErrorRetries : PerforceSyncOptions.DefaultNumSyncErrorRetries;
		}

		private void OkButton_Click(object sender, EventArgs e)
		{
			_settings.SyncOptions.NumRetries = (int)numericUpDownNumRetries.Value;
			_settings.SyncOptions.TcpBufferSize = (int)numericUpDownTcpBufferSize.Value * 1024;
			_settings.SyncOptions.FileBufferSize = (int)numericUpDownFileBufferSize.Value * 1024;
			_settings.SyncOptions.MaxCommandsPerBatch = (int)numericUpDownMaxCommandsPerBatch.Value;
			_settings.SyncOptions.MaxSizePerBatch = (int)numericUpDownMaxSizePerBatch.Value * 1024 * 1024;
			_settings.SyncOptions.NumSyncErrorRetries = (int)numericUpDownRetriesOnSyncError.Value;
			_settings.Save(_logger);

			DialogResult = System.Windows.Forms.DialogResult.OK;
			Close();
		}

		private void CancButton_Click(object sender, EventArgs e)
		{
			DialogResult = System.Windows.Forms.DialogResult.Cancel;
			Close();
		}

		private void ResetButton_Click(object sender, EventArgs e)
		{
			PerforceSyncOptions syncOptions = _settings.SyncOptions;
			numericUpDownNumRetries.Value = PerforceSyncOptions.DefaultNumRetries;
			numericUpDownTcpBufferSize.Value = PerforceSyncOptions.DefaultTcpBufferSize / 1024;
			numericUpDownFileBufferSize.Value = PerforceSyncOptions.DefaultFileBufferSize / 1024;
			numericUpDownMaxCommandsPerBatch.Value = PerforceSyncOptions.DefaultMaxCommandsPerBatch;
			numericUpDownMaxSizePerBatch.Value = PerforceSyncOptions.DefaultMaxSizePerBatch / 1024 / 1024;
			numericUpDownRetriesOnSyncError.Value = PerforceSyncOptions.DefaultNumSyncErrorRetries;
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows.Forms;
using Microsoft.Extensions.Logging;

namespace UnrealGameSync
{
	partial class PerforceSyncSettingsWindow : Form
	{
		readonly UserSettings _settings;
		readonly ILogger _logger;

		public PerforceSyncSettingsWindow(UserSettings settings, ILogger logger)
		{
			_settings = settings;
			_logger = logger;

			InitializeComponent();
			Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
		}

		private void PerforceSettingsWindow_Load(object sender, EventArgs e)
		{
			PerforceSyncOptions syncOptions = _settings.SyncOptions;
			numericUpDownMaxCommandsPerBatch.Value = syncOptions.MaxCommandsPerBatch ?? PerforceSyncOptions.DefaultMaxCommandsPerBatch;
			numericUpDownMaxSizePerBatch.Value = (syncOptions.MaxSizePerBatch ?? PerforceSyncOptions.DefaultMaxSizePerBatch) / 1024 / 1024;
			numericUpDownRetriesOnSyncError.Value = syncOptions.NumSyncErrorRetries ?? PerforceSyncOptions.DefaultNumSyncErrorRetries;
		}

		private void OkButton_Click(object sender, EventArgs e)
		{
			_settings.SyncOptions.MaxCommandsPerBatch = GetValueIfNotDefault((int)numericUpDownMaxCommandsPerBatch.Value, PerforceSyncOptions.DefaultMaxCommandsPerBatch);
			_settings.SyncOptions.MaxSizePerBatch = GetValueIfNotDefault((int)numericUpDownMaxSizePerBatch.Value, PerforceSyncOptions.DefaultMaxSizePerBatch) * 1024 * 1024;
			_settings.SyncOptions.NumSyncErrorRetries = GetValueIfNotDefault((int)numericUpDownRetriesOnSyncError.Value, PerforceSyncOptions.DefaultNumSyncErrorRetries);
			_settings.Save(_logger);

			DialogResult = System.Windows.Forms.DialogResult.OK;
			Close();
		}

		private static int? GetValueIfNotDefault(int value, int defaultValue)
		{
			return (value == defaultValue) ? (int?)null : value;
		}

		private void CancButton_Click(object sender, EventArgs e)
		{
			DialogResult = System.Windows.Forms.DialogResult.Cancel;
			Close();
		}

		private void ResetButton_Click(object sender, EventArgs e)
		{
			numericUpDownMaxCommandsPerBatch.Value = PerforceSyncOptions.DefaultMaxCommandsPerBatch;
			numericUpDownMaxSizePerBatch.Value = PerforceSyncOptions.DefaultMaxSizePerBatch / 1024 / 1024;
			numericUpDownRetriesOnSyncError.Value = PerforceSyncOptions.DefaultNumSyncErrorRetries;
		}
	}
}

namespace UnrealGameSync
{
	partial class PerforceSyncSettingsWindow
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(PerforceSyncSettingsWindow));
			this.OkButton = new System.Windows.Forms.Button();
			this.CancButton = new System.Windows.Forms.Button();
			this.ResetButton = new System.Windows.Forms.Button();
			this.labelNumRetries = new System.Windows.Forms.Label();
			this.labelTcpBufferSize = new System.Windows.Forms.Label();
			this.labelMaxCommandsPerBatch = new System.Windows.Forms.Label();
			this.labelMaxSizePerBatch = new System.Windows.Forms.Label();
			this.numericUpDownMaxSizePerBatch = new System.Windows.Forms.NumericUpDown();
			this.numericUpDownMaxCommandsPerBatch = new System.Windows.Forms.NumericUpDown();
			this.numericUpDownNumRetries = new System.Windows.Forms.NumericUpDown();
			this.numericUpDownTcpBufferSize = new System.Windows.Forms.NumericUpDown();
			this.labelFileBufferSize = new System.Windows.Forms.Label();
			this.numericUpDownFileBufferSize = new System.Windows.Forms.NumericUpDown();
			this.groupBoxSyncing = new System.Windows.Forms.GroupBox();
			this.numericUpDownRetriesOnSyncError = new System.Windows.Forms.NumericUpDown();
			this.labelRetriesOnSyncError = new System.Windows.Forms.Label();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownMaxSizePerBatch)).BeginInit();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownMaxCommandsPerBatch)).BeginInit();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownNumRetries)).BeginInit();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownTcpBufferSize)).BeginInit();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownFileBufferSize)).BeginInit();
			this.groupBoxSyncing.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownRetriesOnSyncError)).BeginInit();
			this.SuspendLayout();
			// 
			// OkButton
			// 
			this.OkButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkButton.Location = new System.Drawing.Point(348, 280);
			this.OkButton.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.OkButton.Name = "OkButton";
			this.OkButton.Size = new System.Drawing.Size(109, 32);
			this.OkButton.TabIndex = 12;
			this.OkButton.Text = "Ok";
			this.OkButton.UseVisualStyleBackColor = true;
			this.OkButton.Click += new System.EventHandler(this.OkButton_Click);
			// 
			// CancButton
			// 
			this.CancButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancButton.Location = new System.Drawing.Point(464, 280);
			this.CancButton.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.CancButton.Name = "CancButton";
			this.CancButton.Size = new System.Drawing.Size(109, 32);
			this.CancButton.TabIndex = 13;
			this.CancButton.Text = "Cancel";
			this.CancButton.UseVisualStyleBackColor = true;
			this.CancButton.Click += new System.EventHandler(this.CancButton_Click);
			// 
			// ResetButton
			// 
			this.ResetButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.ResetButton.Location = new System.Drawing.Point(15, 280);
			this.ResetButton.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.ResetButton.Name = "ResetButton";
			this.ResetButton.Size = new System.Drawing.Size(149, 32);
			this.ResetButton.TabIndex = 11;
			this.ResetButton.Text = "Reset to Default";
			this.ResetButton.UseVisualStyleBackColor = true;
			this.ResetButton.Click += new System.EventHandler(this.ResetButton_Click);
			// 
			// labelNumRetries
			// 
			this.labelNumRetries.AutoSize = true;
			this.labelNumRetries.Location = new System.Drawing.Point(8, 30);
			this.labelNumRetries.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.labelNumRetries.Name = "labelNumRetries";
			this.labelNumRetries.Size = new System.Drawing.Size(213, 20);
			this.labelNumRetries.TabIndex = 1;
			this.labelNumRetries.Text = "Number of Retries on Timeout:";
			// 
			// labelTcpBufferSize
			// 
			this.labelTcpBufferSize.AutoSize = true;
			this.labelTcpBufferSize.Location = new System.Drawing.Point(8, 66);
			this.labelTcpBufferSize.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.labelTcpBufferSize.Name = "labelTcpBufferSize";
			this.labelTcpBufferSize.Size = new System.Drawing.Size(143, 20);
			this.labelTcpBufferSize.TabIndex = 3;
			this.labelTcpBufferSize.Text = "TCP Buffer Size (KB):";
			// 
			// labelMaxCommandsPerBatch
			// 
			this.labelMaxCommandsPerBatch.AutoSize = true;
			this.labelMaxCommandsPerBatch.Location = new System.Drawing.Point(8, 139);
			this.labelMaxCommandsPerBatch.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.labelMaxCommandsPerBatch.Name = "labelMaxCommandsPerBatch";
			this.labelMaxCommandsPerBatch.Size = new System.Drawing.Size(184, 20);
			this.labelMaxCommandsPerBatch.TabIndex = 5;
			this.labelMaxCommandsPerBatch.Text = "Max Commands Per Batch:";
			// 
			// labelMaxSizePerBatch
			// 
			this.labelMaxSizePerBatch.AutoSize = true;
			this.labelMaxSizePerBatch.Location = new System.Drawing.Point(8, 175);
			this.labelMaxSizePerBatch.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.labelMaxSizePerBatch.Name = "labelMaxSizePerBatch";
			this.labelMaxSizePerBatch.Size = new System.Drawing.Size(172, 20);
			this.labelMaxSizePerBatch.TabIndex = 7;
			this.labelMaxSizePerBatch.Text = "Max Size Per Batch (MB):";
			// 
			// numericUpDownMaxSizePerBatch
			// 
			this.numericUpDownMaxSizePerBatch.Increment = new decimal(new int[] {
			64,
			0,
			0,
			0});
			this.numericUpDownMaxSizePerBatch.Location = new System.Drawing.Point(325, 172);
			this.numericUpDownMaxSizePerBatch.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.numericUpDownMaxSizePerBatch.Maximum = new decimal(new int[] {
			8192,
			0,
			0,
			0});
			this.numericUpDownMaxSizePerBatch.Name = "numericUpDownMaxSizePerBatch";
			this.numericUpDownMaxSizePerBatch.Size = new System.Drawing.Size(225, 27);
			this.numericUpDownMaxSizePerBatch.TabIndex = 8;
			// 
			// numericUpDownMaxCommandsPerBatch
			// 
			this.numericUpDownMaxCommandsPerBatch.Increment = new decimal(new int[] {
			50,
			0,
			0,
			0});
			this.numericUpDownMaxCommandsPerBatch.Location = new System.Drawing.Point(325, 136);
			this.numericUpDownMaxCommandsPerBatch.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.numericUpDownMaxCommandsPerBatch.Maximum = new decimal(new int[] {
			1000,
			0,
			0,
			0});
			this.numericUpDownMaxCommandsPerBatch.Name = "numericUpDownMaxCommandsPerBatch";
			this.numericUpDownMaxCommandsPerBatch.Size = new System.Drawing.Size(225, 27);
			this.numericUpDownMaxCommandsPerBatch.TabIndex = 6;
			// 
			// numericUpDownNumRetries
			// 
			this.numericUpDownNumRetries.Location = new System.Drawing.Point(325, 28);
			this.numericUpDownNumRetries.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.numericUpDownNumRetries.Name = "numericUpDownNumRetries";
			this.numericUpDownNumRetries.Size = new System.Drawing.Size(225, 27);
			this.numericUpDownNumRetries.TabIndex = 2;
			// 
			// numericUpDownTcpBufferSize
			// 
			this.numericUpDownTcpBufferSize.Increment = new decimal(new int[] {
			512,
			0,
			0,
			0});
			this.numericUpDownTcpBufferSize.Location = new System.Drawing.Point(325, 64);
			this.numericUpDownTcpBufferSize.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.numericUpDownTcpBufferSize.Maximum = new decimal(new int[] {
			8192,
			0,
			0,
			0});
			this.numericUpDownTcpBufferSize.Name = "numericUpDownTcpBufferSize";
			this.numericUpDownTcpBufferSize.Size = new System.Drawing.Size(225, 27);
			this.numericUpDownTcpBufferSize.TabIndex = 4;
			// 
			// labelFileBufferSize
			// 
			this.labelFileBufferSize.AutoSize = true;
			this.labelFileBufferSize.Location = new System.Drawing.Point(8, 102);
			this.labelFileBufferSize.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.labelFileBufferSize.Name = "labelFileBufferSize";
			this.labelFileBufferSize.Size = new System.Drawing.Size(142, 20);
			this.labelFileBufferSize.TabIndex = 11;
			this.labelFileBufferSize.Text = "File Buffer Size (KB):";
			// 
			// numericUpDownFileBufferSize
			// 
			this.numericUpDownFileBufferSize.Increment = new decimal(new int[] {
			512,
			0,
			0,
			0});
			this.numericUpDownFileBufferSize.Location = new System.Drawing.Point(325, 100);
			this.numericUpDownFileBufferSize.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.numericUpDownFileBufferSize.Maximum = new decimal(new int[] {
			8192,
			0,
			0,
			0});
			this.numericUpDownFileBufferSize.Name = "numericUpDownFileBufferSize";
			this.numericUpDownFileBufferSize.Size = new System.Drawing.Size(225, 27);
			this.numericUpDownFileBufferSize.TabIndex = 12;
			// 
			// groupBoxSyncing
			// 
			this.groupBoxSyncing.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
			| System.Windows.Forms.AnchorStyles.Left) 
			| System.Windows.Forms.AnchorStyles.Right)));
			this.groupBoxSyncing.Controls.Add(this.numericUpDownRetriesOnSyncError);
			this.groupBoxSyncing.Controls.Add(this.labelRetriesOnSyncError);
			this.groupBoxSyncing.Controls.Add(this.numericUpDownFileBufferSize);
			this.groupBoxSyncing.Controls.Add(this.labelFileBufferSize);
			this.groupBoxSyncing.Controls.Add(this.numericUpDownTcpBufferSize);
			this.groupBoxSyncing.Controls.Add(this.numericUpDownNumRetries);
			this.groupBoxSyncing.Controls.Add(this.numericUpDownMaxCommandsPerBatch);
			this.groupBoxSyncing.Controls.Add(this.numericUpDownMaxSizePerBatch);
			this.groupBoxSyncing.Controls.Add(this.labelMaxSizePerBatch);
			this.groupBoxSyncing.Controls.Add(this.labelMaxCommandsPerBatch);
			this.groupBoxSyncing.Controls.Add(this.labelTcpBufferSize);
			this.groupBoxSyncing.Controls.Add(this.labelNumRetries);
			this.groupBoxSyncing.Location = new System.Drawing.Point(15, 15);
			this.groupBoxSyncing.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.groupBoxSyncing.Name = "groupBoxSyncing";
			this.groupBoxSyncing.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.groupBoxSyncing.Size = new System.Drawing.Size(558, 246);
			this.groupBoxSyncing.TabIndex = 0;
			this.groupBoxSyncing.TabStop = false;
			this.groupBoxSyncing.Text = "Syncing";
			// 
			// numericUpDownRetriesOnSyncError
			// 
			this.numericUpDownRetriesOnSyncError.Increment = new decimal(new int[] {
			64,
			0,
			0,
			0});
			this.numericUpDownRetriesOnSyncError.Location = new System.Drawing.Point(325, 207);
			this.numericUpDownRetriesOnSyncError.Margin = new System.Windows.Forms.Padding(4);
			this.numericUpDownRetriesOnSyncError.Maximum = new decimal(new int[] {
			8192,
			0,
			0,
			0});
			this.numericUpDownRetriesOnSyncError.Name = "numericUpDownRetriesOnSyncError";
			this.numericUpDownRetriesOnSyncError.Size = new System.Drawing.Size(225, 27);
			this.numericUpDownRetriesOnSyncError.TabIndex = 14;
			// 
			// labelRetriesOnSyncError
			// 
			this.labelRetriesOnSyncError.AutoSize = true;
			this.labelRetriesOnSyncError.Location = new System.Drawing.Point(8, 210);
			this.labelRetriesOnSyncError.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.labelRetriesOnSyncError.Name = "labelRetriesOnSyncError";
			this.labelRetriesOnSyncError.Size = new System.Drawing.Size(146, 20);
			this.labelRetriesOnSyncError.TabIndex = 13;
			this.labelRetriesOnSyncError.Text = "Retries on sync error:";
			// 
			// PerforceSyncSettingsWindow
			// 
			this.AcceptButton = this.OkButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(120F, 120F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancButton;
			this.ClientSize = new System.Drawing.Size(588, 326);
			this.ControlBox = false;
			this.Controls.Add(this.ResetButton);
			this.Controls.Add(this.CancButton);
			this.Controls.Add(this.OkButton);
			this.Controls.Add(this.groupBoxSyncing);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.Name = "PerforceSyncSettingsWindow";
			this.ShowInTaskbar = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Perforce Sync Settings";
			this.Load += new System.EventHandler(this.PerforceSettingsWindow_Load);
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownMaxSizePerBatch)).EndInit();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownMaxCommandsPerBatch)).EndInit();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownNumRetries)).EndInit();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownTcpBufferSize)).EndInit();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownFileBufferSize)).EndInit();
			this.groupBoxSyncing.ResumeLayout(false);
			this.groupBoxSyncing.PerformLayout();
			((System.ComponentModel.ISupportInitialize)(this.numericUpDownRetriesOnSyncError)).EndInit();
			this.ResumeLayout(false);

		}

		#endregion
		private System.Windows.Forms.Button OkButton;
		private System.Windows.Forms.Button CancButton;
		private System.Windows.Forms.Button ResetButton;
		private System.Windows.Forms.Label labelNumRetries;
		private System.Windows.Forms.Label labelTcpBufferSize;
		private System.Windows.Forms.Label labelMaxCommandsPerBatch;
		private System.Windows.Forms.Label labelMaxSizePerBatch;
		private System.Windows.Forms.NumericUpDown numericUpDownMaxSizePerBatch;
		private System.Windows.Forms.NumericUpDown numericUpDownMaxCommandsPerBatch;
		private System.Windows.Forms.NumericUpDown numericUpDownNumRetries;
		private System.Windows.Forms.NumericUpDown numericUpDownTcpBufferSize;
		private System.Windows.Forms.Label labelFileBufferSize;
		private System.Windows.Forms.NumericUpDown numericUpDownFileBufferSize;
		private System.Windows.Forms.GroupBox groupBoxSyncing;
		private System.Windows.Forms.NumericUpDown numericUpDownRetriesOnSyncError;
		private System.Windows.Forms.Label labelRetriesOnSyncError;
	}
}
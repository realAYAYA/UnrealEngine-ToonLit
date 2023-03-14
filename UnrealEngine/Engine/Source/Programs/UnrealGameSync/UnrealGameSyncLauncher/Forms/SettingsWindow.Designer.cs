// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSyncLauncher
{
	partial class SettingsWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(SettingsWindow));
			this.ServerTextBox = new System.Windows.Forms.TextBox();
			this.DepotPathTextBox = new System.Windows.Forms.TextBox();
			this.label1 = new System.Windows.Forms.Label();
			this.DepotPathLabel = new System.Windows.Forms.Label();
			this.PromptLabel = new System.Windows.Forms.Label();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.UsePreviewBuildCheckBox = new System.Windows.Forms.CheckBox();
			this.UserNameTextBox = new System.Windows.Forms.TextBox();
			this.label3 = new System.Windows.Forms.Label();
			this.RetryBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.ViewLogBtn = new System.Windows.Forms.Button();
			this.groupBox1.SuspendLayout();
			this.SuspendLayout();
			// 
			// ServerTextBox
			// 
			this.ServerTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ServerTextBox.Location = new System.Drawing.Point(106, 26);
			this.ServerTextBox.Name = "ServerTextBox";
			this.ServerTextBox.Size = new System.Drawing.Size(699, 23);
			this.ServerTextBox.TabIndex = 1;
			// 
			// DepotPathTextBox
			// 
			this.DepotPathTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.DepotPathTextBox.Location = new System.Drawing.Point(106, 84);
			this.DepotPathTextBox.Name = "DepotPathTextBox";
			this.DepotPathTextBox.Size = new System.Drawing.Size(559, 23);
			this.DepotPathTextBox.TabIndex = 5;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(28, 29);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(42, 15);
			this.label1.TabIndex = 0;
			this.label1.Text = "Server:";
			// 
			// DepotPathLabel
			// 
			this.DepotPathLabel.AutoSize = true;
			this.DepotPathLabel.Location = new System.Drawing.Point(28, 87);
			this.DepotPathLabel.Name = "DepotPathLabel";
			this.DepotPathLabel.Size = new System.Drawing.Size(69, 15);
			this.DepotPathLabel.TabIndex = 4;
			this.DepotPathLabel.Text = "Depot Path:";
			// 
			// PromptLabel
			// 
			this.PromptLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.PromptLabel.AutoSize = true;
			this.PromptLabel.Location = new System.Drawing.Point(12, 19);
			this.PromptLabel.Name = "PromptLabel";
			this.PromptLabel.Size = new System.Drawing.Size(409, 15);
			this.PromptLabel.TabIndex = 0;
			this.PromptLabel.Text = "UnrealGameSync will be updated from Perforce using the following settings.";
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.UsePreviewBuildCheckBox);
			this.groupBox1.Controls.Add(this.UserNameTextBox);
			this.groupBox1.Controls.Add(this.label3);
			this.groupBox1.Controls.Add(this.DepotPathTextBox);
			this.groupBox1.Controls.Add(this.ServerTextBox);
			this.groupBox1.Controls.Add(this.DepotPathLabel);
			this.groupBox1.Controls.Add(this.label1);
			this.groupBox1.Location = new System.Drawing.Point(25, 49);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(838, 129);
			this.groupBox1.TabIndex = 1;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Connection Settings";
			// 
			// UseUnstableBuildCheckBox
			// 
			this.UsePreviewBuildCheckBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.UsePreviewBuildCheckBox.AutoSize = true;
			this.UsePreviewBuildCheckBox.Location = new System.Drawing.Point(681, 86);
			this.UsePreviewBuildCheckBox.Name = "UsePreviewBuildCheckBox";
			this.UsePreviewBuildCheckBox.Size = new System.Drawing.Size(124, 19);
			this.UsePreviewBuildCheckBox.TabIndex = 6;
			this.UsePreviewBuildCheckBox.Text = "Use Preview Build";
			this.UsePreviewBuildCheckBox.UseVisualStyleBackColor = true;
			// 
			// UserNameTextBox
			// 
			this.UserNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.UserNameTextBox.Location = new System.Drawing.Point(106, 55);
			this.UserNameTextBox.Name = "UserNameTextBox";
			this.UserNameTextBox.Size = new System.Drawing.Size(699, 23);
			this.UserNameTextBox.TabIndex = 3;
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(28, 58);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(33, 15);
			this.label3.TabIndex = 2;
			this.label3.Text = "User:";
			// 
			// RetryBtn
			// 
			this.RetryBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.RetryBtn.Location = new System.Drawing.Point(786, 193);
			this.RetryBtn.Name = "RetryBtn";
			this.RetryBtn.Size = new System.Drawing.Size(89, 26);
			this.RetryBtn.TabIndex = 4;
			this.RetryBtn.Text = "Connect";
			this.RetryBtn.UseVisualStyleBackColor = true;
			this.RetryBtn.Click += new System.EventHandler(this.ConnectBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(691, 193);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(89, 26);
			this.CancelBtn.TabIndex = 3;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			this.CancelBtn.Click += new System.EventHandler(this.CancelBtn_Click);
			// 
			// ViewLogBtn
			// 
			this.ViewLogBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.ViewLogBtn.Location = new System.Drawing.Point(12, 193);
			this.ViewLogBtn.Name = "ViewLogBtn";
			this.ViewLogBtn.Size = new System.Drawing.Size(87, 26);
			this.ViewLogBtn.TabIndex = 2;
			this.ViewLogBtn.Text = "View Log";
			this.ViewLogBtn.UseVisualStyleBackColor = true;
			this.ViewLogBtn.Click += new System.EventHandler(this.ViewLogBtn_Click);
			// 
			// SettingsWindow
			// 
			this.AcceptButton = this.RetryBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(887, 231);
			this.Controls.Add(this.ViewLogBtn);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.RetryBtn);
			this.Controls.Add(this.groupBox1);
			this.Controls.Add(this.PromptLabel);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "SettingsWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "UnrealGameSync Launcher";
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TextBox ServerTextBox;
		private System.Windows.Forms.TextBox DepotPathTextBox;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label DepotPathLabel;
		private System.Windows.Forms.Label PromptLabel;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.Button RetryBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button ViewLogBtn;
		private System.Windows.Forms.TextBox UserNameTextBox;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.CheckBox UsePreviewBuildCheckBox;
	}
}
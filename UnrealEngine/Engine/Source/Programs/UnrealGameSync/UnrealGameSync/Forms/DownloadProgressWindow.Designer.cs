// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync.Forms
{
	partial class DownloadProgressWindow
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			StatusMessage = new System.Windows.Forms.Label();
			SuspendLayout();
			// 
			// StatusMessage
			// 
			StatusMessage.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			StatusMessage.Location = new System.Drawing.Point(12, 9);
			StatusMessage.Name = "StatusMessage";
			StatusMessage.Size = new System.Drawing.Size(418, 42);
			StatusMessage.TabIndex = 0;
			StatusMessage.Text = "Downloading files, please wait...";
			StatusMessage.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// DownloadProgressWindow
			// 
			AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			ClientSize = new System.Drawing.Size(440, 60);
			Controls.Add(StatusMessage);
			Icon = Properties.Resources.Icon;
			MaximizeBox = false;
			MinimizeBox = false;
			Name = "DownloadProgressWindow";
			SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
			StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			Text = "Downloading Files...";
			FormClosing += DownloadProgressWindow_FormClosing;
			Load += DownloadProcessWindow_Load;
			ResumeLayout(false);
		}

		#endregion

		private System.Windows.Forms.Label StatusMessage;
	}
}
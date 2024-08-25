// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class UpdateSettingsWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(UpdateSettingsWindow));
			ServerTextBox = new System.Windows.Forms.TextBox();
			DepotPathTextBox = new System.Windows.Forms.TextBox();
			DepotPathLabel = new System.Windows.Forms.Label();
			PromptLabel = new System.Windows.Forms.Label();
			tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			UserNameTextBox = new System.Windows.Forms.TextBox();
			tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			UsePreviewBuildCheckBox = new System.Windows.Forms.CheckBox();
			label3 = new System.Windows.Forms.Label();
			label1 = new System.Windows.Forms.Label();
			PerforceRadioBtn = new System.Windows.Forms.RadioButton();
			RetryBtn = new System.Windows.Forms.Button();
			CancelBtn = new System.Windows.Forms.Button();
			ViewLogBtn = new System.Windows.Forms.Button();
			HordeRadioBtn = new System.Windows.Forms.RadioButton();
			PerforceGroupBox = new System.Windows.Forms.GroupBox();
			HordeGroupBox = new System.Windows.Forms.GroupBox();
			tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
			HordeServerTextBox = new System.Windows.Forms.TextBox();
			label5 = new System.Windows.Forms.Label();
			DisableRadioBtn = new System.Windows.Forms.RadioButton();
			DisableGroupBox = new System.Windows.Forms.GroupBox();
			tableLayoutPanel4 = new System.Windows.Forms.TableLayoutPanel();
			DisableUpdateLabel = new System.Windows.Forms.Label();
			tableLayoutPanel1.SuspendLayout();
			tableLayoutPanel2.SuspendLayout();
			PerforceGroupBox.SuspendLayout();
			HordeGroupBox.SuspendLayout();
			tableLayoutPanel3.SuspendLayout();
			DisableGroupBox.SuspendLayout();
			tableLayoutPanel4.SuspendLayout();
			SuspendLayout();
			// 
			// ServerTextBox
			// 
			ServerTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			ServerTextBox.Location = new System.Drawing.Point(103, 5);
			ServerTextBox.Name = "ServerTextBox";
			ServerTextBox.Size = new System.Drawing.Size(664, 23);
			ServerTextBox.TabIndex = 1;
			// 
			// DepotPathTextBox
			// 
			DepotPathTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			DepotPathTextBox.Location = new System.Drawing.Point(3, 5);
			DepotPathTextBox.Name = "DepotPathTextBox";
			DepotPathTextBox.Size = new System.Drawing.Size(532, 23);
			DepotPathTextBox.TabIndex = 5;
			// 
			// DepotPathLabel
			// 
			DepotPathLabel.Anchor = System.Windows.Forms.AnchorStyles.Left;
			DepotPathLabel.AutoSize = true;
			DepotPathLabel.Location = new System.Drawing.Point(3, 75);
			DepotPathLabel.Name = "DepotPathLabel";
			DepotPathLabel.Size = new System.Drawing.Size(69, 15);
			DepotPathLabel.TabIndex = 4;
			DepotPathLabel.Text = "Depot Path:";
			// 
			// PromptLabel
			// 
			PromptLabel.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			PromptLabel.AutoSize = true;
			PromptLabel.Location = new System.Drawing.Point(12, 16);
			PromptLabel.Name = "PromptLabel";
			PromptLabel.Size = new System.Drawing.Size(333, 15);
			PromptLabel.TabIndex = 0;
			PromptLabel.Text = "UnrealGameSync will be updated using the following settings.";
			// 
			// tableLayoutPanel1
			// 
			tableLayoutPanel1.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			tableLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			tableLayoutPanel1.ColumnCount = 2;
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 100F));
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel1.Controls.Add(UserNameTextBox, 1, 1);
			tableLayoutPanel1.Controls.Add(tableLayoutPanel2, 1, 2);
			tableLayoutPanel1.Controls.Add(ServerTextBox, 1, 0);
			tableLayoutPanel1.Controls.Add(label3, 0, 1);
			tableLayoutPanel1.Controls.Add(DepotPathLabel, 0, 2);
			tableLayoutPanel1.Controls.Add(label1, 0, 0);
			tableLayoutPanel1.Location = new System.Drawing.Point(13, 25);
			tableLayoutPanel1.Name = "tableLayoutPanel1";
			tableLayoutPanel1.RowCount = 3;
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.3333321F));
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.33333F));
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.33334F));
			tableLayoutPanel1.Size = new System.Drawing.Size(770, 100);
			tableLayoutPanel1.TabIndex = 5;
			// 
			// UserNameTextBox
			// 
			UserNameTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			UserNameTextBox.Location = new System.Drawing.Point(103, 38);
			UserNameTextBox.Name = "UserNameTextBox";
			UserNameTextBox.Size = new System.Drawing.Size(664, 23);
			UserNameTextBox.TabIndex = 3;
			// 
			// tableLayoutPanel2
			// 
			tableLayoutPanel2.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			tableLayoutPanel2.AutoSize = true;
			tableLayoutPanel2.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			tableLayoutPanel2.ColumnCount = 2;
			tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			tableLayoutPanel2.Controls.Add(UsePreviewBuildCheckBox, 1, 0);
			tableLayoutPanel2.Controls.Add(DepotPathTextBox, 0, 0);
			tableLayoutPanel2.Location = new System.Drawing.Point(100, 66);
			tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(0);
			tableLayoutPanel2.Name = "tableLayoutPanel2";
			tableLayoutPanel2.RowCount = 1;
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel2.Size = new System.Drawing.Size(670, 34);
			tableLayoutPanel2.TabIndex = 6;
			// 
			// UsePreviewBuildCheckBox
			// 
			UsePreviewBuildCheckBox.Anchor = System.Windows.Forms.AnchorStyles.None;
			UsePreviewBuildCheckBox.AutoSize = true;
			UsePreviewBuildCheckBox.Location = new System.Drawing.Point(548, 7);
			UsePreviewBuildCheckBox.Margin = new System.Windows.Forms.Padding(10, 3, 3, 3);
			UsePreviewBuildCheckBox.Name = "UsePreviewBuildCheckBox";
			UsePreviewBuildCheckBox.Size = new System.Drawing.Size(119, 19);
			UsePreviewBuildCheckBox.TabIndex = 6;
			UsePreviewBuildCheckBox.Text = "Use Preview Build";
			UsePreviewBuildCheckBox.UseVisualStyleBackColor = true;
			// 
			// label3
			// 
			label3.Anchor = System.Windows.Forms.AnchorStyles.Left;
			label3.AutoSize = true;
			label3.Location = new System.Drawing.Point(3, 42);
			label3.Name = "label3";
			label3.Size = new System.Drawing.Size(33, 15);
			label3.TabIndex = 2;
			label3.Text = "User:";
			// 
			// label1
			// 
			label1.Anchor = System.Windows.Forms.AnchorStyles.Left;
			label1.AutoSize = true;
			label1.Location = new System.Drawing.Point(3, 9);
			label1.Name = "label1";
			label1.Size = new System.Drawing.Size(42, 15);
			label1.TabIndex = 7;
			label1.Text = "Server:";
			// 
			// PerforceRadioBtn
			// 
			PerforceRadioBtn.AutoSize = true;
			PerforceRadioBtn.Location = new System.Drawing.Point(28, 135);
			PerforceRadioBtn.Name = "PerforceRadioBtn";
			PerforceRadioBtn.Size = new System.Drawing.Size(69, 19);
			PerforceRadioBtn.TabIndex = 0;
			PerforceRadioBtn.TabStop = true;
			PerforceRadioBtn.Text = "Perforce";
			PerforceRadioBtn.UseVisualStyleBackColor = true;
			PerforceRadioBtn.CheckedChanged += PerforceRadioBtn_CheckedChanged;
			// 
			// RetryBtn
			// 
			RetryBtn.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right;
			RetryBtn.AutoSize = true;
			RetryBtn.Location = new System.Drawing.Point(689, 368);
			RetryBtn.Name = "RetryBtn";
			RetryBtn.Padding = new System.Windows.Forms.Padding(20, 0, 20, 0);
			RetryBtn.Size = new System.Drawing.Size(119, 30);
			RetryBtn.TabIndex = 0;
			RetryBtn.Text = "Continue";
			RetryBtn.UseVisualStyleBackColor = true;
			RetryBtn.Click += ConnectBtn_Click;
			// 
			// CancelBtn
			// 
			CancelBtn.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right;
			CancelBtn.AutoSize = true;
			CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			CancelBtn.Location = new System.Drawing.Point(564, 368);
			CancelBtn.Name = "CancelBtn";
			CancelBtn.Padding = new System.Windows.Forms.Padding(20, 0, 20, 0);
			CancelBtn.Size = new System.Drawing.Size(119, 30);
			CancelBtn.TabIndex = 3;
			CancelBtn.Text = "Cancel";
			CancelBtn.UseVisualStyleBackColor = true;
			CancelBtn.Click += CancelBtn_Click;
			// 
			// ViewLogBtn
			// 
			ViewLogBtn.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left;
			ViewLogBtn.Location = new System.Drawing.Point(12, 368);
			ViewLogBtn.Name = "ViewLogBtn";
			ViewLogBtn.Size = new System.Drawing.Size(113, 30);
			ViewLogBtn.TabIndex = 2;
			ViewLogBtn.Text = "View Log";
			ViewLogBtn.UseVisualStyleBackColor = true;
			ViewLogBtn.Click += ViewLogBtn_Click;
			// 
			// HordeRadioBtn
			// 
			HordeRadioBtn.AutoSize = true;
			HordeRadioBtn.Location = new System.Drawing.Point(28, 49);
			HordeRadioBtn.Name = "HordeRadioBtn";
			HordeRadioBtn.Size = new System.Drawing.Size(58, 19);
			HordeRadioBtn.TabIndex = 0;
			HordeRadioBtn.TabStop = true;
			HordeRadioBtn.Text = "Horde";
			HordeRadioBtn.UseVisualStyleBackColor = true;
			HordeRadioBtn.CheckedChanged += HordeRadioBtn_CheckedChanged;
			// 
			// PerforceGroupBox
			// 
			PerforceGroupBox.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			PerforceGroupBox.Controls.Add(tableLayoutPanel1);
			PerforceGroupBox.Location = new System.Drawing.Point(12, 135);
			PerforceGroupBox.Name = "PerforceGroupBox";
			PerforceGroupBox.Size = new System.Drawing.Size(796, 142);
			PerforceGroupBox.TabIndex = 9;
			PerforceGroupBox.TabStop = false;
			// 
			// HordeGroupBox
			// 
			HordeGroupBox.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			HordeGroupBox.Controls.Add(tableLayoutPanel3);
			HordeGroupBox.Location = new System.Drawing.Point(12, 49);
			HordeGroupBox.Name = "HordeGroupBox";
			HordeGroupBox.Size = new System.Drawing.Size(796, 80);
			HordeGroupBox.TabIndex = 10;
			HordeGroupBox.TabStop = false;
			// 
			// tableLayoutPanel3
			// 
			tableLayoutPanel3.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			tableLayoutPanel3.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			tableLayoutPanel3.ColumnCount = 2;
			tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 100F));
			tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel3.Controls.Add(HordeServerTextBox, 1, 0);
			tableLayoutPanel3.Controls.Add(label5, 0, 0);
			tableLayoutPanel3.Location = new System.Drawing.Point(13, 25);
			tableLayoutPanel3.Name = "tableLayoutPanel3";
			tableLayoutPanel3.RowCount = 1;
			tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
			tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
			tableLayoutPanel3.Size = new System.Drawing.Size(770, 40);
			tableLayoutPanel3.TabIndex = 6;
			// 
			// HordeServerTextBox
			// 
			HordeServerTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			HordeServerTextBox.Location = new System.Drawing.Point(103, 8);
			HordeServerTextBox.Name = "HordeServerTextBox";
			HordeServerTextBox.Size = new System.Drawing.Size(664, 23);
			HordeServerTextBox.TabIndex = 1;
			// 
			// label5
			// 
			label5.Anchor = System.Windows.Forms.AnchorStyles.Left;
			label5.AutoSize = true;
			label5.Location = new System.Drawing.Point(3, 12);
			label5.Name = "label5";
			label5.Size = new System.Drawing.Size(42, 15);
			label5.TabIndex = 7;
			label5.Text = "Server:";
			// 
			// DisableRadioBtn
			// 
			DisableRadioBtn.AutoSize = true;
			DisableRadioBtn.Location = new System.Drawing.Point(28, 285);
			DisableRadioBtn.Margin = new System.Windows.Forms.Padding(6);
			DisableRadioBtn.Name = "DisableRadioBtn";
			DisableRadioBtn.Size = new System.Drawing.Size(63, 19);
			DisableRadioBtn.TabIndex = 11;
			DisableRadioBtn.TabStop = true;
			DisableRadioBtn.Text = "Disable";
			DisableRadioBtn.UseVisualStyleBackColor = true;
			DisableRadioBtn.CheckedChanged += DisableRadioBtn_CheckedChanged;
			// 
			// DisableGroupBox
			// 
			DisableGroupBox.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			DisableGroupBox.Controls.Add(tableLayoutPanel4);
			DisableGroupBox.Location = new System.Drawing.Point(12, 286);
			DisableGroupBox.Margin = new System.Windows.Forms.Padding(6);
			DisableGroupBox.Name = "DisableGroupBox";
			DisableGroupBox.Padding = new System.Windows.Forms.Padding(6);
			DisableGroupBox.Size = new System.Drawing.Size(796, 64);
			DisableGroupBox.TabIndex = 12;
			DisableGroupBox.TabStop = false;
			// 
			// tableLayoutPanel4
			// 
			tableLayoutPanel4.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			tableLayoutPanel4.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			tableLayoutPanel4.ColumnCount = 1;
			tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 200F));
			tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel4.Controls.Add(DisableUpdateLabel, 0, 0);
			tableLayoutPanel4.Location = new System.Drawing.Point(13, 21);
			tableLayoutPanel4.Margin = new System.Windows.Forms.Padding(6);
			tableLayoutPanel4.Name = "tableLayoutPanel4";
			tableLayoutPanel4.RowCount = 1;
			tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 80F));
			tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 80F));
			tableLayoutPanel4.Size = new System.Drawing.Size(770, 28);
			tableLayoutPanel4.TabIndex = 7;
			// 
			// DisableUpdateLabel
			// 
			DisableUpdateLabel.Anchor = System.Windows.Forms.AnchorStyles.Left;
			DisableUpdateLabel.AutoSize = true;
			DisableUpdateLabel.Location = new System.Drawing.Point(6, 6);
			DisableUpdateLabel.Margin = new System.Windows.Forms.Padding(6, 0, 6, 0);
			DisableUpdateLabel.Name = "DisableUpdateLabel";
			DisableUpdateLabel.Size = new System.Drawing.Size(365, 15);
			DisableUpdateLabel.TabIndex = 7;
			DisableUpdateLabel.Text = "Disable auto-update functionality and run the locally installed build.";
			// 
			// UpdateSettingsWindow
			// 
			AcceptButton = RetryBtn;
			AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			CancelButton = CancelBtn;
			ClientSize = new System.Drawing.Size(820, 410);
			MinimumSize = new System.Drawing.Size(836, 449);
			Controls.Add(DisableRadioBtn);
			Controls.Add(DisableGroupBox);
			Controls.Add(PerforceRadioBtn);
			Controls.Add(HordeRadioBtn);
			Controls.Add(HordeGroupBox);
			Controls.Add(PerforceGroupBox);
			Controls.Add(RetryBtn);
			Controls.Add(CancelBtn);
			Controls.Add(ViewLogBtn);
			Controls.Add(PromptLabel);
			Icon = (System.Drawing.Icon)resources.GetObject("$this.Icon");
			MaximizeBox = false;
			MinimizeBox = false;
			Name = "UpdateSettingsWindow";
			StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			Text = "Update Settings";
			tableLayoutPanel1.ResumeLayout(false);
			tableLayoutPanel1.PerformLayout();
			tableLayoutPanel2.ResumeLayout(false);
			tableLayoutPanel2.PerformLayout();
			PerforceGroupBox.ResumeLayout(false);
			HordeGroupBox.ResumeLayout(false);
			tableLayoutPanel3.ResumeLayout(false);
			tableLayoutPanel3.PerformLayout();
			DisableGroupBox.ResumeLayout(false);
			tableLayoutPanel4.ResumeLayout(false);
			tableLayoutPanel4.PerformLayout();
			ResumeLayout(false);
			PerformLayout();
		}

		#endregion

		private System.Windows.Forms.TextBox ServerTextBox;
		private System.Windows.Forms.TextBox DepotPathTextBox;
		private System.Windows.Forms.Label DepotPathLabel;
		private System.Windows.Forms.Label PromptLabel;
		private System.Windows.Forms.Button RetryBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button ViewLogBtn;
		private System.Windows.Forms.TextBox UserNameTextBox;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.CheckBox UsePreviewBuildCheckBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.RadioButton PerforceRadioBtn;
		private System.Windows.Forms.RadioButton HordeRadioBtn;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.GroupBox PerforceGroupBox;
		private System.Windows.Forms.GroupBox HordeGroupBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
		private System.Windows.Forms.TextBox HordeServerTextBox;
		private System.Windows.Forms.Label label5;
		private System.Windows.Forms.RadioButton DisableRadioBtn;
		private System.Windows.Forms.GroupBox DisableGroupBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel4;
		private System.Windows.Forms.Label DisableUpdateLabel;
	}
}
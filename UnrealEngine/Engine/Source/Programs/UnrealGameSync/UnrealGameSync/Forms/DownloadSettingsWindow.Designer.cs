// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync.Forms
{
	partial class DownloadSettingsWindow
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
			DownloadBtn = new System.Windows.Forms.Button();
			CancelBtn = new System.Windows.Forms.Button();
			OutputFolderBrowseBtn = new System.Windows.Forms.Button();
			label1 = new System.Windows.Forms.Label();
			OutputFolderText = new System.Windows.Forms.TextBox();
			tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			tableLayoutPanel4 = new System.Windows.Forms.TableLayoutPanel();
			tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
			label2 = new System.Windows.Forms.Label();
			SourceText = new System.Windows.Forms.TextBox();
			tableLayoutPanel1.SuspendLayout();
			tableLayoutPanel2.SuspendLayout();
			tableLayoutPanel4.SuspendLayout();
			tableLayoutPanel3.SuspendLayout();
			SuspendLayout();
			// 
			// DownloadBtn
			// 
			DownloadBtn.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right;
			DownloadBtn.DialogResult = System.Windows.Forms.DialogResult.OK;
			DownloadBtn.Location = new System.Drawing.Point(107, 3);
			DownloadBtn.Name = "DownloadBtn";
			DownloadBtn.Size = new System.Drawing.Size(98, 29);
			DownloadBtn.TabIndex = 0;
			DownloadBtn.Text = "Download";
			DownloadBtn.UseVisualStyleBackColor = true;
			// 
			// CancelBtn
			// 
			CancelBtn.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right;
			CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			CancelBtn.Location = new System.Drawing.Point(3, 3);
			CancelBtn.Name = "CancelBtn";
			CancelBtn.Size = new System.Drawing.Size(98, 29);
			CancelBtn.TabIndex = 1;
			CancelBtn.Text = "Cancel";
			CancelBtn.UseVisualStyleBackColor = true;
			// 
			// OutputFolderBrowseBtn
			// 
			OutputFolderBrowseBtn.Anchor = System.Windows.Forms.AnchorStyles.Left;
			OutputFolderBrowseBtn.Location = new System.Drawing.Point(598, 3);
			OutputFolderBrowseBtn.Name = "OutputFolderBrowseBtn";
			OutputFolderBrowseBtn.Size = new System.Drawing.Size(33, 23);
			OutputFolderBrowseBtn.TabIndex = 2;
			OutputFolderBrowseBtn.Text = "...";
			OutputFolderBrowseBtn.UseVisualStyleBackColor = true;
			OutputFolderBrowseBtn.Click += OutputFolderBrowseBtn_Click;
			// 
			// label1
			// 
			label1.Anchor = System.Windows.Forms.AnchorStyles.Left;
			label1.AutoSize = true;
			label1.Location = new System.Drawing.Point(3, 7);
			label1.Name = "label1";
			label1.Size = new System.Drawing.Size(82, 15);
			label1.TabIndex = 3;
			label1.Text = "Output folder:";
			// 
			// OutputFolderText
			// 
			OutputFolderText.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			OutputFolderText.Location = new System.Drawing.Point(91, 3);
			OutputFolderText.Name = "OutputFolderText";
			OutputFolderText.Size = new System.Drawing.Size(501, 23);
			OutputFolderText.TabIndex = 4;
			// 
			// tableLayoutPanel1
			// 
			tableLayoutPanel1.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			tableLayoutPanel1.AutoSize = true;
			tableLayoutPanel1.ColumnCount = 3;
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			tableLayoutPanel1.Controls.Add(label1, 0, 0);
			tableLayoutPanel1.Controls.Add(OutputFolderBrowseBtn, 2, 0);
			tableLayoutPanel1.Controls.Add(OutputFolderText, 1, 0);
			tableLayoutPanel1.Location = new System.Drawing.Point(0, 41);
			tableLayoutPanel1.Margin = new System.Windows.Forms.Padding(0, 4, 0, 4);
			tableLayoutPanel1.Name = "tableLayoutPanel1";
			tableLayoutPanel1.RowCount = 1;
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
			tableLayoutPanel1.Size = new System.Drawing.Size(634, 29);
			tableLayoutPanel1.TabIndex = 5;
			// 
			// tableLayoutPanel2
			// 
			tableLayoutPanel2.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			tableLayoutPanel2.AutoSize = true;
			tableLayoutPanel2.ColumnCount = 1;
			tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel2.Controls.Add(tableLayoutPanel4, 0, 2);
			tableLayoutPanel2.Controls.Add(tableLayoutPanel3, 0, 0);
			tableLayoutPanel2.Controls.Add(tableLayoutPanel1, 0, 1);
			tableLayoutPanel2.Location = new System.Drawing.Point(12, 12);
			tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(0);
			tableLayoutPanel2.Name = "tableLayoutPanel2";
			tableLayoutPanel2.RowCount = 3;
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
			tableLayoutPanel2.Size = new System.Drawing.Size(634, 115);
			tableLayoutPanel2.TabIndex = 6;
			// 
			// tableLayoutPanel4
			// 
			tableLayoutPanel4.Anchor = System.Windows.Forms.AnchorStyles.Right;
			tableLayoutPanel4.AutoSize = true;
			tableLayoutPanel4.ColumnCount = 2;
			tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel4.Controls.Add(CancelBtn, 0, 0);
			tableLayoutPanel4.Controls.Add(DownloadBtn, 1, 0);
			tableLayoutPanel4.Location = new System.Drawing.Point(426, 77);
			tableLayoutPanel4.Margin = new System.Windows.Forms.Padding(0, 3, 0, 3);
			tableLayoutPanel4.Name = "tableLayoutPanel4";
			tableLayoutPanel4.RowCount = 1;
			tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel4.Size = new System.Drawing.Size(208, 35);
			tableLayoutPanel4.TabIndex = 7;
			// 
			// tableLayoutPanel3
			// 
			tableLayoutPanel3.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			tableLayoutPanel3.AutoSize = true;
			tableLayoutPanel3.ColumnCount = 2;
			tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 20F));
			tableLayoutPanel3.Controls.Add(label2, 0, 0);
			tableLayoutPanel3.Controls.Add(SourceText, 1, 0);
			tableLayoutPanel3.Location = new System.Drawing.Point(0, 4);
			tableLayoutPanel3.Margin = new System.Windows.Forms.Padding(0, 4, 0, 4);
			tableLayoutPanel3.Name = "tableLayoutPanel3";
			tableLayoutPanel3.RowCount = 1;
			tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel3.Size = new System.Drawing.Size(634, 29);
			tableLayoutPanel3.TabIndex = 6;
			// 
			// label2
			// 
			label2.Anchor = System.Windows.Forms.AnchorStyles.Left;
			label2.AutoSize = true;
			label2.Location = new System.Drawing.Point(3, 7);
			label2.Name = "label2";
			label2.Size = new System.Drawing.Size(46, 15);
			label2.TabIndex = 3;
			label2.Text = "Source:";
			// 
			// SourceText
			// 
			SourceText.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			SourceText.Location = new System.Drawing.Point(55, 3);
			SourceText.Name = "SourceText";
			SourceText.ReadOnly = true;
			SourceText.Size = new System.Drawing.Size(576, 23);
			SourceText.TabIndex = 4;
			// 
			// DownloadSettingsWindow
			// 
			AcceptButton = DownloadBtn;
			AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			CancelButton = CancelBtn;
			ClientSize = new System.Drawing.Size(658, 141);
			Controls.Add(tableLayoutPanel2);
			Icon = Properties.Resources.Icon;
			MaximizeBox = false;
			MinimizeBox = false;
			MinimumSize = new System.Drawing.Size(300, 0);
			Name = "DownloadSettingsWindow";
			StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			Text = "Download Build";
			Load += DownloadSettingsWindow_Load;
			tableLayoutPanel1.ResumeLayout(false);
			tableLayoutPanel1.PerformLayout();
			tableLayoutPanel2.ResumeLayout(false);
			tableLayoutPanel2.PerformLayout();
			tableLayoutPanel4.ResumeLayout(false);
			tableLayoutPanel3.ResumeLayout(false);
			tableLayoutPanel3.PerformLayout();
			ResumeLayout(false);
			PerformLayout();
		}

		#endregion

		private System.Windows.Forms.Button DownloadBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button OutputFolderBrowseBtn;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.TextBox OutputFolderText;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel4;
		private System.Windows.Forms.TextBox SourceText;
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class ApplicationSettingsWindow
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
			label1 = new System.Windows.Forms.Label();
			groupBox1 = new System.Windows.Forms.GroupBox();
			tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			tableLayoutPanel6 = new System.Windows.Forms.TableLayoutPanel();
			ParallelSyncThreadsSpinner = new System.Windows.Forms.NumericUpDown();
			AdvancedBtn = new System.Windows.Forms.Button();
			label2 = new System.Windows.Forms.Label();
			UserNameTextBox = new TextBoxWithCueBanner();
			ServerTextBox = new TextBoxWithCueBanner();
			label3 = new System.Windows.Forms.Label();
			OkBtn = new System.Windows.Forms.Button();
			CancelBtn = new System.Windows.Forms.Button();
			ViewLogBtn = new System.Windows.Forms.Button();
			groupBox2 = new System.Windows.Forms.GroupBox();
			tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
			KeepInTrayCheckBox = new System.Windows.Forms.CheckBox();
			AutomaticallyRunAtStartupCheckBox = new System.Windows.Forms.CheckBox();
			groupBox4 = new System.Windows.Forms.GroupBox();
			tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			EnableProtocolHandlerCheckBox = new System.Windows.Forms.CheckBox();
			flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
			EnableAutomationCheckBox = new System.Windows.Forms.CheckBox();
			AutomationPortTextBox = new System.Windows.Forms.TextBox();
			groupBox5 = new System.Windows.Forms.GroupBox();
			CustomToolsListBox = new System.Windows.Forms.CheckedListBox();
			groupBox3 = new System.Windows.Forms.GroupBox();
			tableLayoutPanel4 = new System.Windows.Forms.TableLayoutPanel();
			HordeServerTextBox = new TextBoxWithCueBanner();
			label4 = new System.Windows.Forms.Label();
			UpdateSettingsBtn = new System.Windows.Forms.Button();
			groupBox1.SuspendLayout();
			tableLayoutPanel1.SuspendLayout();
			tableLayoutPanel6.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)ParallelSyncThreadsSpinner).BeginInit();
			groupBox2.SuspendLayout();
			tableLayoutPanel3.SuspendLayout();
			groupBox4.SuspendLayout();
			tableLayoutPanel2.SuspendLayout();
			flowLayoutPanel1.SuspendLayout();
			groupBox5.SuspendLayout();
			groupBox3.SuspendLayout();
			tableLayoutPanel4.SuspendLayout();
			SuspendLayout();
			// 
			// label1
			// 
			label1.Anchor = System.Windows.Forms.AnchorStyles.Left;
			label1.AutoSize = true;
			label1.Location = new System.Drawing.Point(3, 9);
			label1.MinimumSize = new System.Drawing.Size(80, 0);
			label1.Name = "label1";
			label1.Size = new System.Drawing.Size(80, 15);
			label1.TabIndex = 0;
			label1.Text = "Server:";
			// 
			// groupBox1
			// 
			groupBox1.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			groupBox1.Controls.Add(tableLayoutPanel1);
			groupBox1.Location = new System.Drawing.Point(17, 179);
			groupBox1.Name = "groupBox1";
			groupBox1.Size = new System.Drawing.Size(822, 149);
			groupBox1.TabIndex = 1;
			groupBox1.TabStop = false;
			groupBox1.Text = "Perforce";
			// 
			// tableLayoutPanel1
			// 
			tableLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			tableLayoutPanel1.ColumnCount = 2;
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 130F));
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel1.Controls.Add(tableLayoutPanel6, 1, 2);
			tableLayoutPanel1.Controls.Add(label2, 0, 2);
			tableLayoutPanel1.Controls.Add(UserNameTextBox, 1, 1);
			tableLayoutPanel1.Controls.Add(label1, 0, 0);
			tableLayoutPanel1.Controls.Add(ServerTextBox, 1, 0);
			tableLayoutPanel1.Controls.Add(label3, 0, 1);
			tableLayoutPanel1.Location = new System.Drawing.Point(22, 27);
			tableLayoutPanel1.Name = "tableLayoutPanel1";
			tableLayoutPanel1.RowCount = 3;
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.3333321F));
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.3333321F));
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.3333321F));
			tableLayoutPanel1.Size = new System.Drawing.Size(787, 102);
			tableLayoutPanel1.TabIndex = 0;
			// 
			// tableLayoutPanel6
			// 
			tableLayoutPanel6.ColumnCount = 3;
			tableLayoutPanel6.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			tableLayoutPanel6.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel6.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			tableLayoutPanel6.Controls.Add(ParallelSyncThreadsSpinner, 0, 0);
			tableLayoutPanel6.Controls.Add(AdvancedBtn, 2, 0);
			tableLayoutPanel6.Location = new System.Drawing.Point(130, 66);
			tableLayoutPanel6.Margin = new System.Windows.Forms.Padding(0);
			tableLayoutPanel6.Name = "tableLayoutPanel6";
			tableLayoutPanel6.RowCount = 1;
			tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel6.Size = new System.Drawing.Size(657, 36);
			tableLayoutPanel6.TabIndex = 9;
			// 
			// ParallelSyncThreadsSpinner
			// 
			ParallelSyncThreadsSpinner.Anchor = System.Windows.Forms.AnchorStyles.Left;
			ParallelSyncThreadsSpinner.Location = new System.Drawing.Point(3, 6);
			ParallelSyncThreadsSpinner.Maximum = new decimal(new int[] { 8, 0, 0, 0 });
			ParallelSyncThreadsSpinner.Minimum = new decimal(new int[] { 1, 0, 0, 0 });
			ParallelSyncThreadsSpinner.Name = "ParallelSyncThreadsSpinner";
			ParallelSyncThreadsSpinner.Size = new System.Drawing.Size(163, 23);
			ParallelSyncThreadsSpinner.TabIndex = 3;
			ParallelSyncThreadsSpinner.Value = new decimal(new int[] { 1, 0, 0, 0 });
			// 
			// AdvancedBtn
			// 
			AdvancedBtn.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left;
			AdvancedBtn.Location = new System.Drawing.Point(512, 6);
			AdvancedBtn.Name = "AdvancedBtn";
			AdvancedBtn.Size = new System.Drawing.Size(142, 27);
			AdvancedBtn.TabIndex = 6;
			AdvancedBtn.Text = "Advanced...";
			AdvancedBtn.UseVisualStyleBackColor = true;
			AdvancedBtn.Click += AdvancedBtn_Click;
			// 
			// label2
			// 
			label2.Anchor = System.Windows.Forms.AnchorStyles.Left;
			label2.AutoSize = true;
			label2.Location = new System.Drawing.Point(3, 76);
			label2.Margin = new System.Windows.Forms.Padding(3, 0, 10, 0);
			label2.MinimumSize = new System.Drawing.Size(80, 0);
			label2.Name = "label2";
			label2.Size = new System.Drawing.Size(117, 15);
			label2.TabIndex = 4;
			label2.Text = "Parallel sync threads:";
			// 
			// UserNameTextBox
			// 
			UserNameTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			UserNameTextBox.CueBanner = "Default";
			UserNameTextBox.Location = new System.Drawing.Point(133, 38);
			UserNameTextBox.Name = "UserNameTextBox";
			UserNameTextBox.Size = new System.Drawing.Size(651, 23);
			UserNameTextBox.TabIndex = 1;
			// 
			// ServerTextBox
			// 
			ServerTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			ServerTextBox.CueBanner = "Default";
			ServerTextBox.Location = new System.Drawing.Point(133, 5);
			ServerTextBox.Name = "ServerTextBox";
			ServerTextBox.Size = new System.Drawing.Size(651, 23);
			ServerTextBox.TabIndex = 0;
			// 
			// label3
			// 
			label3.Anchor = System.Windows.Forms.AnchorStyles.Left;
			label3.AutoSize = true;
			label3.Location = new System.Drawing.Point(3, 42);
			label3.Margin = new System.Windows.Forms.Padding(3, 0, 10, 0);
			label3.Name = "label3";
			label3.Size = new System.Drawing.Size(33, 15);
			label3.TabIndex = 2;
			label3.Text = "User:";
			// 
			// OkBtn
			// 
			OkBtn.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right;
			OkBtn.Location = new System.Drawing.Point(655, 592);
			OkBtn.Name = "OkBtn";
			OkBtn.Size = new System.Drawing.Size(89, 27);
			OkBtn.TabIndex = 2;
			OkBtn.Text = "Ok";
			OkBtn.UseVisualStyleBackColor = true;
			OkBtn.Click += OkBtn_Click;
			// 
			// CancelBtn
			// 
			CancelBtn.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right;
			CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			CancelBtn.Location = new System.Drawing.Point(750, 592);
			CancelBtn.Name = "CancelBtn";
			CancelBtn.Size = new System.Drawing.Size(89, 27);
			CancelBtn.TabIndex = 3;
			CancelBtn.Text = "Cancel";
			CancelBtn.UseVisualStyleBackColor = true;
			CancelBtn.Click += CancelBtn_Click;
			// 
			// ViewLogBtn
			// 
			ViewLogBtn.Location = new System.Drawing.Point(0, 0);
			ViewLogBtn.Name = "ViewLogBtn";
			ViewLogBtn.Size = new System.Drawing.Size(75, 23);
			ViewLogBtn.TabIndex = 0;
			// 
			// groupBox2
			// 
			groupBox2.Controls.Add(tableLayoutPanel3);
			groupBox2.Location = new System.Drawing.Point(17, 12);
			groupBox2.Name = "groupBox2";
			groupBox2.Size = new System.Drawing.Size(822, 88);
			groupBox2.TabIndex = 0;
			groupBox2.TabStop = false;
			groupBox2.Text = "Startup and Shutdown";
			// 
			// tableLayoutPanel3
			// 
			tableLayoutPanel3.ColumnCount = 1;
			tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel3.Controls.Add(KeepInTrayCheckBox, 0, 1);
			tableLayoutPanel3.Controls.Add(AutomaticallyRunAtStartupCheckBox, 0, 0);
			tableLayoutPanel3.Location = new System.Drawing.Point(22, 24);
			tableLayoutPanel3.Name = "tableLayoutPanel3";
			tableLayoutPanel3.RowCount = 2;
			tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel3.Size = new System.Drawing.Size(787, 52);
			tableLayoutPanel3.TabIndex = 6;
			// 
			// KeepInTrayCheckBox
			// 
			KeepInTrayCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			KeepInTrayCheckBox.AutoSize = true;
			KeepInTrayCheckBox.Location = new System.Drawing.Point(3, 29);
			KeepInTrayCheckBox.Name = "KeepInTrayCheckBox";
			KeepInTrayCheckBox.Size = new System.Drawing.Size(377, 19);
			KeepInTrayCheckBox.TabIndex = 1;
			KeepInTrayCheckBox.Text = "Keep program running in the system notification area when closed";
			KeepInTrayCheckBox.UseVisualStyleBackColor = true;
			// 
			// AutomaticallyRunAtStartupCheckBox
			// 
			AutomaticallyRunAtStartupCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			AutomaticallyRunAtStartupCheckBox.AutoSize = true;
			AutomaticallyRunAtStartupCheckBox.Location = new System.Drawing.Point(3, 3);
			AutomaticallyRunAtStartupCheckBox.Name = "AutomaticallyRunAtStartupCheckBox";
			AutomaticallyRunAtStartupCheckBox.Size = new System.Drawing.Size(174, 19);
			AutomaticallyRunAtStartupCheckBox.TabIndex = 0;
			AutomaticallyRunAtStartupCheckBox.Text = "Automatically run at startup";
			AutomaticallyRunAtStartupCheckBox.UseVisualStyleBackColor = true;
			// 
			// groupBox4
			// 
			groupBox4.Controls.Add(tableLayoutPanel2);
			groupBox4.Location = new System.Drawing.Point(17, 334);
			groupBox4.Name = "groupBox4";
			groupBox4.Size = new System.Drawing.Size(822, 95);
			groupBox4.TabIndex = 5;
			groupBox4.TabStop = false;
			groupBox4.Text = "Integration";
			// 
			// tableLayoutPanel2
			// 
			tableLayoutPanel2.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			tableLayoutPanel2.ColumnCount = 1;
			tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel2.Controls.Add(EnableProtocolHandlerCheckBox, 0, 1);
			tableLayoutPanel2.Controls.Add(flowLayoutPanel1, 0, 0);
			tableLayoutPanel2.Location = new System.Drawing.Point(18, 23);
			tableLayoutPanel2.Name = "tableLayoutPanel2";
			tableLayoutPanel2.RowCount = 2;
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25.00062F));
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25.00062F));
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25.00062F));
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 24.99813F));
			tableLayoutPanel2.Size = new System.Drawing.Size(787, 58);
			tableLayoutPanel2.TabIndex = 7;
			// 
			// EnableProtocolHandlerCheckBox
			// 
			EnableProtocolHandlerCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			EnableProtocolHandlerCheckBox.AutoSize = true;
			EnableProtocolHandlerCheckBox.Location = new System.Drawing.Point(3, 34);
			EnableProtocolHandlerCheckBox.Name = "EnableProtocolHandlerCheckBox";
			EnableProtocolHandlerCheckBox.Size = new System.Drawing.Size(260, 19);
			EnableProtocolHandlerCheckBox.TabIndex = 0;
			EnableProtocolHandlerCheckBox.Text = "Enable \"ugs://\" protocol handler for all users";
			EnableProtocolHandlerCheckBox.UseVisualStyleBackColor = true;
			// 
			// flowLayoutPanel1
			// 
			flowLayoutPanel1.Anchor = System.Windows.Forms.AnchorStyles.Left;
			flowLayoutPanel1.AutoSize = true;
			flowLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			flowLayoutPanel1.Controls.Add(EnableAutomationCheckBox);
			flowLayoutPanel1.Controls.Add(AutomationPortTextBox);
			flowLayoutPanel1.Location = new System.Drawing.Point(0, 1);
			flowLayoutPanel1.Margin = new System.Windows.Forms.Padding(0);
			flowLayoutPanel1.Name = "flowLayoutPanel1";
			flowLayoutPanel1.Size = new System.Drawing.Size(281, 26);
			flowLayoutPanel1.TabIndex = 2;
			// 
			// EnableAutomationCheckBox
			// 
			EnableAutomationCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			EnableAutomationCheckBox.AutoSize = true;
			EnableAutomationCheckBox.Location = new System.Drawing.Point(3, 3);
			EnableAutomationCheckBox.Name = "EnableAutomationCheckBox";
			EnableAutomationCheckBox.Size = new System.Drawing.Size(169, 19);
			EnableAutomationCheckBox.TabIndex = 0;
			EnableAutomationCheckBox.Text = "Enable automation via port";
			EnableAutomationCheckBox.UseVisualStyleBackColor = true;
			EnableAutomationCheckBox.CheckedChanged += EnableAutomationCheckBox_CheckedChanged;
			// 
			// AutomationPortTextBox
			// 
			AutomationPortTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			AutomationPortTextBox.Location = new System.Drawing.Point(178, 0);
			AutomationPortTextBox.Margin = new System.Windows.Forms.Padding(3, 0, 3, 3);
			AutomationPortTextBox.Name = "AutomationPortTextBox";
			AutomationPortTextBox.Size = new System.Drawing.Size(100, 23);
			AutomationPortTextBox.TabIndex = 1;
			// 
			// groupBox5
			// 
			groupBox5.Controls.Add(CustomToolsListBox);
			groupBox5.Location = new System.Drawing.Point(17, 435);
			groupBox5.Name = "groupBox5";
			groupBox5.Size = new System.Drawing.Size(822, 142);
			groupBox5.TabIndex = 7;
			groupBox5.TabStop = false;
			groupBox5.Text = "Custom Tools";
			// 
			// CustomToolsListBox
			// 
			CustomToolsListBox.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			CustomToolsListBox.CheckOnClick = true;
			CustomToolsListBox.FormattingEnabled = true;
			CustomToolsListBox.IntegralHeight = false;
			CustomToolsListBox.Location = new System.Drawing.Point(18, 23);
			CustomToolsListBox.Name = "CustomToolsListBox";
			CustomToolsListBox.Size = new System.Drawing.Size(787, 100);
			CustomToolsListBox.TabIndex = 0;
			CustomToolsListBox.ItemCheck += CustomToolsListBox_ItemCheck;
			// 
			// groupBox3
			// 
			groupBox3.Controls.Add(tableLayoutPanel4);
			groupBox3.Location = new System.Drawing.Point(17, 106);
			groupBox3.Name = "groupBox3";
			groupBox3.Size = new System.Drawing.Size(822, 67);
			groupBox3.TabIndex = 8;
			groupBox3.TabStop = false;
			groupBox3.Text = "Horde";
			// 
			// tableLayoutPanel4
			// 
			tableLayoutPanel4.ColumnCount = 2;
			tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 130F));
			tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel4.Controls.Add(HordeServerTextBox, 1, 0);
			tableLayoutPanel4.Controls.Add(label4, 0, 0);
			tableLayoutPanel4.Location = new System.Drawing.Point(22, 22);
			tableLayoutPanel4.Name = "tableLayoutPanel4";
			tableLayoutPanel4.RowCount = 1;
			tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel4.Size = new System.Drawing.Size(788, 30);
			tableLayoutPanel4.TabIndex = 0;
			// 
			// HordeServerTextBox
			// 
			HordeServerTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			HordeServerTextBox.CueBanner = "Default";
			HordeServerTextBox.Location = new System.Drawing.Point(133, 3);
			HordeServerTextBox.Name = "HordeServerTextBox";
			HordeServerTextBox.Size = new System.Drawing.Size(652, 23);
			HordeServerTextBox.TabIndex = 1;
			// 
			// label4
			// 
			label4.Anchor = System.Windows.Forms.AnchorStyles.Left;
			label4.AutoSize = true;
			label4.Location = new System.Drawing.Point(3, 7);
			label4.Name = "label4";
			label4.Size = new System.Drawing.Size(42, 15);
			label4.TabIndex = 0;
			label4.Text = "Server:";
			// 
			// UpdateSettingsBtn
			// 
			UpdateSettingsBtn.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left;
			UpdateSettingsBtn.Location = new System.Drawing.Point(17, 592);
			UpdateSettingsBtn.Name = "UpdateSettingsBtn";
			UpdateSettingsBtn.Size = new System.Drawing.Size(142, 27);
			UpdateSettingsBtn.TabIndex = 9;
			UpdateSettingsBtn.Text = "Update Settings...";
			UpdateSettingsBtn.UseVisualStyleBackColor = true;
			UpdateSettingsBtn.Click += UpdateSettingsBtn_Click;
			// 
			// ApplicationSettingsWindow
			// 
			AcceptButton = OkBtn;
			AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			CancelButton = CancelBtn;
			ClientSize = new System.Drawing.Size(857, 631);
			Controls.Add(UpdateSettingsBtn);
			Controls.Add(groupBox3);
			Controls.Add(groupBox5);
			Controls.Add(groupBox4);
			Controls.Add(groupBox2);
			Controls.Add(CancelBtn);
			Controls.Add(OkBtn);
			Controls.Add(groupBox1);
			FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			Icon = Properties.Resources.Icon;
			MaximizeBox = false;
			MinimizeBox = false;
			Name = "ApplicationSettingsWindow";
			StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			Text = "Application Settings";
			groupBox1.ResumeLayout(false);
			tableLayoutPanel1.ResumeLayout(false);
			tableLayoutPanel1.PerformLayout();
			tableLayoutPanel6.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)ParallelSyncThreadsSpinner).EndInit();
			groupBox2.ResumeLayout(false);
			tableLayoutPanel3.ResumeLayout(false);
			tableLayoutPanel3.PerformLayout();
			groupBox4.ResumeLayout(false);
			tableLayoutPanel2.ResumeLayout(false);
			tableLayoutPanel2.PerformLayout();
			flowLayoutPanel1.ResumeLayout(false);
			flowLayoutPanel1.PerformLayout();
			groupBox5.ResumeLayout(false);
			groupBox3.ResumeLayout(false);
			tableLayoutPanel4.ResumeLayout(false);
			tableLayoutPanel4.PerformLayout();
			ResumeLayout(false);
		}

		#endregion
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button ViewLogBtn;
		private System.Windows.Forms.Label label3;
		private TextBoxWithCueBanner ServerTextBox;
		private TextBoxWithCueBanner UserNameTextBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.CheckBox AutomaticallyRunAtStartupCheckBox;
		private System.Windows.Forms.CheckBox KeepInTrayCheckBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
		private System.Windows.Forms.GroupBox groupBox4;
		private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
		private System.Windows.Forms.CheckBox EnableAutomationCheckBox;
		private System.Windows.Forms.TextBox AutomationPortTextBox;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.NumericUpDown ParallelSyncThreadsSpinner;
		private System.Windows.Forms.Button AdvancedBtn;
		private System.Windows.Forms.CheckBox EnableProtocolHandlerCheckBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.GroupBox groupBox5;
		private System.Windows.Forms.CheckedListBox CustomToolsListBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel6;
		private System.Windows.Forms.GroupBox groupBox3;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel4;
		private TextBoxWithCueBanner HordeServerTextBox;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.Button UpdateSettingsBtn;
	}
}
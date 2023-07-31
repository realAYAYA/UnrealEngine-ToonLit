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
			this.label1 = new System.Windows.Forms.Label();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			this.label2 = new System.Windows.Forms.Label();
			this.ParallelSyncThreadsSpinner = new System.Windows.Forms.NumericUpDown();
			this.UserNameTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.ServerTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.label3 = new System.Windows.Forms.Label();
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.ViewLogBtn = new System.Windows.Forms.Button();
			this.groupBox2 = new System.Windows.Forms.GroupBox();
			this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
			this.KeepInTrayCheckBox = new System.Windows.Forms.CheckBox();
			this.AutomaticallyRunAtStartupCheckBox = new System.Windows.Forms.CheckBox();
			this.groupBox3 = new System.Windows.Forms.GroupBox();
			this.tableLayoutPanel4 = new System.Windows.Forms.TableLayoutPanel();
			this.label6 = new System.Windows.Forms.Label();
			this.tableLayoutPanel5 = new System.Windows.Forms.TableLayoutPanel();
			this.UsePreviewBuildCheckBox = new System.Windows.Forms.CheckBox();
			this.DepotPathTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.groupBox4 = new System.Windows.Forms.GroupBox();
			this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			this.EnableProtocolHandlerCheckBox = new System.Windows.Forms.CheckBox();
			this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
			this.EnableAutomationCheckBox = new System.Windows.Forms.CheckBox();
			this.AutomationPortTextBox = new System.Windows.Forms.TextBox();
			this.AdvancedBtn = new System.Windows.Forms.Button();
			this.groupBox5 = new System.Windows.Forms.GroupBox();
			this.CustomToolsListBox = new System.Windows.Forms.CheckedListBox();
			this.groupBox1.SuspendLayout();
			this.tableLayoutPanel1.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.ParallelSyncThreadsSpinner)).BeginInit();
			this.groupBox2.SuspendLayout();
			this.tableLayoutPanel3.SuspendLayout();
			this.groupBox3.SuspendLayout();
			this.tableLayoutPanel4.SuspendLayout();
			this.tableLayoutPanel5.SuspendLayout();
			this.groupBox4.SuspendLayout();
			this.tableLayoutPanel2.SuspendLayout();
			this.flowLayoutPanel1.SuspendLayout();
			this.groupBox5.SuspendLayout();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(3, 9);
			this.label1.MinimumSize = new System.Drawing.Size(80, 0);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(80, 15);
			this.label1.TabIndex = 0;
			this.label1.Text = "Server:";
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.tableLayoutPanel1);
			this.groupBox1.Location = new System.Drawing.Point(17, 106);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(822, 149);
			this.groupBox1.TabIndex = 1;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Default Perforce Settings";
			// 
			// tableLayoutPanel1
			// 
			this.tableLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.tableLayoutPanel1.ColumnCount = 2;
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Controls.Add(this.label2, 0, 2);
			this.tableLayoutPanel1.Controls.Add(this.ParallelSyncThreadsSpinner, 1, 2);
			this.tableLayoutPanel1.Controls.Add(this.UserNameTextBox, 1, 1);
			this.tableLayoutPanel1.Controls.Add(this.label1, 0, 0);
			this.tableLayoutPanel1.Controls.Add(this.ServerTextBox, 1, 0);
			this.tableLayoutPanel1.Controls.Add(this.label3, 0, 1);
			this.tableLayoutPanel1.Location = new System.Drawing.Point(22, 27);
			this.tableLayoutPanel1.Name = "tableLayoutPanel1";
			this.tableLayoutPanel1.RowCount = 3;
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.33333F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.33333F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.33333F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
			this.tableLayoutPanel1.Size = new System.Drawing.Size(787, 102);
			this.tableLayoutPanel1.TabIndex = 0;
			// 
			// label2
			// 
			this.label2.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(3, 77);
			this.label2.Margin = new System.Windows.Forms.Padding(3, 0, 10, 0);
			this.label2.MinimumSize = new System.Drawing.Size(80, 0);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(117, 15);
			this.label2.TabIndex = 4;
			this.label2.Text = "Parallel sync threads:";
			// 
			// ParallelSyncThreadsSpinner
			// 
			this.ParallelSyncThreadsSpinner.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.ParallelSyncThreadsSpinner.Location = new System.Drawing.Point(133, 73);
			this.ParallelSyncThreadsSpinner.Maximum = new decimal(new int[] {
            8,
            0,
            0,
            0});
			this.ParallelSyncThreadsSpinner.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            0});
			this.ParallelSyncThreadsSpinner.Name = "ParallelSyncThreadsSpinner";
			this.ParallelSyncThreadsSpinner.Size = new System.Drawing.Size(163, 23);
			this.ParallelSyncThreadsSpinner.TabIndex = 3;
			this.ParallelSyncThreadsSpinner.Value = new decimal(new int[] {
            1,
            0,
            0,
            0});
			// 
			// UserNameTextBox
			// 
			this.UserNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.UserNameTextBox.CueBanner = "Default";
			this.UserNameTextBox.Location = new System.Drawing.Point(133, 39);
			this.UserNameTextBox.Name = "UserNameTextBox";
			this.UserNameTextBox.Size = new System.Drawing.Size(651, 23);
			this.UserNameTextBox.TabIndex = 1;
			// 
			// ServerTextBox
			// 
			this.ServerTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.ServerTextBox.CueBanner = "Default";
			this.ServerTextBox.Location = new System.Drawing.Point(133, 5);
			this.ServerTextBox.Name = "ServerTextBox";
			this.ServerTextBox.Size = new System.Drawing.Size(651, 23);
			this.ServerTextBox.TabIndex = 0;
			// 
			// label3
			// 
			this.label3.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(3, 43);
			this.label3.Margin = new System.Windows.Forms.Padding(3, 0, 10, 0);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(33, 15);
			this.label3.TabIndex = 2;
			this.label3.Text = "User:";
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(661, 608);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(89, 27);
			this.OkBtn.TabIndex = 2;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(756, 608);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(89, 27);
			this.CancelBtn.TabIndex = 3;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			this.CancelBtn.Click += new System.EventHandler(this.CancelBtn_Click);
			// 
			// ViewLogBtn
			// 
			this.ViewLogBtn.Location = new System.Drawing.Point(0, 0);
			this.ViewLogBtn.Name = "ViewLogBtn";
			this.ViewLogBtn.Size = new System.Drawing.Size(75, 23);
			this.ViewLogBtn.TabIndex = 0;
			// 
			// groupBox2
			// 
			this.groupBox2.Controls.Add(this.tableLayoutPanel3);
			this.groupBox2.Location = new System.Drawing.Point(17, 12);
			this.groupBox2.Name = "groupBox2";
			this.groupBox2.Size = new System.Drawing.Size(822, 88);
			this.groupBox2.TabIndex = 0;
			this.groupBox2.TabStop = false;
			this.groupBox2.Text = "Startup and Shutdown";
			// 
			// tableLayoutPanel3
			// 
			this.tableLayoutPanel3.ColumnCount = 1;
			this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel3.Controls.Add(this.KeepInTrayCheckBox, 0, 1);
			this.tableLayoutPanel3.Controls.Add(this.AutomaticallyRunAtStartupCheckBox, 0, 0);
			this.tableLayoutPanel3.Location = new System.Drawing.Point(22, 24);
			this.tableLayoutPanel3.Name = "tableLayoutPanel3";
			this.tableLayoutPanel3.RowCount = 2;
			this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel3.Size = new System.Drawing.Size(787, 52);
			this.tableLayoutPanel3.TabIndex = 6;
			// 
			// KeepInTrayCheckBox
			// 
			this.KeepInTrayCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.KeepInTrayCheckBox.AutoSize = true;
			this.KeepInTrayCheckBox.Location = new System.Drawing.Point(3, 29);
			this.KeepInTrayCheckBox.Name = "KeepInTrayCheckBox";
			this.KeepInTrayCheckBox.Size = new System.Drawing.Size(377, 19);
			this.KeepInTrayCheckBox.TabIndex = 1;
			this.KeepInTrayCheckBox.Text = "Keep program running in the system notification area when closed";
			this.KeepInTrayCheckBox.UseVisualStyleBackColor = true;
			// 
			// AutomaticallyRunAtStartupCheckBox
			// 
			this.AutomaticallyRunAtStartupCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.AutomaticallyRunAtStartupCheckBox.AutoSize = true;
			this.AutomaticallyRunAtStartupCheckBox.Location = new System.Drawing.Point(3, 3);
			this.AutomaticallyRunAtStartupCheckBox.Name = "AutomaticallyRunAtStartupCheckBox";
			this.AutomaticallyRunAtStartupCheckBox.Size = new System.Drawing.Size(174, 19);
			this.AutomaticallyRunAtStartupCheckBox.TabIndex = 0;
			this.AutomaticallyRunAtStartupCheckBox.Text = "Automatically run at startup";
			this.AutomaticallyRunAtStartupCheckBox.UseVisualStyleBackColor = true;
			// 
			// groupBox3
			// 
			this.groupBox3.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox3.Controls.Add(this.tableLayoutPanel4);
			this.groupBox3.Location = new System.Drawing.Point(17, 261);
			this.groupBox3.Name = "groupBox3";
			this.groupBox3.Size = new System.Drawing.Size(822, 77);
			this.groupBox3.TabIndex = 4;
			this.groupBox3.TabStop = false;
			this.groupBox3.Text = "Updates";
			// 
			// tableLayoutPanel4
			// 
			this.tableLayoutPanel4.ColumnCount = 2;
			this.tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel4.Controls.Add(this.label6, 0, 0);
			this.tableLayoutPanel4.Controls.Add(this.tableLayoutPanel5, 1, 0);
			this.tableLayoutPanel4.Location = new System.Drawing.Point(22, 27);
			this.tableLayoutPanel4.Name = "tableLayoutPanel4";
			this.tableLayoutPanel4.RowCount = 1;
			this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 34F));
			this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 34F));
			this.tableLayoutPanel4.Size = new System.Drawing.Size(787, 34);
			this.tableLayoutPanel4.TabIndex = 0;
			// 
			// label6
			// 
			this.label6.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label6.Location = new System.Drawing.Point(3, 10);
			this.label6.MinimumSize = new System.Drawing.Size(80, 0);
			this.label6.Name = "label6";
			this.label6.Size = new System.Drawing.Size(80, 13);
			this.label6.TabIndex = 4;
			this.label6.Text = "Depot Path:";
			// 
			// tableLayoutPanel5
			// 
			this.tableLayoutPanel5.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel5.ColumnCount = 2;
			this.tableLayoutPanel5.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel5.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel5.Controls.Add(this.UsePreviewBuildCheckBox, 1, 0);
			this.tableLayoutPanel5.Controls.Add(this.DepotPathTextBox, 0, 0);
			this.tableLayoutPanel5.Location = new System.Drawing.Point(86, 2);
			this.tableLayoutPanel5.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel5.Name = "tableLayoutPanel5";
			this.tableLayoutPanel5.RowCount = 1;
			this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle());
			this.tableLayoutPanel5.Size = new System.Drawing.Size(701, 29);
			this.tableLayoutPanel5.TabIndex = 5;
			// 
			// UseUnstableBuildCheckBox
			// 
			this.UsePreviewBuildCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Right;
			this.UsePreviewBuildCheckBox.AutoSize = true;
			this.UsePreviewBuildCheckBox.Location = new System.Drawing.Point(574, 5);
			this.UsePreviewBuildCheckBox.Margin = new System.Windows.Forms.Padding(10, 3, 3, 3);
			this.UsePreviewBuildCheckBox.Name = "UsePreviewBuildCheckBox";
			this.UsePreviewBuildCheckBox.Size = new System.Drawing.Size(124, 19);
			this.UsePreviewBuildCheckBox.TabIndex = 1;
			this.UsePreviewBuildCheckBox.Text = "Use Preview Build";
			this.UsePreviewBuildCheckBox.UseVisualStyleBackColor = true;
			// 
			// DepotPathTextBox
			// 
			this.DepotPathTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.DepotPathTextBox.CueBanner = null;
			this.DepotPathTextBox.Location = new System.Drawing.Point(3, 3);
			this.DepotPathTextBox.Name = "DepotPathTextBox";
			this.DepotPathTextBox.Size = new System.Drawing.Size(558, 23);
			this.DepotPathTextBox.TabIndex = 0;
			// 
			// groupBox4
			// 
			this.groupBox4.Controls.Add(this.tableLayoutPanel2);
			this.groupBox4.Location = new System.Drawing.Point(17, 344);
			this.groupBox4.Name = "groupBox4";
			this.groupBox4.Size = new System.Drawing.Size(822, 95);
			this.groupBox4.TabIndex = 5;
			this.groupBox4.TabStop = false;
			this.groupBox4.Text = "Integration";
			// 
			// tableLayoutPanel2
			// 
			this.tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel2.ColumnCount = 1;
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.Controls.Add(this.EnableProtocolHandlerCheckBox, 0, 1);
			this.tableLayoutPanel2.Controls.Add(this.flowLayoutPanel1, 0, 0);
			this.tableLayoutPanel2.Location = new System.Drawing.Point(18, 23);
			this.tableLayoutPanel2.Name = "tableLayoutPanel2";
			this.tableLayoutPanel2.RowCount = 2;
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25.00062F));
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25.00062F));
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25.00062F));
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 24.99813F));
			this.tableLayoutPanel2.Size = new System.Drawing.Size(787, 58);
			this.tableLayoutPanel2.TabIndex = 7;
			// 
			// EnableProtocolHandlerCheckBox
			// 
			this.EnableProtocolHandlerCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.EnableProtocolHandlerCheckBox.AutoSize = true;
			this.EnableProtocolHandlerCheckBox.Location = new System.Drawing.Point(3, 34);
			this.EnableProtocolHandlerCheckBox.Name = "EnableProtocolHandlerCheckBox";
			this.EnableProtocolHandlerCheckBox.Size = new System.Drawing.Size(197, 19);
			this.EnableProtocolHandlerCheckBox.TabIndex = 0;
			this.EnableProtocolHandlerCheckBox.Text = "Enable \"ugs://\" protocol handler for all users";
			this.EnableProtocolHandlerCheckBox.UseVisualStyleBackColor = true;
			// 
			// flowLayoutPanel1
			// 
			this.flowLayoutPanel1.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.flowLayoutPanel1.AutoSize = true;
			this.flowLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.flowLayoutPanel1.Controls.Add(this.EnableAutomationCheckBox);
			this.flowLayoutPanel1.Controls.Add(this.AutomationPortTextBox);
			this.flowLayoutPanel1.Location = new System.Drawing.Point(0, 1);
			this.flowLayoutPanel1.Margin = new System.Windows.Forms.Padding(0);
			this.flowLayoutPanel1.Name = "flowLayoutPanel1";
			this.flowLayoutPanel1.Size = new System.Drawing.Size(281, 26);
			this.flowLayoutPanel1.TabIndex = 2;
			// 
			// EnableAutomationCheckBox
			// 
			this.EnableAutomationCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.EnableAutomationCheckBox.AutoSize = true;
			this.EnableAutomationCheckBox.Location = new System.Drawing.Point(3, 3);
			this.EnableAutomationCheckBox.Name = "EnableAutomationCheckBox";
			this.EnableAutomationCheckBox.Size = new System.Drawing.Size(169, 19);
			this.EnableAutomationCheckBox.TabIndex = 0;
			this.EnableAutomationCheckBox.Text = "Enable automation via port";
			this.EnableAutomationCheckBox.UseVisualStyleBackColor = true;
			this.EnableAutomationCheckBox.CheckedChanged += new System.EventHandler(this.EnableAutomationCheckBox_CheckedChanged);
			// 
			// AutomationPortTextBox
			// 
			this.AutomationPortTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.AutomationPortTextBox.Location = new System.Drawing.Point(178, 0);
			this.AutomationPortTextBox.Margin = new System.Windows.Forms.Padding(3, 0, 3, 3);
			this.AutomationPortTextBox.Name = "AutomationPortTextBox";
			this.AutomationPortTextBox.Size = new System.Drawing.Size(100, 23);
			this.AutomationPortTextBox.TabIndex = 1;
			// 
			// AdvancedBtn
			// 
			this.AdvancedBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.AdvancedBtn.Location = new System.Drawing.Point(17, 608);
			this.AdvancedBtn.Name = "AdvancedBtn";
			this.AdvancedBtn.Size = new System.Drawing.Size(105, 27);
			this.AdvancedBtn.TabIndex = 6;
			this.AdvancedBtn.Text = "Advanced";
			this.AdvancedBtn.UseVisualStyleBackColor = true;
			this.AdvancedBtn.Click += new System.EventHandler(this.AdvancedBtn_Click);
			// 
			// groupBox5
			// 
			this.groupBox5.Controls.Add(this.CustomToolsListBox);
			this.groupBox5.Location = new System.Drawing.Point(17, 445);
			this.groupBox5.Name = "groupBox5";
			this.groupBox5.Size = new System.Drawing.Size(822, 142);
			this.groupBox5.TabIndex = 7;
			this.groupBox5.TabStop = false;
			this.groupBox5.Text = "Custom Tools";
			// 
			// CustomToolsListBox
			// 
			this.CustomToolsListBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.CustomToolsListBox.CheckOnClick = true;
			this.CustomToolsListBox.FormattingEnabled = true;
			this.CustomToolsListBox.IntegralHeight = false;
			this.CustomToolsListBox.Location = new System.Drawing.Point(18, 23);
			this.CustomToolsListBox.Name = "CustomToolsListBox";
			this.CustomToolsListBox.Size = new System.Drawing.Size(787, 100);
			this.CustomToolsListBox.Sorted = true;
			this.CustomToolsListBox.TabIndex = 0;
			// 
			// ApplicationSettingsWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(857, 647);
			this.Controls.Add(this.groupBox5);
			this.Controls.Add(this.AdvancedBtn);
			this.Controls.Add(this.groupBox4);
			this.Controls.Add(this.groupBox3);
			this.Controls.Add(this.groupBox2);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.groupBox1);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = global::UnrealGameSync.Properties.Resources.Icon;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ApplicationSettingsWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Application Settings";
			this.groupBox1.ResumeLayout(false);
			this.tableLayoutPanel1.ResumeLayout(false);
			this.tableLayoutPanel1.PerformLayout();
			((System.ComponentModel.ISupportInitialize)(this.ParallelSyncThreadsSpinner)).EndInit();
			this.groupBox2.ResumeLayout(false);
			this.tableLayoutPanel3.ResumeLayout(false);
			this.tableLayoutPanel3.PerformLayout();
			this.groupBox3.ResumeLayout(false);
			this.tableLayoutPanel4.ResumeLayout(false);
			this.tableLayoutPanel5.ResumeLayout(false);
			this.tableLayoutPanel5.PerformLayout();
			this.groupBox4.ResumeLayout(false);
			this.tableLayoutPanel2.ResumeLayout(false);
			this.tableLayoutPanel2.PerformLayout();
			this.flowLayoutPanel1.ResumeLayout(false);
			this.flowLayoutPanel1.PerformLayout();
			this.groupBox5.ResumeLayout(false);
			this.ResumeLayout(false);

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
		private System.Windows.Forms.GroupBox groupBox3;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel4;
		private System.Windows.Forms.Label label6;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel5;
		public System.Windows.Forms.CheckBox UsePreviewBuildCheckBox;
		private TextBoxWithCueBanner DepotPathTextBox;
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
	}
}
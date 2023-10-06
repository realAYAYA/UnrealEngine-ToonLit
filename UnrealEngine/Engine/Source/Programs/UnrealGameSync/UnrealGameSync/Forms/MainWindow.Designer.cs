// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class MainWindow
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
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainWindow));
			this.TabPanel = new System.Windows.Forms.Panel();
			this.DefaultControl = new UnrealGameSync.StatusPanel();
			this.TabMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.TabMenu_OpenProject = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_RecentProjects = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_Recent_Separator = new System.Windows.Forms.ToolStripSeparator();
			this.TabMenu_RecentProjects_ClearList = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
			this.labelsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_TabNames_Stream = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_TabNames_WorkspaceName = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_TabNames_WorkspaceRoot = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_TabNames_ProjectFile = new System.Windows.Forms.ToolStripMenuItem();
			this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			this.TabControl = new UnrealGameSync.TabControl();
			this.UpdateAlertPositionsTimer = new System.Windows.Forms.Timer(this.components);
			this.CheckLauncherVersionTimer = new System.Windows.Forms.Timer(this.components);
			this.TabPanel.SuspendLayout();
			this.TabMenu.SuspendLayout();
			this.tableLayoutPanel1.SuspendLayout();
			this.SuspendLayout();
			// 
			// TabPanel
			// 
			this.TabPanel.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.TabPanel.Controls.Add(this.DefaultControl);
			this.TabPanel.Location = new System.Drawing.Point(16, 50);
			this.TabPanel.Margin = new System.Windows.Forms.Padding(0);
			this.TabPanel.Name = "TabPanel";
			this.TabPanel.Size = new System.Drawing.Size(1335, 746);
			this.TabPanel.TabIndex = 3;
			// 
			// DefaultControl
			// 
			this.DefaultControl.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.DefaultControl.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(250)))), ((int)(((byte)(250)))), ((int)(((byte)(250)))));
			this.DefaultControl.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.DefaultControl.Location = new System.Drawing.Point(0, 0);
			this.DefaultControl.Margin = new System.Windows.Forms.Padding(0);
			this.DefaultControl.Name = "DefaultControl";
			this.DefaultControl.Size = new System.Drawing.Size(1335, 746);
			this.DefaultControl.TabIndex = 0;
			// 
			// TabMenu
			// 
			this.TabMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.TabMenu_OpenProject,
            this.TabMenu_RecentProjects,
            this.toolStripSeparator2,
            this.labelsToolStripMenuItem});
			this.TabMenu.Name = "TabMenu";
			this.TabMenu.Size = new System.Drawing.Size(156, 76);
			this.TabMenu.Closed += new System.Windows.Forms.ToolStripDropDownClosedEventHandler(this.TabMenu_Closed);
			// 
			// TabMenu_OpenProject
			// 
			this.TabMenu_OpenProject.Name = "TabMenu_OpenProject";
			this.TabMenu_OpenProject.Size = new System.Drawing.Size(155, 22);
			this.TabMenu_OpenProject.Text = "Open Project...";
			this.TabMenu_OpenProject.Click += new System.EventHandler(this.TabMenu_OpenProject_Click);
			// 
			// TabMenu_RecentProjects
			// 
			this.TabMenu_RecentProjects.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.TabMenu_Recent_Separator,
            this.TabMenu_RecentProjects_ClearList});
			this.TabMenu_RecentProjects.Name = "TabMenu_RecentProjects";
			this.TabMenu_RecentProjects.Size = new System.Drawing.Size(155, 22);
			this.TabMenu_RecentProjects.Text = "Recent Projects";
			// 
			// TabMenu_Recent_Separator
			// 
			this.TabMenu_Recent_Separator.Name = "TabMenu_Recent_Separator";
			this.TabMenu_Recent_Separator.Size = new System.Drawing.Size(119, 6);
			// 
			// TabMenu_RecentProjects_ClearList
			// 
			this.TabMenu_RecentProjects_ClearList.Name = "TabMenu_RecentProjects_ClearList";
			this.TabMenu_RecentProjects_ClearList.Size = new System.Drawing.Size(122, 22);
			this.TabMenu_RecentProjects_ClearList.Text = "Clear List";
			this.TabMenu_RecentProjects_ClearList.Click += new System.EventHandler(this.TabMenu_RecentProjects_ClearList_Click);
			// 
			// toolStripSeparator2
			// 
			this.toolStripSeparator2.Name = "toolStripSeparator2";
			this.toolStripSeparator2.Size = new System.Drawing.Size(152, 6);
			// 
			// labelsToolStripMenuItem
			// 
			this.labelsToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.TabMenu_TabNames_Stream,
            this.TabMenu_TabNames_WorkspaceName,
            this.TabMenu_TabNames_WorkspaceRoot,
            this.TabMenu_TabNames_ProjectFile});
			this.labelsToolStripMenuItem.Name = "labelsToolStripMenuItem";
			this.labelsToolStripMenuItem.Size = new System.Drawing.Size(155, 22);
			this.labelsToolStripMenuItem.Text = "Labels";
			// 
			// TabMenu_TabNames_Stream
			// 
			this.TabMenu_TabNames_Stream.Name = "TabMenu_TabNames_Stream";
			this.TabMenu_TabNames_Stream.Size = new System.Drawing.Size(167, 22);
			this.TabMenu_TabNames_Stream.Text = "Stream";
			this.TabMenu_TabNames_Stream.Click += new System.EventHandler(this.TabMenu_TabNames_Stream_Click);
			// 
			// TabMenu_TabNames_WorkspaceName
			// 
			this.TabMenu_TabNames_WorkspaceName.Name = "TabMenu_TabNames_WorkspaceName";
			this.TabMenu_TabNames_WorkspaceName.Size = new System.Drawing.Size(167, 22);
			this.TabMenu_TabNames_WorkspaceName.Text = "Workspace Name";
			this.TabMenu_TabNames_WorkspaceName.Click += new System.EventHandler(this.TabMenu_TabNames_WorkspaceName_Click);
			// 
			// TabMenu_TabNames_WorkspaceRoot
			// 
			this.TabMenu_TabNames_WorkspaceRoot.Name = "TabMenu_TabNames_WorkspaceRoot";
			this.TabMenu_TabNames_WorkspaceRoot.Size = new System.Drawing.Size(167, 22);
			this.TabMenu_TabNames_WorkspaceRoot.Text = "Workspace Root";
			this.TabMenu_TabNames_WorkspaceRoot.Click += new System.EventHandler(this.TabMenu_TabNames_WorkspaceRoot_Click);
			// 
			// TabMenu_TabNames_ProjectFile
			// 
			this.TabMenu_TabNames_ProjectFile.Name = "TabMenu_TabNames_ProjectFile";
			this.TabMenu_TabNames_ProjectFile.Size = new System.Drawing.Size(167, 22);
			this.TabMenu_TabNames_ProjectFile.Text = "Project File";
			this.TabMenu_TabNames_ProjectFile.Click += new System.EventHandler(this.TabMenu_TabNames_ProjectFile_Click);
			// 
			// tableLayoutPanel1
			// 
			this.tableLayoutPanel1.ColumnCount = 1;
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Controls.Add(this.TabControl, 0, 0);
			this.tableLayoutPanel1.Controls.Add(this.TabPanel, 0, 2);
			this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
			this.tableLayoutPanel1.Name = "tableLayoutPanel1";
			this.tableLayoutPanel1.Padding = new System.Windows.Forms.Padding(16, 12, 16, 12);
			this.tableLayoutPanel1.RowCount = 3;
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 32F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 6F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Size = new System.Drawing.Size(1367, 808);
			this.tableLayoutPanel1.TabIndex = 4;
			// 
			// TabControl
			// 
			this.TabControl.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.TabControl.Location = new System.Drawing.Point(16, 12);
			this.TabControl.Margin = new System.Windows.Forms.Padding(0);
			this.TabControl.Name = "TabControl";
			this.TabControl.Size = new System.Drawing.Size(1335, 32);
			this.TabControl.TabIndex = 2;
			this.TabControl.Text = "TabControl";
			// 
			// UpdateAlertPositionsTimer
			// 
			this.UpdateAlertPositionsTimer.Enabled = true;
			this.UpdateAlertPositionsTimer.Interval = 1000;
			this.UpdateAlertPositionsTimer.Tick += new System.EventHandler(this.UpdateAlertPositionsTimer_Tick);
			// 
			// CheckLauncherVersionTimer
			// 
			this.CheckLauncherVersionTimer.Enabled = true;
			this.CheckLauncherVersionTimer.Interval = 1800000;
			this.CheckLauncherVersionTimer.Tick += new System.EventHandler(this.CheckLauncherVersionTimer_Tick);
			// 
			// MainWindow
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.ClientSize = new System.Drawing.Size(1367, 808);
			this.Controls.Add(this.tableLayoutPanel1);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MinimumSize = new System.Drawing.Size(800, 350);
			this.Name = "MainWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "UnrealGameSync";
			this.Activated += new System.EventHandler(this.MainWindow_Activated);
			this.Deactivate += new System.EventHandler(this.MainWindow_Deactivate);
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.MainWindow_FormClosing);
			this.Load += new System.EventHandler(this.MainWindow_Load);
			this.TabPanel.ResumeLayout(false);
			this.TabMenu.ResumeLayout(false);
			this.tableLayoutPanel1.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion

		private TabControl TabControl;
		private System.Windows.Forms.Panel TabPanel;
		private StatusPanel DefaultControl;
		private System.Windows.Forms.ContextMenuStrip TabMenu;
		private System.Windows.Forms.ToolStripMenuItem labelsToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_TabNames_Stream;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_TabNames_WorkspaceName;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_TabNames_WorkspaceRoot;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_TabNames_ProjectFile;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_OpenProject;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_RecentProjects;
		private System.Windows.Forms.ToolStripSeparator TabMenu_Recent_Separator;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_RecentProjects_ClearList;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.Timer UpdateAlertPositionsTimer;
		private System.Windows.Forms.Timer CheckLauncherVersionTimer;
	}
}

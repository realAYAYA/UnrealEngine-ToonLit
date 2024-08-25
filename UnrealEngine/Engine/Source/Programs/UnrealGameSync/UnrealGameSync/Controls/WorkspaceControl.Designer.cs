using System.Drawing;

namespace UnrealGameSync
{
	partial class WorkspaceControl
	{
		/// <summary> 
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		#region Component Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.components = new System.ComponentModel.Container();
			this.OptionsContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.OptionsContextMenu_ApplicationSettings = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
			this.OptionsContextMenu_ScheduledSync = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_SyncPrecompiledBinaries = new System.Windows.Forms.ToolStripMenuItem();
			this.disabledToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator11 = new System.Windows.Forms.ToolStripSeparator();
			this.editorToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.editorPhysXToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_AutoResolveConflicts = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_AlwaysClobberFiles = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_SyncFilter = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator3 = new System.Windows.Forms.ToolStripSeparator();
			this.OptionsContextMenu_EditorBuildConfiguration = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_BuildConfig_Debug = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_BuildConfig_DebugGame = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_BuildConfig_Development = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_CustomizeBuildSteps = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_EditorArguments = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator5 = new System.Windows.Forms.ToolStripSeparator();
			this.tabLabelsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_TabNames_Stream = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_TabNames_WorkspaceName = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_TabNames_WorkspaceRoot = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_TabNames_ProjectFile = new System.Windows.Forms.ToolStripMenuItem();
			this.showChangesToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_ShowChanges_ShowUnreviewed = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_ShowChanges_ShowAutomated = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_TimeZone = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_TimeZone_Local = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsContextMenu_TimeZone_PerforceServer = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator6 = new System.Windows.Forms.ToolStripSeparator();
			this.OptionsContextMenu_Diagnostics = new System.Windows.Forms.ToolStripMenuItem();
			this.RunAfterSyncCheckBox = new System.Windows.Forms.CheckBox();
			this.BuildAfterSyncCheckBox = new System.Windows.Forms.CheckBox();
			this.AfterSyncingLabel = new System.Windows.Forms.Label();
			this.BuildListContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.BuildListContextMenu_LaunchEditor = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_Sync = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_SyncContentOnly = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_SyncOnlyThisChange = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_Build = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_Rebuild = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_GenerateProjectFiles = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_Cancel = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_OpenVisualStudio = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_Bisect_Separator = new System.Windows.Forms.ToolStripSeparator();
			this.BuildListContextMenu_Bisect_Pass = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_Bisect_Fail = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_Bisect_Include = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_Bisect_Exclude = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator4 = new System.Windows.Forms.ToolStripSeparator();
			this.BuildListContextMenu_MarkGood = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_MarkBad = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_WithdrawReview = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_LeaveComment = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_EditComment = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_StartInvestigating = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_FinishInvestigating = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.BuildListContextMenu_AddStar = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_RemoveStar = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_TimeZoneSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.BuildListContextMenu_ShowServerTimes = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_ShowLocalTimes = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_CustomTool_Start = new System.Windows.Forms.ToolStripSeparator();
			this.BuildListContextMenu_CustomTool_End = new System.Windows.Forms.ToolStripSeparator();
			this.BuildListContextMenu_ViewInSwarm = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_CopyChangelistNumber = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListContextMenu_MoreInfo = new System.Windows.Forms.ToolStripMenuItem();
			this.NotifyIcon = new System.Windows.Forms.NotifyIcon(this.components);
			this.toolStripSeparator7 = new System.Windows.Forms.ToolStripSeparator();
			this.BuildListToolTip = new System.Windows.Forms.ToolTip(this.components);
			this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
			this.OpenSolutionAfterSyncCheckBox = new System.Windows.Forms.CheckBox();
			this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
			this.OptionsButton = new System.Windows.Forms.Button();
			this.FilterButton = new System.Windows.Forms.Button();
			this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			this.Splitter = new UnrealGameSync.LogSplitContainer();
			this.StatusLayoutPanel = new System.Windows.Forms.TableLayoutPanel();
			this.StatusPanel = new UnrealGameSync.StatusPanel();
			this.BuildList = new UnrealGameSync.BuildListControl();
			this.IconColumn = new System.Windows.Forms.ColumnHeader();
			this.TypeColumn = new System.Windows.Forms.ColumnHeader();
			this.ChangeColumn = new System.Windows.Forms.ColumnHeader();
			this.TimeColumn = new System.Windows.Forms.ColumnHeader();
			this.AuthorColumn = new System.Windows.Forms.ColumnHeader();
			this.DescriptionColumn = new System.Windows.Forms.ColumnHeader();
			this.CISColumn = new System.Windows.Forms.ColumnHeader();
			this.StatusColumn = new System.Windows.Forms.ColumnHeader();
			this.panel1 = new System.Windows.Forms.Panel();
			this.SyncLog = new UnrealGameSync.LogControl();
			this.MoreToolsContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.MoreActionsContextMenu_CustomToolSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.MoreToolsContextMenu_CleanWorkspace = new System.Windows.Forms.ToolStripMenuItem();
			this.MoreToolsContextMenu_UpdateTools = new System.Windows.Forms.ToolStripMenuItem();
			this.SyncContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.toolStripSeparator8 = new System.Windows.Forms.ToolStripSeparator();
			this.SyncContexMenu_EnterChangelist = new System.Windows.Forms.ToolStripMenuItem();
			this.StreamContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.RecentMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.RecentMenu_Browse = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator9 = new System.Windows.Forms.ToolStripSeparator();
			this.RecentMenu_Separator = new System.Windows.Forms.ToolStripSeparator();
			this.RecentMenu_ClearList = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListMultiContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.BuildListMultiContextMenu_Bisect = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListMultiContextMenu_TimeZoneSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.BuildListMultiContextMenu_ShowServerTimes = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildListMultiContextMenu_ShowLocalTimes = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.FilterContextMenu_Default = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_BeforeBadgeSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.FilterContextMenu_Type = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_Type_ShowAll = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator10 = new System.Windows.Forms.ToolStripSeparator();
			this.FilterContextMenu_Type_Code = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_Type_Content = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_Badges = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_Robomerge = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_Robomerge_ShowAll = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_Robomerge_ShowBadged = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_Robomerge_ShowNone = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_AfterRobomergeShowSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.FilterContextMenu_Robomerge_Annotate = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_Author = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterContextMenu_Author_Name = new System.Windows.Forms.ToolStripTextBox();
			this.FilterContextMenu_AfterBadgeSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.FilterContextMenu_ShowBuildMachineChanges = new System.Windows.Forms.ToolStripMenuItem();
			this.BadgeContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.BuildHealthContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.BuildHealthContextMenu_Browse = new System.Windows.Forms.ToolStripMenuItem();
			this.BuildHealthContextMenu_MinSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.BuildHealthContextMenu_MaxSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.BuildHealthContextMenu_Settings = new System.Windows.Forms.ToolStripMenuItem();
			this.EditorConfigWatcher = new System.IO.FileSystemWatcher();
			this.OptionsContextMenu.SuspendLayout();
			this.BuildListContextMenu.SuspendLayout();
			this.flowLayoutPanel1.SuspendLayout();
			this.tableLayoutPanel3.SuspendLayout();
			this.tableLayoutPanel2.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.Splitter)).BeginInit();
			this.Splitter.Panel1.SuspendLayout();
			this.Splitter.Panel2.SuspendLayout();
			this.Splitter.SuspendLayout();
			this.StatusLayoutPanel.SuspendLayout();
			this.panel1.SuspendLayout();
			this.MoreToolsContextMenu.SuspendLayout();
			this.SyncContextMenu.SuspendLayout();
			this.RecentMenu.SuspendLayout();
			this.BuildListMultiContextMenu.SuspendLayout();
			this.FilterContextMenu.SuspendLayout();
			this.BadgeContextMenu.SuspendLayout();
			this.BuildHealthContextMenu.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.EditorConfigWatcher)).BeginInit();
			this.SuspendLayout();
			// 
			// OptionsContextMenu
			// 
			this.OptionsContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.OptionsContextMenu_ApplicationSettings,
			this.toolStripSeparator2,
			this.OptionsContextMenu_ScheduledSync,
			this.OptionsContextMenu_SyncPrecompiledBinaries,
			this.OptionsContextMenu_AutoResolveConflicts,
			this.OptionsContextMenu_AlwaysClobberFiles,
			this.OptionsContextMenu_SyncFilter,
			this.toolStripSeparator3,
			this.OptionsContextMenu_EditorBuildConfiguration,
			this.OptionsContextMenu_CustomizeBuildSteps,
			this.OptionsContextMenu_EditorArguments,
			this.toolStripSeparator5,
			this.tabLabelsToolStripMenuItem,
			this.showChangesToolStripMenuItem,
			this.OptionsContextMenu_TimeZone,
			this.toolStripSeparator6,
			this.OptionsContextMenu_Diagnostics});
			this.OptionsContextMenu.Name = "ToolsMenuStrip";
			this.OptionsContextMenu.Size = new System.Drawing.Size(262, 292);
			// 
			// OptionsContextMenu_ApplicationSettings
			// 
			this.OptionsContextMenu_ApplicationSettings.Name = "OptionsContextMenu_ApplicationSettings";
			this.OptionsContextMenu_ApplicationSettings.Size = new System.Drawing.Size(261, 22);
			this.OptionsContextMenu_ApplicationSettings.Text = "Application Settings...";
			this.OptionsContextMenu_ApplicationSettings.Click += new System.EventHandler(this.OptionsContextMenu_ApplicationSettings_Click);
			// 
			// toolStripSeparator2
			// 
			this.toolStripSeparator2.Name = "toolStripSeparator2";
			this.toolStripSeparator2.Size = new System.Drawing.Size(258, 6);
			// 
			// OptionsContextMenu_ScheduledSync
			// 
			this.OptionsContextMenu_ScheduledSync.Name = "OptionsContextMenu_ScheduledSync";
			this.OptionsContextMenu_ScheduledSync.Size = new System.Drawing.Size(261, 22);
			this.OptionsContextMenu_ScheduledSync.Text = "Scheduled Sync...";
			this.OptionsContextMenu_ScheduledSync.Click += new System.EventHandler(this.OptionsContextMenu_ScheduleSync_Click);
			// 
			// OptionsContextMenu_SyncPrecompiledBinaries
			// 
			this.OptionsContextMenu_SyncPrecompiledBinaries.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.disabledToolStripMenuItem,
			this.toolStripSeparator11,
			this.editorToolStripMenuItem,
			this.editorPhysXToolStripMenuItem});
			this.OptionsContextMenu_SyncPrecompiledBinaries.Name = "OptionsContextMenu_SyncPrecompiledBinaries";
			this.OptionsContextMenu_SyncPrecompiledBinaries.Size = new System.Drawing.Size(261, 22);
			this.OptionsContextMenu_SyncPrecompiledBinaries.Text = "Sync Precompiled Binaries";
			this.OptionsContextMenu_SyncPrecompiledBinaries.Click += new System.EventHandler(this.OptionsContextMenu_SyncPrecompiledBinaries_Click);
			// 
			// disabledToolStripMenuItem
			// 
			this.disabledToolStripMenuItem.Name = "disabledToolStripMenuItem";
			this.disabledToolStripMenuItem.Size = new System.Drawing.Size(210, 22);
			this.disabledToolStripMenuItem.Text = "Disabled (compile locally)";
			// 
			// toolStripSeparator11
			// 
			this.toolStripSeparator11.Name = "toolStripSeparator11";
			this.toolStripSeparator11.Size = new System.Drawing.Size(207, 6);
			// 
			// editorToolStripMenuItem
			// 
			this.editorToolStripMenuItem.Name = "editorToolStripMenuItem";
			this.editorToolStripMenuItem.Size = new System.Drawing.Size(210, 22);
			this.editorToolStripMenuItem.Text = "Editor";
			// 
			// editorPhysXToolStripMenuItem
			// 
			this.editorPhysXToolStripMenuItem.Name = "editorPhysXToolStripMenuItem";
			this.editorPhysXToolStripMenuItem.Size = new System.Drawing.Size(210, 22);
			this.editorPhysXToolStripMenuItem.Text = "Editor (PhysX)";
			// 
			// OptionsContextMenu_AutoResolveConflicts
			// 
			this.OptionsContextMenu_AutoResolveConflicts.Name = "OptionsContextMenu_AutoResolveConflicts";
			this.OptionsContextMenu_AutoResolveConflicts.Size = new System.Drawing.Size(261, 22);
			this.OptionsContextMenu_AutoResolveConflicts.Text = "Auto-Resolve Conflicts";
			this.OptionsContextMenu_AutoResolveConflicts.Click += new System.EventHandler(this.OptionsContextMenu_AutoResolveConflicts_Click);
			// 
			// OptionsContextMenu_AlwaysClobberFiles
			// 
			OptionsContextMenu_AlwaysClobberFiles.Name = "OptionsContextMenu_AlwaysClobberFiles";
			OptionsContextMenu_AlwaysClobberFiles.Size = new Size(261, 22);
			OptionsContextMenu_AlwaysClobberFiles.Text = "Always Clobber Files";
			OptionsContextMenu_AlwaysClobberFiles.Click += OptionsContextMenu_AlwaysClobberFiles_Click;
			// 
			// OptionsContextMenu_SyncFilter
			// 
			this.OptionsContextMenu_SyncFilter.Name = "OptionsContextMenu_SyncFilter";
			this.OptionsContextMenu_SyncFilter.Size = new System.Drawing.Size(261, 22);
			this.OptionsContextMenu_SyncFilter.Text = "Sync Filter...";
			this.OptionsContextMenu_SyncFilter.Click += new System.EventHandler(this.OptionsContextMenu_SyncFilter_Click);
			// 
			// toolStripSeparator3
			// 
			this.toolStripSeparator3.Name = "toolStripSeparator3";
			this.toolStripSeparator3.Size = new System.Drawing.Size(258, 6);
			// 
			// OptionsContextMenu_EditorBuildConfiguration
			// 
			this.OptionsContextMenu_EditorBuildConfiguration.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.OptionsContextMenu_BuildConfig_Debug,
			this.OptionsContextMenu_BuildConfig_DebugGame,
			this.OptionsContextMenu_BuildConfig_Development});
			this.OptionsContextMenu_EditorBuildConfiguration.Name = "OptionsContextMenu_EditorBuildConfiguration";
			this.OptionsContextMenu_EditorBuildConfiguration.Size = new System.Drawing.Size(261, 22);
			this.OptionsContextMenu_EditorBuildConfiguration.Text = "Editor Build Configuration";
			// 
			// OptionsContextMenu_BuildConfig_Debug
			// 
			this.OptionsContextMenu_BuildConfig_Debug.Name = "OptionsContextMenu_BuildConfig_Debug";
			this.OptionsContextMenu_BuildConfig_Debug.Size = new System.Drawing.Size(145, 22);
			this.OptionsContextMenu_BuildConfig_Debug.Text = "Debug";
			this.OptionsContextMenu_BuildConfig_Debug.Click += new System.EventHandler(this.OptionsContextMenu_BuildConfig_Debug_Click);
			// 
			// OptionsContextMenu_BuildConfig_DebugGame
			// 
			this.OptionsContextMenu_BuildConfig_DebugGame.Name = "OptionsContextMenu_BuildConfig_DebugGame";
			this.OptionsContextMenu_BuildConfig_DebugGame.Size = new System.Drawing.Size(145, 22);
			this.OptionsContextMenu_BuildConfig_DebugGame.Text = "DebugGame";
			this.OptionsContextMenu_BuildConfig_DebugGame.Click += new System.EventHandler(this.OptionsContextMenu_BuildConfig_DebugGame_Click);
			// 
			// OptionsContextMenu_BuildConfig_Development
			// 
			this.OptionsContextMenu_BuildConfig_Development.Checked = true;
			this.OptionsContextMenu_BuildConfig_Development.CheckState = System.Windows.Forms.CheckState.Checked;
			this.OptionsContextMenu_BuildConfig_Development.Name = "OptionsContextMenu_BuildConfig_Development";
			this.OptionsContextMenu_BuildConfig_Development.Size = new System.Drawing.Size(145, 22);
			this.OptionsContextMenu_BuildConfig_Development.Text = "Development";
			this.OptionsContextMenu_BuildConfig_Development.Click += new System.EventHandler(this.OptionsContextMenu_BuildConfig_Development_Click);
			// 
			// OptionsContextMenu_CustomizeBuildSteps
			// 
			this.OptionsContextMenu_CustomizeBuildSteps.Name = "OptionsContextMenu_CustomizeBuildSteps";
			this.OptionsContextMenu_CustomizeBuildSteps.Size = new System.Drawing.Size(261, 22);
			this.OptionsContextMenu_CustomizeBuildSteps.Text = "Customize Commands...";
			this.OptionsContextMenu_CustomizeBuildSteps.Click += new System.EventHandler(this.OptionsContextMenu_EditBuildSteps_Click);
			// 
			// OptionsContextMenu_EditorArguments
			// 
			this.OptionsContextMenu_EditorArguments.Name = "OptionsContextMenu_EditorArguments";
			this.OptionsContextMenu_EditorArguments.Size = new System.Drawing.Size(261, 22);
			this.OptionsContextMenu_EditorArguments.Text = "Editor Command Line Arguments...";
			this.OptionsContextMenu_EditorArguments.Click += new System.EventHandler(this.OptionsContextMenu_EditorArguments_Click);
			// 
			// toolStripSeparator5
			// 
			this.toolStripSeparator5.Name = "toolStripSeparator5";
			this.toolStripSeparator5.Size = new System.Drawing.Size(258, 6);
			// 
			// tabLabelsToolStripMenuItem
			// 
			this.tabLabelsToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.OptionsContextMenu_TabNames_Stream,
			this.OptionsContextMenu_TabNames_WorkspaceName,
			this.OptionsContextMenu_TabNames_WorkspaceRoot,
			this.OptionsContextMenu_TabNames_ProjectFile});
			this.tabLabelsToolStripMenuItem.Name = "tabLabelsToolStripMenuItem";
			this.tabLabelsToolStripMenuItem.Size = new System.Drawing.Size(261, 22);
			this.tabLabelsToolStripMenuItem.Text = "Tab Names";
			// 
			// OptionsContextMenu_TabNames_Stream
			// 
			this.OptionsContextMenu_TabNames_Stream.Name = "OptionsContextMenu_TabNames_Stream";
			this.OptionsContextMenu_TabNames_Stream.Size = new System.Drawing.Size(167, 22);
			this.OptionsContextMenu_TabNames_Stream.Text = "Stream";
			this.OptionsContextMenu_TabNames_Stream.Click += new System.EventHandler(this.OptionsContextMenu_TabNames_Stream_Click);
			// 
			// OptionsContextMenu_TabNames_WorkspaceName
			// 
			this.OptionsContextMenu_TabNames_WorkspaceName.Name = "OptionsContextMenu_TabNames_WorkspaceName";
			this.OptionsContextMenu_TabNames_WorkspaceName.Size = new System.Drawing.Size(167, 22);
			this.OptionsContextMenu_TabNames_WorkspaceName.Text = "Workspace Name";
			this.OptionsContextMenu_TabNames_WorkspaceName.Click += new System.EventHandler(this.OptionsContextMenu_TabNames_WorkspaceName_Click);
			// 
			// OptionsContextMenu_TabNames_WorkspaceRoot
			// 
			this.OptionsContextMenu_TabNames_WorkspaceRoot.Name = "OptionsContextMenu_TabNames_WorkspaceRoot";
			this.OptionsContextMenu_TabNames_WorkspaceRoot.Size = new System.Drawing.Size(167, 22);
			this.OptionsContextMenu_TabNames_WorkspaceRoot.Text = "Workspace Root";
			this.OptionsContextMenu_TabNames_WorkspaceRoot.Click += new System.EventHandler(this.OptionsContextMenu_TabNames_WorkspaceRoot_Click);
			// 
			// OptionsContextMenu_TabNames_ProjectFile
			// 
			this.OptionsContextMenu_TabNames_ProjectFile.Name = "OptionsContextMenu_TabNames_ProjectFile";
			this.OptionsContextMenu_TabNames_ProjectFile.Size = new System.Drawing.Size(167, 22);
			this.OptionsContextMenu_TabNames_ProjectFile.Text = "Project File";
			this.OptionsContextMenu_TabNames_ProjectFile.Click += new System.EventHandler(this.OptionsContextMenu_TabNames_ProjectFile_Click);
			// 
			// showChangesToolStripMenuItem
			// 
			this.showChangesToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.OptionsContextMenu_ShowChanges_ShowUnreviewed,
			this.OptionsContextMenu_ShowChanges_ShowAutomated});
			this.showChangesToolStripMenuItem.Name = "showChangesToolStripMenuItem";
			this.showChangesToolStripMenuItem.Size = new System.Drawing.Size(261, 22);
			this.showChangesToolStripMenuItem.Text = "Show Changes";
			// 
			// OptionsContextMenu_ShowChanges_ShowUnreviewed
			// 
			this.OptionsContextMenu_ShowChanges_ShowUnreviewed.Name = "OptionsContextMenu_ShowChanges_ShowUnreviewed";
			this.OptionsContextMenu_ShowChanges_ShowUnreviewed.Size = new System.Drawing.Size(281, 22);
			this.OptionsContextMenu_ShowChanges_ShowUnreviewed.Text = "Show changes without reviews";
			this.OptionsContextMenu_ShowChanges_ShowUnreviewed.Click += new System.EventHandler(this.OptionsContextMenu_ShowChanges_ShowUnreviewed_Click);
			// 
			// OptionsContextMenu_ShowChanges_ShowAutomated
			// 
			this.OptionsContextMenu_ShowChanges_ShowAutomated.Name = "OptionsContextMenu_ShowChanges_ShowAutomated";
			this.OptionsContextMenu_ShowChanges_ShowAutomated.Size = new System.Drawing.Size(281, 22);
			this.OptionsContextMenu_ShowChanges_ShowAutomated.Text = "Show changes by automated processes";
			this.OptionsContextMenu_ShowChanges_ShowAutomated.Click += new System.EventHandler(this.OptionsContextMenu_ShowChanges_ShowAutomated_Click);
			// 
			// OptionsContextMenu_TimeZone
			// 
			this.OptionsContextMenu_TimeZone.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.OptionsContextMenu_TimeZone_Local,
			this.OptionsContextMenu_TimeZone_PerforceServer});
			this.OptionsContextMenu_TimeZone.Name = "OptionsContextMenu_TimeZone";
			this.OptionsContextMenu_TimeZone.Size = new System.Drawing.Size(261, 22);
			this.OptionsContextMenu_TimeZone.Text = "Time Zone";
			// 
			// OptionsContextMenu_TimeZone_Local
			// 
			this.OptionsContextMenu_TimeZone_Local.Name = "OptionsContextMenu_TimeZone_Local";
			this.OptionsContextMenu_TimeZone_Local.Size = new System.Drawing.Size(153, 22);
			this.OptionsContextMenu_TimeZone_Local.Text = "Local";
			this.OptionsContextMenu_TimeZone_Local.Click += new System.EventHandler(this.BuildListContextMenu_ShowLocalTimes_Click);
			// 
			// OptionsContextMenu_TimeZone_PerforceServer
			// 
			this.OptionsContextMenu_TimeZone_PerforceServer.Name = "OptionsContextMenu_TimeZone_PerforceServer";
			this.OptionsContextMenu_TimeZone_PerforceServer.Size = new System.Drawing.Size(153, 22);
			this.OptionsContextMenu_TimeZone_PerforceServer.Text = "Perforce Server";
			this.OptionsContextMenu_TimeZone_PerforceServer.Click += new System.EventHandler(this.BuildListContextMenu_ShowServerTimes_Click);
			// 
			// toolStripSeparator6
			// 
			this.toolStripSeparator6.Name = "toolStripSeparator6";
			this.toolStripSeparator6.Size = new System.Drawing.Size(258, 6);
			// 
			// OptionsContextMenu_Diagnostics
			// 
			this.OptionsContextMenu_Diagnostics.Name = "OptionsContextMenu_Diagnostics";
			this.OptionsContextMenu_Diagnostics.Size = new System.Drawing.Size(261, 22);
			this.OptionsContextMenu_Diagnostics.Text = "Diagnostics...";
			this.OptionsContextMenu_Diagnostics.Click += new System.EventHandler(this.OptionsContextMenu_Diagnostics_Click);
			// 
			// RunAfterSyncCheckBox
			// 
			this.RunAfterSyncCheckBox.AutoSize = true;
			this.RunAfterSyncCheckBox.Location = new System.Drawing.Point(153, 0);
			this.RunAfterSyncCheckBox.Margin = new System.Windows.Forms.Padding(3, 0, 3, 0);
			this.RunAfterSyncCheckBox.Name = "RunAfterSyncCheckBox";
			this.RunAfterSyncCheckBox.Size = new System.Drawing.Size(47, 19);
			this.RunAfterSyncCheckBox.TabIndex = 6;
			this.RunAfterSyncCheckBox.Text = "Run";
			this.RunAfterSyncCheckBox.UseVisualStyleBackColor = true;
			this.RunAfterSyncCheckBox.CheckedChanged += new System.EventHandler(this.RunAfterSyncCheckBox_CheckedChanged);
			// 
			// BuildAfterSyncCheckBox
			// 
			this.BuildAfterSyncCheckBox.AutoSize = true;
			this.BuildAfterSyncCheckBox.Location = new System.Drawing.Point(94, 0);
			this.BuildAfterSyncCheckBox.Margin = new System.Windows.Forms.Padding(3, 0, 3, 0);
			this.BuildAfterSyncCheckBox.Name = "BuildAfterSyncCheckBox";
			this.BuildAfterSyncCheckBox.Size = new System.Drawing.Size(53, 19);
			this.BuildAfterSyncCheckBox.TabIndex = 5;
			this.BuildAfterSyncCheckBox.Text = "Build";
			this.BuildAfterSyncCheckBox.UseVisualStyleBackColor = true;
			this.BuildAfterSyncCheckBox.CheckedChanged += new System.EventHandler(this.BuildAfterSyncCheckBox_CheckedChanged);
			// 
			// AfterSyncingLabel
			// 
			this.AfterSyncingLabel.AutoSize = true;
			this.AfterSyncingLabel.Location = new System.Drawing.Point(3, 0);
			this.AfterSyncingLabel.Name = "AfterSyncingLabel";
			this.AfterSyncingLabel.Padding = new System.Windows.Forms.Padding(0, 1, 5, 0);
			this.AfterSyncingLabel.Size = new System.Drawing.Size(85, 16);
			this.AfterSyncingLabel.TabIndex = 4;
			this.AfterSyncingLabel.Text = "After syncing:";
			// 
			// BuildListContextMenu
			// 
			this.BuildListContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.BuildListContextMenu_LaunchEditor,
			this.BuildListContextMenu_Sync,
			this.BuildListContextMenu_SyncContentOnly,
			this.BuildListContextMenu_SyncOnlyThisChange,
			this.BuildListContextMenu_Build,
			this.BuildListContextMenu_Rebuild,
			this.BuildListContextMenu_GenerateProjectFiles,
			this.BuildListContextMenu_Cancel,
			this.BuildListContextMenu_OpenVisualStudio,
			this.BuildListContextMenu_Bisect_Separator,
			this.BuildListContextMenu_Bisect_Pass,
			this.BuildListContextMenu_Bisect_Fail,
			this.BuildListContextMenu_Bisect_Include,
			this.BuildListContextMenu_Bisect_Exclude,
			this.toolStripSeparator4,
			this.BuildListContextMenu_MarkGood,
			this.BuildListContextMenu_MarkBad,
			this.BuildListContextMenu_WithdrawReview,
			this.BuildListContextMenu_LeaveComment,
			this.BuildListContextMenu_EditComment,
			this.BuildListContextMenu_StartInvestigating,
			this.BuildListContextMenu_FinishInvestigating,
			this.toolStripSeparator1,
			this.BuildListContextMenu_AddStar,
			this.BuildListContextMenu_RemoveStar,
			this.BuildListContextMenu_TimeZoneSeparator,
			this.BuildListContextMenu_ShowServerTimes,
			this.BuildListContextMenu_ShowLocalTimes,
			this.BuildListContextMenu_CustomTool_Start,
			this.BuildListContextMenu_CustomTool_End,
			this.BuildListContextMenu_ViewInSwarm,
			this.BuildListContextMenu_CopyChangelistNumber,
			this.BuildListContextMenu_MoreInfo});
			this.BuildListContextMenu.Name = "BuildListContextMenu";
			this.BuildListContextMenu.Size = new System.Drawing.Size(199, 634);
			// 
			// BuildListContextMenu_LaunchEditor
			// 
			this.BuildListContextMenu_LaunchEditor.Name = "BuildListContextMenu_LaunchEditor";
			this.BuildListContextMenu_LaunchEditor.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_LaunchEditor.Text = "Launch editor";
			this.BuildListContextMenu_LaunchEditor.Click += new System.EventHandler(this.BuildListContextMenu_LaunchEditor_Click);
			// 
			// BuildListContextMenu_Sync
			// 
			this.BuildListContextMenu_Sync.Name = "BuildListContextMenu_Sync";
			this.BuildListContextMenu_Sync.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_Sync.Text = "Sync";
			this.BuildListContextMenu_Sync.Click += new System.EventHandler(this.BuildListContextMenu_Sync_Click);
			// 
			// BuildListContextMenu_SyncContentOnly
			// 
			this.BuildListContextMenu_SyncContentOnly.Name = "BuildListContextMenu_SyncContentOnly";
			this.BuildListContextMenu_SyncContentOnly.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_SyncContentOnly.Text = "Sync (Just Content)";
			this.BuildListContextMenu_SyncContentOnly.Click += new System.EventHandler(this.BuildListContextMenu_SyncContentOnly_Click);
			// 
			// BuildListContextMenu_SyncOnlyThisChange
			// 
			this.BuildListContextMenu_SyncOnlyThisChange.Name = "BuildListContextMenu_SyncOnlyThisChange";
			this.BuildListContextMenu_SyncOnlyThisChange.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_SyncOnlyThisChange.Text = "Sync (Just This Change)";
			this.BuildListContextMenu_SyncOnlyThisChange.Click += new System.EventHandler(this.BuildListContextMenu_SyncOnlyThisChange_Click);
			// 
			// BuildListContextMenu_Build
			// 
			this.BuildListContextMenu_Build.Name = "BuildListContextMenu_Build";
			this.BuildListContextMenu_Build.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_Build.Text = "Build";
			this.BuildListContextMenu_Build.Click += new System.EventHandler(this.BuildListContextMenu_Build_Click);
			// 
			// BuildListContextMenu_Rebuild
			// 
			this.BuildListContextMenu_Rebuild.Name = "BuildListContextMenu_Rebuild";
			this.BuildListContextMenu_Rebuild.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_Rebuild.Text = "Rebuild";
			this.BuildListContextMenu_Rebuild.Click += new System.EventHandler(this.BuildListContextMenu_Rebuild_Click);
			// 
			// BuildListContextMenu_GenerateProjectFiles
			// 
			this.BuildListContextMenu_GenerateProjectFiles.Name = "BuildListContextMenu_GenerateProjectFiles";
			this.BuildListContextMenu_GenerateProjectFiles.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_GenerateProjectFiles.Text = "Generate project files";
			this.BuildListContextMenu_GenerateProjectFiles.Click += new System.EventHandler(this.BuildListContextMenu_GenerateProjectFiles_Click);
			// 
			// BuildListContextMenu_Cancel
			// 
			this.BuildListContextMenu_Cancel.Name = "BuildListContextMenu_Cancel";
			this.BuildListContextMenu_Cancel.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_Cancel.Text = "Cancel";
			this.BuildListContextMenu_Cancel.Click += new System.EventHandler(this.BuildListContextMenu_CancelSync_Click);
			// 
			// BuildListContextMenu_OpenVisualStudio
			// 
			this.BuildListContextMenu_OpenVisualStudio.Name = "BuildListContextMenu_OpenVisualStudio";
			this.BuildListContextMenu_OpenVisualStudio.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_OpenVisualStudio.Text = "Open in Visual Studio...";
			this.BuildListContextMenu_OpenVisualStudio.Click += new System.EventHandler(this.BuildListContextMenu_OpenVisualStudio_Click);
			// 
			// BuildListContextMenu_Bisect_Separator
			// 
			this.BuildListContextMenu_Bisect_Separator.Name = "BuildListContextMenu_Bisect_Separator";
			this.BuildListContextMenu_Bisect_Separator.Size = new System.Drawing.Size(195, 6);
			// 
			// BuildListContextMenu_Bisect_Pass
			// 
			this.BuildListContextMenu_Bisect_Pass.Name = "BuildListContextMenu_Bisect_Pass";
			this.BuildListContextMenu_Bisect_Pass.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_Bisect_Pass.Text = "Bisect: Pass";
			this.BuildListContextMenu_Bisect_Pass.Click += new System.EventHandler(this.BuildListContextMenu_Bisect_Pass_Click);
			// 
			// BuildListContextMenu_Bisect_Fail
			// 
			this.BuildListContextMenu_Bisect_Fail.Name = "BuildListContextMenu_Bisect_Fail";
			this.BuildListContextMenu_Bisect_Fail.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_Bisect_Fail.Text = "Bisect: Fail";
			this.BuildListContextMenu_Bisect_Fail.Click += new System.EventHandler(this.BuildListContextMenu_Bisect_Fail_Click);
			// 
			// BuildListContextMenu_Bisect_Include
			// 
			this.BuildListContextMenu_Bisect_Include.Name = "BuildListContextMenu_Bisect_Include";
			this.BuildListContextMenu_Bisect_Include.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_Bisect_Include.Text = "Bisect: Include";
			this.BuildListContextMenu_Bisect_Include.Click += new System.EventHandler(this.BuildListContextMenu_Bisect_Include_Click);
			// 
			// BuildListContextMenu_Bisect_Exclude
			// 
			this.BuildListContextMenu_Bisect_Exclude.Name = "BuildListContextMenu_Bisect_Exclude";
			this.BuildListContextMenu_Bisect_Exclude.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_Bisect_Exclude.Text = "Bisect: Exclude";
			this.BuildListContextMenu_Bisect_Exclude.Click += new System.EventHandler(this.BuildListContextMenu_Bisect_Exclude_Click);
			// 
			// toolStripSeparator4
			// 
			this.toolStripSeparator4.Name = "toolStripSeparator4";
			this.toolStripSeparator4.Size = new System.Drawing.Size(195, 6);
			// 
			// BuildListContextMenu_MarkGood
			// 
			this.BuildListContextMenu_MarkGood.Name = "BuildListContextMenu_MarkGood";
			this.BuildListContextMenu_MarkGood.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_MarkGood.Text = "Mark as good";
			this.BuildListContextMenu_MarkGood.Click += new System.EventHandler(this.BuildListContextMenu_MarkGood_Click);
			// 
			// BuildListContextMenu_MarkBad
			// 
			this.BuildListContextMenu_MarkBad.Name = "BuildListContextMenu_MarkBad";
			this.BuildListContextMenu_MarkBad.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_MarkBad.Text = "Mark as bad";
			this.BuildListContextMenu_MarkBad.Click += new System.EventHandler(this.BuildListContextMenu_MarkBad_Click);
			// 
			// BuildListContextMenu_WithdrawReview
			// 
			this.BuildListContextMenu_WithdrawReview.Name = "BuildListContextMenu_WithdrawReview";
			this.BuildListContextMenu_WithdrawReview.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_WithdrawReview.Text = "Withdraw review";
			this.BuildListContextMenu_WithdrawReview.Click += new System.EventHandler(this.BuildListContextMenu_WithdrawReview_Click);
			// 
			// BuildListContextMenu_LeaveComment
			// 
			this.BuildListContextMenu_LeaveComment.Name = "BuildListContextMenu_LeaveComment";
			this.BuildListContextMenu_LeaveComment.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_LeaveComment.Text = "Leave comment...";
			this.BuildListContextMenu_LeaveComment.Click += new System.EventHandler(this.BuildListContextMenu_LeaveOrEditComment_Click);
			// 
			// BuildListContextMenu_EditComment
			// 
			this.BuildListContextMenu_EditComment.Name = "BuildListContextMenu_EditComment";
			this.BuildListContextMenu_EditComment.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_EditComment.Text = "Edit comment...";
			this.BuildListContextMenu_EditComment.Click += new System.EventHandler(this.BuildListContextMenu_LeaveOrEditComment_Click);
			// 
			// BuildListContextMenu_StartInvestigating
			// 
			this.BuildListContextMenu_StartInvestigating.Name = "BuildListContextMenu_StartInvestigating";
			this.BuildListContextMenu_StartInvestigating.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_StartInvestigating.Text = "Start investigating";
			this.BuildListContextMenu_StartInvestigating.Click += new System.EventHandler(this.BuildListContextMenu_StartInvestigating_Click);
			// 
			// BuildListContextMenu_FinishInvestigating
			// 
			this.BuildListContextMenu_FinishInvestigating.Name = "BuildListContextMenu_FinishInvestigating";
			this.BuildListContextMenu_FinishInvestigating.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_FinishInvestigating.Text = "Finish investigating";
			this.BuildListContextMenu_FinishInvestigating.Click += new System.EventHandler(this.BuildListContextMenu_FinishInvestigating_Click);
			// 
			// toolStripSeparator1
			// 
			this.toolStripSeparator1.Name = "toolStripSeparator1";
			this.toolStripSeparator1.Size = new System.Drawing.Size(195, 6);
			// 
			// BuildListContextMenu_AddStar
			// 
			this.BuildListContextMenu_AddStar.Name = "BuildListContextMenu_AddStar";
			this.BuildListContextMenu_AddStar.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_AddStar.Text = "Add Star";
			this.BuildListContextMenu_AddStar.Click += new System.EventHandler(this.BuildListContextMenu_AddStar_Click);
			// 
			// BuildListContextMenu_RemoveStar
			// 
			this.BuildListContextMenu_RemoveStar.Name = "BuildListContextMenu_RemoveStar";
			this.BuildListContextMenu_RemoveStar.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_RemoveStar.Text = "Remove Star";
			this.BuildListContextMenu_RemoveStar.Click += new System.EventHandler(this.BuildListContextMenu_RemoveStar_Click);
			// 
			// BuildListContextMenu_TimeZoneSeparator
			// 
			this.BuildListContextMenu_TimeZoneSeparator.Name = "BuildListContextMenu_TimeZoneSeparator";
			this.BuildListContextMenu_TimeZoneSeparator.Size = new System.Drawing.Size(195, 6);
			// 
			// BuildListContextMenu_ShowServerTimes
			// 
			this.BuildListContextMenu_ShowServerTimes.Name = "BuildListContextMenu_ShowServerTimes";
			this.BuildListContextMenu_ShowServerTimes.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_ShowServerTimes.Text = "Show server times";
			this.BuildListContextMenu_ShowServerTimes.Click += new System.EventHandler(this.BuildListContextMenu_ShowServerTimes_Click);
			// 
			// BuildListContextMenu_ShowLocalTimes
			// 
			this.BuildListContextMenu_ShowLocalTimes.Name = "BuildListContextMenu_ShowLocalTimes";
			this.BuildListContextMenu_ShowLocalTimes.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_ShowLocalTimes.Text = "Show local times";
			this.BuildListContextMenu_ShowLocalTimes.Click += new System.EventHandler(this.BuildListContextMenu_ShowLocalTimes_Click);
			// 
			// BuildListContextMenu_CustomTool_Start
			// 
			this.BuildListContextMenu_CustomTool_Start.Name = "BuildListContextMenu_CustomTool_Start";
			this.BuildListContextMenu_CustomTool_Start.Size = new System.Drawing.Size(195, 6);
			// 
			// BuildListContextMenu_CustomTool_End
			// 
			this.BuildListContextMenu_CustomTool_End.Name = "BuildListContextMenu_CustomTool_End";
			this.BuildListContextMenu_CustomTool_End.Size = new System.Drawing.Size(195, 6);
			// 
			// BuildListContextMenu_ViewInSwarm
			// 
			this.BuildListContextMenu_ViewInSwarm.Name = "BuildListContextMenu_ViewInSwarm";
			this.BuildListContextMenu_ViewInSwarm.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_ViewInSwarm.Text = "View in Swarm...";
			this.BuildListContextMenu_ViewInSwarm.Click += new System.EventHandler(this.BuildListContextMenu_ViewInSwarm_Click);
			// 
			// BuildListContextMenu_CopyChangelistNumber
			// 
			this.BuildListContextMenu_CopyChangelistNumber.Name = "BuildListContextMenu_CopyChangelistNumber";
			this.BuildListContextMenu_CopyChangelistNumber.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_CopyChangelistNumber.Text = "Copy Changelist";
			this.BuildListContextMenu_CopyChangelistNumber.Click += new System.EventHandler(this.BuildListContextMenu_CopyChangelistNumber_Click);
			// 
			// BuildListContextMenu_MoreInfo
			// 
			this.BuildListContextMenu_MoreInfo.Name = "BuildListContextMenu_MoreInfo";
			this.BuildListContextMenu_MoreInfo.Size = new System.Drawing.Size(198, 22);
			this.BuildListContextMenu_MoreInfo.Text = "More Info...";
			this.BuildListContextMenu_MoreInfo.Click += new System.EventHandler(this.BuildListContextMenu_MoreInfo_Click);
			// 
			// toolStripSeparator7
			// 
			this.toolStripSeparator7.Name = "toolStripSeparator7";
			this.toolStripSeparator7.Size = new System.Drawing.Size(193, 6);
			// 
			// flowLayoutPanel1
			// 
			this.flowLayoutPanel1.Anchor = System.Windows.Forms.AnchorStyles.None;
			this.flowLayoutPanel1.AutoSize = true;
			this.flowLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.flowLayoutPanel1.Controls.Add(this.AfterSyncingLabel);
			this.flowLayoutPanel1.Controls.Add(this.BuildAfterSyncCheckBox);
			this.flowLayoutPanel1.Controls.Add(this.RunAfterSyncCheckBox);
			this.flowLayoutPanel1.Controls.Add(this.OpenSolutionAfterSyncCheckBox);
			this.flowLayoutPanel1.Location = new System.Drawing.Point(526, 17);
			this.flowLayoutPanel1.Margin = new System.Windows.Forms.Padding(0, 4, 0, 3);
			this.flowLayoutPanel1.Name = "flowLayoutPanel1";
			this.flowLayoutPanel1.Size = new System.Drawing.Size(311, 19);
			this.flowLayoutPanel1.TabIndex = 8;
			this.flowLayoutPanel1.WrapContents = false;
			// 
			// OpenSolutionAfterSyncCheckBox
			// 
			this.OpenSolutionAfterSyncCheckBox.AutoSize = true;
			this.OpenSolutionAfterSyncCheckBox.Location = new System.Drawing.Point(206, 0);
			this.OpenSolutionAfterSyncCheckBox.Margin = new System.Windows.Forms.Padding(3, 0, 3, 0);
			this.OpenSolutionAfterSyncCheckBox.Name = "OpenSolutionAfterSyncCheckBox";
			this.OpenSolutionAfterSyncCheckBox.Size = new System.Drawing.Size(102, 19);
			this.OpenSolutionAfterSyncCheckBox.TabIndex = 7;
			this.OpenSolutionAfterSyncCheckBox.Text = "Open Solution";
			this.OpenSolutionAfterSyncCheckBox.UseVisualStyleBackColor = true;
			this.OpenSolutionAfterSyncCheckBox.CheckedChanged += new System.EventHandler(this.OpenSolutionAfterSyncCheckBox_CheckedChanged);
			// 
			// tableLayoutPanel3
			// 
			this.tableLayoutPanel3.AutoSize = true;
			this.tableLayoutPanel3.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.tableLayoutPanel3.BackColor = System.Drawing.SystemColors.Control;
			this.tableLayoutPanel3.ColumnCount = 3;
			this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel3.Controls.Add(this.flowLayoutPanel1, 1, 0);
			this.tableLayoutPanel3.Controls.Add(this.OptionsButton, 2, 0);
			this.tableLayoutPanel3.Controls.Add(this.FilterButton, 0, 0);
			this.tableLayoutPanel3.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tableLayoutPanel3.Location = new System.Drawing.Point(0, 713);
			this.tableLayoutPanel3.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel3.Name = "tableLayoutPanel3";
			this.tableLayoutPanel3.Padding = new System.Windows.Forms.Padding(0, 13, 0, 0);
			this.tableLayoutPanel3.RowCount = 1;
			this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
			this.tableLayoutPanel3.Size = new System.Drawing.Size(1363, 39);
			this.tableLayoutPanel3.TabIndex = 11;
			// 
			// OptionsButton
			// 
			this.OptionsButton.Anchor = System.Windows.Forms.AnchorStyles.Right;
			this.OptionsButton.Image = global::UnrealGameSync.Properties.Resources.DropList;
			this.OptionsButton.ImageAlign = System.Drawing.ContentAlignment.MiddleRight;
			this.OptionsButton.Location = new System.Drawing.Point(1222, 13);
			this.OptionsButton.Margin = new System.Windows.Forms.Padding(0);
			this.OptionsButton.Name = "OptionsButton";
			this.OptionsButton.Size = new System.Drawing.Size(120, 26);
			this.OptionsButton.TabIndex = 7;
			this.OptionsButton.Text = "Options  ";
			this.OptionsButton.UseVisualStyleBackColor = true;
			this.OptionsButton.Click += new System.EventHandler(this.OptionsButton_Click);
			// 
			// FilterButton
			// 
			this.FilterButton.Anchor = System.Windows.Forms.AnchorStyles.Right;
			this.FilterButton.Image = global::UnrealGameSync.Properties.Resources.DropList;
			this.FilterButton.ImageAlign = System.Drawing.ContentAlignment.MiddleRight;
			this.FilterButton.Location = new System.Drawing.Point(0, 13);
			this.FilterButton.Margin = new System.Windows.Forms.Padding(0);
			this.FilterButton.Name = "FilterButton";
			this.FilterButton.Size = new System.Drawing.Size(120, 26);
			this.FilterButton.TabIndex = 9;
			this.FilterButton.Text = "Filter  ";
			this.FilterButton.UseVisualStyleBackColor = true;
			this.FilterButton.Click += new System.EventHandler(this.FilterButton_Click);
			// 
			// tableLayoutPanel2
			// 
			this.tableLayoutPanel2.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.tableLayoutPanel2.ColumnCount = 1;
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.Controls.Add(this.Splitter, 0, 0);
			this.tableLayoutPanel2.Controls.Add(this.tableLayoutPanel3, 0, 1);
			this.tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tableLayoutPanel2.GrowStyle = System.Windows.Forms.TableLayoutPanelGrowStyle.FixedSize;
			this.tableLayoutPanel2.Location = new System.Drawing.Point(0, 0);
			this.tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel2.Name = "tableLayoutPanel2";
			this.tableLayoutPanel2.RowCount = 2;
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.AutoSize));
			this.tableLayoutPanel2.Size = new System.Drawing.Size(1363, 752);
			this.tableLayoutPanel2.TabIndex = 12;
			// 
			// Splitter
			// 
			this.Splitter.Caption = "Log";
			this.Splitter.Dock = System.Windows.Forms.DockStyle.Fill;
			this.Splitter.Location = new System.Drawing.Point(0, 0);
			this.Splitter.Margin = new System.Windows.Forms.Padding(0);
			this.Splitter.Name = "Splitter";
			this.Splitter.Orientation = System.Windows.Forms.Orientation.Horizontal;
			// 
			// Splitter.Panel1
			// 
			this.Splitter.Panel1.Controls.Add(this.StatusLayoutPanel);
			// 
			// Splitter.Panel2
			// 
			this.Splitter.Panel2.Controls.Add(this.panel1);
			this.Splitter.Panel2MinSize = 50;
			this.Splitter.Size = new System.Drawing.Size(1363, 713);
			this.Splitter.SplitterDistance = 444;
			this.Splitter.SplitterWidth = 28;
			this.Splitter.TabIndex = 0;
			// 
			// StatusLayoutPanel
			// 
			this.StatusLayoutPanel.ColumnCount = 1;
			this.StatusLayoutPanel.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.StatusLayoutPanel.Controls.Add(this.StatusPanel, 0, 0);
			this.StatusLayoutPanel.Controls.Add(this.BuildList, 0, 1);
			this.StatusLayoutPanel.Dock = System.Windows.Forms.DockStyle.Fill;
			this.StatusLayoutPanel.Location = new System.Drawing.Point(0, 0);
			this.StatusLayoutPanel.Name = "StatusLayoutPanel";
			this.StatusLayoutPanel.RowCount = 2;
			this.StatusLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 148F));
			this.StatusLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.StatusLayoutPanel.Size = new System.Drawing.Size(1363, 444);
			this.StatusLayoutPanel.TabIndex = 1;
			// 
			// StatusPanel
			// 
			this.StatusPanel.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(250)))), ((int)(((byte)(250)))), ((int)(((byte)(250)))));
			this.StatusPanel.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.StatusPanel.Dock = System.Windows.Forms.DockStyle.Fill;
			this.StatusPanel.Location = new System.Drawing.Point(0, 0);
			this.StatusPanel.Margin = new System.Windows.Forms.Padding(0, 0, 0, 3);
			this.StatusPanel.Name = "StatusPanel";
			this.StatusPanel.Size = new System.Drawing.Size(1363, 145);
			this.StatusPanel.TabIndex = 1;
			// 
			// BuildList
			// 
			this.BuildList.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
			this.IconColumn,
			this.TypeColumn,
			this.ChangeColumn,
			this.TimeColumn,
			this.AuthorColumn,
			this.DescriptionColumn,
			this.CISColumn,
			this.StatusColumn});
			this.BuildList.Dock = System.Windows.Forms.DockStyle.Fill;
			this.BuildList.FullRowSelect = true;
			this.BuildList.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.Nonclickable;
			this.BuildList.HideSelection = false;
			this.BuildList.Location = new System.Drawing.Point(0, 148);
			this.BuildList.Margin = new System.Windows.Forms.Padding(0);
			this.BuildList.Name = "BuildList";
			this.BuildList.OwnerDraw = true;
			this.BuildList.Size = new System.Drawing.Size(1363, 296);
			this.BuildList.TabIndex = 0;
			this.BuildList.UseCompatibleStateImageBehavior = false;
			this.BuildList.View = System.Windows.Forms.View.Details;
			this.BuildList.ColumnWidthChanged += new System.Windows.Forms.ColumnWidthChangedEventHandler(this.BuildList_ColumnWidthChanged);
			this.BuildList.ColumnWidthChanging += new System.Windows.Forms.ColumnWidthChangingEventHandler(this.BuildList_ColumnWidthChanging);
			this.BuildList.DrawColumnHeader += new System.Windows.Forms.DrawListViewColumnHeaderEventHandler(this.BuildList_DrawColumnHeader);
			this.BuildList.DrawItem += new System.Windows.Forms.DrawListViewItemEventHandler(this.BuildList_DrawItem);
			this.BuildList.DrawSubItem += new System.Windows.Forms.DrawListViewSubItemEventHandler(this.BuildList_DrawSubItem);
			this.BuildList.ItemMouseHover += new System.Windows.Forms.ListViewItemMouseHoverEventHandler(this.BuildList_ItemMouseHover);
			this.BuildList.SelectedIndexChanged += new System.EventHandler(this.BuildList_SelectedIndexChanged);
			this.BuildList.FontChanged += new System.EventHandler(this.BuildList_FontChanged);
			this.BuildList.KeyDown += new System.Windows.Forms.KeyEventHandler(this.BuildList_KeyDown);
			this.BuildList.MouseClick += new System.Windows.Forms.MouseEventHandler(this.BuildList_MouseClick);
			this.BuildList.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.BuildList_MouseDoubleClick);
			this.BuildList.MouseLeave += new System.EventHandler(this.BuildList_MouseLeave);
			this.BuildList.MouseMove += new System.Windows.Forms.MouseEventHandler(this.BuildList_MouseMove);
			this.BuildList.Resize += new System.EventHandler(this.BuildList_Resize);
			// 
			// IconColumn
			// 
			this.IconColumn.Text = "";
			this.IconColumn.Width = 45;
			// 
			// TypeColumn
			// 
			this.TypeColumn.Text = "Type";
			this.TypeColumn.Width = 100;
			// 
			// ChangeColumn
			// 
			this.ChangeColumn.Text = "Change";
			this.ChangeColumn.Width = 74;
			// 
			// TimeColumn
			// 
			this.TimeColumn.Text = "Time";
			// 
			// AuthorColumn
			// 
			this.AuthorColumn.Text = "Author";
			this.AuthorColumn.Width = 120;
			// 
			// DescriptionColumn
			// 
			this.DescriptionColumn.Text = "Description";
			this.DescriptionColumn.Width = 245;
			// 
			// CISColumn
			// 
			this.CISColumn.Text = "CIS";
			this.CISColumn.Width = 184;
			// 
			// StatusColumn
			// 
			this.StatusColumn.Text = "Status";
			this.StatusColumn.Width = 375;
			// 
			// panel1
			// 
			this.panel1.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.panel1.Controls.Add(this.SyncLog);
			this.panel1.Dock = System.Windows.Forms.DockStyle.Fill;
			this.panel1.Location = new System.Drawing.Point(0, 0);
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(1363, 241);
			this.panel1.TabIndex = 0;
			// 
			// SyncLog
			// 
			this.SyncLog.BackColor = System.Drawing.Color.White;
			this.SyncLog.Cursor = System.Windows.Forms.Cursors.IBeam;
			this.SyncLog.Dock = System.Windows.Forms.DockStyle.Fill;
			this.SyncLog.Font = new System.Drawing.Font("Courier New", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point);
			this.SyncLog.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(32)))), ((int)(((byte)(32)))), ((int)(((byte)(32)))));
			this.SyncLog.Location = new System.Drawing.Point(0, 0);
			this.SyncLog.Name = "SyncLog";
			this.SyncLog.Size = new System.Drawing.Size(1361, 239);
			this.SyncLog.TabIndex = 0;
			// 
			// MoreToolsContextMenu
			// 
			this.MoreToolsContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.MoreActionsContextMenu_CustomToolSeparator,
			this.MoreToolsContextMenu_UpdateTools,
			this.MoreToolsContextMenu_CleanWorkspace});
			this.MoreToolsContextMenu.Name = "MoreActionsContextMenu";
			this.MoreToolsContextMenu.Size = new System.Drawing.Size(175, 32);
			// 
			// MoreActionsContextMenu_CustomToolSeparator
			// 
			this.MoreActionsContextMenu_CustomToolSeparator.Name = "MoreActionsContextMenu_CustomToolSeparator";
			this.MoreActionsContextMenu_CustomToolSeparator.Size = new System.Drawing.Size(171, 6);
			// 
			// MoreToolsContextMenu_CleanWorkspace
			// 
			this.MoreToolsContextMenu_CleanWorkspace.Name = "MoreToolsContextMenu_CleanWorkspace";
			this.MoreToolsContextMenu_CleanWorkspace.Size = new System.Drawing.Size(174, 22);
			this.MoreToolsContextMenu_CleanWorkspace.Text = "Clean Workspace...";
			this.MoreToolsContextMenu_CleanWorkspace.Click += new System.EventHandler(this.MoreToolsContextMenu_CleanWorkspace_Click);
			// 
			// MoreToolsContextMenu_UpdateTools
			// 
			this.MoreToolsContextMenu_UpdateTools.Name = "MoreToolsContextMenu_UpdateTools";
			this.MoreToolsContextMenu_UpdateTools.Size = new System.Drawing.Size(174, 22);
			this.MoreToolsContextMenu_UpdateTools.Text = "Check for Tools Updates";
			this.MoreToolsContextMenu_UpdateTools.Click += new System.EventHandler(this.MoreToolsContextMenu_UpdateTools_Click);
			// 
			// SyncContextMenu
			// 
			this.SyncContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.SyncContexMenu_EnterChangelist,
			this.toolStripSeparator8});
			this.SyncContextMenu.Name = "SyncContextMenu";
			this.SyncContextMenu.Size = new System.Drawing.Size(184, 32);
			// 
			// toolStripSeparator8
			// 
			this.toolStripSeparator8.Name = "toolStripSeparator8";
			this.toolStripSeparator8.Size = new System.Drawing.Size(180, 6);
			// 
			// SyncContexMenu_EnterChangelist
			// 
			this.SyncContexMenu_EnterChangelist.Name = "SyncContexMenu_EnterChangelist";
			this.SyncContexMenu_EnterChangelist.Size = new System.Drawing.Size(183, 22);
			this.SyncContexMenu_EnterChangelist.Text = "Specific Changelist...";
			this.SyncContexMenu_EnterChangelist.Click += new System.EventHandler(this.SyncContextMenu_EnterChangelist_Click);
			// 
			// StreamContextMenu
			// 
			this.StreamContextMenu.Name = "StreamContextMenu";
			this.StreamContextMenu.Size = new System.Drawing.Size(61, 4);
			// 
			// RecentMenu
			// 
			this.RecentMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.RecentMenu_Browse,
			this.toolStripSeparator9,
			this.RecentMenu_Separator,
			this.RecentMenu_ClearList});
			this.RecentMenu.Name = "RecentMenu";
			this.RecentMenu.Size = new System.Drawing.Size(123, 60);
			// 
			// RecentMenu_Browse
			// 
			this.RecentMenu_Browse.Name = "RecentMenu_Browse";
			this.RecentMenu_Browse.Size = new System.Drawing.Size(122, 22);
			this.RecentMenu_Browse.Text = "Browse...";
			this.RecentMenu_Browse.Click += new System.EventHandler(this.RecentMenu_Browse_Click);
			// 
			// toolStripSeparator9
			// 
			this.toolStripSeparator9.Name = "toolStripSeparator9";
			this.toolStripSeparator9.Size = new System.Drawing.Size(119, 6);
			// 
			// RecentMenu_Separator
			// 
			this.RecentMenu_Separator.Name = "RecentMenu_Separator";
			this.RecentMenu_Separator.Size = new System.Drawing.Size(119, 6);
			// 
			// RecentMenu_ClearList
			// 
			this.RecentMenu_ClearList.Name = "RecentMenu_ClearList";
			this.RecentMenu_ClearList.Size = new System.Drawing.Size(122, 22);
			this.RecentMenu_ClearList.Text = "Clear List";
			this.RecentMenu_ClearList.Click += new System.EventHandler(this.RecentMenu_ClearList_Click);
			// 
			// BuildListMultiContextMenu
			// 
			this.BuildListMultiContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.BuildListMultiContextMenu_Bisect,
			this.BuildListMultiContextMenu_TimeZoneSeparator,
			this.BuildListMultiContextMenu_ShowServerTimes,
			this.BuildListMultiContextMenu_ShowLocalTimes});
			this.BuildListMultiContextMenu.Name = "BuildListContextMenu";
			this.BuildListMultiContextMenu.Size = new System.Drawing.Size(184, 76);
			// 
			// BuildListMultiContextMenu_Bisect
			// 
			this.BuildListMultiContextMenu_Bisect.Name = "BuildListMultiContextMenu_Bisect";
			this.BuildListMultiContextMenu_Bisect.Size = new System.Drawing.Size(183, 22);
			this.BuildListMultiContextMenu_Bisect.Text = "Bisect these changes";
			this.BuildListMultiContextMenu_Bisect.Click += new System.EventHandler(this.BuidlListMultiContextMenu_Bisect_Click);
			// 
			// BuildListMultiContextMenu_TimeZoneSeparator
			// 
			this.BuildListMultiContextMenu_TimeZoneSeparator.Name = "BuildListMultiContextMenu_TimeZoneSeparator";
			this.BuildListMultiContextMenu_TimeZoneSeparator.Size = new System.Drawing.Size(180, 6);
			// 
			// BuildListMultiContextMenu_ShowServerTimes
			// 
			this.BuildListMultiContextMenu_ShowServerTimes.Name = "BuildListMultiContextMenu_ShowServerTimes";
			this.BuildListMultiContextMenu_ShowServerTimes.Size = new System.Drawing.Size(183, 22);
			this.BuildListMultiContextMenu_ShowServerTimes.Text = "Show server times";
			this.BuildListMultiContextMenu_ShowServerTimes.Click += new System.EventHandler(this.BuildListContextMenu_ShowServerTimes_Click);
			// 
			// BuildListMultiContextMenu_ShowLocalTimes
			// 
			this.BuildListMultiContextMenu_ShowLocalTimes.Name = "BuildListMultiContextMenu_ShowLocalTimes";
			this.BuildListMultiContextMenu_ShowLocalTimes.Size = new System.Drawing.Size(183, 22);
			this.BuildListMultiContextMenu_ShowLocalTimes.Text = "Show local times";
			this.BuildListMultiContextMenu_ShowLocalTimes.Click += new System.EventHandler(this.BuildListContextMenu_ShowServerTimes_Click);
			// 
			// FilterContextMenu
			// 
			this.FilterContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.FilterContextMenu_Default,
			this.FilterContextMenu_BeforeBadgeSeparator,
			this.FilterContextMenu_Type,
			this.FilterContextMenu_Badges,
			this.FilterContextMenu_Robomerge,
			this.FilterContextMenu_Author,
			this.FilterContextMenu_AfterBadgeSeparator,
			this.FilterContextMenu_ShowBuildMachineChanges});
			this.FilterContextMenu.Name = "FilterContextMenu";
			this.FilterContextMenu.Size = new System.Drawing.Size(232, 148);
			// 
			// FilterContextMenu_Default
			// 
			this.FilterContextMenu_Default.Name = "FilterContextMenu_Default";
			this.FilterContextMenu_Default.Size = new System.Drawing.Size(231, 22);
			this.FilterContextMenu_Default.Text = "Default";
			this.FilterContextMenu_Default.Click += new System.EventHandler(this.FilterContextMenu_Default_Click);
			// 
			// FilterContextMenu_BeforeBadgeSeparator
			// 
			this.FilterContextMenu_BeforeBadgeSeparator.Name = "FilterContextMenu_BeforeBadgeSeparator";
			this.FilterContextMenu_BeforeBadgeSeparator.Size = new System.Drawing.Size(228, 6);
			// 
			// FilterContextMenu_Type
			// 
			this.FilterContextMenu_Type.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.FilterContextMenu_Type_ShowAll,
			this.toolStripSeparator10,
			this.FilterContextMenu_Type_Code,
			this.FilterContextMenu_Type_Content});
			this.FilterContextMenu_Type.Name = "FilterContextMenu_Type";
			this.FilterContextMenu_Type.Size = new System.Drawing.Size(231, 22);
			this.FilterContextMenu_Type.Text = "Type";
			// 
			// FilterContextMenu_Type_ShowAll
			// 
			this.FilterContextMenu_Type_ShowAll.Name = "FilterContextMenu_Type_ShowAll";
			this.FilterContextMenu_Type_ShowAll.Size = new System.Drawing.Size(120, 22);
			this.FilterContextMenu_Type_ShowAll.Text = "Show All";
			this.FilterContextMenu_Type_ShowAll.Click += new System.EventHandler(this.FilterContextMenu_Type_ShowAll_Click);
			// 
			// toolStripSeparator10
			// 
			this.toolStripSeparator10.Name = "toolStripSeparator10";
			this.toolStripSeparator10.Size = new System.Drawing.Size(117, 6);
			// 
			// FilterContextMenu_Type_Code
			// 
			this.FilterContextMenu_Type_Code.Name = "FilterContextMenu_Type_Code";
			this.FilterContextMenu_Type_Code.Size = new System.Drawing.Size(120, 22);
			this.FilterContextMenu_Type_Code.Text = "Code";
			this.FilterContextMenu_Type_Code.Click += new System.EventHandler(this.FilterContextMenu_Type_Code_Click);
			// 
			// FilterContextMenu_Type_Content
			// 
			this.FilterContextMenu_Type_Content.Name = "FilterContextMenu_Type_Content";
			this.FilterContextMenu_Type_Content.Size = new System.Drawing.Size(120, 22);
			this.FilterContextMenu_Type_Content.Text = "Content";
			this.FilterContextMenu_Type_Content.Click += new System.EventHandler(this.FilterContextMenu_Type_Content_Click);
			// 
			// FilterContextMenu_Badges
			// 
			this.FilterContextMenu_Badges.Name = "FilterContextMenu_Badges";
			this.FilterContextMenu_Badges.Size = new System.Drawing.Size(231, 22);
			this.FilterContextMenu_Badges.Text = "Badges";
			// 
			// FilterContextMenu_Robomerge
			// 
			this.FilterContextMenu_Robomerge.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.FilterContextMenu_Robomerge_ShowAll,
			this.FilterContextMenu_Robomerge_ShowBadged,
			this.FilterContextMenu_Robomerge_ShowNone,
			this.FilterContextMenu_AfterRobomergeShowSeparator,
			this.FilterContextMenu_Robomerge_Annotate});
			this.FilterContextMenu_Robomerge.Name = "FilterContextMenu_Robomerge";
			this.FilterContextMenu_Robomerge.Size = new System.Drawing.Size(231, 22);
			this.FilterContextMenu_Robomerge.Text = "Robomerge";
			// 
			// FilterContextMenu_Robomerge_ShowAll
			// 
			this.FilterContextMenu_Robomerge_ShowAll.Name = "FilterContextMenu_Robomerge_ShowAll";
			this.FilterContextMenu_Robomerge_ShowAll.Size = new System.Drawing.Size(237, 22);
			this.FilterContextMenu_Robomerge_ShowAll.Text = "Show All";
			this.FilterContextMenu_Robomerge_ShowAll.Click += new System.EventHandler(this.FilterContextMenu_Robomerge_ShowAll_Click);
			// 
			// FilterContextMenu_Robomerge_ShowBadged
			// 
			this.FilterContextMenu_Robomerge_ShowBadged.Name = "FilterContextMenu_Robomerge_ShowBadged";
			this.FilterContextMenu_Robomerge_ShowBadged.Size = new System.Drawing.Size(237, 22);
			this.FilterContextMenu_Robomerge_ShowBadged.Text = "Show Changes with Badges";
			this.FilterContextMenu_Robomerge_ShowBadged.Click += new System.EventHandler(this.FilterContextMenu_Robomerge_ShowBadged_Click);
			// 
			// FilterContextMenu_Robomerge_ShowNone
			// 
			this.FilterContextMenu_Robomerge_ShowNone.Name = "FilterContextMenu_Robomerge_ShowNone";
			this.FilterContextMenu_Robomerge_ShowNone.Size = new System.Drawing.Size(237, 22);
			this.FilterContextMenu_Robomerge_ShowNone.Text = "Show None";
			this.FilterContextMenu_Robomerge_ShowNone.Click += new System.EventHandler(this.FilterContextMenu_Robomerge_ShowNone_Click);
			// 
			// FilterContextMenu_AfterRobomergeShowSeparator
			// 
			this.FilterContextMenu_AfterRobomergeShowSeparator.Name = "FilterContextMenu_AfterRobomergeShowSeparator";
			this.FilterContextMenu_AfterRobomergeShowSeparator.Size = new System.Drawing.Size(234, 6);
			// 
			// FilterContextMenu_Robomerge_Annotate
			// 
			this.FilterContextMenu_Robomerge_Annotate.Name = "FilterContextMenu_Robomerge_Annotate";
			this.FilterContextMenu_Robomerge_Annotate.Size = new System.Drawing.Size(237, 22);
			this.FilterContextMenu_Robomerge_Annotate.Text = "Annotate Robomerge Changes";
			this.FilterContextMenu_Robomerge_Annotate.Click += new System.EventHandler(this.FilterContextMenu_Robomerge_Annotate_Click);
			// 
			// FilterContextMenu_Author
			// 
			this.FilterContextMenu_Author.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.FilterContextMenu_Author_Name});
			this.FilterContextMenu_Author.Name = "FilterContextMenu_Author";
			this.FilterContextMenu_Author.Size = new System.Drawing.Size(231, 22);
			this.FilterContextMenu_Author.Text = "Author";
			// 
			// FilterContextMenu_Author_Name
			// 
			this.FilterContextMenu_Author_Name.Name = "FilterContextMenu_Author_Name";
			this.FilterContextMenu_Author_Name.Size = new System.Drawing.Size(100, 22);
			this.FilterContextMenu_Author_Name.Text = AuthorFilterPlaceholderText;
			// no placeholder text property in 4.x so do it manually
			this.FilterContextMenu_Author_Name.TextBox.ForeColor = Color.DarkGray;
			this.FilterContextMenu_Author_Name.TextBox.GotFocus += new System.EventHandler(this.FilterContextMenu_Author_Name_GotFocus);
			this.FilterContextMenu_Author_Name.TextBox.LostFocus += new System.EventHandler(this.FilterContextMenu_Author_Name_LostFocus);
			this.FilterContextMenu_Author_Name.TextBox.TextChanged += new System.EventHandler(this.FilterContextMenu_Author_Name_Changed);
			// 
			// FilterContextMenu_AfterBadgeSeparator
			// 
			this.FilterContextMenu_AfterBadgeSeparator.Name = "FilterContextMenu_AfterBadgeSeparator";
			this.FilterContextMenu_AfterBadgeSeparator.Size = new System.Drawing.Size(228, 6);
			// 
			// FilterContextMenu_ShowBuildMachineChanges
			// 
			this.FilterContextMenu_ShowBuildMachineChanges.Name = "FilterContextMenu_ShowBuildMachineChanges";
			this.FilterContextMenu_ShowBuildMachineChanges.Size = new System.Drawing.Size(231, 22);
			this.FilterContextMenu_ShowBuildMachineChanges.Text = "Show Build Machine Changes";
			this.FilterContextMenu_ShowBuildMachineChanges.Click += new System.EventHandler(this.FilterContextMenu_ShowBuildMachineChanges_Click);
			// 
			// BadgeContextMenu
			//
			this.BadgeContextMenu.Name = "BadgeContextMenu";
			//
			// BuildHealthContextMenu
			// 
			this.BuildHealthContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.BuildHealthContextMenu_Browse,
			this.BuildHealthContextMenu_MinSeparator,
			this.BuildHealthContextMenu_MaxSeparator,
			this.BuildHealthContextMenu_Settings});
			this.BuildHealthContextMenu.Name = "BuildHealthContextMenu";
			this.BuildHealthContextMenu.Size = new System.Drawing.Size(130, 60);
			// 
			// BuildHealthContextMenu_Browse
			// 
			this.BuildHealthContextMenu_Browse.Name = "BuildHealthContextMenu_Browse";
			this.BuildHealthContextMenu_Browse.Size = new System.Drawing.Size(129, 22);
			this.BuildHealthContextMenu_Browse.Text = "Show All...";
			this.BuildHealthContextMenu_Browse.Click += new System.EventHandler(this.BuildHealthContextMenu_Browse_Click);
			// 
			// BuildHealthContextMenu_MinSeparator
			// 
			this.BuildHealthContextMenu_MinSeparator.Name = "BuildHealthContextMenu_MinSeparator";
			this.BuildHealthContextMenu_MinSeparator.Size = new System.Drawing.Size(126, 6);
			// 
			// BuildHealthContextMenu_MaxSeparator
			// 
			this.BuildHealthContextMenu_MaxSeparator.Name = "BuildHealthContextMenu_MaxSeparator";
			this.BuildHealthContextMenu_MaxSeparator.Size = new System.Drawing.Size(126, 6);
			// 
			// BuildHealthContextMenu_Settings
			// 
			this.BuildHealthContextMenu_Settings.Name = "BuildHealthContextMenu_Settings";
			this.BuildHealthContextMenu_Settings.Size = new System.Drawing.Size(129, 22);
			this.BuildHealthContextMenu_Settings.Text = "Settings...";
			this.BuildHealthContextMenu_Settings.Click += new System.EventHandler(this.BuildHealthContextMenu_Settings_Click);
			// 
			// EditorConfigWatcher
			// 
			this.EditorConfigWatcher.EnableRaisingEvents = true;
			this.EditorConfigWatcher.Filter = "EditorPerProjectUserSettings.ini";
			this.EditorConfigWatcher.IncludeSubdirectories = false;
			this.EditorConfigWatcher.InternalBufferSize = 8192;
			this.EditorConfigWatcher.NotifyFilter = System.IO.NotifyFilters.LastWrite;
			this.EditorConfigWatcher.Path = "";
			this.EditorConfigWatcher.SynchronizingObject = this;
			this.EditorConfigWatcher.Changed += new System.IO.FileSystemEventHandler(this.EditorConfigWatcher_Changed);
			this.EditorConfigWatcher.Created += new System.IO.FileSystemEventHandler(this.EditorConfigWatcher_Changed);
			this.EditorConfigWatcher.Deleted += new System.IO.FileSystemEventHandler(this.EditorConfigWatcher_Changed);
			this.EditorConfigWatcher.Renamed += new System.IO.RenamedEventHandler(this.EditorConfigWatcher_Renamed);
			// 
			// WorkspaceControl
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.BackColor = System.Drawing.SystemColors.Control;
			this.Controls.Add(this.tableLayoutPanel2);
			this.Margin = new System.Windows.Forms.Padding(0);
			this.Name = "WorkspaceControl";
			this.Size = new System.Drawing.Size(1363, 752);
			this.Load += new System.EventHandler(this.MainWindow_Load);
			this.VisibleChanged += new System.EventHandler(this.WorkspaceControl_VisibleChanged);
			this.OptionsContextMenu.ResumeLayout(false);
			this.BuildListContextMenu.ResumeLayout(false);
			this.flowLayoutPanel1.ResumeLayout(false);
			this.flowLayoutPanel1.PerformLayout();
			this.tableLayoutPanel3.ResumeLayout(false);
			this.tableLayoutPanel3.PerformLayout();
			this.tableLayoutPanel2.ResumeLayout(false);
			this.tableLayoutPanel2.PerformLayout();
			this.Splitter.Panel1.ResumeLayout(false);
			this.Splitter.Panel2.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.Splitter)).EndInit();
			this.Splitter.ResumeLayout(false);
			this.StatusLayoutPanel.ResumeLayout(false);
			this.panel1.ResumeLayout(false);
			this.MoreToolsContextMenu.ResumeLayout(false);
			this.SyncContextMenu.ResumeLayout(false);
			this.RecentMenu.ResumeLayout(false);
			this.BuildListMultiContextMenu.ResumeLayout(false);
			this.FilterContextMenu.ResumeLayout(false);
			this.BuildHealthContextMenu.ResumeLayout(false);
			this.BadgeContextMenu.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.EditorConfigWatcher)).EndInit();
			this.ResumeLayout(false);

		}

		#endregion

		private BuildListControl BuildList;
		private LogControl SyncLog;
		private System.Windows.Forms.ColumnHeader IconColumn;
		private System.Windows.Forms.ColumnHeader TimeColumn;
		private System.Windows.Forms.ColumnHeader DescriptionColumn;
		private System.Windows.Forms.ColumnHeader StatusColumn;
		private System.Windows.Forms.ColumnHeader ChangeColumn;
		private System.Windows.Forms.ColumnHeader AuthorColumn;
		private System.Windows.Forms.Button OptionsButton;
		private System.Windows.Forms.ContextMenuStrip OptionsContextMenu;
		private System.Windows.Forms.CheckBox RunAfterSyncCheckBox;
		private System.Windows.Forms.CheckBox BuildAfterSyncCheckBox;
		private System.Windows.Forms.Label AfterSyncingLabel;
		private System.Windows.Forms.ContextMenuStrip BuildListContextMenu;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Cancel;
		private LogSplitContainer Splitter;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_MoreInfo;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_AddStar;
		private System.Windows.Forms.ToolStripSeparator BuildListContextMenu_CustomTool_End;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_MarkGood;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_MarkBad;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_StartInvestigating;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Sync;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator4;
		private System.Windows.Forms.NotifyIcon NotifyIcon;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_RemoveStar;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_FinishInvestigating;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_LaunchEditor;
		private System.Windows.Forms.ToolTip BuildListToolTip;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_AutoResolveConflicts;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_AlwaysClobberFiles;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_EditorArguments;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_OpenVisualStudio;
		private System.Windows.Forms.Panel panel1;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Build;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_GenerateProjectFiles;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_EditorBuildConfiguration;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_BuildConfig_Debug;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_BuildConfig_DebugGame;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_BuildConfig_Development;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Rebuild;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_WithdrawReview;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_ScheduledSync;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_LeaveComment;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator7;
		private System.Windows.Forms.ColumnHeader CISColumn;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_EditComment;
		private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.ToolStripSeparator BuildListContextMenu_TimeZoneSeparator;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_ShowServerTimes;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_ShowLocalTimes;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_CustomizeBuildSteps;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TimeZone;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TimeZone_Local;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TimeZone_PerforceServer;
		private System.Windows.Forms.CheckBox OpenSolutionAfterSyncCheckBox;
		private System.Windows.Forms.TableLayoutPanel StatusLayoutPanel;
		private StatusPanel StatusPanel;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_SyncContentOnly;
		private System.Windows.Forms.ContextMenuStrip MoreToolsContextMenu;
		private System.Windows.Forms.ToolStripMenuItem MoreToolsContextMenu_CleanWorkspace;
		private System.Windows.Forms.ToolStripMenuItem MoreToolsContextMenu_UpdateTools;
		private System.Windows.Forms.ToolStripSeparator MoreActionsContextMenu_CustomToolSeparator;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator3;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator5;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator6;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_Diagnostics;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_SyncFilter;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_SyncOnlyThisChange;
		private System.Windows.Forms.ContextMenuStrip SyncContextMenu;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator8;
		private System.Windows.Forms.ToolStripMenuItem SyncContexMenu_EnterChangelist;
		private System.Windows.Forms.ContextMenuStrip StreamContextMenu;
		private System.Windows.Forms.ToolStripMenuItem tabLabelsToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TabNames_Stream;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TabNames_WorkspaceName;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TabNames_WorkspaceRoot;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TabNames_ProjectFile;
		private System.Windows.Forms.ContextMenuStrip RecentMenu;
		private System.Windows.Forms.ToolStripSeparator RecentMenu_Separator;
		private System.Windows.Forms.ToolStripMenuItem RecentMenu_ClearList;
		private System.Windows.Forms.ToolStripMenuItem RecentMenu_Browse;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator9;
		private System.Windows.Forms.ToolStripSeparator BuildListContextMenu_CustomTool_Start;
		private System.Windows.Forms.ToolStripMenuItem showChangesToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_ShowChanges_ShowUnreviewed;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_ShowChanges_ShowAutomated;
		private System.Windows.Forms.ColumnHeader TypeColumn;
		private System.Windows.Forms.ContextMenuStrip BuildListMultiContextMenu;
		private System.Windows.Forms.ToolStripMenuItem BuildListMultiContextMenu_Bisect;
		private System.Windows.Forms.ToolStripSeparator BuildListMultiContextMenu_TimeZoneSeparator;
		private System.Windows.Forms.ToolStripMenuItem BuildListMultiContextMenu_ShowServerTimes;
		private System.Windows.Forms.ToolStripMenuItem BuildListMultiContextMenu_ShowLocalTimes;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Bisect_Pass;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Bisect_Fail;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Bisect_Exclude;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Bisect_Include;
		private System.Windows.Forms.ToolStripSeparator BuildListContextMenu_Bisect_Separator;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_ApplicationSettings;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
		private System.Windows.Forms.Button FilterButton;
		private System.Windows.Forms.ContextMenuStrip FilterContextMenu;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Default;
		private System.Windows.Forms.ToolStripSeparator FilterContextMenu_BeforeBadgeSeparator;
		private System.Windows.Forms.ToolStripSeparator FilterContextMenu_AfterBadgeSeparator;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_ShowBuildMachineChanges;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Badges;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Type;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Type_Code;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Type_Content;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Type_ShowAll;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Robomerge;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Robomerge_ShowAll;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Robomerge_ShowBadged;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Robomerge_ShowNone;
		private System.Windows.Forms.ToolStripSeparator FilterContextMenu_AfterRobomergeShowSeparator;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Robomerge_Annotate;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Author;
		private System.Windows.Forms.ToolStripTextBox FilterContextMenu_Author_Name;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator10;
		private System.Windows.Forms.ContextMenuStrip BuildHealthContextMenu;
		private System.Windows.Forms.ContextMenuStrip BadgeContextMenu;
		private System.Windows.Forms.ToolStripSeparator BuildHealthContextMenu_MaxSeparator;
		private System.Windows.Forms.ToolStripMenuItem BuildHealthContextMenu_Settings;
		private System.Windows.Forms.ToolStripMenuItem BuildHealthContextMenu_Browse;
		private System.Windows.Forms.ToolStripSeparator BuildHealthContextMenu_MinSeparator;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_SyncPrecompiledBinaries;
		private System.Windows.Forms.ToolStripMenuItem disabledToolStripMenuItem;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator11;
		private System.Windows.Forms.ToolStripMenuItem editorToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem editorPhysXToolStripMenuItem;
		private System.IO.FileSystemWatcher EditorConfigWatcher;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_ViewInSwarm;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_CopyChangelistNumber;
	}
}

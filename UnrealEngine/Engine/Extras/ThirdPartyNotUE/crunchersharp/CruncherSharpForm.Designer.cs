namespace CruncherSharp
{
    public partial class CruncherSharpForm
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
            this.components = new System.ComponentModel.Container();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle1 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle2 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle3 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle4 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle5 = new System.Windows.Forms.DataGridViewCellStyle();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(CruncherSharpForm));
            this.mainMenu = new System.Windows.Forms.MenuStrip();
            this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.loadPDBToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.compareWithPDBToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.loadInstanceCountToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.exportCsvToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.exitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.findToolStripMenuItem1 = new System.Windows.Forms.ToolStripMenuItem();
            this.findUnusedVtablesToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.findMSVCExtraPaddingToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.findMSVCEmptyBaseClassToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.findUnusedInterfacesToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.findUnusedVirtualToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.findMaskingFunctionsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.findRemovedInlineToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.restrictToSymbolsImportedFroCSVToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.unrealEngineToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.addMemPoolsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openPdbDialog = new System.Windows.Forms.OpenFileDialog();
            this.openCsvDialog = new System.Windows.Forms.OpenFileDialog();
            this.saveCsvDialog = new System.Windows.Forms.SaveFileDialog();
            this.checkedListBoxNamespaces = new System.Windows.Forms.CheckedListBox();
            this.chkShowTemplates = new System.Windows.Forms.CheckBox();
            this.textBoxCache = new System.Windows.Forms.MaskedTextBox();
            this.label2 = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.textBoxFilter = new System.Windows.Forms.TextBox();
            this.statusStripBar = new System.Windows.Forms.StatusStrip();
            this.toolStripStatusLabel = new System.Windows.Forms.ToolStripStatusLabel();
            this.toolStripProgressBar = new System.Windows.Forms.ToolStripProgressBar();
            this.contextMenuStripMembers = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.copyTypeLayoutToClipboardToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.setPrefetchStartOffsetToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.loadPDBBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.loadCSVBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.panel1 = new System.Windows.Forms.Panel();
            this.checkBoxFunctionAnalysis = new System.Windows.Forms.CheckBox();
            this.checkBoxShowOverlap = new System.Windows.Forms.CheckBox();
            this.checkBoxBitPadding = new System.Windows.Forms.CheckBox();
            this.checkBoxPadding = new System.Windows.Forms.CheckBox();
            this.checkBoxCacheLines = new System.Windows.Forms.CheckBox();
            this.checkBoxRegularExpressions = new System.Windows.Forms.CheckBox();
            this.checkBoxMatchWholeExpression = new System.Windows.Forms.CheckBox();
            this.checkBoxMatchCase = new System.Windows.Forms.CheckBox();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.btnLoad = new System.Windows.Forms.Button();
            this.btnReset = new System.Windows.Forms.Button();
            this.chkSmartCacheLines = new System.Windows.Forms.CheckBox();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.dataGridSymbols = new System.Windows.Forms.DataGridView();
            this.splitContainer2 = new System.Windows.Forms.SplitContainer();
            this.labelCurrentSymbol = new System.Windows.Forms.Label();
            this.Infos = new System.Windows.Forms.TabControl();
            this.tabMembers = new System.Windows.Forms.TabPage();
            this.dataGridViewSymbolInfo = new System.Windows.Forms.DataGridView();
            this.Expand = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.colField = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.colFieldType = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.colFieldOffset = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.colBitPosition = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.colFieldSize = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.colFieldPadding = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.tabFunctions = new System.Windows.Forms.TabPage();
            this.panel2 = new System.Windows.Forms.Panel();
            this.dataGridViewFunctionsInfo = new System.Windows.Forms.DataGridView();
            this.dataGridViewTextBoxFunction = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.dataGridViewTextBoxVirtual = new System.Windows.Forms.DataGridViewCheckBoxColumn();
            this.Pure = new System.Windows.Forms.DataGridViewCheckBoxColumn();
            this.IsOverride = new System.Windows.Forms.DataGridViewCheckBoxColumn();
            this.Overloaded = new System.Windows.Forms.DataGridViewCheckBoxColumn();
            this.IsMasking = new System.Windows.Forms.DataGridViewCheckBoxColumn();
            this.contextMenuStripClassInfo = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.toolStripMenuItemParentClasses = new System.Windows.Forms.ToolStripMenuItem();
            this.contextMenuStripFunctions = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.ignoreFunctionToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.bindingSourceSymbols = new System.Windows.Forms.BindingSource(this.components);
            this.mainMenu.SuspendLayout();
            this.statusStripBar.SuspendLayout();
            this.contextMenuStripMembers.SuspendLayout();
            this.tableLayoutPanel1.SuspendLayout();
            this.panel1.SuspendLayout();
            this.groupBox1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.dataGridSymbols)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).BeginInit();
            this.splitContainer2.Panel1.SuspendLayout();
            this.splitContainer2.Panel2.SuspendLayout();
            this.splitContainer2.SuspendLayout();
            this.Infos.SuspendLayout();
            this.tabMembers.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.dataGridViewSymbolInfo)).BeginInit();
            this.tabFunctions.SuspendLayout();
            this.panel2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.dataGridViewFunctionsInfo)).BeginInit();
            this.contextMenuStripClassInfo.SuspendLayout();
            this.contextMenuStripFunctions.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.bindingSourceSymbols)).BeginInit();
            this.SuspendLayout();
            // 
            // mainMenu
            // 
            this.mainMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.findToolStripMenuItem1,
            this.unrealEngineToolStripMenuItem});
            this.mainMenu.Location = new System.Drawing.Point(0, 0);
            this.mainMenu.Name = "mainMenu";
            this.mainMenu.Size = new System.Drawing.Size(1794, 24);
            this.mainMenu.TabIndex = 0;
            this.mainMenu.Text = "menuStrip1";
            // 
            // fileToolStripMenuItem
            // 
            this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.loadPDBToolStripMenuItem,
            this.compareWithPDBToolStripMenuItem,
            this.loadInstanceCountToolStripMenuItem,
            this.exportCsvToolStripMenuItem,
            this.exitToolStripMenuItem});
            this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
            this.fileToolStripMenuItem.Size = new System.Drawing.Size(37, 20);
            this.fileToolStripMenuItem.Text = "File";
            // 
            // loadPDBToolStripMenuItem
            // 
            this.loadPDBToolStripMenuItem.Name = "loadPDBToolStripMenuItem";
            this.loadPDBToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.O)));
            this.loadPDBToolStripMenuItem.Size = new System.Drawing.Size(190, 22);
            this.loadPDBToolStripMenuItem.Text = "Select PDB...";
            this.loadPDBToolStripMenuItem.Click += new System.EventHandler(this.loadPDBToolStripMenuItem_Click);
            // 
            // compareWithPDBToolStripMenuItem
            // 
            this.compareWithPDBToolStripMenuItem.Enabled = false;
            this.compareWithPDBToolStripMenuItem.Name = "compareWithPDBToolStripMenuItem";
            this.compareWithPDBToolStripMenuItem.Size = new System.Drawing.Size(190, 22);
            this.compareWithPDBToolStripMenuItem.Text = "Compare with PDB...";
            this.compareWithPDBToolStripMenuItem.Click += new System.EventHandler(this.compareWithPDBToolStripMenuItem_Click);
            // 
            // loadInstanceCountToolStripMenuItem
            // 
            this.loadInstanceCountToolStripMenuItem.Enabled = false;
            this.loadInstanceCountToolStripMenuItem.Name = "loadInstanceCountToolStripMenuItem";
            this.loadInstanceCountToolStripMenuItem.Size = new System.Drawing.Size(190, 22);
            this.loadInstanceCountToolStripMenuItem.Text = "Load instance count...";
            this.loadInstanceCountToolStripMenuItem.Click += new System.EventHandler(this.loadInstanceCountToolStripMenuItem_Click);
            // 
            // exportCsvToolStripMenuItem
            // 
            this.exportCsvToolStripMenuItem.Enabled = false;
            this.exportCsvToolStripMenuItem.Name = "exportCsvToolStripMenuItem";
            this.exportCsvToolStripMenuItem.Size = new System.Drawing.Size(190, 22);
            this.exportCsvToolStripMenuItem.Text = "Export csv";
            this.exportCsvToolStripMenuItem.Click += new System.EventHandler(this.exportCsvToolStripMenuItem_Click);
            // 
            // exitToolStripMenuItem
            // 
            this.exitToolStripMenuItem.Name = "exitToolStripMenuItem";
            this.exitToolStripMenuItem.Size = new System.Drawing.Size(190, 22);
            this.exitToolStripMenuItem.Text = "Exit";
            this.exitToolStripMenuItem.Click += new System.EventHandler(this.exitToolStripMenuItem_Click);
            // 
            // findToolStripMenuItem1
            // 
            this.findToolStripMenuItem1.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.findUnusedVtablesToolStripMenuItem,
            this.findMSVCExtraPaddingToolStripMenuItem,
            this.findMSVCEmptyBaseClassToolStripMenuItem,
            this.findUnusedInterfacesToolStripMenuItem,
            this.findUnusedVirtualToolStripMenuItem,
            this.findMaskingFunctionsToolStripMenuItem,
            this.findRemovedInlineToolStripMenuItem,
            this.restrictToSymbolsImportedFroCSVToolStripMenuItem});
            this.findToolStripMenuItem1.Name = "findToolStripMenuItem1";
            this.findToolStripMenuItem1.Size = new System.Drawing.Size(42, 20);
            this.findToolStripMenuItem1.Text = "Find";
            // 
            // findUnusedVtablesToolStripMenuItem
            // 
            this.findUnusedVtablesToolStripMenuItem.Enabled = false;
            this.findUnusedVtablesToolStripMenuItem.Name = "findUnusedVtablesToolStripMenuItem";
            this.findUnusedVtablesToolStripMenuItem.Size = new System.Drawing.Size(279, 22);
            this.findUnusedVtablesToolStripMenuItem.Text = "Find unused Vtables";
            this.findUnusedVtablesToolStripMenuItem.Click += new System.EventHandler(this.findUnusedVtablesToolStripMenuItem_Click);
            // 
            // findMSVCExtraPaddingToolStripMenuItem
            // 
            this.findMSVCExtraPaddingToolStripMenuItem.Enabled = false;
            this.findMSVCExtraPaddingToolStripMenuItem.Name = "findMSVCExtraPaddingToolStripMenuItem";
            this.findMSVCExtraPaddingToolStripMenuItem.Size = new System.Drawing.Size(279, 22);
            this.findMSVCExtraPaddingToolStripMenuItem.Text = "Find MSVC extra padding";
            this.findMSVCExtraPaddingToolStripMenuItem.Click += new System.EventHandler(this.findMSVCExtraPaddingToolStripMenuItem_Click);
            // 
            // findMSVCEmptyBaseClassToolStripMenuItem
            // 
            this.findMSVCEmptyBaseClassToolStripMenuItem.Enabled = false;
            this.findMSVCEmptyBaseClassToolStripMenuItem.Name = "findMSVCEmptyBaseClassToolStripMenuItem";
            this.findMSVCEmptyBaseClassToolStripMenuItem.Size = new System.Drawing.Size(279, 22);
            this.findMSVCEmptyBaseClassToolStripMenuItem.Text = "Find MSVC empty base class";
            this.findMSVCEmptyBaseClassToolStripMenuItem.Click += new System.EventHandler(this.findMSVCEmptyBaseClassToolStripMenuItem_Click);
            // 
            // findUnusedInterfacesToolStripMenuItem
            // 
            this.findUnusedInterfacesToolStripMenuItem.Enabled = false;
            this.findUnusedInterfacesToolStripMenuItem.Name = "findUnusedInterfacesToolStripMenuItem";
            this.findUnusedInterfacesToolStripMenuItem.Size = new System.Drawing.Size(279, 22);
            this.findUnusedInterfacesToolStripMenuItem.Text = "Find unused interfaces";
            this.findUnusedInterfacesToolStripMenuItem.Click += new System.EventHandler(this.findUnusedInterfacesToolStripMenuItem_Click);
            // 
            // findUnusedVirtualToolStripMenuItem
            // 
            this.findUnusedVirtualToolStripMenuItem.Enabled = false;
            this.findUnusedVirtualToolStripMenuItem.Name = "findUnusedVirtualToolStripMenuItem";
            this.findUnusedVirtualToolStripMenuItem.Size = new System.Drawing.Size(279, 22);
            this.findUnusedVirtualToolStripMenuItem.Text = "Find unused virtual";
            this.findUnusedVirtualToolStripMenuItem.Click += new System.EventHandler(this.findUnusedVirtualToolStripMenuItem_Click);
            // 
            // findMaskingFunctionsToolStripMenuItem
            // 
            this.findMaskingFunctionsToolStripMenuItem.Enabled = false;
            this.findMaskingFunctionsToolStripMenuItem.Name = "findMaskingFunctionsToolStripMenuItem";
            this.findMaskingFunctionsToolStripMenuItem.Size = new System.Drawing.Size(279, 22);
            this.findMaskingFunctionsToolStripMenuItem.Text = "Find masking functions";
            this.findMaskingFunctionsToolStripMenuItem.Click += new System.EventHandler(this.findMaskingFunctionsToolStripMenuItem_Click);
            // 
            // findRemovedInlineToolStripMenuItem
            // 
            this.findRemovedInlineToolStripMenuItem.Enabled = false;
            this.findRemovedInlineToolStripMenuItem.Name = "findRemovedInlineToolStripMenuItem";
            this.findRemovedInlineToolStripMenuItem.Size = new System.Drawing.Size(279, 22);
            this.findRemovedInlineToolStripMenuItem.Text = "Find removed inline";
            this.findRemovedInlineToolStripMenuItem.Click += new System.EventHandler(this.findRemovedInlineToolStripMenuItem_Click);
            // 
            // restrictToSymbolsImportedFroCSVToolStripMenuItem
            // 
            this.restrictToSymbolsImportedFroCSVToolStripMenuItem.Name = "restrictToSymbolsImportedFroCSVToolStripMenuItem";
            this.restrictToSymbolsImportedFroCSVToolStripMenuItem.Size = new System.Drawing.Size(279, 22);
            this.restrictToSymbolsImportedFroCSVToolStripMenuItem.Text = "Restrict to symbols imported from CSV";
            this.restrictToSymbolsImportedFroCSVToolStripMenuItem.Click += new System.EventHandler(this.restrictToSymbolsImportedFroCSVToolStripMenuItem_Click);
            // 
            // unrealEngineToolStripMenuItem
            // 
            this.unrealEngineToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.addMemPoolsToolStripMenuItem});
            this.unrealEngineToolStripMenuItem.Name = "unrealEngineToolStripMenuItem";
            this.unrealEngineToolStripMenuItem.Size = new System.Drawing.Size(92, 20);
            this.unrealEngineToolStripMenuItem.Text = "Unreal Engine";
            // 
            // addMemPoolsToolStripMenuItem
            // 
            this.addMemPoolsToolStripMenuItem.Name = "addMemPoolsToolStripMenuItem";
            this.addMemPoolsToolStripMenuItem.Size = new System.Drawing.Size(170, 22);
            this.addMemPoolsToolStripMenuItem.Text = "Set memory pools";
            this.addMemPoolsToolStripMenuItem.Click += new System.EventHandler(this.addMemPoolsToolStripMenuItem_Click);
            // 
            // openPdbDialog
            // 
            this.openPdbDialog.Filter = "Symbol files|*.pdb|All files|*.*";
            // 
            // openCsvDialog
            // 
            this.openCsvDialog.Filter = "Symbol files|*.csv|All files|*.*";
            // 
            // saveCsvDialog
            // 
            this.saveCsvDialog.Filter = "Symbol files|*.csv|All files|*.*";
            // 
            // checkedListBoxNamespaces
            // 
            this.checkedListBoxNamespaces.Dock = System.Windows.Forms.DockStyle.Fill;
            this.checkedListBoxNamespaces.FormattingEnabled = true;
            this.checkedListBoxNamespaces.Location = new System.Drawing.Point(3, 16);
            this.checkedListBoxNamespaces.MultiColumn = true;
            this.checkedListBoxNamespaces.Name = "checkedListBoxNamespaces";
            this.checkedListBoxNamespaces.Size = new System.Drawing.Size(1175, 85);
            this.checkedListBoxNamespaces.TabIndex = 10;
            this.checkedListBoxNamespaces.ItemCheck += new System.Windows.Forms.ItemCheckEventHandler(this.checkedListBoxNamespaces_ItemCheck);
            // 
            // chkShowTemplates
            // 
            this.chkShowTemplates.AutoSize = true;
            this.chkShowTemplates.Location = new System.Drawing.Point(476, 35);
            this.chkShowTemplates.Name = "chkShowTemplates";
            this.chkShowTemplates.Size = new System.Drawing.Size(101, 17);
            this.chkShowTemplates.TabIndex = 6;
            this.chkShowTemplates.Text = "Show templates";
            this.chkShowTemplates.UseVisualStyleBackColor = true;
            this.chkShowTemplates.CheckedChanged += new System.EventHandler(this.chkShowTemplates_CheckedChanged);
            // 
            // textBoxCache
            // 
            this.textBoxCache.Location = new System.Drawing.Point(186, 58);
            this.textBoxCache.Mask = "0000";
            this.textBoxCache.Name = "textBoxCache";
            this.textBoxCache.PromptChar = ' ';
            this.textBoxCache.Size = new System.Drawing.Size(30, 20);
            this.textBoxCache.TabIndex = 8;
            this.textBoxCache.Text = "64";
            this.textBoxCache.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textBoxCache_KeyPress);
            this.textBoxCache.Leave += new System.EventHandler(this.textBoxCache_Leave);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(127, 62);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(57, 13);
            this.label2.TabIndex = 3;
            this.label2.Text = "Cache line";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(4, 10);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(29, 13);
            this.label1.TabIndex = 1;
            this.label1.Text = "Filter";
            // 
            // textBoxFilter
            // 
            this.textBoxFilter.Location = new System.Drawing.Point(39, 6);
            this.textBoxFilter.Name = "textBoxFilter";
            this.textBoxFilter.Size = new System.Drawing.Size(331, 20);
            this.textBoxFilter.TabIndex = 0;
            this.textBoxFilter.TextChanged += new System.EventHandler(this.textBoxFilter_TextChanged);
            this.textBoxFilter.KeyUp += new System.Windows.Forms.KeyEventHandler(this.textBoxFilter_KeyUp);
            // 
            // statusStripBar
            // 
            this.statusStripBar.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripStatusLabel,
            this.toolStripProgressBar});
            this.statusStripBar.Location = new System.Drawing.Point(0, 735);
            this.statusStripBar.Name = "statusStripBar";
            this.statusStripBar.Size = new System.Drawing.Size(1794, 22);
            this.statusStripBar.TabIndex = 3;
            this.statusStripBar.Text = "statusStrip1";
            // 
            // toolStripStatusLabel
            // 
            this.toolStripStatusLabel.Name = "toolStripStatusLabel";
            this.toolStripStatusLabel.Size = new System.Drawing.Size(0, 17);
            // 
            // toolStripProgressBar
            // 
            this.toolStripProgressBar.Name = "toolStripProgressBar";
            this.toolStripProgressBar.Size = new System.Drawing.Size(800, 16);
            // 
            // contextMenuStripMembers
            // 
            this.contextMenuStripMembers.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.copyTypeLayoutToClipboardToolStripMenuItem,
            this.setPrefetchStartOffsetToolStripMenuItem});
            this.contextMenuStripMembers.Name = "contextMenuStrip1";
            this.contextMenuStripMembers.Size = new System.Drawing.Size(239, 48);
            // 
            // copyTypeLayoutToClipboardToolStripMenuItem
            // 
            this.copyTypeLayoutToClipboardToolStripMenuItem.Name = "copyTypeLayoutToClipboardToolStripMenuItem";
            this.copyTypeLayoutToClipboardToolStripMenuItem.Size = new System.Drawing.Size(238, 22);
            this.copyTypeLayoutToClipboardToolStripMenuItem.Text = "Copy Type Layout To Clipboard";
            this.copyTypeLayoutToClipboardToolStripMenuItem.Click += new System.EventHandler(this.copyTypeLayoutToClipboardToolStripMenuItem_Click);
            // 
            // setPrefetchStartOffsetToolStripMenuItem
            // 
            this.setPrefetchStartOffsetToolStripMenuItem.Name = "setPrefetchStartOffsetToolStripMenuItem";
            this.setPrefetchStartOffsetToolStripMenuItem.Size = new System.Drawing.Size(238, 22);
            this.setPrefetchStartOffsetToolStripMenuItem.Text = "Set Prefetch Start Offset";
            this.setPrefetchStartOffsetToolStripMenuItem.Click += new System.EventHandler(this.setPrefetchStartOffsetToolStripMenuItem_Click);
            // 
            // loadPDBBackgroundWorker
            // 
            this.loadPDBBackgroundWorker.WorkerReportsProgress = true;
            this.loadPDBBackgroundWorker.WorkerSupportsCancellation = true;
            this.loadPDBBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.loadPDBBackgroundWorker_DoWork);
            this.loadPDBBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.loadPDBBackgroundWorker_ProgressChanged);
            this.loadPDBBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.loadPDBBackgroundWorker_RunWorkerCompleted);
            // 
            // loadCSVBackgroundWorker
            // 
            this.loadCSVBackgroundWorker.WorkerReportsProgress = true;
            this.loadCSVBackgroundWorker.WorkerSupportsCancellation = true;
            this.loadCSVBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.loadCSVBackgroundWorker_DoWork);
            this.loadCSVBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.loadCSVBackgroundWorker_ProgressChanged);
            this.loadCSVBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.loadCSVBackgroundWorker_RunWorkerCompleted);
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.tableLayoutPanel1.AutoSize = true;
            this.tableLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.tableLayoutPanel1.ColumnCount = 1;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Controls.Add(this.panel1, 0, 0);
            this.tableLayoutPanel1.Controls.Add(this.splitContainer1, 0, 1);
            this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 24);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 2;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 110F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Size = new System.Drawing.Size(1797, 711);
            this.tableLayoutPanel1.TabIndex = 4;
            // 
            // panel1
            // 
            this.panel1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.panel1.AutoSize = true;
            this.panel1.Controls.Add(this.checkBoxFunctionAnalysis);
            this.panel1.Controls.Add(this.checkBoxShowOverlap);
            this.panel1.Controls.Add(this.checkBoxBitPadding);
            this.panel1.Controls.Add(this.checkBoxPadding);
            this.panel1.Controls.Add(this.checkBoxCacheLines);
            this.panel1.Controls.Add(this.checkBoxRegularExpressions);
            this.panel1.Controls.Add(this.checkBoxMatchWholeExpression);
            this.panel1.Controls.Add(this.checkBoxMatchCase);
            this.panel1.Controls.Add(this.groupBox1);
            this.panel1.Controls.Add(this.btnLoad);
            this.panel1.Controls.Add(this.btnReset);
            this.panel1.Controls.Add(this.chkSmartCacheLines);
            this.panel1.Controls.Add(this.chkShowTemplates);
            this.panel1.Controls.Add(this.label1);
            this.panel1.Controls.Add(this.textBoxCache);
            this.panel1.Controls.Add(this.textBoxFilter);
            this.panel1.Controls.Add(this.label2);
            this.panel1.Location = new System.Drawing.Point(3, 3);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(1791, 104);
            this.panel1.TabIndex = 4;
            // 
            // checkBoxFunctionAnalysis
            // 
            this.checkBoxFunctionAnalysis.AutoSize = true;
            this.checkBoxFunctionAnalysis.Location = new System.Drawing.Point(335, 83);
            this.checkBoxFunctionAnalysis.Name = "checkBoxFunctionAnalysis";
            this.checkBoxFunctionAnalysis.Size = new System.Drawing.Size(107, 17);
            this.checkBoxFunctionAnalysis.TabIndex = 17;
            this.checkBoxFunctionAnalysis.Text = "Function analysis";
            this.checkBoxFunctionAnalysis.UseVisualStyleBackColor = true;
            this.checkBoxFunctionAnalysis.CheckedChanged += new System.EventHandler(this.checkBoxFunctionAnalysis_CheckedChanged);
            // 
            // checkBoxShowOverlap
            // 
            this.checkBoxShowOverlap.AutoSize = true;
            this.checkBoxShowOverlap.Checked = true;
            this.checkBoxShowOverlap.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxShowOverlap.Location = new System.Drawing.Point(435, 62);
            this.checkBoxShowOverlap.Name = "checkBoxShowOverlap";
            this.checkBoxShowOverlap.Size = new System.Drawing.Size(148, 17);
            this.checkBoxShowOverlap.TabIndex = 16;
            this.checkBoxShowOverlap.Text = "Show cache lines overlap";
            this.checkBoxShowOverlap.UseVisualStyleBackColor = true;
            // 
            // checkBoxBitPadding
            // 
            this.checkBoxBitPadding.AutoSize = true;
            this.checkBoxBitPadding.Location = new System.Drawing.Point(153, 83);
            this.checkBoxBitPadding.Name = "checkBoxBitPadding";
            this.checkBoxBitPadding.Size = new System.Drawing.Size(127, 17);
            this.checkBoxBitPadding.TabIndex = 14;
            this.checkBoxBitPadding.Text = "Show bitfield padding";
            this.checkBoxBitPadding.UseVisualStyleBackColor = true;
            this.checkBoxBitPadding.CheckedChanged += new System.EventHandler(this.checkBoxBitPadding_CheckedChanged);
            // 
            // checkBoxPadding
            // 
            this.checkBoxPadding.AutoSize = true;
            this.checkBoxPadding.Checked = true;
            this.checkBoxPadding.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxPadding.Location = new System.Drawing.Point(11, 83);
            this.checkBoxPadding.Name = "checkBoxPadding";
            this.checkBoxPadding.Size = new System.Drawing.Size(94, 17);
            this.checkBoxPadding.TabIndex = 13;
            this.checkBoxPadding.Text = "Show padding";
            this.checkBoxPadding.UseVisualStyleBackColor = true;
            this.checkBoxPadding.CheckedChanged += new System.EventHandler(this.checkBoxPadding_CheckedChanged);
            // 
            // checkBoxCacheLines
            // 
            this.checkBoxCacheLines.AutoSize = true;
            this.checkBoxCacheLines.Location = new System.Drawing.Point(11, 60);
            this.checkBoxCacheLines.Name = "checkBoxCacheLines";
            this.checkBoxCacheLines.Size = new System.Drawing.Size(110, 17);
            this.checkBoxCacheLines.TabIndex = 7;
            this.checkBoxCacheLines.Text = "Show cache lines";
            this.checkBoxCacheLines.UseVisualStyleBackColor = true;
            this.checkBoxCacheLines.CheckedChanged += new System.EventHandler(this.checkBoxCacheLines_CheckedChanged);
            // 
            // checkBoxRegularExpressions
            // 
            this.checkBoxRegularExpressions.AutoSize = true;
            this.checkBoxRegularExpressions.Location = new System.Drawing.Point(303, 35);
            this.checkBoxRegularExpressions.Name = "checkBoxRegularExpressions";
            this.checkBoxRegularExpressions.Size = new System.Drawing.Size(138, 17);
            this.checkBoxRegularExpressions.TabIndex = 5;
            this.checkBoxRegularExpressions.Text = "Use regular expressions";
            this.checkBoxRegularExpressions.UseVisualStyleBackColor = true;
            this.checkBoxRegularExpressions.CheckedChanged += new System.EventHandler(this.checkBoxRegularExpressions_CheckedChanged);
            // 
            // checkBoxMatchWholeExpression
            // 
            this.checkBoxMatchWholeExpression.AutoSize = true;
            this.checkBoxMatchWholeExpression.Checked = true;
            this.checkBoxMatchWholeExpression.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxMatchWholeExpression.Location = new System.Drawing.Point(128, 35);
            this.checkBoxMatchWholeExpression.Name = "checkBoxMatchWholeExpression";
            this.checkBoxMatchWholeExpression.Size = new System.Drawing.Size(140, 17);
            this.checkBoxMatchWholeExpression.TabIndex = 4;
            this.checkBoxMatchWholeExpression.Text = "Match whole expression";
            this.checkBoxMatchWholeExpression.UseVisualStyleBackColor = true;
            this.checkBoxMatchWholeExpression.CheckedChanged += new System.EventHandler(this.checkBoxMatchWholeExpression_CheckedChanged);
            // 
            // checkBoxMatchCase
            // 
            this.checkBoxMatchCase.AutoSize = true;
            this.checkBoxMatchCase.Checked = true;
            this.checkBoxMatchCase.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxMatchCase.Location = new System.Drawing.Point(11, 35);
            this.checkBoxMatchCase.Name = "checkBoxMatchCase";
            this.checkBoxMatchCase.Size = new System.Drawing.Size(82, 17);
            this.checkBoxMatchCase.TabIndex = 3;
            this.checkBoxMatchCase.Text = "Match case";
            this.checkBoxMatchCase.UseVisualStyleBackColor = true;
            this.checkBoxMatchCase.CheckedChanged += new System.EventHandler(this.checkBoxMatchCase_CheckedChanged);
            // 
            // groupBox1
            // 
            this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.groupBox1.Controls.Add(this.checkedListBoxNamespaces);
            this.groupBox1.Location = new System.Drawing.Point(606, 0);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(1181, 104);
            this.groupBox1.TabIndex = 12;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Namespaces";
            // 
            // btnLoad
            // 
            this.btnLoad.Enabled = false;
            this.btnLoad.Location = new System.Drawing.Point(376, 5);
            this.btnLoad.Name = "btnLoad";
            this.btnLoad.Size = new System.Drawing.Size(144, 21);
            this.btnLoad.TabIndex = 1;
            this.btnLoad.Text = "No PDB selected";
            this.btnLoad.UseVisualStyleBackColor = true;
            this.btnLoad.Click += new System.EventHandler(this.btnLoad_Click);
            // 
            // btnReset
            // 
            this.btnReset.Enabled = false;
            this.btnReset.Location = new System.Drawing.Point(526, 5);
            this.btnReset.Name = "btnReset";
            this.btnReset.Size = new System.Drawing.Size(57, 21);
            this.btnReset.TabIndex = 2;
            this.btnReset.Text = "Reset";
            this.btnReset.UseVisualStyleBackColor = true;
            this.btnReset.Click += new System.EventHandler(this.Reset_Click);
            // 
            // chkSmartCacheLines
            // 
            this.chkSmartCacheLines.AutoSize = true;
            this.chkSmartCacheLines.Checked = true;
            this.chkSmartCacheLines.CheckState = System.Windows.Forms.CheckState.Checked;
            this.chkSmartCacheLines.Location = new System.Drawing.Point(245, 61);
            this.chkSmartCacheLines.Name = "chkSmartCacheLines";
            this.chkSmartCacheLines.Size = new System.Drawing.Size(174, 17);
            this.chkSmartCacheLines.TabIndex = 9;
            this.chkSmartCacheLines.Text = "Merge consecutive cache lines";
            this.chkSmartCacheLines.UseVisualStyleBackColor = true;
            this.chkSmartCacheLines.CheckedChanged += new System.EventHandler(this.chkSmartCacheLines_CheckedChanged);
            // 
            // splitContainer1
            // 
            this.splitContainer1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.splitContainer1.Location = new System.Drawing.Point(3, 113);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.dataGridSymbols);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.splitContainer2);
            this.splitContainer1.Size = new System.Drawing.Size(1791, 595);
            this.splitContainer1.SplitterDistance = 1000;
            this.splitContainer1.TabIndex = 3;
            // 
            // dataGridSymbols
            // 
            this.dataGridSymbols.AllowUserToAddRows = false;
            this.dataGridSymbols.AllowUserToDeleteRows = false;
            this.dataGridSymbols.AllowUserToResizeRows = false;
            dataGridViewCellStyle1.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(224)))), ((int)(((byte)(224)))), ((int)(((byte)(224)))));
            this.dataGridSymbols.AlternatingRowsDefaultCellStyle = dataGridViewCellStyle1;
            this.dataGridSymbols.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dataGridSymbols.Dock = System.Windows.Forms.DockStyle.Fill;
            this.dataGridSymbols.Location = new System.Drawing.Point(0, 0);
            this.dataGridSymbols.Name = "dataGridSymbols";
            this.dataGridSymbols.ReadOnly = true;
            this.dataGridSymbols.RowHeadersVisible = false;
            this.dataGridSymbols.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.dataGridSymbols.Size = new System.Drawing.Size(1000, 595);
            this.dataGridSymbols.TabIndex = 11;
            this.dataGridSymbols.CellMouseDoubleClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.dataGridSymbols_CellMouseDoubleClick);
            this.dataGridSymbols.SelectionChanged += new System.EventHandler(this.dataGridSymbols_SelectionChanged);
            this.dataGridSymbols.SortCompare += new System.Windows.Forms.DataGridViewSortCompareEventHandler(this.dataGridSymbols_SortCompare);
            this.dataGridSymbols.MouseClick += new System.Windows.Forms.MouseEventHandler(this.dataGridSymbols_MouseClick);
            // 
            // splitContainer2
            // 
            this.splitContainer2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer2.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
            this.splitContainer2.Location = new System.Drawing.Point(0, 0);
            this.splitContainer2.Name = "splitContainer2";
            this.splitContainer2.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer2.Panel1
            // 
            this.splitContainer2.Panel1.Controls.Add(this.labelCurrentSymbol);
            // 
            // splitContainer2.Panel2
            // 
            this.splitContainer2.Panel2.Controls.Add(this.Infos);
            this.splitContainer2.Size = new System.Drawing.Size(787, 595);
            this.splitContainer2.SplitterDistance = 25;
            this.splitContainer2.TabIndex = 16;
            // 
            // labelCurrentSymbol
            // 
            this.labelCurrentSymbol.AutoSize = true;
            this.labelCurrentSymbol.Dock = System.Windows.Forms.DockStyle.Fill;
            this.labelCurrentSymbol.Location = new System.Drawing.Point(0, 0);
            this.labelCurrentSymbol.Name = "labelCurrentSymbol";
            this.labelCurrentSymbol.Size = new System.Drawing.Size(0, 13);
            this.labelCurrentSymbol.TabIndex = 17;
            // 
            // Infos
            // 
            this.Infos.Controls.Add(this.tabMembers);
            this.Infos.Controls.Add(this.tabFunctions);
            this.Infos.Dock = System.Windows.Forms.DockStyle.Fill;
            this.Infos.Location = new System.Drawing.Point(0, 0);
            this.Infos.Name = "Infos";
            this.Infos.SelectedIndex = 0;
            this.Infos.Size = new System.Drawing.Size(787, 566);
            this.Infos.TabIndex = 12;
            // 
            // tabMembers
            // 
            this.tabMembers.Controls.Add(this.dataGridViewSymbolInfo);
            this.tabMembers.Location = new System.Drawing.Point(4, 22);
            this.tabMembers.Name = "tabMembers";
            this.tabMembers.Padding = new System.Windows.Forms.Padding(3);
            this.tabMembers.Size = new System.Drawing.Size(779, 540);
            this.tabMembers.TabIndex = 18;
            this.tabMembers.Text = "Members";
            this.tabMembers.UseVisualStyleBackColor = true;
            // 
            // dataGridViewSymbolInfo
            // 
            this.dataGridViewSymbolInfo.AllowUserToAddRows = false;
            this.dataGridViewSymbolInfo.AllowUserToDeleteRows = false;
            this.dataGridViewSymbolInfo.AllowUserToResizeRows = false;
            this.dataGridViewSymbolInfo.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dataGridViewSymbolInfo.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.Expand,
            this.colField,
            this.colFieldType,
            this.colFieldOffset,
            this.colBitPosition,
            this.colFieldSize,
            this.colFieldPadding});
            this.dataGridViewSymbolInfo.Dock = System.Windows.Forms.DockStyle.Fill;
            this.dataGridViewSymbolInfo.Location = new System.Drawing.Point(3, 3);
            this.dataGridViewSymbolInfo.MultiSelect = false;
            this.dataGridViewSymbolInfo.Name = "dataGridViewSymbolInfo";
            this.dataGridViewSymbolInfo.ReadOnly = true;
            this.dataGridViewSymbolInfo.RowHeadersVisible = false;
            this.dataGridViewSymbolInfo.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.dataGridViewSymbolInfo.Size = new System.Drawing.Size(773, 534);
            this.dataGridViewSymbolInfo.TabIndex = 13;
            this.dataGridViewSymbolInfo.CellFormatting += new System.Windows.Forms.DataGridViewCellFormattingEventHandler(this.dataGridViewSymbolInfo_CellFormatting);
            this.dataGridViewSymbolInfo.CellMouseDoubleClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.dataGridViewSymbolInfo_CellMouseDoubleClick);
            this.dataGridViewSymbolInfo.SortCompare += new System.Windows.Forms.DataGridViewSortCompareEventHandler(this.dataGridSymbols_SortCompare);
            this.dataGridViewSymbolInfo.KeyDown += new System.Windows.Forms.KeyEventHandler(this.dataGridViewSymbolInfo_KeyDown);
            this.dataGridViewSymbolInfo.MouseClick += new System.Windows.Forms.MouseEventHandler(this.dataGridViewSymbolInfo_MouseClick);
            // 
            // Expand
            // 
            this.Expand.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
            this.Expand.FillWeight = 10F;
            this.Expand.HeaderText = "";
            this.Expand.Name = "Expand";
            this.Expand.ReadOnly = true;
            this.Expand.Resizable = System.Windows.Forms.DataGridViewTriState.False;
            this.Expand.Width = 19;
            // 
            // colField
            // 
            this.colField.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
            this.colField.HeaderText = "Field";
            this.colField.Name = "colField";
            this.colField.ReadOnly = true;
            this.colField.Width = 54;
            // 
            // colFieldType
            // 
            this.colFieldType.HeaderText = "Type";
            this.colFieldType.Name = "colFieldType";
            this.colFieldType.ReadOnly = true;
            this.colFieldType.Width = 210;
            // 
            // colFieldOffset
            // 
            this.colFieldOffset.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
            dataGridViewCellStyle2.Format = "N0";
            dataGridViewCellStyle2.NullValue = null;
            this.colFieldOffset.DefaultCellStyle = dataGridViewCellStyle2;
            this.colFieldOffset.HeaderText = "Offset";
            this.colFieldOffset.Name = "colFieldOffset";
            this.colFieldOffset.ReadOnly = true;
            this.colFieldOffset.Width = 60;
            // 
            // colBitPosition
            // 
            this.colBitPosition.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
            dataGridViewCellStyle3.Format = "N0";
            dataGridViewCellStyle3.NullValue = null;
            this.colBitPosition.DefaultCellStyle = dataGridViewCellStyle3;
            this.colBitPosition.HeaderText = "Bit offset";
            this.colBitPosition.Name = "colBitPosition";
            this.colBitPosition.ReadOnly = true;
            this.colBitPosition.Width = 73;
            // 
            // colFieldSize
            // 
            this.colFieldSize.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
            dataGridViewCellStyle4.Format = "N0";
            dataGridViewCellStyle4.NullValue = null;
            this.colFieldSize.DefaultCellStyle = dataGridViewCellStyle4;
            this.colFieldSize.HeaderText = "Size";
            this.colFieldSize.Name = "colFieldSize";
            this.colFieldSize.ReadOnly = true;
            this.colFieldSize.Width = 52;
            // 
            // colFieldPadding
            // 
            this.colFieldPadding.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
            dataGridViewCellStyle5.Format = "N0";
            dataGridViewCellStyle5.NullValue = null;
            this.colFieldPadding.DefaultCellStyle = dataGridViewCellStyle5;
            this.colFieldPadding.HeaderText = "Padding";
            this.colFieldPadding.Name = "colFieldPadding";
            this.colFieldPadding.ReadOnly = true;
            this.colFieldPadding.Width = 71;
            // 
            // tabFunctions
            // 
            this.tabFunctions.Controls.Add(this.panel2);
            this.tabFunctions.Location = new System.Drawing.Point(4, 22);
            this.tabFunctions.Name = "tabFunctions";
            this.tabFunctions.Padding = new System.Windows.Forms.Padding(3);
            this.tabFunctions.Size = new System.Drawing.Size(779, 540);
            this.tabFunctions.TabIndex = 15;
            this.tabFunctions.Text = "Functions";
            this.tabFunctions.UseVisualStyleBackColor = true;
            // 
            // panel2
            // 
            this.panel2.Controls.Add(this.dataGridViewFunctionsInfo);
            this.panel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel2.Location = new System.Drawing.Point(3, 3);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(773, 534);
            this.panel2.TabIndex = 14;
            // 
            // dataGridViewFunctionsInfo
            // 
            this.dataGridViewFunctionsInfo.AllowUserToAddRows = false;
            this.dataGridViewFunctionsInfo.AllowUserToDeleteRows = false;
            this.dataGridViewFunctionsInfo.AllowUserToResizeRows = false;
            this.dataGridViewFunctionsInfo.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dataGridViewFunctionsInfo.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.dataGridViewTextBoxFunction,
            this.dataGridViewTextBoxVirtual,
            this.Pure,
            this.IsOverride,
            this.Overloaded,
            this.IsMasking});
            this.dataGridViewFunctionsInfo.Dock = System.Windows.Forms.DockStyle.Fill;
            this.dataGridViewFunctionsInfo.Location = new System.Drawing.Point(0, 0);
            this.dataGridViewFunctionsInfo.MultiSelect = false;
            this.dataGridViewFunctionsInfo.Name = "dataGridViewFunctionsInfo";
            this.dataGridViewFunctionsInfo.ReadOnly = true;
            this.dataGridViewFunctionsInfo.RowHeadersVisible = false;
            this.dataGridViewFunctionsInfo.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.dataGridViewFunctionsInfo.Size = new System.Drawing.Size(773, 534);
            this.dataGridViewFunctionsInfo.TabIndex = 2;
            this.dataGridViewFunctionsInfo.CellFormatting += new System.Windows.Forms.DataGridViewCellFormattingEventHandler(this.dataGridViewFunctionsInfo_CellFormatting);
            this.dataGridViewFunctionsInfo.MouseClick += new System.Windows.Forms.MouseEventHandler(this.dataGridViewFunctionsInfo_MouseClick);
            // 
            // dataGridViewTextBoxFunction
            // 
            this.dataGridViewTextBoxFunction.HeaderText = "Function";
            this.dataGridViewTextBoxFunction.Name = "dataGridViewTextBoxFunction";
            this.dataGridViewTextBoxFunction.ReadOnly = true;
            this.dataGridViewTextBoxFunction.Width = 360;
            // 
            // dataGridViewTextBoxVirtual
            // 
            this.dataGridViewTextBoxVirtual.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.ColumnHeader;
            this.dataGridViewTextBoxVirtual.HeaderText = "Virtual";
            this.dataGridViewTextBoxVirtual.Name = "dataGridViewTextBoxVirtual";
            this.dataGridViewTextBoxVirtual.ReadOnly = true;
            this.dataGridViewTextBoxVirtual.Resizable = System.Windows.Forms.DataGridViewTriState.True;
            this.dataGridViewTextBoxVirtual.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.Automatic;
            this.dataGridViewTextBoxVirtual.Width = 61;
            // 
            // Pure
            // 
            this.Pure.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.ColumnHeader;
            this.Pure.HeaderText = "Pure";
            this.Pure.Name = "Pure";
            this.Pure.ReadOnly = true;
            this.Pure.Width = 35;
            // 
            // IsOverride
            // 
            this.IsOverride.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.ColumnHeader;
            this.IsOverride.HeaderText = "Is override";
            this.IsOverride.Name = "IsOverride";
            this.IsOverride.ReadOnly = true;
            this.IsOverride.Width = 62;
            // 
            // Overloaded
            // 
            this.Overloaded.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.ColumnHeader;
            this.Overloaded.HeaderText = "Overloaded";
            this.Overloaded.Name = "Overloaded";
            this.Overloaded.ReadOnly = true;
            this.Overloaded.Width = 68;
            // 
            // IsMasking
            // 
            this.IsMasking.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.ColumnHeader;
            this.IsMasking.HeaderText = "Is masking";
            this.IsMasking.Name = "IsMasking";
            this.IsMasking.ReadOnly = true;
            this.IsMasking.Width = 63;
            // 
            // contextMenuStripClassInfo
            // 
            this.contextMenuStripClassInfo.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripMenuItemParentClasses});
            this.contextMenuStripClassInfo.Name = "contextMenuStripClassInfo";
            this.contextMenuStripClassInfo.Size = new System.Drawing.Size(148, 26);
            // 
            // toolStripMenuItemParentClasses
            // 
            this.toolStripMenuItemParentClasses.Name = "toolStripMenuItemParentClasses";
            this.toolStripMenuItemParentClasses.Size = new System.Drawing.Size(147, 22);
            this.toolStripMenuItemParentClasses.Text = "Parent classes";
            // 
            // contextMenuStripFunctions
            // 
            this.contextMenuStripFunctions.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.ignoreFunctionToolStripMenuItem});
            this.contextMenuStripFunctions.Name = "contextMenuStripFunctions";
            this.contextMenuStripFunctions.Size = new System.Drawing.Size(157, 26);
            // 
            // ignoreFunctionToolStripMenuItem
            // 
            this.ignoreFunctionToolStripMenuItem.Name = "ignoreFunctionToolStripMenuItem";
            this.ignoreFunctionToolStripMenuItem.Size = new System.Drawing.Size(156, 22);
            this.ignoreFunctionToolStripMenuItem.Text = "Ignore function";
            this.ignoreFunctionToolStripMenuItem.Click += new System.EventHandler(this.ignoreFunctionToolStripMenuItem_Click);
            // 
            // CruncherSharpForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1794, 757);
            this.Controls.Add(this.tableLayoutPanel1);
            this.Controls.Add(this.statusStripBar);
            this.Controls.Add(this.mainMenu);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MainMenuStrip = this.mainMenu;
            this.Name = "CruncherSharpForm";
            this.Text = "Cruncher #";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.CruncherSharpForm_FormClosing);
            this.mainMenu.ResumeLayout(false);
            this.mainMenu.PerformLayout();
            this.statusStripBar.ResumeLayout(false);
            this.statusStripBar.PerformLayout();
            this.contextMenuStripMembers.ResumeLayout(false);
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            this.panel1.ResumeLayout(false);
            this.panel1.PerformLayout();
            this.groupBox1.ResumeLayout(false);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.dataGridSymbols)).EndInit();
            this.splitContainer2.Panel1.ResumeLayout(false);
            this.splitContainer2.Panel1.PerformLayout();
            this.splitContainer2.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).EndInit();
            this.splitContainer2.ResumeLayout(false);
            this.Infos.ResumeLayout(false);
            this.tabMembers.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.dataGridViewSymbolInfo)).EndInit();
            this.tabFunctions.ResumeLayout(false);
            this.panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.dataGridViewFunctionsInfo)).EndInit();
            this.contextMenuStripClassInfo.ResumeLayout(false);
            this.contextMenuStripFunctions.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.bindingSourceSymbols)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.MenuStrip mainMenu;
        private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem loadPDBToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem exitToolStripMenuItem;
        private System.Windows.Forms.OpenFileDialog openPdbDialog;
        private System.Windows.Forms.OpenFileDialog openCsvDialog;
        private System.Windows.Forms.SaveFileDialog saveCsvDialog;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox textBoxFilter;
        private System.Windows.Forms.ContextMenuStrip contextMenuStripMembers;
        private System.Windows.Forms.ToolStripMenuItem copyTypeLayoutToClipboardToolStripMenuItem;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.MaskedTextBox textBoxCache;
        private System.Windows.Forms.ToolStripMenuItem setPrefetchStartOffsetToolStripMenuItem;
        private System.Windows.Forms.CheckBox chkShowTemplates;
        private System.Windows.Forms.CheckedListBox checkedListBoxNamespaces;
        private System.ComponentModel.BackgroundWorker loadPDBBackgroundWorker;
		private System.ComponentModel.BackgroundWorker loadCSVBackgroundWorker;
		private System.Windows.Forms.ToolStripMenuItem loadInstanceCountToolStripMenuItem;
        private System.Windows.Forms.StatusStrip statusStripBar;
        private System.Windows.Forms.ToolStripStatusLabel toolStripStatusLabel;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.DataGridView dataGridSymbols;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.ToolStripProgressBar toolStripProgressBar;
        private System.Windows.Forms.ToolStripMenuItem compareWithPDBToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem exportCsvToolStripMenuItem;
        private System.Windows.Forms.CheckBox chkSmartCacheLines;
        private System.Windows.Forms.ContextMenuStrip contextMenuStripClassInfo;
        private System.Windows.Forms.ToolStripMenuItem toolStripMenuItemParentClasses;
        private System.Windows.Forms.Button btnReset;
        private System.Windows.Forms.ToolStripMenuItem findMSVCExtraPaddingToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem findMSVCEmptyBaseClassToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem findUnusedInterfacesToolStripMenuItem;
        private System.Windows.Forms.SplitContainer splitContainer2;
        private System.Windows.Forms.TabControl Infos;
        private System.Windows.Forms.TabPage tabMembers;
        private System.Windows.Forms.DataGridView dataGridViewSymbolInfo;
        private System.Windows.Forms.TabPage tabFunctions;
        private System.Windows.Forms.Label labelCurrentSymbol;
        private System.Windows.Forms.Panel panel2;
        private System.Windows.Forms.DataGridView dataGridViewFunctionsInfo;
        private System.Windows.Forms.ToolStripMenuItem findToolStripMenuItem1;
        private System.Windows.Forms.ToolStripMenuItem findUnusedVirtualToolStripMenuItem;
        private System.Windows.Forms.DataGridViewTextBoxColumn dataGridViewTextBoxFunction;
        private System.Windows.Forms.DataGridViewCheckBoxColumn dataGridViewTextBoxVirtual;
        private System.Windows.Forms.DataGridViewCheckBoxColumn Pure;
        private System.Windows.Forms.DataGridViewCheckBoxColumn IsOverride;
        private System.Windows.Forms.DataGridViewCheckBoxColumn Overloaded;
        private System.Windows.Forms.DataGridViewCheckBoxColumn IsMasking;
        private System.Windows.Forms.ToolStripMenuItem findMaskingFunctionsToolStripMenuItem;
        private System.Windows.Forms.ContextMenuStrip contextMenuStripFunctions;
        private System.Windows.Forms.ToolStripMenuItem ignoreFunctionToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem findRemovedInlineToolStripMenuItem;
        private System.Windows.Forms.Button btnLoad;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.CheckBox checkBoxMatchWholeExpression;
        private System.Windows.Forms.CheckBox checkBoxMatchCase;
        private System.Windows.Forms.CheckBox checkBoxRegularExpressions;
        private System.Windows.Forms.CheckBox checkBoxCacheLines;
        private System.Windows.Forms.DataGridViewTextBoxColumn Expand;
        private System.Windows.Forms.DataGridViewTextBoxColumn colField;
        private System.Windows.Forms.DataGridViewTextBoxColumn colFieldType;
        private System.Windows.Forms.DataGridViewTextBoxColumn colFieldOffset;
        private System.Windows.Forms.DataGridViewTextBoxColumn colBitPosition;
        private System.Windows.Forms.DataGridViewTextBoxColumn colFieldSize;
        private System.Windows.Forms.DataGridViewTextBoxColumn colFieldPadding;
        private System.Windows.Forms.CheckBox checkBoxPadding;
        private System.Windows.Forms.CheckBox checkBoxBitPadding;
		private System.Windows.Forms.CheckBox checkBoxShowOverlap;
		private System.Windows.Forms.CheckBox checkBoxFunctionAnalysis;
		private System.Windows.Forms.ToolStripMenuItem findUnusedVtablesToolStripMenuItem;
		private System.Windows.Forms.BindingSource bindingSourceSymbols;
		private System.Windows.Forms.ToolStripMenuItem restrictToSymbolsImportedFroCSVToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem unrealEngineToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem addMemPoolsToolStripMenuItem;
	}
}


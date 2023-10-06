// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class IssueBrowserWindow
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
			this.OkBtn = new System.Windows.Forms.Button();
			this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			this.FilterBtn = new System.Windows.Forms.Button();
			this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
			this.StatusLabel = new System.Windows.Forms.Label();
			this.FetchMoreResultsLinkLabel = new System.Windows.Forms.LinkLabel();
			this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			this.IssueListView = new UnrealGameSync.CustomListViewControl();
			this.IconHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.IdHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.CreatedHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.ResolvedHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.TimeToFixHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.OwnerHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.DescriptionHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.FilterMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.FilterMenu_ShowAll = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterMenu_Separator = new System.Windows.Forms.ToolStripSeparator();
			this.RefreshIssuesTimer = new System.Windows.Forms.Timer(this.components);
			this.tableLayoutPanel1.SuspendLayout();
			this.flowLayoutPanel1.SuspendLayout();
			this.tableLayoutPanel2.SuspendLayout();
			this.FilterMenu.SuspendLayout();
			this.SuspendLayout();
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = System.Windows.Forms.AnchorStyles.None;
			this.OkBtn.Location = new System.Drawing.Point(1060, 3);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(113, 29);
			this.OkBtn.TabIndex = 3;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// tableLayoutPanel1
			// 
			this.tableLayoutPanel1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel1.AutoSize = true;
			this.tableLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.tableLayoutPanel1.ColumnCount = 3;
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 20F));
			this.tableLayoutPanel1.Controls.Add(this.OkBtn, 2, 0);
			this.tableLayoutPanel1.Controls.Add(this.FilterBtn, 0, 0);
			this.tableLayoutPanel1.Controls.Add(this.flowLayoutPanel1, 1, 0);
			this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 447);
			this.tableLayoutPanel1.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel1.Name = "tableLayoutPanel1";
			this.tableLayoutPanel1.RowCount = 1;
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
			this.tableLayoutPanel1.Size = new System.Drawing.Size(1176, 35);
			this.tableLayoutPanel1.TabIndex = 5;
			// 
			// FilterBtn
			// 
			this.FilterBtn.Anchor = System.Windows.Forms.AnchorStyles.None;
			this.FilterBtn.Image = global::UnrealGameSync.Properties.Resources.DropList;
			this.FilterBtn.ImageAlign = System.Drawing.ContentAlignment.MiddleRight;
			this.FilterBtn.Location = new System.Drawing.Point(3, 3);
			this.FilterBtn.Name = "FilterBtn";
			this.FilterBtn.Size = new System.Drawing.Size(125, 29);
			this.FilterBtn.TabIndex = 4;
			this.FilterBtn.Text = "Filter";
			this.FilterBtn.UseVisualStyleBackColor = true;
			this.FilterBtn.Click += new System.EventHandler(this.FilterBtn_Click);
			// 
			// flowLayoutPanel1
			// 
			this.flowLayoutPanel1.Anchor = System.Windows.Forms.AnchorStyles.None;
			this.flowLayoutPanel1.AutoSize = true;
			this.flowLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.flowLayoutPanel1.Controls.Add(this.StatusLabel);
			this.flowLayoutPanel1.Controls.Add(this.FetchMoreResultsLinkLabel);
			this.flowLayoutPanel1.Location = new System.Drawing.Point(511, 10);
			this.flowLayoutPanel1.Name = "flowLayoutPanel1";
			this.flowLayoutPanel1.Size = new System.Drawing.Size(166, 15);
			this.flowLayoutPanel1.TabIndex = 0;
			this.flowLayoutPanel1.WrapContents = false;
			// 
			// StatusLabel
			// 
			this.StatusLabel.Anchor = System.Windows.Forms.AnchorStyles.None;
			this.StatusLabel.AutoSize = true;
			this.StatusLabel.Location = new System.Drawing.Point(0, 0);
			this.StatusLabel.Margin = new System.Windows.Forms.Padding(0);
			this.StatusLabel.Name = "StatusLabel";
			this.StatusLabel.Size = new System.Drawing.Size(99, 15);
			this.StatusLabel.TabIndex = 3;
			this.StatusLabel.Text = "Querying server...";
			// 
			// FetchMoreResultsLinkLabel
			// 
			this.FetchMoreResultsLinkLabel.Anchor = System.Windows.Forms.AnchorStyles.None;
			this.FetchMoreResultsLinkLabel.AutoSize = true;
			this.FetchMoreResultsLinkLabel.Location = new System.Drawing.Point(99, 0);
			this.FetchMoreResultsLinkLabel.Margin = new System.Windows.Forms.Padding(0);
			this.FetchMoreResultsLinkLabel.Name = "FetchMoreResultsLinkLabel";
			this.FetchMoreResultsLinkLabel.Size = new System.Drawing.Size(67, 15);
			this.FetchMoreResultsLinkLabel.TabIndex = 4;
			this.FetchMoreResultsLinkLabel.TabStop = true;
			this.FetchMoreResultsLinkLabel.Text = "Fetch more";
			this.FetchMoreResultsLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.FetchMoreResultsLinkLabel_LinkClicked);
			// 
			// tableLayoutPanel2
			// 
			this.tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel2.ColumnCount = 1;
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.Controls.Add(this.IssueListView, 0, 0);
			this.tableLayoutPanel2.Controls.Add(this.tableLayoutPanel1, 0, 1);
			this.tableLayoutPanel2.Location = new System.Drawing.Point(12, 12);
			this.tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel2.Name = "tableLayoutPanel2";
			this.tableLayoutPanel2.RowCount = 2;
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
			this.tableLayoutPanel2.Size = new System.Drawing.Size(1176, 482);
			this.tableLayoutPanel2.TabIndex = 6;
			// 
			// IssueListView
			// 
			this.IssueListView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.IssueListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.IconHeader,
            this.IdHeader,
            this.CreatedHeader,
            this.ResolvedHeader,
            this.TimeToFixHeader,
            this.OwnerHeader,
            this.DescriptionHeader});
			this.IssueListView.FullRowSelect = true;
			this.IssueListView.HideSelection = false;
			this.IssueListView.Location = new System.Drawing.Point(3, 3);
			this.IssueListView.Margin = new System.Windows.Forms.Padding(3, 3, 3, 8);
			this.IssueListView.MultiSelect = false;
			this.IssueListView.Name = "IssueListView";
			this.IssueListView.OwnerDraw = true;
			this.IssueListView.Size = new System.Drawing.Size(1170, 436);
			this.IssueListView.TabIndex = 0;
			this.IssueListView.UseCompatibleStateImageBehavior = false;
			this.IssueListView.View = System.Windows.Forms.View.Details;
			this.IssueListView.DrawColumnHeader += new System.Windows.Forms.DrawListViewColumnHeaderEventHandler(this.IssueListView_DrawColumnHeader);
			this.IssueListView.DrawItem += new System.Windows.Forms.DrawListViewItemEventHandler(this.IssueListView_DrawItem);
			this.IssueListView.DrawSubItem += new System.Windows.Forms.DrawListViewSubItemEventHandler(this.IssueListView_DrawSubItem);
			this.IssueListView.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.IssueListView_MouseDoubleClick);
			// 
			// IconHeader
			// 
			this.IconHeader.Text = "";
			this.IconHeader.Width = 28;
			// 
			// IdHeader
			// 
			this.IdHeader.Text = "Id";
			this.IdHeader.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			// 
			// CreatedHeader
			// 
			this.CreatedHeader.Text = "Created";
			this.CreatedHeader.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.CreatedHeader.Width = 117;
			// 
			// ResolvedHeader
			// 
			this.ResolvedHeader.Text = "Resolved";
			this.ResolvedHeader.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.ResolvedHeader.Width = 125;
			// 
			// TimeToFixHeader
			// 
			this.TimeToFixHeader.Text = "Time To Resolve";
			this.TimeToFixHeader.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.TimeToFixHeader.Width = 125;
			// 
			// OwnerHeader
			// 
			this.OwnerHeader.Text = "Owner";
			this.OwnerHeader.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.OwnerHeader.Width = 135;
			// 
			// DescriptionHeader
			// 
			this.DescriptionHeader.Text = "Description";
			this.DescriptionHeader.Width = 533;
			// 
			// FilterMenu
			// 
			this.FilterMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.FilterMenu_ShowAll,
            this.FilterMenu_Separator});
			this.FilterMenu.Name = "FilterMenu";
			this.FilterMenu.Size = new System.Drawing.Size(121, 32);
			// 
			// FilterMenu_ShowAll
			// 
			this.FilterMenu_ShowAll.Name = "FilterMenu_ShowAll";
			this.FilterMenu_ShowAll.Size = new System.Drawing.Size(120, 22);
			this.FilterMenu_ShowAll.Text = "Show All";
			this.FilterMenu_ShowAll.Click += new System.EventHandler(this.FilterMenu_ShowAll_Click);
			// 
			// FilterMenu_Separator
			// 
			this.FilterMenu_Separator.Name = "FilterMenu_Separator";
			this.FilterMenu_Separator.Size = new System.Drawing.Size(117, 6);
			// 
			// RefreshIssuesTimer
			// 
			this.RefreshIssuesTimer.Enabled = true;
			this.RefreshIssuesTimer.Interval = 2000;
			this.RefreshIssuesTimer.Tick += new System.EventHandler(this.RefreshIssuesTimer_Tick);
			// 
			// IssueBrowserWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.ClientSize = new System.Drawing.Size(1200, 506);
			this.Controls.Add(this.tableLayoutPanel2);
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "IssueBrowserWindow";
			this.ShowIcon = false;
			this.ShowInTaskbar = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Issues";
			this.Load += new System.EventHandler(this.IssueBrowserWindow_Load);
			this.tableLayoutPanel1.ResumeLayout(false);
			this.tableLayoutPanel1.PerformLayout();
			this.flowLayoutPanel1.ResumeLayout(false);
			this.flowLayoutPanel1.PerformLayout();
			this.tableLayoutPanel2.ResumeLayout(false);
			this.tableLayoutPanel2.PerformLayout();
			this.FilterMenu.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion

		private CustomListViewControl IssueListView;
		private System.Windows.Forms.ColumnHeader IdHeader;
		private System.Windows.Forms.ColumnHeader CreatedHeader;
		private System.Windows.Forms.ColumnHeader ResolvedHeader;
		private System.Windows.Forms.ColumnHeader DescriptionHeader;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
		private System.Windows.Forms.Label StatusLabel;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.LinkLabel FetchMoreResultsLinkLabel;
		private System.Windows.Forms.ColumnHeader IconHeader;
		private System.Windows.Forms.ColumnHeader OwnerHeader;
		private System.Windows.Forms.Button FilterBtn;
		private System.Windows.Forms.ContextMenuStrip FilterMenu;
		private System.Windows.Forms.ToolStripMenuItem FilterMenu_ShowAll;
		private System.Windows.Forms.ToolStripSeparator FilterMenu_Separator;
		private System.Windows.Forms.Timer RefreshIssuesTimer;
		private System.Windows.Forms.ColumnHeader TimeToFixHeader;
	}
}
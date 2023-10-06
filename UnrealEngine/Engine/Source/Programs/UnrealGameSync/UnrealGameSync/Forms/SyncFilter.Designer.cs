// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class SyncFilter
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(SyncFilter));
			this.OkButton = new System.Windows.Forms.Button();
			this.CancButton = new System.Windows.Forms.Button();
			this.FilterTextGlobal = new System.Windows.Forms.TextBox();
			this.label1 = new System.Windows.Forms.Label();
			this.TabControl = new System.Windows.Forms.TabControl();
			this.GlobalTab = new System.Windows.Forms.TabPage();
			this.WorkspaceTab = new System.Windows.Forms.TabPage();
			this.ShowCombinedView = new System.Windows.Forms.Button();
			this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			this.GlobalControl = new UnrealGameSync.Controls.SyncFilterControl();
			this.WorkspaceControl = new UnrealGameSync.Controls.SyncFilterControl();
			this.TabControl.SuspendLayout();
			this.GlobalTab.SuspendLayout();
			this.WorkspaceTab.SuspendLayout();
			this.tableLayoutPanel1.SuspendLayout();
			this.tableLayoutPanel2.SuspendLayout();
			this.SuspendLayout();
			// 
			// OkButton
			// 
			this.OkButton.Anchor = System.Windows.Forms.AnchorStyles.None;
			this.OkButton.Location = new System.Drawing.Point(868, 0);
			this.OkButton.Margin = new System.Windows.Forms.Padding(3, 0, 3, 0);
			this.OkButton.Name = "OkButton";
			this.OkButton.Size = new System.Drawing.Size(87, 26);
			this.OkButton.TabIndex = 2;
			this.OkButton.Text = "Ok";
			this.OkButton.UseVisualStyleBackColor = true;
			this.OkButton.Click += new System.EventHandler(this.OkButton_Click);
			// 
			// CancButton
			// 
			this.CancButton.Anchor = System.Windows.Forms.AnchorStyles.None;
			this.CancButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancButton.Location = new System.Drawing.Point(961, 0);
			this.CancButton.Margin = new System.Windows.Forms.Padding(3, 0, 0, 0);
			this.CancButton.Name = "CancButton";
			this.CancButton.Size = new System.Drawing.Size(87, 26);
			this.CancButton.TabIndex = 3;
			this.CancButton.Text = "Cancel";
			this.CancButton.UseVisualStyleBackColor = true;
			this.CancButton.Click += new System.EventHandler(this.CancButton_Click);
			// 
			// FilterTextGlobal
			// 
			this.FilterTextGlobal.AcceptsReturn = true;
			this.FilterTextGlobal.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.FilterTextGlobal.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.FilterTextGlobal.Font = new System.Drawing.Font("Courier New", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.FilterTextGlobal.Location = new System.Drawing.Point(0, 7);
			this.FilterTextGlobal.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
			this.FilterTextGlobal.Multiline = true;
			this.FilterTextGlobal.Name = "FilterTextGlobal";
			this.FilterTextGlobal.Size = new System.Drawing.Size(940, 361);
			this.FilterTextGlobal.TabIndex = 1;
			this.FilterTextGlobal.WordWrap = false;
			// 
			// label1
			// 
			this.label1.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(16, 16);
			this.label1.Margin = new System.Windows.Forms.Padding(0);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(917, 15);
			this.label1.TabIndex = 4;
			this.label1.Text = "Files synced from Perforce may be filtered by a custom stream view, and list of p" +
    "redefined categories.  Settings for the current workspace override defaults for " +
    "all workspaces.";
			// 
			// TabControl
			// 
			this.TabControl.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.TabControl.Controls.Add(this.GlobalTab);
			this.TabControl.Controls.Add(this.WorkspaceTab);
			this.TabControl.Location = new System.Drawing.Point(24, 43);
			this.TabControl.Margin = new System.Windows.Forms.Padding(8, 12, 8, 12);
			this.TabControl.Name = "TabControl";
			this.TabControl.SelectedIndex = 0;
			this.TabControl.Size = new System.Drawing.Size(1032, 629);
			this.TabControl.TabIndex = 5;
			// 
			// GlobalTab
			// 
			this.GlobalTab.Controls.Add(this.GlobalControl);
			this.GlobalTab.Location = new System.Drawing.Point(4, 24);
			this.GlobalTab.Name = "GlobalTab";
			this.GlobalTab.Padding = new System.Windows.Forms.Padding(3);
			this.GlobalTab.Size = new System.Drawing.Size(1024, 601);
			this.GlobalTab.TabIndex = 0;
			this.GlobalTab.Text = "All Workspaces";
			this.GlobalTab.UseVisualStyleBackColor = true;
			// 
			// WorkspaceTab
			// 
			this.WorkspaceTab.Controls.Add(this.WorkspaceControl);
			this.WorkspaceTab.Location = new System.Drawing.Point(4, 24);
			this.WorkspaceTab.Name = "WorkspaceTab";
			this.WorkspaceTab.Padding = new System.Windows.Forms.Padding(3);
			this.WorkspaceTab.Size = new System.Drawing.Size(1032, 609);
			this.WorkspaceTab.TabIndex = 3;
			this.WorkspaceTab.Text = "Current Workspace";
			this.WorkspaceTab.UseVisualStyleBackColor = true;
			// 
			// ShowCombinedView
			// 
			this.ShowCombinedView.Anchor = System.Windows.Forms.AnchorStyles.None;
			this.ShowCombinedView.Location = new System.Drawing.Point(0, 0);
			this.ShowCombinedView.Margin = new System.Windows.Forms.Padding(0, 0, 3, 0);
			this.ShowCombinedView.Name = "ShowCombinedView";
			this.ShowCombinedView.Size = new System.Drawing.Size(174, 26);
			this.ShowCombinedView.TabIndex = 6;
			this.ShowCombinedView.Text = "Show Combined Filter";
			this.ShowCombinedView.UseVisualStyleBackColor = true;
			this.ShowCombinedView.Click += new System.EventHandler(this.ShowCombinedView_Click);
			// 
			// tableLayoutPanel1
			// 
			this.tableLayoutPanel1.ColumnCount = 1;
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Controls.Add(this.label1, 0, 0);
			this.tableLayoutPanel1.Controls.Add(this.TabControl, 0, 1);
			this.tableLayoutPanel1.Controls.Add(this.tableLayoutPanel2, 0, 2);
			this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
			this.tableLayoutPanel1.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel1.Name = "tableLayoutPanel1";
			this.tableLayoutPanel1.Padding = new System.Windows.Forms.Padding(16);
			this.tableLayoutPanel1.RowCount = 3;
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
			this.tableLayoutPanel1.Size = new System.Drawing.Size(1080, 726);
			this.tableLayoutPanel1.TabIndex = 7;
			// 
			// tableLayoutPanel2
			// 
			this.tableLayoutPanel2.AutoSize = true;
			this.tableLayoutPanel2.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.tableLayoutPanel2.ColumnCount = 4;
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel2.Controls.Add(this.ShowCombinedView, 0, 0);
			this.tableLayoutPanel2.Controls.Add(this.CancButton, 3, 0);
			this.tableLayoutPanel2.Controls.Add(this.OkButton, 2, 0);
			this.tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tableLayoutPanel2.Location = new System.Drawing.Point(16, 684);
			this.tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel2.Name = "tableLayoutPanel2";
			this.tableLayoutPanel2.RowCount = 1;
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.Size = new System.Drawing.Size(1048, 26);
			this.tableLayoutPanel2.TabIndex = 6;
			// 
			// GlobalControl
			// 
			this.GlobalControl.BackColor = System.Drawing.SystemColors.Window;
			this.GlobalControl.Dock = System.Windows.Forms.DockStyle.Fill;
			this.GlobalControl.Location = new System.Drawing.Point(3, 3);
			this.GlobalControl.Name = "GlobalControl";
			this.GlobalControl.Padding = new System.Windows.Forms.Padding(6);
			this.GlobalControl.Size = new System.Drawing.Size(1018, 595);
			this.GlobalControl.TabIndex = 0;
			// 
			// WorkspaceControl
			// 
			this.WorkspaceControl.BackColor = System.Drawing.SystemColors.Window;
			this.WorkspaceControl.Dock = System.Windows.Forms.DockStyle.Fill;
			this.WorkspaceControl.Location = new System.Drawing.Point(3, 3);
			this.WorkspaceControl.Name = "WorkspaceControl";
			this.WorkspaceControl.Padding = new System.Windows.Forms.Padding(6);
			this.WorkspaceControl.Size = new System.Drawing.Size(1026, 605);
			this.WorkspaceControl.TabIndex = 0;
			// 
			// SyncFilter
			// 
			this.AcceptButton = this.OkButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancButton;
			this.ClientSize = new System.Drawing.Size(1080, 726);
			this.Controls.Add(this.tableLayoutPanel1);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.MinimumSize = new System.Drawing.Size(674, 340);
			this.Name = "SyncFilter";
			this.ShowInTaskbar = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Sync Filter";
			this.TabControl.ResumeLayout(false);
			this.GlobalTab.ResumeLayout(false);
			this.WorkspaceTab.ResumeLayout(false);
			this.tableLayoutPanel1.ResumeLayout(false);
			this.tableLayoutPanel1.PerformLayout();
			this.tableLayoutPanel2.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Button OkButton;
		private System.Windows.Forms.Button CancButton;
		private System.Windows.Forms.TextBox FilterTextGlobal;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.TabControl TabControl;
		private System.Windows.Forms.TabPage GlobalTab;
		private System.Windows.Forms.Button ShowCombinedView;
		private System.Windows.Forms.TabPage WorkspaceTab;
		private Controls.SyncFilterControl GlobalControl;
		private Controls.SyncFilterControl WorkspaceControl;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
	}
}
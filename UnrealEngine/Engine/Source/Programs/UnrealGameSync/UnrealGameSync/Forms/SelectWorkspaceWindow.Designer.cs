namespace UnrealGameSync
{
	partial class SelectWorkspaceWindow
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
			this.WorkspaceListView = new System.Windows.Forms.ListView();
			this.NameColumnHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.HostColumnHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.StreamColumnHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.RootDirColumnHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.OnlyForThisComputer = new System.Windows.Forms.CheckBox();
			this.SuspendLayout();
			// 
			// WorkspaceListView
			// 
			this.WorkspaceListView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.NameColumnHeader,
            this.HostColumnHeader,
            this.StreamColumnHeader,
            this.RootDirColumnHeader});
			this.WorkspaceListView.FullRowSelect = true;
			this.WorkspaceListView.Location = new System.Drawing.Point(15, 17);
			this.WorkspaceListView.Name = "WorkspaceListView";
			this.WorkspaceListView.Size = new System.Drawing.Size(901, 449);
			this.WorkspaceListView.TabIndex = 0;
			this.WorkspaceListView.UseCompatibleStateImageBehavior = false;
			this.WorkspaceListView.View = System.Windows.Forms.View.Details;
			this.WorkspaceListView.SelectedIndexChanged += new System.EventHandler(this.WorkspaceListView_SelectedIndexChanged);
			this.WorkspaceListView.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.WorkspaceListView_MouseDoubleClick);
			// 
			// NameColumnHeader
			// 
			this.NameColumnHeader.Text = "Name";
			this.NameColumnHeader.Width = 205;
			// 
			// HostColumnHeader
			// 
			this.HostColumnHeader.Text = "Host";
			this.HostColumnHeader.Width = 181;
			// 
			// StreamColumnHeader
			// 
			this.StreamColumnHeader.Text = "Stream";
			this.StreamColumnHeader.Width = 190;
			// 
			// RootDirColumnHeader
			// 
			this.RootDirColumnHeader.Text = "Root Directory";
			this.RootDirColumnHeader.Width = 292;
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(736, 479);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(87, 27);
			this.OkBtn.TabIndex = 1;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(829, 479);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(87, 27);
			this.CancelBtn.TabIndex = 2;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			this.CancelBtn.Click += new System.EventHandler(this.CancelBtn_Click);
			// 
			// OnlyForThisComputer
			// 
			this.OnlyForThisComputer.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.OnlyForThisComputer.AutoSize = true;
			this.OnlyForThisComputer.Checked = true;
			this.OnlyForThisComputer.CheckState = System.Windows.Forms.CheckState.Checked;
			this.OnlyForThisComputer.Location = new System.Drawing.Point(15, 484);
			this.OnlyForThisComputer.Name = "OnlyForThisComputer";
			this.OnlyForThisComputer.Size = new System.Drawing.Size(241, 19);
			this.OnlyForThisComputer.TabIndex = 3;
			this.OnlyForThisComputer.Text = "Only show workspaces for this computer";
			this.OnlyForThisComputer.UseVisualStyleBackColor = true;
			this.OnlyForThisComputer.CheckedChanged += new System.EventHandler(this.OnlyForThisComputer_CheckedChanged);
			// 
			// SelectWorkspaceWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(933, 519);
			this.Controls.Add(this.OnlyForThisComputer);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.WorkspaceListView);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = global::UnrealGameSync.Properties.Resources.Icon;
			this.Name = "SelectWorkspaceWindow";
			this.ShowInTaskbar = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Select Workspace";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.ListView WorkspaceListView;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.ColumnHeader NameColumnHeader;
		private System.Windows.Forms.ColumnHeader StreamColumnHeader;
		private System.Windows.Forms.ColumnHeader RootDirColumnHeader;
		private System.Windows.Forms.CheckBox OnlyForThisComputer;
		private System.Windows.Forms.ColumnHeader HostColumnHeader;
	}
}
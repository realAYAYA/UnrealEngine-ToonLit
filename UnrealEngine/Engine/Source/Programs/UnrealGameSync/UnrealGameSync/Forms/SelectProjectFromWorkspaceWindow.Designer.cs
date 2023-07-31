namespace UnrealGameSync
{
	partial class SelectProjectFromWorkspaceWindow
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
			this.ProjectTreeView = new System.Windows.Forms.TreeView();
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.ShowProjectDirsFiles = new System.Windows.Forms.CheckBox();
			this.SuspendLayout();
			// 
			// ProjectTreeView
			// 
			this.ProjectTreeView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ProjectTreeView.HideSelection = false;
			this.ProjectTreeView.Location = new System.Drawing.Point(12, 12);
			this.ProjectTreeView.Name = "ProjectTreeView";
			this.ProjectTreeView.Size = new System.Drawing.Size(595, 432);
			this.ProjectTreeView.TabIndex = 0;
			this.ProjectTreeView.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.ProjectTreeView_AfterSelect);
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(427, 450);
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
			this.CancelBtn.Location = new System.Drawing.Point(520, 450);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(87, 27);
			this.CancelBtn.TabIndex = 2;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			this.CancelBtn.Click += new System.EventHandler(this.CancelBtn_Click);
			// 
			// ShowProjectDirsFiles
			// 
			this.ShowProjectDirsFiles.AutoSize = true;
			this.ShowProjectDirsFiles.Location = new System.Drawing.Point(12, 455);
			this.ShowProjectDirsFiles.Name = "ShowProjectDirsFiles";
			this.ShowProjectDirsFiles.Size = new System.Drawing.Size(153, 19);
			this.ShowProjectDirsFiles.TabIndex = 3;
			this.ShowProjectDirsFiles.Text = "Show *.uprojectdirs files";
			this.ShowProjectDirsFiles.UseVisualStyleBackColor = true;
			this.ShowProjectDirsFiles.CheckedChanged += new System.EventHandler(this.ShowProjectDirsFiles_CheckedChanged);
			// 
			// SelectProjectFromWorkspaceWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(619, 489);
			this.Controls.Add(this.ShowProjectDirsFiles);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.ProjectTreeView);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = global::UnrealGameSync.Properties.Resources.Icon;
			this.Name = "SelectProjectFromWorkspaceWindow";
			this.ShowInTaskbar = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Select Project";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TreeView ProjectTreeView;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.CheckBox ShowProjectDirsFiles;
	}
}
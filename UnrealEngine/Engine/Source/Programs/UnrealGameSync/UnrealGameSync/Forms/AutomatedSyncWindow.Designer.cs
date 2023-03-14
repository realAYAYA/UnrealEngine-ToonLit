namespace UnrealGameSync
{
	partial class AutomatedSyncWindow
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
			this.ProjectTextBox = new System.Windows.Forms.TextBox();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.groupBox2 = new System.Windows.Forms.GroupBox();
			this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			this.WorkspaceNameTextBox = new System.Windows.Forms.TextBox();
			this.WorkspaceNameNewBtn = new System.Windows.Forms.Button();
			this.WorkspaceNameBrowseBtn = new System.Windows.Forms.Button();
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.ChangeLink = new System.Windows.Forms.LinkLabel();
			this.ServerLabel = new System.Windows.Forms.Label();
			this.groupBox1.SuspendLayout();
			this.groupBox2.SuspendLayout();
			this.tableLayoutPanel2.SuspendLayout();
			this.SuspendLayout();
			// 
			// ProjectTextBox
			// 
			this.ProjectTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ProjectTextBox.Location = new System.Drawing.Point(20, 30);
			this.ProjectTextBox.Name = "ProjectTextBox";
			this.ProjectTextBox.ReadOnly = true;
			this.ProjectTextBox.Size = new System.Drawing.Size(787, 23);
			this.ProjectTextBox.TabIndex = 0;
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.ProjectTextBox);
			this.groupBox1.Font = new System.Drawing.Font("Segoe UI", 9F);
			this.groupBox1.Location = new System.Drawing.Point(18, 46);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(824, 73);
			this.groupBox1.TabIndex = 2;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Project";
			// 
			// groupBox2
			// 
			this.groupBox2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox2.Controls.Add(this.tableLayoutPanel2);
			this.groupBox2.Location = new System.Drawing.Point(18, 125);
			this.groupBox2.Name = "groupBox2";
			this.groupBox2.Size = new System.Drawing.Size(824, 71);
			this.groupBox2.TabIndex = 3;
			this.groupBox2.TabStop = false;
			this.groupBox2.Text = "Workspace";
			// 
			// tableLayoutPanel2
			// 
			this.tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel2.ColumnCount = 3;
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 20F));
			this.tableLayoutPanel2.Controls.Add(this.WorkspaceNameTextBox, 0, 0);
			this.tableLayoutPanel2.Controls.Add(this.WorkspaceNameNewBtn, 1, 0);
			this.tableLayoutPanel2.Controls.Add(this.WorkspaceNameBrowseBtn, 2, 0);
			this.tableLayoutPanel2.Location = new System.Drawing.Point(20, 19);
			this.tableLayoutPanel2.Name = "tableLayoutPanel2";
			this.tableLayoutPanel2.RowCount = 1;
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 36F));
			this.tableLayoutPanel2.Size = new System.Drawing.Size(787, 36);
			this.tableLayoutPanel2.TabIndex = 10;
			// 
			// WorkspaceNameTextBox
			// 
			this.WorkspaceNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNameTextBox.Location = new System.Drawing.Point(3, 6);
			this.WorkspaceNameTextBox.Name = "WorkspaceNameTextBox";
			this.WorkspaceNameTextBox.Size = new System.Drawing.Size(586, 23);
			this.WorkspaceNameTextBox.TabIndex = 2;
			// 
			// WorkspaceNameNewBtn
			// 
			this.WorkspaceNameNewBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNameNewBtn.Location = new System.Drawing.Point(595, 5);
			this.WorkspaceNameNewBtn.Name = "WorkspaceNameNewBtn";
			this.WorkspaceNameNewBtn.Size = new System.Drawing.Size(89, 26);
			this.WorkspaceNameNewBtn.TabIndex = 3;
			this.WorkspaceNameNewBtn.Text = "New...";
			this.WorkspaceNameNewBtn.UseVisualStyleBackColor = true;
			this.WorkspaceNameNewBtn.Click += new System.EventHandler(this.WorkspaceNameNewBtn_Click);
			// 
			// WorkspaceNameBrowseBtn
			// 
			this.WorkspaceNameBrowseBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNameBrowseBtn.Location = new System.Drawing.Point(690, 5);
			this.WorkspaceNameBrowseBtn.Name = "WorkspaceNameBrowseBtn";
			this.WorkspaceNameBrowseBtn.Size = new System.Drawing.Size(94, 26);
			this.WorkspaceNameBrowseBtn.TabIndex = 4;
			this.WorkspaceNameBrowseBtn.Text = "Browse...";
			this.WorkspaceNameBrowseBtn.UseVisualStyleBackColor = true;
			this.WorkspaceNameBrowseBtn.Click += new System.EventHandler(this.WorkspaceNameBrowseBtn_Click);
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(677, 205);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(84, 26);
			this.OkBtn.TabIndex = 4;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(767, 205);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(84, 26);
			this.CancelBtn.TabIndex = 5;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			// 
			// ChangeLink
			// 
			this.ChangeLink.AutoSize = true;
			this.ChangeLink.Location = new System.Drawing.Point(349, 18);
			this.ChangeLink.Name = "ChangeLink";
			this.ChangeLink.Size = new System.Drawing.Size(57, 15);
			this.ChangeLink.TabIndex = 8;
			this.ChangeLink.TabStop = true;
			this.ChangeLink.Text = "Change...";
			this.ChangeLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.ChangeLink_LinkClicked);
			// 
			// ServerLabel
			// 
			this.ServerLabel.AutoSize = true;
			this.ServerLabel.Location = new System.Drawing.Point(12, 18);
			this.ServerLabel.Name = "ServerLabel";
			this.ServerLabel.Size = new System.Drawing.Size(331, 15);
			this.ServerLabel.TabIndex = 7;
			this.ServerLabel.Text = "Using default Perforce connection (perforce:1666, Ben.Marsh)";
			// 
			// AutomatedSyncWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(863, 243);
			this.Controls.Add(this.ChangeLink);
			this.Controls.Add(this.ServerLabel);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.groupBox2);
			this.Controls.Add(this.groupBox1);
			this.Font = new System.Drawing.Font("Segoe UI", 9F);
			this.Icon = global::UnrealGameSync.Properties.Resources.Icon;
			this.Name = "AutomatedSyncWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Sync project";
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.groupBox2.ResumeLayout(false);
			this.tableLayoutPanel2.ResumeLayout(false);
			this.tableLayoutPanel2.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TextBox ProjectTextBox;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.LinkLabel ChangeLink;
		private System.Windows.Forms.Label ServerLabel;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.TextBox WorkspaceNameTextBox;
		private System.Windows.Forms.Button WorkspaceNameNewBtn;
		private System.Windows.Forms.Button WorkspaceNameBrowseBtn;
	}
}
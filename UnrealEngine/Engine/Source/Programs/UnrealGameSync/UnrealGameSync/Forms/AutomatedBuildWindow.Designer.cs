namespace UnrealGameSync.Forms
{
	partial class AutomatedBuildWindow
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
			this.ChangeLink = new System.Windows.Forms.LinkLabel();
			this.ServerLabel = new System.Windows.Forms.Label();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.OkBtn = new System.Windows.Forms.Button();
			this.groupBox2 = new System.Windows.Forms.GroupBox();
			this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			this.WorkspaceNameLabel = new System.Windows.Forms.Label();
			this.WorkspaceNameTextBox = new System.Windows.Forms.TextBox();
			this.WorkspaceNameNewBtn = new System.Windows.Forms.Button();
			this.WorkspaceNameBrowseBtn = new System.Windows.Forms.Button();
			this.WorkspacePathLabel = new System.Windows.Forms.Label();
			this.WorkspacePathTextBox = new System.Windows.Forms.TextBox();
			this.WorkspacePathBrowseBtn = new System.Windows.Forms.Button();
			this.groupBox3 = new System.Windows.Forms.GroupBox();
			this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
			this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			this.ExecCommandCheckBox = new System.Windows.Forms.CheckBox();
			this.ExecCommandTextBox = new System.Windows.Forms.TextBox();
			this.SyncToChangeCheckBox = new System.Windows.Forms.CheckBox();
			this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
			this.groupBox2.SuspendLayout();
			this.tableLayoutPanel2.SuspendLayout();
			this.groupBox3.SuspendLayout();
			this.tableLayoutPanel3.SuspendLayout();
			this.tableLayoutPanel1.SuspendLayout();
			this.flowLayoutPanel1.SuspendLayout();
			this.SuspendLayout();
			// 
			// ChangeLink
			// 
			this.ChangeLink.AutoSize = true;
			this.ChangeLink.Location = new System.Drawing.Point(340, 0);
			this.ChangeLink.Name = "ChangeLink";
			this.ChangeLink.Size = new System.Drawing.Size(57, 15);
			this.ChangeLink.TabIndex = 14;
			this.ChangeLink.TabStop = true;
			this.ChangeLink.Text = "Change...";
			this.ChangeLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.ChangeLink_LinkClicked);
			// 
			// ServerLabel
			// 
			this.ServerLabel.AutoSize = true;
			this.ServerLabel.Location = new System.Drawing.Point(3, 0);
			this.ServerLabel.Name = "ServerLabel";
			this.ServerLabel.Size = new System.Drawing.Size(331, 15);
			this.ServerLabel.TabIndex = 13;
			this.ServerLabel.Text = "Using default Perforce connection (perforce:1666, Ben.Marsh)";
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(667, 264);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(84, 26);
			this.CancelBtn.TabIndex = 12;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(577, 264);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(84, 26);
			this.OkBtn.TabIndex = 11;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// groupBox2
			// 
			this.groupBox2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox2.Controls.Add(this.tableLayoutPanel2);
			this.groupBox2.Location = new System.Drawing.Point(20, 48);
			this.groupBox2.Name = "groupBox2";
			this.groupBox2.Size = new System.Drawing.Size(719, 103);
			this.groupBox2.TabIndex = 10;
			this.groupBox2.TabStop = false;
			this.groupBox2.Text = "Workspace";
			// 
			// tableLayoutPanel2
			// 
			this.tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel2.ColumnCount = 4;
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 80F));
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel2.Controls.Add(this.WorkspaceNameLabel, 0, 0);
			this.tableLayoutPanel2.Controls.Add(this.WorkspaceNameTextBox, 1, 0);
			this.tableLayoutPanel2.Controls.Add(this.WorkspaceNameNewBtn, 2, 0);
			this.tableLayoutPanel2.Controls.Add(this.WorkspaceNameBrowseBtn, 3, 0);
			this.tableLayoutPanel2.Controls.Add(this.WorkspacePathLabel, 0, 1);
			this.tableLayoutPanel2.Controls.Add(this.WorkspacePathTextBox, 1, 1);
			this.tableLayoutPanel2.Controls.Add(this.WorkspacePathBrowseBtn, 3, 1);
			this.tableLayoutPanel2.Location = new System.Drawing.Point(17, 23);
			this.tableLayoutPanel2.Name = "tableLayoutPanel2";
			this.tableLayoutPanel2.RowCount = 2;
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel2.Size = new System.Drawing.Size(682, 65);
			this.tableLayoutPanel2.TabIndex = 10;
			// 
			// WorkspaceNameLabel
			// 
			this.WorkspaceNameLabel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNameLabel.AutoSize = true;
			this.WorkspaceNameLabel.Location = new System.Drawing.Point(3, 8);
			this.WorkspaceNameLabel.Name = "WorkspaceNameLabel";
			this.WorkspaceNameLabel.Size = new System.Drawing.Size(74, 15);
			this.WorkspaceNameLabel.TabIndex = 1;
			this.WorkspaceNameLabel.Text = "Name:";
			// 
			// WorkspaceNameTextBox
			// 
			this.WorkspaceNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNameTextBox.Location = new System.Drawing.Point(83, 4);
			this.WorkspaceNameTextBox.Name = "WorkspaceNameTextBox";
			this.WorkspaceNameTextBox.Size = new System.Drawing.Size(401, 23);
			this.WorkspaceNameTextBox.TabIndex = 2;
			this.WorkspaceNameTextBox.TextChanged += new System.EventHandler(this.WorkspaceNameTextBox_TextChanged);
			// 
			// WorkspaceNameNewBtn
			// 
			this.WorkspaceNameNewBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNameNewBtn.Location = new System.Drawing.Point(490, 3);
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
			this.WorkspaceNameBrowseBtn.Location = new System.Drawing.Point(585, 3);
			this.WorkspaceNameBrowseBtn.Name = "WorkspaceNameBrowseBtn";
			this.WorkspaceNameBrowseBtn.Size = new System.Drawing.Size(94, 26);
			this.WorkspaceNameBrowseBtn.TabIndex = 4;
			this.WorkspaceNameBrowseBtn.Text = "Browse...";
			this.WorkspaceNameBrowseBtn.UseVisualStyleBackColor = true;
			this.WorkspaceNameBrowseBtn.Click += new System.EventHandler(this.WorkspaceNameBrowseBtn_Click);
			// 
			// WorkspacePathLabel
			// 
			this.WorkspacePathLabel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspacePathLabel.AutoSize = true;
			this.WorkspacePathLabel.Location = new System.Drawing.Point(3, 41);
			this.WorkspacePathLabel.Name = "WorkspacePathLabel";
			this.WorkspacePathLabel.Size = new System.Drawing.Size(74, 15);
			this.WorkspacePathLabel.TabIndex = 5;
			this.WorkspacePathLabel.Text = "Path:";
			// 
			// WorkspacePathTextBox
			// 
			this.WorkspacePathTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel2.SetColumnSpan(this.WorkspacePathTextBox, 2);
			this.WorkspacePathTextBox.Location = new System.Drawing.Point(83, 37);
			this.WorkspacePathTextBox.Name = "WorkspacePathTextBox";
			this.WorkspacePathTextBox.Size = new System.Drawing.Size(496, 23);
			this.WorkspacePathTextBox.TabIndex = 6;
			// 
			// WorkspacePathBrowseBtn
			// 
			this.WorkspacePathBrowseBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspacePathBrowseBtn.Location = new System.Drawing.Point(585, 35);
			this.WorkspacePathBrowseBtn.Name = "WorkspacePathBrowseBtn";
			this.WorkspacePathBrowseBtn.Size = new System.Drawing.Size(94, 27);
			this.WorkspacePathBrowseBtn.TabIndex = 7;
			this.WorkspacePathBrowseBtn.Text = "Browse...";
			this.WorkspacePathBrowseBtn.UseVisualStyleBackColor = true;
			this.WorkspacePathBrowseBtn.Click += new System.EventHandler(this.WorkspacePathBrowseBtn_Click);
			// 
			// groupBox3
			// 
			this.groupBox3.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox3.Controls.Add(this.tableLayoutPanel3);
			this.groupBox3.Location = new System.Drawing.Point(20, 157);
			this.groupBox3.Name = "groupBox3";
			this.groupBox3.Size = new System.Drawing.Size(719, 92);
			this.groupBox3.TabIndex = 15;
			this.groupBox3.TabStop = false;
			this.groupBox3.Text = "Actions";
			// 
			// tableLayoutPanel3
			// 
			this.tableLayoutPanel3.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel3.ColumnCount = 1;
			this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel3.Controls.Add(this.tableLayoutPanel1, 0, 1);
			this.tableLayoutPanel3.Controls.Add(this.SyncToChangeCheckBox, 0, 0);
			this.tableLayoutPanel3.Location = new System.Drawing.Point(17, 21);
			this.tableLayoutPanel3.Name = "tableLayoutPanel3";
			this.tableLayoutPanel3.RowCount = 2;
			this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel3.Size = new System.Drawing.Size(682, 59);
			this.tableLayoutPanel3.TabIndex = 10;
			// 
			// tableLayoutPanel1
			// 
			this.tableLayoutPanel1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel1.ColumnCount = 2;
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Controls.Add(this.ExecCommandCheckBox, 0, 0);
			this.tableLayoutPanel1.Controls.Add(this.ExecCommandTextBox, 1, 0);
			this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 29);
			this.tableLayoutPanel1.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel1.Name = "tableLayoutPanel1";
			this.tableLayoutPanel1.RowCount = 1;
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Size = new System.Drawing.Size(682, 30);
			this.tableLayoutPanel1.TabIndex = 10;
			// 
			// ExecCommandCheckBox
			// 
			this.ExecCommandCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.ExecCommandCheckBox.AutoSize = true;
			this.ExecCommandCheckBox.Checked = true;
			this.ExecCommandCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.ExecCommandCheckBox.Location = new System.Drawing.Point(3, 5);
			this.ExecCommandCheckBox.Name = "ExecCommandCheckBox";
			this.ExecCommandCheckBox.Size = new System.Drawing.Size(108, 19);
			this.ExecCommandCheckBox.TabIndex = 0;
			this.ExecCommandCheckBox.Text = "Run command:";
			this.ExecCommandCheckBox.UseVisualStyleBackColor = true;
			this.ExecCommandCheckBox.CheckedChanged += new System.EventHandler(this.ExecCommandCheckBox_CheckedChanged);
			// 
			// ExecCommandTextBox
			// 
			this.ExecCommandTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.ExecCommandTextBox.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.ExecCommandTextBox.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.ExecCommandTextBox.Location = new System.Drawing.Point(117, 4);
			this.ExecCommandTextBox.Name = "ExecCommandTextBox";
			this.ExecCommandTextBox.Size = new System.Drawing.Size(562, 22);
			this.ExecCommandTextBox.TabIndex = 1;
			this.ExecCommandTextBox.Text = "RunUAT.bat -Script=Foo -Target=\"Build Something\"";
			// 
			// SyncToChangeCheckBox
			// 
			this.SyncToChangeCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.SyncToChangeCheckBox.AutoSize = true;
			this.SyncToChangeCheckBox.Checked = true;
			this.SyncToChangeCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.SyncToChangeCheckBox.Location = new System.Drawing.Point(3, 5);
			this.SyncToChangeCheckBox.Name = "SyncToChangeCheckBox";
			this.SyncToChangeCheckBox.Size = new System.Drawing.Size(149, 19);
			this.SyncToChangeCheckBox.TabIndex = 0;
			this.SyncToChangeCheckBox.Text = "Sync to changelist 1234";
			this.SyncToChangeCheckBox.UseVisualStyleBackColor = true;
			// 
			// flowLayoutPanel1
			// 
			this.flowLayoutPanel1.Controls.Add(this.ServerLabel);
			this.flowLayoutPanel1.Controls.Add(this.ChangeLink);
			this.flowLayoutPanel1.Location = new System.Drawing.Point(20, 15);
			this.flowLayoutPanel1.Name = "flowLayoutPanel1";
			this.flowLayoutPanel1.Size = new System.Drawing.Size(719, 27);
			this.flowLayoutPanel1.TabIndex = 16;
			this.flowLayoutPanel1.WrapContents = false;
			// 
			// AutomatedBuildWindow
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.ClientSize = new System.Drawing.Size(763, 302);
			this.Controls.Add(this.flowLayoutPanel1);
			this.Controls.Add(this.groupBox3);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.groupBox2);
			this.Font = new System.Drawing.Font("Segoe UI", 9F);
			this.Icon = global::UnrealGameSync.Properties.Resources.Icon;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "AutomatedBuildWindow";
			this.ShowInTaskbar = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Build Locally";
			this.groupBox2.ResumeLayout(false);
			this.tableLayoutPanel2.ResumeLayout(false);
			this.tableLayoutPanel2.PerformLayout();
			this.groupBox3.ResumeLayout(false);
			this.tableLayoutPanel3.ResumeLayout(false);
			this.tableLayoutPanel3.PerformLayout();
			this.tableLayoutPanel1.ResumeLayout(false);
			this.tableLayoutPanel1.PerformLayout();
			this.flowLayoutPanel1.ResumeLayout(false);
			this.flowLayoutPanel1.PerformLayout();
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.LinkLabel ChangeLink;
		private System.Windows.Forms.Label ServerLabel;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.GroupBox groupBox3;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.CheckBox ExecCommandCheckBox;
		private System.Windows.Forms.TextBox ExecCommandTextBox;
		private System.Windows.Forms.CheckBox SyncToChangeCheckBox;
		private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.Label WorkspaceNameLabel;
		private System.Windows.Forms.TextBox WorkspaceNameTextBox;
		private System.Windows.Forms.Button WorkspaceNameNewBtn;
		private System.Windows.Forms.Button WorkspaceNameBrowseBtn;
		private System.Windows.Forms.Label WorkspacePathLabel;
		private System.Windows.Forms.TextBox WorkspacePathTextBox;
		private System.Windows.Forms.Button WorkspacePathBrowseBtn;
	}
}
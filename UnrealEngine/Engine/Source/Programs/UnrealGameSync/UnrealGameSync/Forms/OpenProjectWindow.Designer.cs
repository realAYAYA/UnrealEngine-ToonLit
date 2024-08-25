namespace UnrealGameSync
{
	partial class OpenProjectWindow
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
			components = new System.ComponentModel.Container();
			WorkspaceNameLabel = new System.Windows.Forms.Label();
			WorkspacePathLabel = new System.Windows.Forms.Label();
			groupBox2 = new System.Windows.Forms.GroupBox();
			tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			WorkspaceNameTextBox = new System.Windows.Forms.TextBox();
			WorkspaceNameNewBtn = new System.Windows.Forms.Button();
			WorkspaceNameBrowseBtn = new System.Windows.Forms.Button();
			WorkspacePathTextBox = new System.Windows.Forms.TextBox();
			WorkspacePathBrowseBtn = new System.Windows.Forms.Button();
			WorkspaceRadioBtn = new System.Windows.Forms.RadioButton();
			ServerLabel = new System.Windows.Forms.Label();
			ChangeLink = new System.Windows.Forms.LinkLabel();
			CancelBtn = new System.Windows.Forms.Button();
			OkBtn = new System.Windows.Forms.Button();
			groupBox1 = new System.Windows.Forms.GroupBox();
			tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			LocalFileBrowseBtn = new System.Windows.Forms.Button();
			LocalFileLabel = new System.Windows.Forms.Label();
			LocalFileTextBox = new System.Windows.Forms.TextBox();
			LocalFileRadioBtn = new System.Windows.Forms.RadioButton();
			flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
			GenerateP4ConfigCheckbox = new System.Windows.Forms.CheckBox();
			GenerateP4ConfigToolTip = new System.Windows.Forms.ToolTip(components);
			groupBox2.SuspendLayout();
			tableLayoutPanel2.SuspendLayout();
			groupBox1.SuspendLayout();
			tableLayoutPanel1.SuspendLayout();
			flowLayoutPanel1.SuspendLayout();
			SuspendLayout();
			// 
			// WorkspaceNameLabel
			// 
			WorkspaceNameLabel.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			WorkspaceNameLabel.AutoSize = true;
			WorkspaceNameLabel.Location = new System.Drawing.Point(3, 8);
			WorkspaceNameLabel.Name = "WorkspaceNameLabel";
			WorkspaceNameLabel.Size = new System.Drawing.Size(74, 15);
			WorkspaceNameLabel.TabIndex = 1;
			WorkspaceNameLabel.Text = "Name:";
			// 
			// WorkspacePathLabel
			// 
			WorkspacePathLabel.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			WorkspacePathLabel.AutoSize = true;
			WorkspacePathLabel.Location = new System.Drawing.Point(3, 41);
			WorkspacePathLabel.Name = "WorkspacePathLabel";
			WorkspacePathLabel.Size = new System.Drawing.Size(74, 15);
			WorkspacePathLabel.TabIndex = 5;
			WorkspacePathLabel.Text = "Path:";
			// 
			// groupBox2
			// 
			groupBox2.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			groupBox2.Controls.Add(tableLayoutPanel2);
			groupBox2.Location = new System.Drawing.Point(22, 142);
			groupBox2.Name = "groupBox2";
			groupBox2.Size = new System.Drawing.Size(851, 110);
			groupBox2.TabIndex = 1;
			groupBox2.TabStop = false;
			// 
			// tableLayoutPanel2
			// 
			tableLayoutPanel2.ColumnCount = 4;
			tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 80F));
			tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			tableLayoutPanel2.Controls.Add(WorkspaceNameLabel, 0, 0);
			tableLayoutPanel2.Controls.Add(WorkspaceNameTextBox, 1, 0);
			tableLayoutPanel2.Controls.Add(WorkspaceNameNewBtn, 2, 0);
			tableLayoutPanel2.Controls.Add(WorkspaceNameBrowseBtn, 3, 0);
			tableLayoutPanel2.Controls.Add(WorkspacePathLabel, 0, 1);
			tableLayoutPanel2.Controls.Add(WorkspacePathTextBox, 1, 1);
			tableLayoutPanel2.Controls.Add(WorkspacePathBrowseBtn, 3, 1);
			tableLayoutPanel2.Location = new System.Drawing.Point(18, 26);
			tableLayoutPanel2.Name = "tableLayoutPanel2";
			tableLayoutPanel2.RowCount = 2;
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel2.Size = new System.Drawing.Size(812, 65);
			tableLayoutPanel2.TabIndex = 9;
			// 
			// WorkspaceNameTextBox
			// 
			WorkspaceNameTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			WorkspaceNameTextBox.Location = new System.Drawing.Point(83, 4);
			WorkspaceNameTextBox.Name = "WorkspaceNameTextBox";
			WorkspaceNameTextBox.Size = new System.Drawing.Size(531, 23);
			WorkspaceNameTextBox.TabIndex = 2;
			WorkspaceNameTextBox.TextChanged += WorkspaceNameTextBox_TextChanged;
			WorkspaceNameTextBox.Enter += WorkspaceNameTextBox_Enter;
			// 
			// WorkspaceNameNewBtn
			// 
			WorkspaceNameNewBtn.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			WorkspaceNameNewBtn.Location = new System.Drawing.Point(620, 3);
			WorkspaceNameNewBtn.Name = "WorkspaceNameNewBtn";
			WorkspaceNameNewBtn.Size = new System.Drawing.Size(89, 26);
			WorkspaceNameNewBtn.TabIndex = 3;
			WorkspaceNameNewBtn.Text = "New...";
			WorkspaceNameNewBtn.UseVisualStyleBackColor = true;
			WorkspaceNameNewBtn.Click += WorkspaceNewBtn_Click;
			// 
			// WorkspaceNameBrowseBtn
			// 
			WorkspaceNameBrowseBtn.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			WorkspaceNameBrowseBtn.Location = new System.Drawing.Point(715, 3);
			WorkspaceNameBrowseBtn.Name = "WorkspaceNameBrowseBtn";
			WorkspaceNameBrowseBtn.Size = new System.Drawing.Size(94, 26);
			WorkspaceNameBrowseBtn.TabIndex = 4;
			WorkspaceNameBrowseBtn.Text = "Browse...";
			WorkspaceNameBrowseBtn.UseVisualStyleBackColor = true;
			WorkspaceNameBrowseBtn.Click += WorkspaceBrowseBtn_Click;
			// 
			// WorkspacePathTextBox
			// 
			WorkspacePathTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			tableLayoutPanel2.SetColumnSpan(WorkspacePathTextBox, 2);
			WorkspacePathTextBox.Location = new System.Drawing.Point(83, 37);
			WorkspacePathTextBox.Name = "WorkspacePathTextBox";
			WorkspacePathTextBox.Size = new System.Drawing.Size(626, 23);
			WorkspacePathTextBox.TabIndex = 6;
			WorkspacePathTextBox.TextChanged += WorkspacePathTextBox_TextChanged;
			WorkspacePathTextBox.Enter += WorkspacePathTextBox_Enter;
			// 
			// WorkspacePathBrowseBtn
			// 
			WorkspacePathBrowseBtn.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			WorkspacePathBrowseBtn.Location = new System.Drawing.Point(715, 35);
			WorkspacePathBrowseBtn.Name = "WorkspacePathBrowseBtn";
			WorkspacePathBrowseBtn.Size = new System.Drawing.Size(94, 27);
			WorkspacePathBrowseBtn.TabIndex = 7;
			WorkspacePathBrowseBtn.Text = "Browse...";
			WorkspacePathBrowseBtn.UseVisualStyleBackColor = true;
			WorkspacePathBrowseBtn.Click += WorkspacePathBrowseBtn_Click;
			// 
			// WorkspaceRadioBtn
			// 
			WorkspaceRadioBtn.AutoSize = true;
			WorkspaceRadioBtn.Location = new System.Drawing.Point(39, 140);
			WorkspaceRadioBtn.Name = "WorkspaceRadioBtn";
			WorkspaceRadioBtn.Size = new System.Drawing.Size(83, 19);
			WorkspaceRadioBtn.TabIndex = 8;
			WorkspaceRadioBtn.Text = "Workspace";
			WorkspaceRadioBtn.UseVisualStyleBackColor = true;
			WorkspaceRadioBtn.CheckedChanged += WorkspaceRadioBtn_CheckedChanged;
			// 
			// ServerLabel
			// 
			ServerLabel.AutoSize = true;
			ServerLabel.Location = new System.Drawing.Point(3, 0);
			ServerLabel.Margin = new System.Windows.Forms.Padding(3, 0, 0, 0);
			ServerLabel.Name = "ServerLabel";
			ServerLabel.Size = new System.Drawing.Size(331, 15);
			ServerLabel.TabIndex = 5;
			ServerLabel.Text = "Using default Perforce connection (perforce:1666, Ben.Marsh)";
			// 
			// ChangeLink
			// 
			ChangeLink.AutoSize = true;
			ChangeLink.Location = new System.Drawing.Point(334, 0);
			ChangeLink.Margin = new System.Windows.Forms.Padding(0, 0, 3, 0);
			ChangeLink.Name = "ChangeLink";
			ChangeLink.Size = new System.Drawing.Size(57, 15);
			ChangeLink.TabIndex = 6;
			ChangeLink.TabStop = true;
			ChangeLink.Text = "Change...";
			ChangeLink.LinkClicked += ChangeLink_LinkClicked;
			// 
			// CancelBtn
			// 
			CancelBtn.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right;
			CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			CancelBtn.Location = new System.Drawing.Point(776, 266);
			CancelBtn.Name = "CancelBtn";
			CancelBtn.Size = new System.Drawing.Size(98, 27);
			CancelBtn.TabIndex = 3;
			CancelBtn.Text = "Cancel";
			CancelBtn.UseVisualStyleBackColor = true;
			// 
			// OkBtn
			// 
			OkBtn.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right;
			OkBtn.Location = new System.Drawing.Point(672, 266);
			OkBtn.Name = "OkBtn";
			OkBtn.Size = new System.Drawing.Size(97, 27);
			OkBtn.TabIndex = 4;
			OkBtn.Text = "Ok";
			OkBtn.UseVisualStyleBackColor = true;
			OkBtn.Click += OkBtn_Click;
			// 
			// groupBox1
			// 
			groupBox1.Controls.Add(tableLayoutPanel1);
			groupBox1.Location = new System.Drawing.Point(22, 52);
			groupBox1.Name = "groupBox1";
			groupBox1.Size = new System.Drawing.Size(851, 84);
			groupBox1.TabIndex = 8;
			groupBox1.TabStop = false;
			// 
			// tableLayoutPanel1
			// 
			tableLayoutPanel1.ColumnCount = 3;
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 80F));
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			tableLayoutPanel1.Controls.Add(LocalFileBrowseBtn, 2, 0);
			tableLayoutPanel1.Controls.Add(LocalFileLabel, 0, 0);
			tableLayoutPanel1.Controls.Add(LocalFileTextBox, 1, 0);
			tableLayoutPanel1.Location = new System.Drawing.Point(17, 30);
			tableLayoutPanel1.Name = "tableLayoutPanel1";
			tableLayoutPanel1.RowCount = 1;
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel1.Size = new System.Drawing.Size(813, 35);
			tableLayoutPanel1.TabIndex = 9;
			// 
			// LocalFileBrowseBtn
			// 
			LocalFileBrowseBtn.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			LocalFileBrowseBtn.Location = new System.Drawing.Point(716, 4);
			LocalFileBrowseBtn.Name = "LocalFileBrowseBtn";
			LocalFileBrowseBtn.Size = new System.Drawing.Size(94, 27);
			LocalFileBrowseBtn.TabIndex = 10;
			LocalFileBrowseBtn.Text = "Browse...";
			LocalFileBrowseBtn.UseVisualStyleBackColor = true;
			LocalFileBrowseBtn.Click += LocalFileBrowseBtn_Click;
			// 
			// LocalFileLabel
			// 
			LocalFileLabel.Anchor = System.Windows.Forms.AnchorStyles.Left;
			LocalFileLabel.AutoSize = true;
			LocalFileLabel.Location = new System.Drawing.Point(3, 10);
			LocalFileLabel.Name = "LocalFileLabel";
			LocalFileLabel.Size = new System.Drawing.Size(28, 15);
			LocalFileLabel.TabIndex = 8;
			LocalFileLabel.Text = "File:";
			// 
			// LocalFileTextBox
			// 
			LocalFileTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			LocalFileTextBox.Location = new System.Drawing.Point(83, 6);
			LocalFileTextBox.Name = "LocalFileTextBox";
			LocalFileTextBox.Size = new System.Drawing.Size(627, 23);
			LocalFileTextBox.TabIndex = 9;
			LocalFileTextBox.TextChanged += LocalFileTextBox_TextChanged;
			LocalFileTextBox.Enter += LocalFileTextBox_Enter;
			// 
			// LocalFileRadioBtn
			// 
			LocalFileRadioBtn.AutoSize = true;
			LocalFileRadioBtn.Location = new System.Drawing.Point(40, 50);
			LocalFileRadioBtn.Name = "LocalFileRadioBtn";
			LocalFileRadioBtn.Size = new System.Drawing.Size(74, 19);
			LocalFileRadioBtn.TabIndex = 0;
			LocalFileRadioBtn.Text = "Local File";
			LocalFileRadioBtn.UseVisualStyleBackColor = true;
			LocalFileRadioBtn.CheckedChanged += LocalFileRadioBtn_CheckedChanged;
			// 
			// flowLayoutPanel1
			// 
			flowLayoutPanel1.Controls.Add(ServerLabel);
			flowLayoutPanel1.Controls.Add(ChangeLink);
			flowLayoutPanel1.Location = new System.Drawing.Point(22, 19);
			flowLayoutPanel1.Name = "flowLayoutPanel1";
			flowLayoutPanel1.Size = new System.Drawing.Size(851, 25);
			flowLayoutPanel1.TabIndex = 9;
			// 
			// GenerateP4ConfigCheckbox
			// 
			GenerateP4ConfigCheckbox.AutoSize = true;
			GenerateP4ConfigCheckbox.Checked = true;
			GenerateP4ConfigCheckbox.CheckState = System.Windows.Forms.CheckState.Checked;
			GenerateP4ConfigCheckbox.Location = new System.Drawing.Point(22, 264);
			GenerateP4ConfigCheckbox.Name = "GenerateP4ConfigCheckbox";
			GenerateP4ConfigCheckbox.Size = new System.Drawing.Size(144, 19);
			GenerateP4ConfigCheckbox.TabIndex = 10;
			GenerateP4ConfigCheckbox.Text = "Generate P4Config file";
			GenerateP4ConfigToolTip.SetToolTip(GenerateP4ConfigCheckbox, "When enabled UGS will generate a P4CONFIG file at the root of the project's workspace contianing the current perforce connection settings");
			GenerateP4ConfigCheckbox.UseVisualStyleBackColor = true;
			// 
			// OpenProjectWindow
			// 
			AcceptButton = OkBtn;
			AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			CancelButton = CancelBtn;
			ClientSize = new System.Drawing.Size(893, 305);
			Controls.Add(GenerateP4ConfigCheckbox);
			Controls.Add(flowLayoutPanel1);
			Controls.Add(LocalFileRadioBtn);
			Controls.Add(WorkspaceRadioBtn);
			Controls.Add(groupBox1);
			Controls.Add(OkBtn);
			Controls.Add(CancelBtn);
			Controls.Add(groupBox2);
			FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			Icon = Properties.Resources.Icon;
			Name = "OpenProjectWindow";
			ShowInTaskbar = false;
			StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			Text = "Open Project";
			groupBox2.ResumeLayout(false);
			tableLayoutPanel2.ResumeLayout(false);
			tableLayoutPanel2.PerformLayout();
			groupBox1.ResumeLayout(false);
			tableLayoutPanel1.ResumeLayout(false);
			tableLayoutPanel1.PerformLayout();
			flowLayoutPanel1.ResumeLayout(false);
			flowLayoutPanel1.PerformLayout();
			ResumeLayout(false);
			PerformLayout();
		}

		#endregion
		private System.Windows.Forms.Label WorkspaceNameLabel;
		private System.Windows.Forms.Label WorkspacePathLabel;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.Button WorkspacePathBrowseBtn;
		private System.Windows.Forms.TextBox WorkspacePathTextBox;
		private System.Windows.Forms.TextBox WorkspaceNameTextBox;
		private System.Windows.Forms.Label ServerLabel;
		private System.Windows.Forms.LinkLabel ChangeLink;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.RadioButton WorkspaceRadioBtn;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.RadioButton LocalFileRadioBtn;
		private System.Windows.Forms.Button LocalFileBrowseBtn;
		private System.Windows.Forms.TextBox LocalFileTextBox;
		private System.Windows.Forms.Label LocalFileLabel;
		private System.Windows.Forms.Button WorkspaceNameBrowseBtn;
		private System.Windows.Forms.Button WorkspaceNameNewBtn;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
		private System.Windows.Forms.CheckBox GenerateP4ConfigCheckbox;
		private System.Windows.Forms.ToolTip GenerateP4ConfigToolTip;
	}
}
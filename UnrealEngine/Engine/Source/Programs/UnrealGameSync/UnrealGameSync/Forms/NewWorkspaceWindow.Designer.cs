namespace UnrealGameSync
{
	partial class NewWorkspaceWindow
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
			this.StreamTextBox = new System.Windows.Forms.TextBox();
			this.RootDirTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.label3 = new System.Windows.Forms.Label();
			this.RootDirBrowseBtn = new System.Windows.Forms.Button();
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.StreamBrowseBtn = new System.Windows.Forms.Button();
			this.NameTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.groupBox1.SuspendLayout();
			this.SuspendLayout();
			// 
			// StreamTextBox
			// 
			this.StreamTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.StreamTextBox.Location = new System.Drawing.Point(125, 33);
			this.StreamTextBox.Name = "StreamTextBox";
			this.StreamTextBox.Size = new System.Drawing.Size(549, 23);
			this.StreamTextBox.TabIndex = 1;
			this.StreamTextBox.TextChanged += new System.EventHandler(this.StreamTextBox_TextChanged);
			// 
			// RootDirTextBox
			// 
			this.RootDirTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.RootDirTextBox.CueBanner = null;
			this.RootDirTextBox.Location = new System.Drawing.Point(125, 65);
			this.RootDirTextBox.Name = "RootDirTextBox";
			this.RootDirTextBox.Size = new System.Drawing.Size(549, 23);
			this.RootDirTextBox.TabIndex = 4;
			this.RootDirTextBox.TextChanged += new System.EventHandler(this.RootDirTextBox_TextChanged);
			this.RootDirTextBox.Enter += new System.EventHandler(this.RootDirTextBox_Enter);
			this.RootDirTextBox.Leave += new System.EventHandler(this.RootDirTextBox_Leave);
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(24, 100);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(42, 15);
			this.label1.TabIndex = 6;
			this.label1.Text = "Name:";
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(24, 36);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(47, 15);
			this.label2.TabIndex = 0;
			this.label2.Text = "Stream:";
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(24, 68);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(86, 15);
			this.label3.TabIndex = 3;
			this.label3.Text = "Root Directory:";
			// 
			// RootDirBrowseBtn
			// 
			this.RootDirBrowseBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.RootDirBrowseBtn.Location = new System.Drawing.Point(680, 64);
			this.RootDirBrowseBtn.Name = "RootDirBrowseBtn";
			this.RootDirBrowseBtn.Size = new System.Drawing.Size(91, 25);
			this.RootDirBrowseBtn.TabIndex = 5;
			this.RootDirBrowseBtn.Text = "Browse...";
			this.RootDirBrowseBtn.UseVisualStyleBackColor = true;
			this.RootDirBrowseBtn.Click += new System.EventHandler(this.RootDirBrowseBtn_Click);
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(642, 175);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(85, 27);
			this.OkBtn.TabIndex = 1;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(733, 175);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(85, 27);
			this.CancelBtn.TabIndex = 2;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			this.CancelBtn.Click += new System.EventHandler(this.CancelBtn_Click);
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.StreamBrowseBtn);
			this.groupBox1.Controls.Add(this.NameTextBox);
			this.groupBox1.Controls.Add(this.StreamTextBox);
			this.groupBox1.Controls.Add(this.RootDirTextBox);
			this.groupBox1.Controls.Add(this.RootDirBrowseBtn);
			this.groupBox1.Controls.Add(this.label1);
			this.groupBox1.Controls.Add(this.label3);
			this.groupBox1.Controls.Add(this.label2);
			this.groupBox1.Location = new System.Drawing.Point(15, 12);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(802, 150);
			this.groupBox1.TabIndex = 0;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Settings";
			// 
			// StreamBrowseBtn
			// 
			this.StreamBrowseBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.StreamBrowseBtn.Location = new System.Drawing.Point(680, 31);
			this.StreamBrowseBtn.Name = "StreamBrowseBtn";
			this.StreamBrowseBtn.Size = new System.Drawing.Size(91, 25);
			this.StreamBrowseBtn.TabIndex = 2;
			this.StreamBrowseBtn.Text = "Browse...";
			this.StreamBrowseBtn.UseVisualStyleBackColor = true;
			this.StreamBrowseBtn.Click += new System.EventHandler(this.StreamBrowseBtn_Click);
			// 
			// NameTextBox
			// 
			this.NameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.NameTextBox.CueBanner = "";
			this.NameTextBox.Location = new System.Drawing.Point(125, 97);
			this.NameTextBox.Name = "NameTextBox";
			this.NameTextBox.Size = new System.Drawing.Size(646, 23);
			this.NameTextBox.TabIndex = 7;
			this.NameTextBox.TextChanged += new System.EventHandler(this.NameTextBox_TextChanged);
			this.NameTextBox.Enter += new System.EventHandler(this.NameTextBox_Enter);
			this.NameTextBox.Leave += new System.EventHandler(this.NameTextBox_Leave);
			// 
			// NewWorkspaceWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.AutoSize = true;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(830, 214);
			this.Controls.Add(this.groupBox1);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = global::UnrealGameSync.Properties.Resources.Icon;
			this.Name = "NewWorkspaceWindow";
			this.ShowInTaskbar = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "New Workspace";
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.ResumeLayout(false);

		}

		#endregion

		private TextBoxWithCueBanner NameTextBox;
		private System.Windows.Forms.TextBox StreamTextBox;
		private TextBoxWithCueBanner RootDirTextBox;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.Button RootDirBrowseBtn;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.Button StreamBrowseBtn;
	}
}
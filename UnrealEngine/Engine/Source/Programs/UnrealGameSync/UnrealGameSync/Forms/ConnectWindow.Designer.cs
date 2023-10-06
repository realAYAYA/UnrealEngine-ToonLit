namespace UnrealGameSync
{
	partial class ConnectWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ConnectWindow));
			this.UserNameLabel = new System.Windows.Forms.Label();
			this.UserNameTextBox = new TextBoxWithCueBanner();
			this.UseDefaultConnectionSettings = new System.Windows.Forms.CheckBox();
			this.ServerAndPortTextBox = new TextBoxWithCueBanner();
			this.ServerAndPortLabel = new System.Windows.Forms.Label();
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.BrowseUserBtn = new System.Windows.Forms.Button();
			this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			this.tableLayoutPanel1.SuspendLayout();
			this.tableLayoutPanel2.SuspendLayout();
			this.SuspendLayout();
			// 
			// UserNameLabel
			// 
			this.UserNameLabel.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.UserNameLabel.AutoSize = true;
			this.UserNameLabel.Location = new System.Drawing.Point(3, 42);
			this.UserNameLabel.Name = "UserNameLabel";
			this.UserNameLabel.Padding = new System.Windows.Forms.Padding(0, 0, 15, 0);
			this.UserNameLabel.Size = new System.Drawing.Size(81, 15);
			this.UserNameLabel.TabIndex = 3;
			this.UserNameLabel.Text = "User name:";
			// 
			// UserNameTextBox
			// 
			this.UserNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.UserNameTextBox.Location = new System.Drawing.Point(3, 5);
			this.UserNameTextBox.Name = "UserNameTextBox";
			this.UserNameTextBox.Size = new System.Drawing.Size(364, 23);
			this.UserNameTextBox.TabIndex = 4;
			// 
			// UseDefaultConnectionSettings
			// 
			this.UseDefaultConnectionSettings.AutoSize = true;
			this.UseDefaultConnectionSettings.Location = new System.Drawing.Point(20, 16);
			this.UseDefaultConnectionSettings.Name = "UseDefaultConnectionSettings";
			this.UseDefaultConnectionSettings.Size = new System.Drawing.Size(192, 19);
			this.UseDefaultConnectionSettings.TabIndex = 0;
			this.UseDefaultConnectionSettings.Text = "Use default connection settings";
			this.UseDefaultConnectionSettings.UseVisualStyleBackColor = true;
			this.UseDefaultConnectionSettings.CheckedChanged += new System.EventHandler(this.UseCustomSettings_CheckedChanged);
			// 
			// ServerAndPortTextBox
			// 
			this.ServerAndPortTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.ServerAndPortTextBox.Location = new System.Drawing.Point(90, 5);
			this.ServerAndPortTextBox.Name = "ServerAndPortTextBox";
			this.ServerAndPortTextBox.Size = new System.Drawing.Size(466, 23);
			this.ServerAndPortTextBox.TabIndex = 2;
			// 
			// ServerAndPortLabel
			// 
			this.ServerAndPortLabel.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.ServerAndPortLabel.AutoSize = true;
			this.ServerAndPortLabel.Location = new System.Drawing.Point(3, 9);
			this.ServerAndPortLabel.Name = "ServerAndPortLabel";
			this.ServerAndPortLabel.Padding = new System.Windows.Forms.Padding(0, 0, 15, 0);
			this.ServerAndPortLabel.Size = new System.Drawing.Size(57, 15);
			this.ServerAndPortLabel.TabIndex = 1;
			this.ServerAndPortLabel.Text = "Server:";
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(375, 114);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(99, 27);
			this.OkBtn.TabIndex = 5;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(480, 114);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(99, 27);
			this.CancelBtn.TabIndex = 6;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			this.CancelBtn.Click += new System.EventHandler(this.CancelBtn_Click);
			// 
			// BrowseUserBtn
			// 
			this.BrowseUserBtn.Anchor = System.Windows.Forms.AnchorStyles.Right;
			this.BrowseUserBtn.Location = new System.Drawing.Point(373, 3);
			this.BrowseUserBtn.Name = "BrowseUserBtn";
			this.BrowseUserBtn.Size = new System.Drawing.Size(96, 27);
			this.BrowseUserBtn.TabIndex = 7;
			this.BrowseUserBtn.Text = "Browse...";
			this.BrowseUserBtn.UseVisualStyleBackColor = true;
			this.BrowseUserBtn.Click += new System.EventHandler(this.BrowseUserBtn_Click);
			// 
			// tableLayoutPanel1
			// 
			this.tableLayoutPanel1.ColumnCount = 2;
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Controls.Add(this.ServerAndPortLabel, 0, 0);
			this.tableLayoutPanel1.Controls.Add(this.UserNameLabel, 0, 1);
			this.tableLayoutPanel1.Controls.Add(this.ServerAndPortTextBox, 1, 0);
			this.tableLayoutPanel1.Controls.Add(this.tableLayoutPanel2, 1, 1);
			this.tableLayoutPanel1.Location = new System.Drawing.Point(20, 41);
			this.tableLayoutPanel1.Name = "tableLayoutPanel1";
			this.tableLayoutPanel1.RowCount = 2;
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel1.Size = new System.Drawing.Size(559, 67);
			this.tableLayoutPanel1.TabIndex = 8;
			// 
			// tableLayoutPanel2
			// 
			this.tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel2.AutoSize = true;
			this.tableLayoutPanel2.ColumnCount = 2;
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel2.Controls.Add(this.UserNameTextBox, 0, 0);
			this.tableLayoutPanel2.Controls.Add(this.BrowseUserBtn, 1, 0);
			this.tableLayoutPanel2.Location = new System.Drawing.Point(87, 33);
			this.tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel2.Name = "tableLayoutPanel2";
			this.tableLayoutPanel2.RowCount = 1;
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.Size = new System.Drawing.Size(472, 33);
			this.tableLayoutPanel2.TabIndex = 4;
			// 
			// ConnectWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(591, 156);
			this.Controls.Add(this.tableLayoutPanel1);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.UseDefaultConnectionSettings);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ConnectWindow";
			this.ShowInTaskbar = false;
			this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Connection Settings";
			this.tableLayoutPanel1.ResumeLayout(false);
			this.tableLayoutPanel1.PerformLayout();
			this.tableLayoutPanel2.ResumeLayout(false);
			this.tableLayoutPanel2.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion
		private System.Windows.Forms.Label UserNameLabel;
		private TextBoxWithCueBanner UserNameTextBox;
		private TextBoxWithCueBanner ServerAndPortTextBox;
		private System.Windows.Forms.Label ServerAndPortLabel;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.CheckBox UseDefaultConnectionSettings;
		private System.Windows.Forms.Button BrowseUserBtn;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
	}
}
namespace Perforce.sln.bld.gui
{
    partial class FormMain
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(FormMain));
			this.label1 = new System.Windows.Forms.Label();
			this.mSolutionPath = new System.Windows.Forms.TextBox();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.mShowPasswordChk = new System.Windows.Forms.CheckBox();
			this.mPaswordTxt = new System.Windows.Forms.TextBox();
			this.mUserText = new System.Windows.Forms.TextBox();
			this.mServerConnection = new System.Windows.Forms.TextBox();
			this.label4 = new System.Windows.Forms.Label();
			this.label3 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.mBrowseDepotBtn = new System.Windows.Forms.Button();
			this.mLogText = new System.Windows.Forms.TextBox();
			this.label5 = new System.Windows.Forms.Label();
			this.mBuildFolderTxt = new System.Windows.Forms.TextBox();
			this.mSelectBuildDir = new System.Windows.Forms.Button();
			this.folderBrowseDlg = new System.Windows.Forms.FolderBrowserDialog();
			this.mBuildNowBtn = new System.Windows.Forms.Button();
			this.label7 = new System.Windows.Forms.Label();
			this.mMsBuildPathTxt = new System.Windows.Forms.TextBox();
			this.mSelectMSBuildLoactionBtn = new System.Windows.Forms.Button();
			this.fileBrowseDlg = new System.Windows.Forms.OpenFileDialog();
			this.label8 = new System.Windows.Forms.Label();
			this.mLastChangeLbl = new System.Windows.Forms.Label();
			this.label9 = new System.Windows.Forms.Label();
			this.mBuildIntervalCmb = new System.Windows.Forms.ComboBox();
			this.mBuildFailedLbl = new System.Windows.Forms.Label();
			this.groupBox1.SuspendLayout();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(12, 9);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(64, 13);
			this.label1.TabIndex = 0;
			this.label1.Text = "Depot Path:";
			// 
			// mSolutionPath
			// 
			this.mSolutionPath.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.mSolutionPath.Location = new System.Drawing.Point(82, 6);
			this.mSolutionPath.Name = "mSolutionPath";
			this.mSolutionPath.Size = new System.Drawing.Size(339, 20);
			this.mSolutionPath.TabIndex = 1;
			this.mSolutionPath.TextChanged += new System.EventHandler(this.mSolutionPath_TextChanged);
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.mShowPasswordChk);
			this.groupBox1.Controls.Add(this.mPaswordTxt);
			this.groupBox1.Controls.Add(this.mUserText);
			this.groupBox1.Controls.Add(this.mServerConnection);
			this.groupBox1.Controls.Add(this.label4);
			this.groupBox1.Controls.Add(this.label3);
			this.groupBox1.Controls.Add(this.label2);
			this.groupBox1.Location = new System.Drawing.Point(517, 6);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(255, 135);
			this.groupBox1.TabIndex = 2;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Perforce Connection";
			// 
			// mShowPasswordChk
			// 
			this.mShowPasswordChk.AutoSize = true;
			this.mShowPasswordChk.Location = new System.Drawing.Point(24, 103);
			this.mShowPasswordChk.Name = "mShowPasswordChk";
			this.mShowPasswordChk.Size = new System.Drawing.Size(102, 17);
			this.mShowPasswordChk.TabIndex = 2;
			this.mShowPasswordChk.Text = "Show Password";
			this.mShowPasswordChk.UseVisualStyleBackColor = true;
			this.mShowPasswordChk.CheckedChanged += new System.EventHandler(this.mShowPasswordChk_CheckedChanged);
			// 
			// mPaswordTxt
			// 
			this.mPaswordTxt.Location = new System.Drawing.Point(71, 75);
			this.mPaswordTxt.Name = "mPaswordTxt";
			this.mPaswordTxt.Size = new System.Drawing.Size(178, 20);
			this.mPaswordTxt.TabIndex = 1;
			this.mPaswordTxt.UseSystemPasswordChar = true;
			// 
			// mUserText
			// 
			this.mUserText.Location = new System.Drawing.Point(71, 45);
			this.mUserText.Name = "mUserText";
			this.mUserText.Size = new System.Drawing.Size(178, 20);
			this.mUserText.TabIndex = 1;
			this.mUserText.Text = "admin";
			this.mUserText.TextChanged += new System.EventHandler(this.mUserText_TextChanged);
			// 
			// mServerConnection
			// 
			this.mServerConnection.Location = new System.Drawing.Point(71, 16);
			this.mServerConnection.Name = "mServerConnection";
			this.mServerConnection.Size = new System.Drawing.Size(178, 20);
			this.mServerConnection.TabIndex = 1;
			this.mServerConnection.Text = "localhost:1666";
			// 
			// label4
			// 
			this.label4.AutoSize = true;
			this.label4.Location = new System.Drawing.Point(6, 78);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(56, 13);
			this.label4.TabIndex = 0;
			this.label4.Text = "Password:";
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(6, 48);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(32, 13);
			this.label3.TabIndex = 0;
			this.label3.Text = "User:";
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(6, 19);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(41, 13);
			this.label2.TabIndex = 0;
			this.label2.Text = "Server:";
			// 
			// mBrowseDepotBtn
			// 
			this.mBrowseDepotBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.mBrowseDepotBtn.Location = new System.Drawing.Point(427, 4);
			this.mBrowseDepotBtn.Name = "mBrowseDepotBtn";
			this.mBrowseDepotBtn.Size = new System.Drawing.Size(84, 23);
			this.mBrowseDepotBtn.TabIndex = 3;
			this.mBrowseDepotBtn.Text = "Browse Depot";
			this.mBrowseDepotBtn.UseVisualStyleBackColor = true;
			this.mBrowseDepotBtn.Click += new System.EventHandler(this.mBrowseDepotBtn_Click);
			// 
			// mLogText
			// 
			this.mLogText.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
						| System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.mLogText.Location = new System.Drawing.Point(12, 147);
			this.mLogText.MinimumSize = new System.Drawing.Size(700, 200);
			this.mLogText.Multiline = true;
			this.mLogText.Name = "mLogText";
			this.mLogText.ReadOnly = true;
			this.mLogText.ScrollBars = System.Windows.Forms.ScrollBars.Both;
			this.mLogText.Size = new System.Drawing.Size(760, 411);
			this.mLogText.TabIndex = 4;
			this.mLogText.TabStop = false;
			this.mLogText.WordWrap = false;
			// 
			// label5
			// 
			this.label5.AutoSize = true;
			this.label5.Location = new System.Drawing.Point(12, 35);
			this.label5.Name = "label5";
			this.label5.Size = new System.Drawing.Size(65, 13);
			this.label5.TabIndex = 0;
			this.label5.Text = "Build Folder:";
			// 
			// mBuildFolderTxt
			// 
			this.mBuildFolderTxt.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.mBuildFolderTxt.Location = new System.Drawing.Point(82, 32);
			this.mBuildFolderTxt.Name = "mBuildFolderTxt";
			this.mBuildFolderTxt.Size = new System.Drawing.Size(339, 20);
			this.mBuildFolderTxt.TabIndex = 1;
			this.mBuildFolderTxt.Text = "C:\\Builds";
			// 
			// mSelectBuildDir
			// 
			this.mSelectBuildDir.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.mSelectBuildDir.Location = new System.Drawing.Point(427, 30);
			this.mSelectBuildDir.Name = "mSelectBuildDir";
			this.mSelectBuildDir.Size = new System.Drawing.Size(84, 23);
			this.mSelectBuildDir.TabIndex = 3;
			this.mSelectBuildDir.Text = "Select...";
			this.mSelectBuildDir.UseVisualStyleBackColor = true;
			this.mSelectBuildDir.Click += new System.EventHandler(this.mSelectBuildDir_Click);
			// 
			// mBuildNowBtn
			// 
			this.mBuildNowBtn.Enabled = false;
			this.mBuildNowBtn.Location = new System.Drawing.Point(12, 118);
			this.mBuildNowBtn.Name = "mBuildNowBtn";
			this.mBuildNowBtn.Size = new System.Drawing.Size(75, 23);
			this.mBuildNowBtn.TabIndex = 5;
			this.mBuildNowBtn.Text = "Build Now";
			this.mBuildNowBtn.UseVisualStyleBackColor = true;
			this.mBuildNowBtn.Click += new System.EventHandler(this.mBuildNowBtn_Click);
			// 
			// label7
			// 
			this.label7.AutoSize = true;
			this.label7.Location = new System.Drawing.Point(12, 61);
			this.label7.Name = "label7";
			this.label7.Size = new System.Drawing.Size(69, 13);
			this.label7.TabIndex = 0;
			this.label7.Text = "MSBuild.exe:";
			// 
			// mMsBuildPathTxt
			// 
			this.mMsBuildPathTxt.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.mMsBuildPathTxt.Location = new System.Drawing.Point(82, 58);
			this.mMsBuildPathTxt.Name = "mMsBuildPathTxt";
			this.mMsBuildPathTxt.Size = new System.Drawing.Size(339, 20);
			this.mMsBuildPathTxt.TabIndex = 1;
			this.mMsBuildPathTxt.Text = "C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319\\msbuild.exe";
			// 
			// mSelectMSBuildLoactionBtn
			// 
			this.mSelectMSBuildLoactionBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.mSelectMSBuildLoactionBtn.Location = new System.Drawing.Point(427, 56);
			this.mSelectMSBuildLoactionBtn.Name = "mSelectMSBuildLoactionBtn";
			this.mSelectMSBuildLoactionBtn.Size = new System.Drawing.Size(84, 23);
			this.mSelectMSBuildLoactionBtn.TabIndex = 3;
			this.mSelectMSBuildLoactionBtn.Text = "Select...";
			this.mSelectMSBuildLoactionBtn.UseVisualStyleBackColor = true;
			this.mSelectMSBuildLoactionBtn.Click += new System.EventHandler(this.mSelectMSBuildLoactionBtn_Click);
			// 
			// label8
			// 
			this.label8.AutoSize = true;
			this.label8.Location = new System.Drawing.Point(312, 123);
			this.label8.Name = "label8";
			this.label8.Size = new System.Drawing.Size(151, 13);
			this.label8.TabIndex = 6;
			this.label8.Text = "Change Number For Last Built:";
			// 
			// mLastChangeLbl
			// 
			this.mLastChangeLbl.AutoSize = true;
			this.mLastChangeLbl.Location = new System.Drawing.Point(463, 123);
			this.mLastChangeLbl.Name = "mLastChangeLbl";
			this.mLastChangeLbl.Size = new System.Drawing.Size(39, 13);
			this.mLastChangeLbl.TabIndex = 7;
			this.mLastChangeLbl.Text = "l12343";
			// 
			// label9
			// 
			this.label9.AutoSize = true;
			this.label9.Location = new System.Drawing.Point(12, 93);
			this.label9.Name = "label9";
			this.label9.Size = new System.Drawing.Size(71, 13);
			this.label9.TabIndex = 8;
			this.label9.Text = "Build Interval:";
			// 
			// mBuildIntervalCmb
			// 
			this.mBuildIntervalCmb.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.mBuildIntervalCmb.FormattingEnabled = true;
			this.mBuildIntervalCmb.Items.AddRange(new object[] {
            "1",
            "2",
            "5",
            "15",
            "30",
            "60",
            "120",
            "720",
            "1440"});
			this.mBuildIntervalCmb.Location = new System.Drawing.Point(89, 90);
			this.mBuildIntervalCmb.Name = "mBuildIntervalCmb";
			this.mBuildIntervalCmb.Size = new System.Drawing.Size(121, 21);
			this.mBuildIntervalCmb.TabIndex = 9;
			this.mBuildIntervalCmb.SelectedIndexChanged += new System.EventHandler(this.mBuildIntervalCmb_SelectedIndexChanged);
			// 
			// mBuildFailedLbl
			// 
			this.mBuildFailedLbl.BackColor = System.Drawing.Color.OrangeRed;
			this.mBuildFailedLbl.Font = new System.Drawing.Font("Microsoft Sans Serif", 15.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.mBuildFailedLbl.Location = new System.Drawing.Point(93, 115);
			this.mBuildFailedLbl.Name = "mBuildFailedLbl";
			this.mBuildFailedLbl.Size = new System.Drawing.Size(213, 28);
			this.mBuildFailedLbl.TabIndex = 10;
			this.mBuildFailedLbl.Text = "Last Build Failed!";
			this.mBuildFailedLbl.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			this.mBuildFailedLbl.Visible = false;
			// 
			// FormMain
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(784, 562);
			this.Controls.Add(this.mBuildFailedLbl);
			this.Controls.Add(this.mBuildIntervalCmb);
			this.Controls.Add(this.label9);
			this.Controls.Add(this.mLastChangeLbl);
			this.Controls.Add(this.label8);
			this.Controls.Add(this.mBuildNowBtn);
			this.Controls.Add(this.mLogText);
			this.Controls.Add(this.mSelectMSBuildLoactionBtn);
			this.Controls.Add(this.mSelectBuildDir);
			this.Controls.Add(this.mBrowseDepotBtn);
			this.Controls.Add(this.groupBox1);
			this.Controls.Add(this.mMsBuildPathTxt);
			this.Controls.Add(this.label7);
			this.Controls.Add(this.mBuildFolderTxt);
			this.Controls.Add(this.label5);
			this.Controls.Add(this.mSolutionPath);
			this.Controls.Add(this.label1);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "FormMain";
			this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Show;
			this.Text = "sln-bld-gui";
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.FormMain_FormClosing);
			this.Load += new System.EventHandler(this.FormMain_Load);
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox mSolutionPath;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.CheckBox mShowPasswordChk;
        private System.Windows.Forms.TextBox mPaswordTxt;
        private System.Windows.Forms.TextBox mUserText;
        private System.Windows.Forms.TextBox mServerConnection;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Button mBrowseDepotBtn;
        private System.Windows.Forms.TextBox mLogText;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.TextBox mBuildFolderTxt;
        private System.Windows.Forms.Button mSelectBuildDir;
        private System.Windows.Forms.FolderBrowserDialog folderBrowseDlg;
		private System.Windows.Forms.Button mBuildNowBtn;
        private System.Windows.Forms.Label label7;
        private System.Windows.Forms.TextBox mMsBuildPathTxt;
        private System.Windows.Forms.Button mSelectMSBuildLoactionBtn;
        private System.Windows.Forms.OpenFileDialog fileBrowseDlg;
        private System.Windows.Forms.Label label8;
        private System.Windows.Forms.Label mLastChangeLbl;
        private System.Windows.Forms.Label label9;
        private System.Windows.Forms.ComboBox mBuildIntervalCmb;
        private System.Windows.Forms.Label mBuildFailedLbl;
    }
}


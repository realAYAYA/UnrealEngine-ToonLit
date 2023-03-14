namespace UnrealWindowsForms
{
	partial class BuildLauncher
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
			this.buildList = new System.Windows.Forms.ComboBox();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.platformList = new System.Windows.Forms.ComboBox();
			this.label3 = new System.Windows.Forms.Label();
			this.configurationList = new System.Windows.Forms.ComboBox();
			this.goButton = new System.Windows.Forms.Button();
			this.optionTab = new System.Windows.Forms.TabControl();
			this.tabPage1 = new System.Windows.Forms.TabPage();
			this.optionEditorServer = new System.Windows.Forms.CheckBox();
			this.refreshButton = new System.Windows.Forms.Button();
			this.tabPage2 = new System.Windows.Forms.TabPage();
			this.optionForceStage = new System.Windows.Forms.CheckBox();
			this.optionRealTimeUpdates = new System.Windows.Forms.CheckBox();
			this.optionUsePrecompiledBuild = new System.Windows.Forms.CheckBox();
			this.optionBuildEditor = new System.Windows.Forms.CheckBox();
			this.optionSharedCookedBuild = new System.Windows.Forms.CheckBox();
			this.optionCook = new System.Windows.Forms.CheckBox();
			this.optionBuildClient = new System.Windows.Forms.CheckBox();
			this.label4 = new System.Windows.Forms.Label();
			this.modeList = new System.Windows.Forms.ComboBox();
			this.etaLabel = new System.Windows.Forms.Label();
			this.testTextBox = new System.Windows.Forms.TextBox();
			this.label5 = new System.Windows.Forms.Label();
			this.optionTab.SuspendLayout();
			this.tabPage1.SuspendLayout();
			this.tabPage2.SuspendLayout();
			this.SuspendLayout();
			// 
			// buildList
			// 
			this.buildList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.buildList.FormattingEnabled = true;
			this.buildList.Location = new System.Drawing.Point(56, 17);
			this.buildList.Name = "buildList";
			this.buildList.Size = new System.Drawing.Size(293, 21);
			this.buildList.TabIndex = 0;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(11, 20);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(30, 13);
			this.label1.TabIndex = 1;
			this.label1.Text = "Build";
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(13, 159);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(45, 13);
			this.label2.TabIndex = 3;
			this.label2.Text = "Platform";
			// 
			// platformList
			// 
			this.platformList.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
			this.platformList.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.platformList.FormattingEnabled = true;
			this.platformList.Location = new System.Drawing.Point(88, 156);
			this.platformList.Name = "platformList";
			this.platformList.Size = new System.Drawing.Size(133, 21);
			this.platformList.TabIndex = 2;
			this.platformList.SelectedIndexChanged += new System.EventHandler(this.platformList_SelectedIndexChanged);
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(13, 186);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(69, 13);
			this.label3.TabIndex = 5;
			this.label3.Text = "Configuration";
			// 
			// configurationList
			// 
			this.configurationList.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
			this.configurationList.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.configurationList.FormattingEnabled = true;
			this.configurationList.Location = new System.Drawing.Point(88, 183);
			this.configurationList.Name = "configurationList";
			this.configurationList.Size = new System.Drawing.Size(133, 21);
			this.configurationList.TabIndex = 4;
			// 
			// goButton
			// 
			this.goButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.goButton.Location = new System.Drawing.Point(369, 307);
			this.goButton.Name = "goButton";
			this.goButton.Size = new System.Drawing.Size(75, 23);
			this.goButton.TabIndex = 6;
			this.goButton.Text = "Go!";
			this.goButton.UseVisualStyleBackColor = true;
			this.goButton.Click += new System.EventHandler(this.goButton_Click);
			// 
			// optionTab
			// 
			this.optionTab.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.optionTab.Controls.Add(this.tabPage1);
			this.optionTab.Controls.Add(this.tabPage2);
			this.optionTab.Location = new System.Drawing.Point(12, 12);
			this.optionTab.Name = "optionTab";
			this.optionTab.SelectedIndex = 0;
			this.optionTab.Size = new System.Drawing.Size(435, 138);
			this.optionTab.TabIndex = 8;
			this.optionTab.SelectedIndexChanged += new System.EventHandler(this.optionTab_SelectedIndexChanged);
			this.optionTab.TabIndexChanged += new System.EventHandler(this.optionTab_TabIndexChanged);
			// 
			// tabPage1
			// 
			this.tabPage1.Controls.Add(this.optionEditorServer);
			this.tabPage1.Controls.Add(this.refreshButton);
			this.tabPage1.Controls.Add(this.label1);
			this.tabPage1.Controls.Add(this.buildList);
			this.tabPage1.Location = new System.Drawing.Point(4, 22);
			this.tabPage1.Name = "tabPage1";
			this.tabPage1.Padding = new System.Windows.Forms.Padding(3);
			this.tabPage1.Size = new System.Drawing.Size(427, 112);
			this.tabPage1.TabIndex = 0;
			this.tabPage1.Text = "Network Build";
			this.tabPage1.UseVisualStyleBackColor = true;
			// 
			// optionEditorServer
			// 
			this.optionEditorServer.AutoSize = true;
			this.optionEditorServer.Checked = true;
			this.optionEditorServer.CheckState = System.Windows.Forms.CheckState.Checked;
			this.optionEditorServer.Location = new System.Drawing.Point(56, 44);
			this.optionEditorServer.Name = "optionEditorServer";
			this.optionEditorServer.Size = new System.Drawing.Size(87, 17);
			this.optionEditorServer.TabIndex = 14;
			this.optionEditorServer.Text = "Editor Server";
			this.optionEditorServer.UseVisualStyleBackColor = true;
			this.optionEditorServer.CheckedChanged += new System.EventHandler(this.EditorServer_CheckedChanged);
			// 
			// refreshButton
			// 
			this.refreshButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.refreshButton.Location = new System.Drawing.Point(355, 17);
			this.refreshButton.Name = "refreshButton";
			this.refreshButton.Size = new System.Drawing.Size(66, 23);
			this.refreshButton.TabIndex = 2;
			this.refreshButton.Text = "Refresh";
			this.refreshButton.UseVisualStyleBackColor = true;
			this.refreshButton.Click += new System.EventHandler(this.refreshButton_Click);
			// 
			// tabPage2
			// 
			this.tabPage2.Controls.Add(this.optionForceStage);
			this.tabPage2.Controls.Add(this.optionRealTimeUpdates);
			this.tabPage2.Controls.Add(this.optionUsePrecompiledBuild);
			this.tabPage2.Controls.Add(this.optionBuildEditor);
			this.tabPage2.Controls.Add(this.optionSharedCookedBuild);
			this.tabPage2.Controls.Add(this.optionCook);
			this.tabPage2.Controls.Add(this.optionBuildClient);
			this.tabPage2.Location = new System.Drawing.Point(4, 22);
			this.tabPage2.Name = "tabPage2";
			this.tabPage2.Padding = new System.Windows.Forms.Padding(3);
			this.tabPage2.Size = new System.Drawing.Size(427, 112);
			this.tabPage2.TabIndex = 1;
			this.tabPage2.Text = "Local Build";
			this.tabPage2.UseVisualStyleBackColor = true;
			// 
			// optionForceStage
			// 
			this.optionForceStage.AutoSize = true;
			this.optionForceStage.Location = new System.Drawing.Point(200, 63);
			this.optionForceStage.Name = "optionForceStage";
			this.optionForceStage.Size = new System.Drawing.Size(84, 17);
			this.optionForceStage.TabIndex = 6;
			this.optionForceStage.Text = "Force Stage";
			this.optionForceStage.UseVisualStyleBackColor = true;
			// 
			// optionRealTimeUpdates
			// 
			this.optionRealTimeUpdates.AutoSize = true;
			this.optionRealTimeUpdates.Location = new System.Drawing.Point(6, 63);
			this.optionRealTimeUpdates.Name = "optionRealTimeUpdates";
			this.optionRealTimeUpdates.Size = new System.Drawing.Size(177, 17);
			this.optionRealTimeUpdates.TabIndex = 5;
			this.optionRealTimeUpdates.Text = "Realtime updates (Experimental)";
			this.optionRealTimeUpdates.UseVisualStyleBackColor = true;
			this.optionRealTimeUpdates.CheckedChanged += new System.EventHandler(this.optionRealTimeUpdates_CheckedChanged);
			// 
			// optionUsePrecompiledBuild
			// 
			this.optionUsePrecompiledBuild.AutoSize = true;
			this.optionUsePrecompiledBuild.Checked = true;
			this.optionUsePrecompiledBuild.CheckState = System.Windows.Forms.CheckState.Checked;
			this.optionUsePrecompiledBuild.Location = new System.Drawing.Point(6, 86);
			this.optionUsePrecompiledBuild.Name = "optionUsePrecompiledBuild";
			this.optionUsePrecompiledBuild.Size = new System.Drawing.Size(174, 17);
			this.optionUsePrecompiledBuild.TabIndex = 4;
			this.optionUsePrecompiledBuild.Text = "Use Binaries From Shared Build";
			this.optionUsePrecompiledBuild.UseVisualStyleBackColor = true;
			this.optionUsePrecompiledBuild.CheckedChanged += new System.EventHandler(this.optionUsePrecompiledBuild_CheckedChanged);
			// 
			// optionBuildEditor
			// 
			this.optionBuildEditor.AutoSize = true;
			this.optionBuildEditor.Location = new System.Drawing.Point(200, 17);
			this.optionBuildEditor.Name = "optionBuildEditor";
			this.optionBuildEditor.Size = new System.Drawing.Size(198, 17);
			this.optionBuildEditor.TabIndex = 3;
			this.optionBuildEditor.Text = "Compile Editor (UGS compiles these)";
			this.optionBuildEditor.UseVisualStyleBackColor = true;
			this.optionBuildEditor.CheckedChanged += new System.EventHandler(this.optionBuildEditor_CheckedChanged);
			// 
			// optionSharedCookedBuild
			// 
			this.optionSharedCookedBuild.AutoSize = true;
			this.optionSharedCookedBuild.Checked = true;
			this.optionSharedCookedBuild.CheckState = System.Windows.Forms.CheckState.Checked;
			this.optionSharedCookedBuild.Location = new System.Drawing.Point(6, 40);
			this.optionSharedCookedBuild.Name = "optionSharedCookedBuild";
			this.optionSharedCookedBuild.Size = new System.Drawing.Size(159, 17);
			this.optionSharedCookedBuild.TabIndex = 2;
			this.optionSharedCookedBuild.Text = "Iterate Shared Cooked Build";
			this.optionSharedCookedBuild.UseVisualStyleBackColor = true;
			this.optionSharedCookedBuild.CheckedChanged += new System.EventHandler(this.optionSharedCookedBuild_CheckedChanged);
			// 
			// optionCook
			// 
			this.optionCook.AutoSize = true;
			this.optionCook.Checked = true;
			this.optionCook.CheckState = System.Windows.Forms.CheckState.Checked;
			this.optionCook.Location = new System.Drawing.Point(6, 17);
			this.optionCook.Name = "optionCook";
			this.optionCook.Size = new System.Drawing.Size(91, 17);
			this.optionCook.TabIndex = 1;
			this.optionCook.Text = "Cook Content";
			this.optionCook.UseVisualStyleBackColor = true;
			this.optionCook.CheckedChanged += new System.EventHandler(this.optionCook_CheckedChanged);
			// 
			// optionBuildClient
			// 
			this.optionBuildClient.AutoSize = true;
			this.optionBuildClient.Location = new System.Drawing.Point(200, 40);
			this.optionBuildClient.Name = "optionBuildClient";
			this.optionBuildClient.Size = new System.Drawing.Size(228, 17);
			this.optionBuildClient.TabIndex = 0;
			this.optionBuildClient.Text = "Compile Game (if not using shared binaries)";
			this.optionBuildClient.UseVisualStyleBackColor = true;
			this.optionBuildClient.CheckedChanged += new System.EventHandler(this.optionBuildClient_CheckedChanged);
			// 
			// label4
			// 
			this.label4.AutoSize = true;
			this.label4.Location = new System.Drawing.Point(13, 213);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(34, 13);
			this.label4.TabIndex = 10;
			this.label4.Text = "Mode";
			// 
			// modeList
			// 
			this.modeList.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
			this.modeList.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.modeList.FormattingEnabled = true;
			this.modeList.Location = new System.Drawing.Point(88, 210);
			this.modeList.Name = "modeList";
			this.modeList.Size = new System.Drawing.Size(133, 21);
			this.modeList.TabIndex = 9;
			// 
			// etaLabel
			// 
			this.etaLabel.AutoSize = true;
			this.etaLabel.Location = new System.Drawing.Point(305, 262);
			this.etaLabel.Name = "etaLabel";
			this.etaLabel.Size = new System.Drawing.Size(60, 13);
			this.etaLabel.TabIndex = 3;
			this.etaLabel.Text = "ETA: None";
			// 
			// testTextBox
			// 
			this.testTextBox.Location = new System.Drawing.Point(88, 237);
			this.testTextBox.Name = "testTextBox";
			this.testTextBox.Size = new System.Drawing.Size(100, 20);
			this.testTextBox.TabIndex = 12;
			this.testTextBox.Text = "";
			// 
			// label5
			// 
			this.label5.AutoSize = true;
			this.label5.Location = new System.Drawing.Point(13, 240);
			this.label5.Name = "label5";
			this.label5.Size = new System.Drawing.Size(66, 13);
			this.label5.TabIndex = 13;
			this.label5.Text = "Custom Test";
			this.label5.Click += new System.EventHandler(this.label5_Click);
			// 
			// BuildLauncher
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(456, 342);
			this.Controls.Add(this.label5);
			this.Controls.Add(this.testTextBox);
			this.Controls.Add(this.etaLabel);
			this.Controls.Add(this.label4);
			this.Controls.Add(this.modeList);
			this.Controls.Add(this.goButton);
			this.Controls.Add(this.optionTab);
			this.Controls.Add(this.label3);
			this.Controls.Add(this.configurationList);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.platformList);
			this.Name = "BuildLauncher";
			this.Text = "BuildLauncher";
			this.optionTab.ResumeLayout(false);
			this.tabPage1.ResumeLayout(false);
			this.tabPage1.PerformLayout();
			this.tabPage2.ResumeLayout(false);
			this.tabPage2.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.ComboBox buildList;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.ComboBox platformList;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.ComboBox configurationList;
		private System.Windows.Forms.Button goButton;
		private System.Windows.Forms.TabControl optionTab;
		private System.Windows.Forms.TabPage tabPage1;
		private System.Windows.Forms.TabPage tabPage2;
		private System.Windows.Forms.CheckBox optionCook;
		private System.Windows.Forms.CheckBox optionBuildClient;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.ComboBox modeList;
		private System.Windows.Forms.Button refreshButton;
		private System.Windows.Forms.CheckBox optionSharedCookedBuild;
		private System.Windows.Forms.CheckBox optionBuildEditor;
		private System.Windows.Forms.CheckBox optionUsePrecompiledBuild;
		private System.Windows.Forms.Label etaLabel;
		private System.Windows.Forms.CheckBox optionRealTimeUpdates;
		private System.Windows.Forms.TextBox testTextBox;
		private System.Windows.Forms.Label label5;
		private System.Windows.Forms.CheckBox optionEditorServer;
		private System.Windows.Forms.CheckBox optionForceStage;
	}
}
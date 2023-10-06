// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class IssueFixedWindow
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
            this.components = new System.ComponentModel.Container();
            this.OkBtn = new System.Windows.Forms.Button();
            this.CancelBtn = new System.Windows.Forms.Button();
            this.ChangesListView = new System.Windows.Forms.ListView();
            this.Spacer = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.ChangeHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.StreamHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.DescriptionHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.SpecifyChangeRadioButton = new System.Windows.Forms.RadioButton();
            this.ChangeNumberTextBox = new UnrealGameSync.TextBoxWithCueBanner();
            this.groupBox2 = new System.Windows.Forms.GroupBox();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            this.label2 = new System.Windows.Forms.Label();
            this.UserBrowseBtn = new System.Windows.Forms.Button();
            this.UserNameTextBox = new System.Windows.Forms.TextBox();
            this.RecentChangeRadioButton = new System.Windows.Forms.RadioButton();
            this.ChangesListContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.ChangesContextMenu_MoreInfo = new System.Windows.Forms.ToolStripMenuItem();
            this.groupBox3 = new System.Windows.Forms.GroupBox();
            this.label1 = new System.Windows.Forms.Label();
            this.SystemicFixRadioButton = new System.Windows.Forms.RadioButton();
            this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
            this.groupBox1.SuspendLayout();
            this.groupBox2.SuspendLayout();
            this.tableLayoutPanel1.SuspendLayout();
            this.tableLayoutPanel2.SuspendLayout();
            this.ChangesListContextMenu.SuspendLayout();
            this.groupBox3.SuspendLayout();
            this.tableLayoutPanel3.SuspendLayout();
            this.SuspendLayout();
            // 
            // OkBtn
            // 
            this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.OkBtn.Location = new System.Drawing.Point(760, 512);
            this.OkBtn.Name = "OkBtn";
            this.OkBtn.Size = new System.Drawing.Size(87, 27);
            this.OkBtn.TabIndex = 3;
            this.OkBtn.Text = "Ok";
            this.OkBtn.UseVisualStyleBackColor = true;
            this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
            // 
            // CancelBtn
            // 
            this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.CancelBtn.Location = new System.Drawing.Point(667, 512);
            this.CancelBtn.Name = "CancelBtn";
            this.CancelBtn.Size = new System.Drawing.Size(87, 27);
            this.CancelBtn.TabIndex = 2;
            this.CancelBtn.Text = "Cancel";
            this.CancelBtn.UseVisualStyleBackColor = true;
            // 
            // ChangesListView
            // 
            this.ChangesListView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.ChangesListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.Spacer,
            this.ChangeHeader,
            this.StreamHeader,
            this.DescriptionHeader});
            this.ChangesListView.FullRowSelect = true;
            this.ChangesListView.HideSelection = false;
            this.ChangesListView.Location = new System.Drawing.Point(3, 36);
            this.ChangesListView.MultiSelect = false;
            this.ChangesListView.Name = "ChangesListView";
            this.ChangesListView.Size = new System.Drawing.Size(794, 255);
            this.ChangesListView.TabIndex = 0;
            this.ChangesListView.UseCompatibleStateImageBehavior = false;
            this.ChangesListView.View = System.Windows.Forms.View.Details;
            this.ChangesListView.ItemSelectionChanged += new System.Windows.Forms.ListViewItemSelectionChangedEventHandler(this.ChangesListView_ItemSelectionChanged);
            this.ChangesListView.Enter += new System.EventHandler(this.ChangesListView_Enter);
            this.ChangesListView.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ChangesListView_MouseClick);
            this.ChangesListView.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.ChangesListView_MouseDoubleClick);
            // 
            // Spacer
            // 
            this.Spacer.Text = "";
            this.Spacer.Width = 16;
            // 
            // ChangeHeader
            // 
            this.ChangeHeader.Text = "Change";
            this.ChangeHeader.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
            this.ChangeHeader.Width = 83;
            // 
            // StreamHeader
            // 
            this.StreamHeader.Text = "Stream";
            this.StreamHeader.Width = 146;
            // 
            // DescriptionHeader
            // 
            this.DescriptionHeader.Text = "Description";
            this.DescriptionHeader.Width = 524;
            // 
            // groupBox1
            // 
            this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.groupBox1.Controls.Add(this.SpecifyChangeRadioButton);
            this.groupBox1.Controls.Add(this.ChangeNumberTextBox);
            this.groupBox1.Location = new System.Drawing.Point(3, 349);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(829, 73);
            this.groupBox1.TabIndex = 0;
            this.groupBox1.TabStop = false;
            // 
            // SpecifyChangeRadioButton
            // 
            this.SpecifyChangeRadioButton.AutoSize = true;
            this.SpecifyChangeRadioButton.Location = new System.Drawing.Point(11, -1);
            this.SpecifyChangeRadioButton.Name = "SpecifyChangeRadioButton";
            this.SpecifyChangeRadioButton.Size = new System.Drawing.Size(99, 19);
            this.SpecifyChangeRadioButton.TabIndex = 0;
            this.SpecifyChangeRadioButton.Text = "Other Change";
            this.SpecifyChangeRadioButton.UseVisualStyleBackColor = true;
            this.SpecifyChangeRadioButton.CheckedChanged += new System.EventHandler(this.SpecifyChangeRadioButton_CheckedChanged);
            // 
            // ChangeNumberTextBox
            // 
            this.ChangeNumberTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.ChangeNumberTextBox.CueBanner = "Changelist Number";
            this.ChangeNumberTextBox.Location = new System.Drawing.Point(15, 31);
            this.ChangeNumberTextBox.Name = "ChangeNumberTextBox";
            this.ChangeNumberTextBox.Size = new System.Drawing.Size(794, 23);
            this.ChangeNumberTextBox.TabIndex = 1;
            this.ChangeNumberTextBox.TextChanged += new System.EventHandler(this.ChangeNumberTextBox_TextChanged);
            this.ChangeNumberTextBox.Enter += new System.EventHandler(this.ChangeNumberTextBox_Enter);
            // 
            // groupBox2
            // 
            this.groupBox2.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.groupBox2.Controls.Add(this.tableLayoutPanel1);
            this.groupBox2.Controls.Add(this.RecentChangeRadioButton);
            this.groupBox2.Location = new System.Drawing.Point(3, 3);
            this.groupBox2.Name = "groupBox2";
            this.groupBox2.Size = new System.Drawing.Size(829, 340);
            this.groupBox2.TabIndex = 1;
            this.groupBox2.TabStop = false;
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.ColumnCount = 1;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Controls.Add(this.tableLayoutPanel2, 0, 0);
            this.tableLayoutPanel1.Controls.Add(this.ChangesListView, 0, 1);
            this.tableLayoutPanel1.Location = new System.Drawing.Point(15, 25);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 2;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Size = new System.Drawing.Size(800, 294);
            this.tableLayoutPanel1.TabIndex = 5;
            // 
            // tableLayoutPanel2
            // 
            this.tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.tableLayoutPanel2.AutoSize = true;
            this.tableLayoutPanel2.ColumnCount = 3;
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel2.Controls.Add(this.label2, 0, 0);
            this.tableLayoutPanel2.Controls.Add(this.UserBrowseBtn, 2, 0);
            this.tableLayoutPanel2.Controls.Add(this.UserNameTextBox, 1, 0);
            this.tableLayoutPanel2.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(0);
            this.tableLayoutPanel2.Name = "tableLayoutPanel2";
            this.tableLayoutPanel2.RowCount = 1;
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.Size = new System.Drawing.Size(800, 33);
            this.tableLayoutPanel2.TabIndex = 1;
            // 
            // label2
            // 
            this.label2.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(3, 9);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(47, 15);
            this.label2.TabIndex = 1;
            this.label2.Text = "Author:";
            // 
            // UserBrowseBtn
            // 
            this.UserBrowseBtn.AutoSize = true;
            this.UserBrowseBtn.Location = new System.Drawing.Point(701, 3);
            this.UserBrowseBtn.Name = "UserBrowseBtn";
            this.UserBrowseBtn.Size = new System.Drawing.Size(96, 27);
            this.UserBrowseBtn.TabIndex = 0;
            this.UserBrowseBtn.Text = "Browse...";
            this.UserBrowseBtn.UseVisualStyleBackColor = true;
            this.UserBrowseBtn.Click += new System.EventHandler(this.UserBrowseBtn_Click);
            this.UserBrowseBtn.Enter += new System.EventHandler(this.UserBrowseBtn_Enter);
            // 
            // UserNameTextBox
            // 
            this.UserNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.UserNameTextBox.Location = new System.Drawing.Point(56, 5);
            this.UserNameTextBox.Name = "UserNameTextBox";
            this.UserNameTextBox.Size = new System.Drawing.Size(639, 23);
            this.UserNameTextBox.TabIndex = 2;
            this.UserNameTextBox.TextChanged += new System.EventHandler(this.UserNameTextBox_TextChanged);
            this.UserNameTextBox.Enter += new System.EventHandler(this.UserNameTextBox_Enter);
            // 
            // RecentChangeRadioButton
            // 
            this.RecentChangeRadioButton.AutoSize = true;
            this.RecentChangeRadioButton.Checked = true;
            this.RecentChangeRadioButton.Location = new System.Drawing.Point(11, 0);
            this.RecentChangeRadioButton.Name = "RecentChangeRadioButton";
            this.RecentChangeRadioButton.Size = new System.Drawing.Size(105, 19);
            this.RecentChangeRadioButton.TabIndex = 0;
            this.RecentChangeRadioButton.TabStop = true;
            this.RecentChangeRadioButton.Text = "Recent Change";
            this.RecentChangeRadioButton.UseVisualStyleBackColor = true;
            this.RecentChangeRadioButton.CheckedChanged += new System.EventHandler(this.RecentChangeRadioButton_CheckedChanged);
            // 
            // ChangesListContextMenu
            // 
            this.ChangesListContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.ChangesContextMenu_MoreInfo});
            this.ChangesListContextMenu.Name = "ChangesListContextMenu";
            this.ChangesListContextMenu.Size = new System.Drawing.Size(136, 26);
            // 
            // ChangesContextMenu_MoreInfo
            // 
            this.ChangesContextMenu_MoreInfo.Name = "ChangesContextMenu_MoreInfo";
            this.ChangesContextMenu_MoreInfo.Size = new System.Drawing.Size(135, 22);
            this.ChangesContextMenu_MoreInfo.Text = "More Info...";
            this.ChangesContextMenu_MoreInfo.Click += new System.EventHandler(this.ChangesContextMenu_MoreInfo_Click);
            // 
            // groupBox3
            // 
            this.groupBox3.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.groupBox3.Controls.Add(this.label1);
            this.groupBox3.Controls.Add(this.SystemicFixRadioButton);
            this.groupBox3.Location = new System.Drawing.Point(3, 428);
            this.groupBox3.Name = "groupBox3";
            this.groupBox3.Size = new System.Drawing.Size(829, 63);
            this.groupBox3.TabIndex = 0;
            this.groupBox3.TabStop = false;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(13, 29);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(542, 15);
            this.label1.TabIndex = 1;
            this.label1.Text = "Select this option for issues that were fixed without requiring a changelist to b" +
    "e submitted to Perforce.";
            // 
            // SystemicFixRadioButton
            // 
            this.SystemicFixRadioButton.AutoSize = true;
            this.SystemicFixRadioButton.Location = new System.Drawing.Point(11, 0);
            this.SystemicFixRadioButton.Name = "SystemicFixRadioButton";
            this.SystemicFixRadioButton.Size = new System.Drawing.Size(101, 19);
            this.SystemicFixRadioButton.TabIndex = 0;
            this.SystemicFixRadioButton.TabStop = true;
            this.SystemicFixRadioButton.Text = "Systemic Issue";
            this.SystemicFixRadioButton.UseVisualStyleBackColor = true;
            this.SystemicFixRadioButton.CheckedChanged += new System.EventHandler(this.SystemicFixRadioButton_CheckedChanged);
            // 
            // tableLayoutPanel3
            // 
            this.tableLayoutPanel3.ColumnCount = 1;
            this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel3.Controls.Add(this.groupBox3, 0, 2);
            this.tableLayoutPanel3.Controls.Add(this.groupBox1, 0, 1);
            this.tableLayoutPanel3.Controls.Add(this.groupBox2, 0, 0);
            this.tableLayoutPanel3.Location = new System.Drawing.Point(12, 12);
            this.tableLayoutPanel3.Name = "tableLayoutPanel3";
            this.tableLayoutPanel3.RowCount = 3;
            this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel3.Size = new System.Drawing.Size(835, 494);
            this.tableLayoutPanel3.TabIndex = 5;
            // 
            // IssueFixedWindow
            // 
            this.AcceptButton = this.OkBtn;
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this.CancelBtn;
            this.ClientSize = new System.Drawing.Size(861, 551);
            this.ControlBox = false;
            this.Controls.Add(this.tableLayoutPanel3);
            this.Controls.Add(this.CancelBtn);
            this.Controls.Add(this.OkBtn);
            this.Name = "IssueFixedWindow";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Mark Fixed";
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.groupBox2.ResumeLayout(false);
            this.groupBox2.PerformLayout();
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            this.tableLayoutPanel2.ResumeLayout(false);
            this.tableLayoutPanel2.PerformLayout();
            this.ChangesListContextMenu.ResumeLayout(false);
            this.groupBox3.ResumeLayout(false);
            this.groupBox3.PerformLayout();
            this.tableLayoutPanel3.ResumeLayout(false);
            this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private TextBoxWithCueBanner ChangeNumberTextBox;
		private System.Windows.Forms.ListView ChangesListView;
		private System.Windows.Forms.ColumnHeader ChangeHeader;
		private System.Windows.Forms.ColumnHeader StreamHeader;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.RadioButton SpecifyChangeRadioButton;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.RadioButton RecentChangeRadioButton;
		private System.Windows.Forms.ColumnHeader DescriptionHeader;
		private System.Windows.Forms.ColumnHeader Spacer;
		private System.Windows.Forms.ContextMenuStrip ChangesListContextMenu;
		private System.Windows.Forms.ToolStripMenuItem ChangesContextMenu_MoreInfo;
		private System.Windows.Forms.Button UserBrowseBtn;
		private System.Windows.Forms.TextBox UserNameTextBox;
		private System.Windows.Forms.GroupBox groupBox3;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.RadioButton SystemicFixRadioButton;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
	}
}
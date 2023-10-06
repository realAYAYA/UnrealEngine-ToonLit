// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class IssueAlertWindow
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
            this.SummaryLabel = new System.Windows.Forms.LinkLabel();
            this.OwnerLabel = new System.Windows.Forms.Label();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
            this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            this.LatestBuildLinkLabel = new System.Windows.Forms.LinkLabel();
            this.DeclineBtn = new UnrealGameSync.AlertButtonControl();
            this.AcceptBtn = new UnrealGameSync.AlertButtonControl();
            this.DetailsBtn = new UnrealGameSync.AlertButtonControl();
            this.tableLayoutPanel1.SuspendLayout();
            this.tableLayoutPanel3.SuspendLayout();
            this.tableLayoutPanel2.SuspendLayout();
            this.SuspendLayout();
            // 
            // SummaryLabel
            // 
            this.SummaryLabel.ActiveLinkColor = System.Drawing.Color.FromArgb(((int)(((byte)(16)))), ((int)(((byte)(112)))), ((int)(((byte)(202)))));
            this.SummaryLabel.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.SummaryLabel.AutoSize = true;
            this.SummaryLabel.Font = new System.Drawing.Font("Segoe UI Semibold", 10F);
            this.SummaryLabel.LinkBehavior = System.Windows.Forms.LinkBehavior.HoverUnderline;
            this.SummaryLabel.LinkColor = System.Drawing.Color.FromArgb(((int)(((byte)(32)))), ((int)(((byte)(32)))), ((int)(((byte)(64)))));
            this.SummaryLabel.Location = new System.Drawing.Point(2, 0);
            this.SummaryLabel.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.SummaryLabel.Name = "SummaryLabel";
            this.SummaryLabel.Padding = new System.Windows.Forms.Padding(0, 3, 50, 3);
            this.SummaryLabel.Size = new System.Drawing.Size(119, 23);
            this.SummaryLabel.TabIndex = 0;
            this.SummaryLabel.TabStop = true;
            this.SummaryLabel.Text = "Summary";
            this.SummaryLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.SummaryLabel_LinkClicked);
            // 
            // OwnerLabel
            // 
            this.OwnerLabel.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.OwnerLabel.AutoSize = true;
            this.OwnerLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(32)))), ((int)(((byte)(32)))), ((int)(((byte)(64)))));
            this.OwnerLabel.Location = new System.Drawing.Point(3, 23);
            this.OwnerLabel.Name = "OwnerLabel";
            this.OwnerLabel.Padding = new System.Windows.Forms.Padding(0, 3, 30, 3);
            this.OwnerLabel.Size = new System.Drawing.Size(72, 21);
            this.OwnerLabel.TabIndex = 5;
            this.OwnerLabel.Text = "Owner";
            this.OwnerLabel.Click += new System.EventHandler(this.IssueAlertWindow_Click);
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.AutoSize = true;
            this.tableLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.tableLayoutPanel1.BackColor = System.Drawing.Color.Transparent;
            this.tableLayoutPanel1.ColumnCount = 5;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel1.Controls.Add(this.tableLayoutPanel3, 1, 0);
            this.tableLayoutPanel1.Controls.Add(this.tableLayoutPanel2, 1, 0);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(8, 8);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.Padding = new System.Windows.Forms.Padding(20, 0, 20, 0);
            this.tableLayoutPanel1.RowCount = 1;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 74F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 74F));
            this.tableLayoutPanel1.Size = new System.Drawing.Size(683, 74);
            this.tableLayoutPanel1.TabIndex = 7;
            this.tableLayoutPanel1.Click += new System.EventHandler(this.IssueAlertWindow_Click);
            // 
            // tableLayoutPanel3
            // 
            this.tableLayoutPanel3.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.tableLayoutPanel3.AutoSize = true;
            this.tableLayoutPanel3.ColumnCount = 3;
            this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 33.33333F));
            this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 33.33333F));
            this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 33.33333F));
            this.tableLayoutPanel3.Controls.Add(this.DeclineBtn, 2, 0);
            this.tableLayoutPanel3.Controls.Add(this.AcceptBtn, 1, 0);
            this.tableLayoutPanel3.Controls.Add(this.DetailsBtn, 0, 0);
            this.tableLayoutPanel3.Location = new System.Drawing.Point(360, 19);
            this.tableLayoutPanel3.Margin = new System.Windows.Forms.Padding(0);
            this.tableLayoutPanel3.Name = "tableLayoutPanel3";
            this.tableLayoutPanel3.RowCount = 1;
            this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 35F));
            this.tableLayoutPanel3.Size = new System.Drawing.Size(303, 35);
            this.tableLayoutPanel3.TabIndex = 8;
            // 
            // tableLayoutPanel2
            // 
            this.tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.tableLayoutPanel2.AutoSize = true;
            this.tableLayoutPanel2.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.tableLayoutPanel2.ColumnCount = 1;
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel2.Controls.Add(this.SummaryLabel, 0, 0);
            this.tableLayoutPanel2.Controls.Add(this.OwnerLabel, 0, 1);
            this.tableLayoutPanel2.Controls.Add(this.LatestBuildLinkLabel, 0, 2);
            this.tableLayoutPanel2.Location = new System.Drawing.Point(23, 3);
            this.tableLayoutPanel2.Name = "tableLayoutPanel2";
            this.tableLayoutPanel2.RowCount = 3;
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 35F));
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 32.5F));
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 32.5F));
            this.tableLayoutPanel2.Size = new System.Drawing.Size(334, 68);
            this.tableLayoutPanel2.TabIndex = 7;
            this.tableLayoutPanel2.Click += new System.EventHandler(this.IssueAlertWindow_Click);
            // 
            // LatestBuildLinkLabel
            // 
            this.LatestBuildLinkLabel.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.LatestBuildLinkLabel.AutoSize = true;
            this.LatestBuildLinkLabel.Font = new System.Drawing.Font("Segoe UI Semibold", 9F);
            this.LatestBuildLinkLabel.LinkColor = System.Drawing.Color.FromArgb(((int)(((byte)(16)))), ((int)(((byte)(102)))), ((int)(((byte)(192)))));
            this.LatestBuildLinkLabel.Location = new System.Drawing.Point(3, 46);
            this.LatestBuildLinkLabel.Name = "LatestBuildLinkLabel";
            this.LatestBuildLinkLabel.Padding = new System.Windows.Forms.Padding(0, 3, 0, 3);
            this.LatestBuildLinkLabel.Size = new System.Drawing.Size(68, 21);
            this.LatestBuildLinkLabel.TabIndex = 8;
            this.LatestBuildLinkLabel.TabStop = true;
            this.LatestBuildLinkLabel.Text = "Latest build";
            // 
            // DeclineBtn
            // 
            this.DeclineBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.DeclineBtn.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.DeclineBtn.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Bold);
            this.DeclineBtn.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(255)))), ((int)(((byte)(255)))));
            this.DeclineBtn.Location = new System.Drawing.Point(205, 3);
            this.DeclineBtn.Name = "DeclineBtn";
            this.DeclineBtn.Padding = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.DeclineBtn.Size = new System.Drawing.Size(95, 29);
            this.DeclineBtn.TabIndex = 2;
            this.DeclineBtn.Text = "Not Me";
            this.DeclineBtn.Theme = UnrealGameSync.AlertButtonControl.AlertButtonTheme.Red;
            this.DeclineBtn.UseVisualStyleBackColor = true;
            // 
            // AcceptBtn
            // 
            this.AcceptBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.AcceptBtn.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.AcceptBtn.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.AcceptBtn.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(255)))), ((int)(((byte)(255)))));
            this.AcceptBtn.Location = new System.Drawing.Point(104, 3);
            this.AcceptBtn.Name = "AcceptBtn";
            this.AcceptBtn.Padding = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.AcceptBtn.Size = new System.Drawing.Size(95, 29);
            this.AcceptBtn.TabIndex = 1;
            this.AcceptBtn.Text = "Will Fix";
            this.AcceptBtn.Theme = UnrealGameSync.AlertButtonControl.AlertButtonTheme.Green;
            this.AcceptBtn.UseVisualStyleBackColor = true;
            // 
            // DetailsBtn
            // 
            this.DetailsBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.DetailsBtn.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.DetailsBtn.Font = new System.Drawing.Font("Segoe UI Semibold", 9F);
            this.DetailsBtn.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(64)))), ((int)(((byte)(86)))), ((int)(((byte)(106)))));
            this.DetailsBtn.Location = new System.Drawing.Point(3, 3);
            this.DetailsBtn.Name = "DetailsBtn";
            this.DetailsBtn.Padding = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.DetailsBtn.Size = new System.Drawing.Size(95, 29);
            this.DetailsBtn.TabIndex = 3;
            this.DetailsBtn.Text = "Details";
            this.DetailsBtn.Theme = UnrealGameSync.AlertButtonControl.AlertButtonTheme.Normal;
            this.DetailsBtn.UseVisualStyleBackColor = true;
            // 
            // IssueAlertWindow
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
            this.AutoSize = true;
            this.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(699, 90);
            this.Controls.Add(this.tableLayoutPanel1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.None;
            this.Name = "IssueAlertWindow";
            this.Padding = new System.Windows.Forms.Padding(8);
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.Manual;
            this.Text = "BuildIssueAlertWindow";
            this.Click += new System.EventHandler(this.IssueAlertWindow_Click);
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            this.tableLayoutPanel3.ResumeLayout(false);
            this.tableLayoutPanel2.ResumeLayout(false);
            this.tableLayoutPanel2.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.LinkLabel SummaryLabel;
		public AlertButtonControl AcceptBtn;
		public AlertButtonControl DeclineBtn;
		public AlertButtonControl DetailsBtn;
		private System.Windows.Forms.Label OwnerLabel;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
		private System.Windows.Forms.LinkLabel LatestBuildLinkLabel;
	}
}
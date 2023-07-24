// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class IssueSettingsWindow
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
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.groupBox2 = new System.Windows.Forms.GroupBox();
			this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			this.NotifyProjectsCheckBox = new System.Windows.Forms.CheckBox();
			this.NotifyProjectsTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.flowLayoutPanel3 = new System.Windows.Forms.FlowLayoutPanel();
			this.NotifyUnacknowledgedCheckBox = new System.Windows.Forms.CheckBox();
			this.NotifyUnacknowledgedTextBox = new System.Windows.Forms.TextBox();
			this.label1 = new System.Windows.Forms.Label();
			this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
			this.NotifyUnassignedCheckBox = new System.Windows.Forms.CheckBox();
			this.NotifyUnassignedTextBox = new System.Windows.Forms.TextBox();
			this.label2 = new System.Windows.Forms.Label();
			this.flowLayoutPanel2 = new System.Windows.Forms.FlowLayoutPanel();
			this.NotifyUnresolvedCheckBox = new System.Windows.Forms.CheckBox();
			this.NotifyUnresolvedTextBox = new System.Windows.Forms.TextBox();
			this.label3 = new System.Windows.Forms.Label();
			this.groupBox2.SuspendLayout();
			this.tableLayoutPanel1.SuspendLayout();
			this.tableLayoutPanel2.SuspendLayout();
			this.flowLayoutPanel3.SuspendLayout();
			this.flowLayoutPanel1.SuspendLayout();
			this.flowLayoutPanel2.SuspendLayout();
			this.SuspendLayout();
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(356, 213);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(87, 27);
			this.OkBtn.TabIndex = 0;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(263, 213);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(87, 27);
			this.CancelBtn.TabIndex = 1;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			this.CancelBtn.Click += new System.EventHandler(this.CancelBtn_Click);
			// 
			// groupBox2
			// 
			this.groupBox2.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox2.Controls.Add(this.tableLayoutPanel1);
			this.groupBox2.Location = new System.Drawing.Point(12, 12);
			this.groupBox2.Name = "groupBox2";
			this.groupBox2.Size = new System.Drawing.Size(431, 195);
			this.groupBox2.TabIndex = 6;
			this.groupBox2.TabStop = false;
			this.groupBox2.Text = "Optional Notifications";
			// 
			// tableLayoutPanel1
			// 
			this.tableLayoutPanel1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.tableLayoutPanel1.ColumnCount = 1;
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Controls.Add(this.tableLayoutPanel2, 0, 0);
			this.tableLayoutPanel1.Controls.Add(this.flowLayoutPanel3, 0, 1);
			this.tableLayoutPanel1.Controls.Add(this.flowLayoutPanel1, 0, 2);
			this.tableLayoutPanel1.Controls.Add(this.flowLayoutPanel2, 0, 3);
			this.tableLayoutPanel1.Location = new System.Drawing.Point(17, 31);
			this.tableLayoutPanel1.Name = "tableLayoutPanel1";
			this.tableLayoutPanel1.RowCount = 4;
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25.00062F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25.00062F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25.00062F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 24.99813F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
			this.tableLayoutPanel1.Size = new System.Drawing.Size(388, 143);
			this.tableLayoutPanel1.TabIndex = 9;
			// 
			// tableLayoutPanel2
			// 
			this.tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel2.ColumnCount = 2;
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.Controls.Add(this.NotifyProjectsCheckBox, 0, 0);
			this.tableLayoutPanel2.Controls.Add(this.NotifyProjectsTextBox, 1, 0);
			this.tableLayoutPanel2.Location = new System.Drawing.Point(0, 2);
			this.tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel2.Name = "tableLayoutPanel2";
			this.tableLayoutPanel2.RowCount = 1;
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.Size = new System.Drawing.Size(388, 30);
			this.tableLayoutPanel2.TabIndex = 7;
			// 
			// NotifyProjectsCheckBox
			// 
			this.NotifyProjectsCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.NotifyProjectsCheckBox.AutoSize = true;
			this.NotifyProjectsCheckBox.Location = new System.Drawing.Point(3, 5);
			this.NotifyProjectsCheckBox.Name = "NotifyProjectsCheckBox";
			this.NotifyProjectsCheckBox.Size = new System.Drawing.Size(97, 19);
			this.NotifyProjectsCheckBox.TabIndex = 4;
			this.NotifyProjectsCheckBox.Text = "Filter projects";
			this.NotifyProjectsCheckBox.UseVisualStyleBackColor = true;
			this.NotifyProjectsCheckBox.CheckedChanged += new System.EventHandler(this.NotifyProjectsCheckBox_CheckedChanged);
			// 
			// NotifyProjectsTextBox
			// 
			this.NotifyProjectsTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.NotifyProjectsTextBox.CueBanner = "List of project names separated by spaces";
			this.NotifyProjectsTextBox.Location = new System.Drawing.Point(106, 3);
			this.NotifyProjectsTextBox.Name = "NotifyProjectsTextBox";
			this.NotifyProjectsTextBox.Size = new System.Drawing.Size(279, 23);
			this.NotifyProjectsTextBox.TabIndex = 5;
			// 
			// flowLayoutPanel3
			// 
			this.flowLayoutPanel3.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.flowLayoutPanel3.AutoSize = true;
			this.flowLayoutPanel3.Controls.Add(this.NotifyUnacknowledgedCheckBox);
			this.flowLayoutPanel3.Controls.Add(this.NotifyUnacknowledgedTextBox);
			this.flowLayoutPanel3.Controls.Add(this.label1);
			this.flowLayoutPanel3.Location = new System.Drawing.Point(0, 38);
			this.flowLayoutPanel3.Margin = new System.Windows.Forms.Padding(0);
			this.flowLayoutPanel3.Name = "flowLayoutPanel3";
			this.flowLayoutPanel3.Size = new System.Drawing.Size(375, 29);
			this.flowLayoutPanel3.TabIndex = 9;
			this.flowLayoutPanel3.WrapContents = false;
			// 
			// NotifyUnacknowledgedCheckBox
			// 
			this.NotifyUnacknowledgedCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.NotifyUnacknowledgedCheckBox.AutoSize = true;
			this.NotifyUnacknowledgedCheckBox.Location = new System.Drawing.Point(3, 5);
			this.NotifyUnacknowledgedCheckBox.Name = "NotifyUnacknowledgedCheckBox";
			this.NotifyUnacknowledgedCheckBox.Size = new System.Drawing.Size(223, 19);
			this.NotifyUnacknowledgedCheckBox.TabIndex = 4;
			this.NotifyUnacknowledgedCheckBox.Text = "Alert on unacknowledged issues after";
			this.NotifyUnacknowledgedCheckBox.UseVisualStyleBackColor = true;
			this.NotifyUnacknowledgedCheckBox.CheckedChanged += new System.EventHandler(this.NotifyUnacknowledgedCheckBox_CheckedChanged);
			// 
			// NotifyUnacknowledgedTextBox
			// 
			this.NotifyUnacknowledgedTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.NotifyUnacknowledgedTextBox.Location = new System.Drawing.Point(232, 3);
			this.NotifyUnacknowledgedTextBox.Name = "NotifyUnacknowledgedTextBox";
			this.NotifyUnacknowledgedTextBox.Size = new System.Drawing.Size(84, 23);
			this.NotifyUnacknowledgedTextBox.TabIndex = 5;
			// 
			// label1
			// 
			this.label1.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(322, 7);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(50, 15);
			this.label1.TabIndex = 6;
			this.label1.Text = "minutes";
			// 
			// flowLayoutPanel1
			// 
			this.flowLayoutPanel1.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.flowLayoutPanel1.AutoSize = true;
			this.flowLayoutPanel1.Controls.Add(this.NotifyUnassignedCheckBox);
			this.flowLayoutPanel1.Controls.Add(this.NotifyUnassignedTextBox);
			this.flowLayoutPanel1.Controls.Add(this.label2);
			this.flowLayoutPanel1.Location = new System.Drawing.Point(0, 73);
			this.flowLayoutPanel1.Margin = new System.Windows.Forms.Padding(0);
			this.flowLayoutPanel1.Name = "flowLayoutPanel1";
			this.flowLayoutPanel1.Size = new System.Drawing.Size(344, 29);
			this.flowLayoutPanel1.TabIndex = 7;
			this.flowLayoutPanel1.WrapContents = false;
			// 
			// NotifyUnassignedCheckBox
			// 
			this.NotifyUnassignedCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.NotifyUnassignedCheckBox.AutoSize = true;
			this.NotifyUnassignedCheckBox.Location = new System.Drawing.Point(3, 5);
			this.NotifyUnassignedCheckBox.Name = "NotifyUnassignedCheckBox";
			this.NotifyUnassignedCheckBox.Size = new System.Drawing.Size(192, 19);
			this.NotifyUnassignedCheckBox.TabIndex = 4;
			this.NotifyUnassignedCheckBox.Text = "Alert on unassigned issues after";
			this.NotifyUnassignedCheckBox.UseVisualStyleBackColor = true;
			this.NotifyUnassignedCheckBox.CheckedChanged += new System.EventHandler(this.NotifyUnassignedCheckBox_CheckedChanged);
			// 
			// NotifyUnassignedTextBox
			// 
			this.NotifyUnassignedTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.NotifyUnassignedTextBox.Location = new System.Drawing.Point(201, 3);
			this.NotifyUnassignedTextBox.Name = "NotifyUnassignedTextBox";
			this.NotifyUnassignedTextBox.Size = new System.Drawing.Size(84, 23);
			this.NotifyUnassignedTextBox.TabIndex = 5;
			// 
			// label2
			// 
			this.label2.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(291, 7);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(50, 15);
			this.label2.TabIndex = 6;
			this.label2.Text = "minutes";
			// 
			// flowLayoutPanel2
			// 
			this.flowLayoutPanel2.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.flowLayoutPanel2.AutoSize = true;
			this.flowLayoutPanel2.Controls.Add(this.NotifyUnresolvedCheckBox);
			this.flowLayoutPanel2.Controls.Add(this.NotifyUnresolvedTextBox);
			this.flowLayoutPanel2.Controls.Add(this.label3);
			this.flowLayoutPanel2.Location = new System.Drawing.Point(0, 109);
			this.flowLayoutPanel2.Margin = new System.Windows.Forms.Padding(0);
			this.flowLayoutPanel2.Name = "flowLayoutPanel2";
			this.flowLayoutPanel2.Size = new System.Drawing.Size(342, 29);
			this.flowLayoutPanel2.TabIndex = 8;
			this.flowLayoutPanel2.WrapContents = false;
			// 
			// NotifyUnresolvedCheckBox
			// 
			this.NotifyUnresolvedCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.NotifyUnresolvedCheckBox.AutoSize = true;
			this.NotifyUnresolvedCheckBox.Location = new System.Drawing.Point(3, 5);
			this.NotifyUnresolvedCheckBox.Name = "NotifyUnresolvedCheckBox";
			this.NotifyUnresolvedCheckBox.Size = new System.Drawing.Size(190, 19);
			this.NotifyUnresolvedCheckBox.TabIndex = 4;
			this.NotifyUnresolvedCheckBox.Text = "Alert on unresolved issues after";
			this.NotifyUnresolvedCheckBox.UseVisualStyleBackColor = true;
			this.NotifyUnresolvedCheckBox.CheckedChanged += new System.EventHandler(this.NotifyUnresolvedCheckBox_CheckedChanged);
			// 
			// NotifyUnresolvedTextBox
			// 
			this.NotifyUnresolvedTextBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.NotifyUnresolvedTextBox.Location = new System.Drawing.Point(199, 3);
			this.NotifyUnresolvedTextBox.Name = "NotifyUnresolvedTextBox";
			this.NotifyUnresolvedTextBox.Size = new System.Drawing.Size(84, 23);
			this.NotifyUnresolvedTextBox.TabIndex = 5;
			// 
			// label3
			// 
			this.label3.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(289, 7);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(50, 15);
			this.label3.TabIndex = 6;
			this.label3.Text = "minutes";
			// 
			// IssueSettingsWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(455, 252);
			this.ControlBox = false;
			this.Controls.Add(this.groupBox2);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.Font = new System.Drawing.Font("Segoe UI", 9F);
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "IssueSettingsWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Notification Settings";
			this.groupBox2.ResumeLayout(false);
			this.tableLayoutPanel1.ResumeLayout(false);
			this.tableLayoutPanel1.PerformLayout();
			this.tableLayoutPanel2.ResumeLayout(false);
			this.tableLayoutPanel2.PerformLayout();
			this.flowLayoutPanel3.ResumeLayout(false);
			this.flowLayoutPanel3.PerformLayout();
			this.flowLayoutPanel1.ResumeLayout(false);
			this.flowLayoutPanel1.PerformLayout();
			this.flowLayoutPanel2.ResumeLayout(false);
			this.flowLayoutPanel2.PerformLayout();
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
		private System.Windows.Forms.CheckBox NotifyUnassignedCheckBox;
		private System.Windows.Forms.TextBox NotifyUnassignedTextBox;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel2;
		private System.Windows.Forms.CheckBox NotifyUnresolvedCheckBox;
		private System.Windows.Forms.TextBox NotifyUnresolvedTextBox;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel3;
		private System.Windows.Forms.CheckBox NotifyUnacknowledgedCheckBox;
		private System.Windows.Forms.TextBox NotifyUnacknowledgedTextBox;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.CheckBox NotifyProjectsCheckBox;
		private TextBoxWithCueBanner NotifyProjectsTextBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
	}
}
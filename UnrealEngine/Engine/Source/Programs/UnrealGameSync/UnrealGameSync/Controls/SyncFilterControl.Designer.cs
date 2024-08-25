namespace UnrealGameSync.Controls
{
	partial class SyncFilterControl
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

		#region Component Designer generated code

		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			ViewGroupBox = new System.Windows.Forms.GroupBox();
			tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			ViewTextBox = new System.Windows.Forms.TextBox();
			SyntaxButton = new System.Windows.Forms.LinkLabel();
			CategoriesGroupBox = new System.Windows.Forms.GroupBox();
			tableLayoutPanel4 = new System.Windows.Forms.TableLayoutPanel();
			CategoriesCheckList = new System.Windows.Forms.CheckedListBox();
			SplitContainer = new System.Windows.Forms.SplitContainer();
			tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			groupBox1 = new System.Windows.Forms.GroupBox();
			tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
			SyncAllProjects = new System.Windows.Forms.CheckBox();
			IncludeAllProjectsInSolution = new System.Windows.Forms.CheckBox();
			ViewGroupBox.SuspendLayout();
			tableLayoutPanel2.SuspendLayout();
			CategoriesGroupBox.SuspendLayout();
			tableLayoutPanel4.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)SplitContainer).BeginInit();
			SplitContainer.Panel1.SuspendLayout();
			SplitContainer.Panel2.SuspendLayout();
			SplitContainer.SuspendLayout();
			tableLayoutPanel1.SuspendLayout();
			groupBox1.SuspendLayout();
			tableLayoutPanel3.SuspendLayout();
			SuspendLayout();
			// 
			// ViewGroupBox
			// 
			ViewGroupBox.Controls.Add(tableLayoutPanel2);
			ViewGroupBox.Controls.Add(SyntaxButton);
			ViewGroupBox.Dock = System.Windows.Forms.DockStyle.Fill;
			ViewGroupBox.Location = new System.Drawing.Point(0, 0);
			ViewGroupBox.Name = "ViewGroupBox";
			ViewGroupBox.Padding = new System.Windows.Forms.Padding(16, 8, 16, 8);
			ViewGroupBox.Size = new System.Drawing.Size(1008, 225);
			ViewGroupBox.TabIndex = 5;
			ViewGroupBox.TabStop = false;
			ViewGroupBox.Text = "Custom View";
			// 
			// tableLayoutPanel2
			// 
			tableLayoutPanel2.ColumnCount = 1;
			tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel2.Controls.Add(ViewTextBox, 0, 0);
			tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
			tableLayoutPanel2.Location = new System.Drawing.Point(16, 23);
			tableLayoutPanel2.Name = "tableLayoutPanel2";
			tableLayoutPanel2.RowCount = 1;
			tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel2.Size = new System.Drawing.Size(976, 194);
			tableLayoutPanel2.TabIndex = 8;
			// 
			// ViewTextBox
			// 
			ViewTextBox.AcceptsReturn = true;
			ViewTextBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
			ViewTextBox.Dock = System.Windows.Forms.DockStyle.Fill;
			ViewTextBox.Font = new System.Drawing.Font("Courier New", 8F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point);
			ViewTextBox.Location = new System.Drawing.Point(7, 7);
			ViewTextBox.Margin = new System.Windows.Forms.Padding(7);
			ViewTextBox.Multiline = true;
			ViewTextBox.Name = "ViewTextBox";
			ViewTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
			ViewTextBox.Size = new System.Drawing.Size(962, 180);
			ViewTextBox.TabIndex = 6;
			ViewTextBox.WordWrap = false;
			// 
			// SyntaxButton
			// 
			SyntaxButton.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right;
			SyntaxButton.AutoSize = true;
			SyntaxButton.Location = new System.Drawing.Point(933, 0);
			SyntaxButton.Name = "SyntaxButton";
			SyntaxButton.Size = new System.Drawing.Size(40, 13);
			SyntaxButton.TabIndex = 7;
			SyntaxButton.TabStop = true;
			SyntaxButton.Text = "Syntax";
			SyntaxButton.LinkClicked += SyntaxButton_LinkClicked;
			// 
			// CategoriesGroupBox
			// 
			CategoriesGroupBox.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			CategoriesGroupBox.Controls.Add(tableLayoutPanel4);
			CategoriesGroupBox.Dock = System.Windows.Forms.DockStyle.Fill;
			CategoriesGroupBox.Location = new System.Drawing.Point(0, 86);
			CategoriesGroupBox.Margin = new System.Windows.Forms.Padding(0, 3, 0, 0);
			CategoriesGroupBox.Name = "CategoriesGroupBox";
			CategoriesGroupBox.Padding = new System.Windows.Forms.Padding(16, 8, 16, 8);
			CategoriesGroupBox.Size = new System.Drawing.Size(1008, 344);
			CategoriesGroupBox.TabIndex = 4;
			CategoriesGroupBox.TabStop = false;
			CategoriesGroupBox.Text = "Categories";
			// 
			// tableLayoutPanel4
			// 
			tableLayoutPanel4.ColumnCount = 1;
			tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel4.Controls.Add(CategoriesCheckList, 0, 0);
			tableLayoutPanel4.Dock = System.Windows.Forms.DockStyle.Fill;
			tableLayoutPanel4.Location = new System.Drawing.Point(16, 23);
			tableLayoutPanel4.Name = "tableLayoutPanel4";
			tableLayoutPanel4.RowCount = 1;
			tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel4.Size = new System.Drawing.Size(976, 313);
			tableLayoutPanel4.TabIndex = 8;
			// 
			// CategoriesCheckList
			// 
			CategoriesCheckList.BorderStyle = System.Windows.Forms.BorderStyle.None;
			CategoriesCheckList.CheckOnClick = true;
			CategoriesCheckList.Dock = System.Windows.Forms.DockStyle.Fill;
			CategoriesCheckList.FormattingEnabled = true;
			CategoriesCheckList.IntegralHeight = false;
			CategoriesCheckList.Location = new System.Drawing.Point(3, 3);
			CategoriesCheckList.Name = "CategoriesCheckList";
			CategoriesCheckList.Size = new System.Drawing.Size(970, 307);
			CategoriesCheckList.Sorted = true;
			CategoriesCheckList.TabIndex = 7;
			// 
			// SplitContainer
			// 
			SplitContainer.Dock = System.Windows.Forms.DockStyle.Fill;
			SplitContainer.Location = new System.Drawing.Point(7, 7);
			SplitContainer.Name = "SplitContainer";
			SplitContainer.Orientation = System.Windows.Forms.Orientation.Horizontal;
			// 
			// SplitContainer.Panel1
			// 
			SplitContainer.Panel1.Controls.Add(tableLayoutPanel1);
			// 
			// SplitContainer.Panel2
			// 
			SplitContainer.Panel2.Controls.Add(ViewGroupBox);
			SplitContainer.Size = new System.Drawing.Size(1008, 667);
			SplitContainer.SplitterDistance = 430;
			SplitContainer.SplitterWidth = 12;
			SplitContainer.TabIndex = 8;
			// 
			// tableLayoutPanel1
			// 
			tableLayoutPanel1.AutoSize = true;
			tableLayoutPanel1.ColumnCount = 1;
			tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel1.Controls.Add(groupBox1, 0, 0);
			tableLayoutPanel1.Controls.Add(CategoriesGroupBox, 0, 1);
			tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
			tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
			tableLayoutPanel1.Margin = new System.Windows.Forms.Padding(0);
			tableLayoutPanel1.Name = "tableLayoutPanel1";
			tableLayoutPanel1.RowCount = 2;
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
			tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel1.Size = new System.Drawing.Size(1008, 430);
			tableLayoutPanel1.TabIndex = 8;
			// 
			// groupBox1
			// 
			groupBox1.AutoSize = true;
			groupBox1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			groupBox1.Controls.Add(tableLayoutPanel3);
			groupBox1.Dock = System.Windows.Forms.DockStyle.Fill;
			groupBox1.Location = new System.Drawing.Point(0, 3);
			groupBox1.Margin = new System.Windows.Forms.Padding(0, 3, 0, 3);
			groupBox1.Name = "groupBox1";
			groupBox1.Padding = new System.Windows.Forms.Padding(16, 8, 16, 8);
			groupBox1.Size = new System.Drawing.Size(1008, 77);
			groupBox1.TabIndex = 8;
			groupBox1.TabStop = false;
			groupBox1.Text = "General";
			// 
			// tableLayoutPanel3
			// 
			tableLayoutPanel3.AutoSize = true;
			tableLayoutPanel3.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			tableLayoutPanel3.ColumnCount = 1;
			tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			tableLayoutPanel3.Controls.Add(SyncAllProjects, 0, 0);
			tableLayoutPanel3.Controls.Add(IncludeAllProjectsInSolution, 0, 1);
			tableLayoutPanel3.Dock = System.Windows.Forms.DockStyle.Fill;
			tableLayoutPanel3.Location = new System.Drawing.Point(16, 23);
			tableLayoutPanel3.Name = "tableLayoutPanel3";
			tableLayoutPanel3.RowCount = 2;
			tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			tableLayoutPanel3.Size = new System.Drawing.Size(976, 46);
			tableLayoutPanel3.TabIndex = 8;
			// 
			// SyncAllProjects
			// 
			SyncAllProjects.Anchor = System.Windows.Forms.AnchorStyles.Left;
			SyncAllProjects.AutoSize = true;
			SyncAllProjects.Location = new System.Drawing.Point(3, 3);
			SyncAllProjects.Name = "SyncAllProjects";
			SyncAllProjects.Size = new System.Drawing.Size(158, 17);
			SyncAllProjects.TabIndex = 6;
			SyncAllProjects.Text = "Sync all projects in stream";
			SyncAllProjects.UseVisualStyleBackColor = true;
			// 
			// IncludeAllProjectsInSolution
			// 
			IncludeAllProjectsInSolution.Anchor = System.Windows.Forms.AnchorStyles.Left;
			IncludeAllProjectsInSolution.AutoSize = true;
			IncludeAllProjectsInSolution.Location = new System.Drawing.Point(3, 26);
			IncludeAllProjectsInSolution.Name = "IncludeAllProjectsInSolution";
			IncludeAllProjectsInSolution.Size = new System.Drawing.Size(220, 17);
			IncludeAllProjectsInSolution.TabIndex = 7;
			IncludeAllProjectsInSolution.Text = "Include all synced projects in solution";
			IncludeAllProjectsInSolution.UseVisualStyleBackColor = true;
			// 
			// SyncFilterControl
			// 
			AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			BackColor = System.Drawing.SystemColors.Window;
			Controls.Add(SplitContainer);
			Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point);
			Name = "SyncFilterControl";
			Padding = new System.Windows.Forms.Padding(7);
			Size = new System.Drawing.Size(1022, 681);
			ViewGroupBox.ResumeLayout(false);
			ViewGroupBox.PerformLayout();
			tableLayoutPanel2.ResumeLayout(false);
			tableLayoutPanel2.PerformLayout();
			CategoriesGroupBox.ResumeLayout(false);
			tableLayoutPanel4.ResumeLayout(false);
			SplitContainer.Panel1.ResumeLayout(false);
			SplitContainer.Panel1.PerformLayout();
			SplitContainer.Panel2.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)SplitContainer).EndInit();
			SplitContainer.ResumeLayout(false);
			tableLayoutPanel1.ResumeLayout(false);
			tableLayoutPanel1.PerformLayout();
			groupBox1.ResumeLayout(false);
			groupBox1.PerformLayout();
			tableLayoutPanel3.ResumeLayout(false);
			tableLayoutPanel3.PerformLayout();
			ResumeLayout(false);
		}

		#endregion

		private System.Windows.Forms.GroupBox ViewGroupBox;
		private System.Windows.Forms.GroupBox CategoriesGroupBox;
		public System.Windows.Forms.CheckedListBox CategoriesCheckList;
		private System.Windows.Forms.SplitContainer SplitContainer;
		private System.Windows.Forms.LinkLabel SyntaxButton;
		private System.Windows.Forms.TextBox ViewTextBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.GroupBox groupBox1;
		public System.Windows.Forms.CheckBox SyncAllProjects;
		public System.Windows.Forms.CheckBox IncludeAllProjectsInSolution;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel4;
	}
}

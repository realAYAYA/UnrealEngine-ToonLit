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
			this.ViewGroupBox = new System.Windows.Forms.GroupBox();
			this.SyntaxButton = new System.Windows.Forms.LinkLabel();
			this.ViewTextBox = new System.Windows.Forms.TextBox();
			this.CategoriesGroupBox = new System.Windows.Forms.GroupBox();
			this.CategoriesCheckList = new System.Windows.Forms.CheckedListBox();
			this.SplitContainer = new System.Windows.Forms.SplitContainer();
			this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.IncludeAllProjectsInSolution = new System.Windows.Forms.CheckBox();
			this.SyncAllProjects = new System.Windows.Forms.CheckBox();
			this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			this.ViewGroupBox.SuspendLayout();
			this.CategoriesGroupBox.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.SplitContainer)).BeginInit();
			this.SplitContainer.Panel1.SuspendLayout();
			this.SplitContainer.Panel2.SuspendLayout();
			this.SplitContainer.SuspendLayout();
			this.tableLayoutPanel1.SuspendLayout();
			this.groupBox1.SuspendLayout();
			this.tableLayoutPanel2.SuspendLayout();
			this.SuspendLayout();
			// 
			// ViewGroupBox
			// 
			this.ViewGroupBox.Controls.Add(this.SyntaxButton);
			this.ViewGroupBox.Controls.Add(this.ViewTextBox);
			this.ViewGroupBox.Dock = System.Windows.Forms.DockStyle.Fill;
			this.ViewGroupBox.Location = new System.Drawing.Point(0, 0);
			this.ViewGroupBox.Name = "ViewGroupBox";
			this.ViewGroupBox.Size = new System.Drawing.Size(1008, 225);
			this.ViewGroupBox.TabIndex = 5;
			this.ViewGroupBox.TabStop = false;
			this.ViewGroupBox.Text = "Custom View";
			// 
			// SyntaxButton
			// 
			this.SyntaxButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.SyntaxButton.AutoSize = true;
			this.SyntaxButton.Location = new System.Drawing.Point(946, 0);
			this.SyntaxButton.Name = "SyntaxButton";
			this.SyntaxButton.Size = new System.Drawing.Size(41, 15);
			this.SyntaxButton.TabIndex = 7;
			this.SyntaxButton.TabStop = true;
			this.SyntaxButton.Text = "Syntax";
			this.SyntaxButton.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.SyntaxButton_LinkClicked);
			// 
			// ViewTextBox
			// 
			this.ViewTextBox.AcceptsReturn = true;
			this.ViewTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ViewTextBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.ViewTextBox.Font = new System.Drawing.Font("Courier New", 8F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.ViewTextBox.Location = new System.Drawing.Point(15, 25);
			this.ViewTextBox.Margin = new System.Windows.Forms.Padding(7);
			this.ViewTextBox.Multiline = true;
			this.ViewTextBox.Name = "ViewTextBox";
			this.ViewTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
			this.ViewTextBox.Size = new System.Drawing.Size(976, 188);
			this.ViewTextBox.TabIndex = 6;
			this.ViewTextBox.WordWrap = false;
			// 
			// CategoriesGroupBox
			// 
			this.CategoriesGroupBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.CategoriesGroupBox.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.CategoriesGroupBox.Controls.Add(this.tableLayoutPanel2);
			this.CategoriesGroupBox.Location = new System.Drawing.Point(0, 88);
			this.CategoriesGroupBox.Margin = new System.Windows.Forms.Padding(0, 3, 0, 0);
			this.CategoriesGroupBox.Name = "CategoriesGroupBox";
			this.CategoriesGroupBox.Size = new System.Drawing.Size(1008, 342);
			this.CategoriesGroupBox.TabIndex = 4;
			this.CategoriesGroupBox.TabStop = false;
			this.CategoriesGroupBox.Text = "Categories";
			// 
			// CategoriesCheckList
			// 
			this.CategoriesCheckList.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.CategoriesCheckList.CheckOnClick = true;
			this.CategoriesCheckList.Dock = System.Windows.Forms.DockStyle.Fill;
			this.CategoriesCheckList.FormattingEnabled = true;
			this.CategoriesCheckList.IntegralHeight = false;
			this.CategoriesCheckList.Location = new System.Drawing.Point(2, 2);
			this.CategoriesCheckList.Name = "CategoriesCheckList";
			this.CategoriesCheckList.Size = new System.Drawing.Size(248, 77);
			this.CategoriesCheckList.Sorted = true;
			this.CategoriesCheckList.TabIndex = 7;
			// 
			// SplitContainer
			// 
			this.SplitContainer.Dock = System.Windows.Forms.DockStyle.Fill;
			this.SplitContainer.Location = new System.Drawing.Point(7, 7);
			this.SplitContainer.Name = "SplitContainer";
			this.SplitContainer.Orientation = System.Windows.Forms.Orientation.Horizontal;
			// 
			// SplitContainer.Panel1
			// 
			this.SplitContainer.Panel1.Controls.Add(this.tableLayoutPanel1);
			// 
			// SplitContainer.Panel2
			// 
			this.SplitContainer.Panel2.Controls.Add(this.ViewGroupBox);
			this.SplitContainer.Size = new System.Drawing.Size(1008, 667);
			this.SplitContainer.SplitterDistance = 430;
			this.SplitContainer.SplitterWidth = 12;
			this.SplitContainer.TabIndex = 8;
			// 
			// tableLayoutPanel1
			// 
			this.tableLayoutPanel1.AutoSize = true;
			this.tableLayoutPanel1.ColumnCount = 1;
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Controls.Add(this.groupBox1, 0, 0);
			this.tableLayoutPanel1.Controls.Add(this.CategoriesGroupBox, 0, 1);
			this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
			this.tableLayoutPanel1.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel1.Name = "tableLayoutPanel1";
			this.tableLayoutPanel1.RowCount = 2;
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Size = new System.Drawing.Size(1008, 430);
			this.tableLayoutPanel1.TabIndex = 8;
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.IncludeAllProjectsInSolution);
			this.groupBox1.Controls.Add(this.SyncAllProjects);
			this.groupBox1.Location = new System.Drawing.Point(0, 3);
			this.groupBox1.Margin = new System.Windows.Forms.Padding(0, 3, 0, 3);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(1008, 79);
			this.groupBox1.TabIndex = 8;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "General";
			// 
			// IncludeAllProjectsInSolution
			// 
			this.IncludeAllProjectsInSolution.AutoSize = true;
			this.IncludeAllProjectsInSolution.Location = new System.Drawing.Point(12, 48);
			this.IncludeAllProjectsInSolution.Name = "IncludeAllProjectsInSolution";
			this.IncludeAllProjectsInSolution.Size = new System.Drawing.Size(224, 19);
			this.IncludeAllProjectsInSolution.TabIndex = 7;
			this.IncludeAllProjectsInSolution.Text = "Include all synced projects in solution";
			this.IncludeAllProjectsInSolution.UseVisualStyleBackColor = true;
			// 
			// SyncAllProjects
			// 
			this.SyncAllProjects.AutoSize = true;
			this.SyncAllProjects.Location = new System.Drawing.Point(12, 23);
			this.SyncAllProjects.Name = "SyncAllProjects";
			this.SyncAllProjects.Size = new System.Drawing.Size(163, 19);
			this.SyncAllProjects.TabIndex = 6;
			this.SyncAllProjects.Text = "Sync all projects in stream";
			this.SyncAllProjects.UseVisualStyleBackColor = true;
			// 
			// tableLayoutPanel2
			// 
			this.tableLayoutPanel2.ColumnCount = 1;
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel2.Controls.Add(this.CategoriesCheckList, 0, 0);
			this.tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tableLayoutPanel2.Location = new System.Drawing.Point(2, 10);
			this.tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(9);
			this.tableLayoutPanel2.Name = "tableLayoutPanel2";
			this.tableLayoutPanel2.RowCount = 1;
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel2.Size = new System.Drawing.Size(501, 160);
			this.tableLayoutPanel2.TabIndex = 8;
			// 
			// SyncFilterControl
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.BackColor = System.Drawing.SystemColors.Window;
			this.Controls.Add(this.SplitContainer);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Name = "SyncFilterControl";
			this.Padding = new System.Windows.Forms.Padding(7);
			this.Size = new System.Drawing.Size(1022, 681);
			this.ViewGroupBox.ResumeLayout(false);
			this.ViewGroupBox.PerformLayout();
			this.CategoriesGroupBox.ResumeLayout(false);
			this.SplitContainer.Panel1.ResumeLayout(false);
			this.SplitContainer.Panel1.PerformLayout();
			this.SplitContainer.Panel2.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.SplitContainer)).EndInit();
			this.SplitContainer.ResumeLayout(false);
			this.tableLayoutPanel1.ResumeLayout(false);
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.tableLayoutPanel2.ResumeLayout(false);
			this.ResumeLayout(false);

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
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
	}
}

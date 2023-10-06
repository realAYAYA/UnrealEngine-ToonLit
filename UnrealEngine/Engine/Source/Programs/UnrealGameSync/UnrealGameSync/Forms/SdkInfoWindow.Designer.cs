namespace UnrealGameSync
{
	partial class SdkInfoWindow
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
			System.Windows.Forms.ColumnHeader columnHeader1;
			System.Windows.Forms.ListViewGroup listViewGroup1 = new System.Windows.Forms.ListViewGroup("ListViewGroup", System.Windows.Forms.HorizontalAlignment.Left);
			this.OkBtn = new System.Windows.Forms.Button();
			this.SdkListView = new UnrealGameSync.CustomListViewControl();
			this.columnHeader2 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader3 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			columnHeader1 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.SuspendLayout();
			// 
			// columnHeader1
			// 
			columnHeader1.Text = "";
			columnHeader1.Width = 17;
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(501, 412);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(87, 27);
			this.OkBtn.TabIndex = 1;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// SdkListView
			// 
			this.SdkListView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.SdkListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            columnHeader1,
            this.columnHeader2,
            this.columnHeader3});
			this.SdkListView.FullRowSelect = true;
			listViewGroup1.Header = "ListViewGroup";
			listViewGroup1.Name = "listViewGroup1";
			this.SdkListView.Groups.AddRange(new System.Windows.Forms.ListViewGroup[] {
            listViewGroup1});
			this.SdkListView.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.None;
			this.SdkListView.HideSelection = false;
			this.SdkListView.Location = new System.Drawing.Point(12, 12);
			this.SdkListView.Name = "SdkListView";
			this.SdkListView.OwnerDraw = true;
			this.SdkListView.Size = new System.Drawing.Size(576, 394);
			this.SdkListView.TabIndex = 4;
			this.SdkListView.UseCompatibleStateImageBehavior = false;
			this.SdkListView.View = System.Windows.Forms.View.Details;
			this.SdkListView.DrawItem += new System.Windows.Forms.DrawListViewItemEventHandler(this.SdkListView_DrawItem);
			this.SdkListView.DrawSubItem += new System.Windows.Forms.DrawListViewSubItemEventHandler(this.SdkListView_DrawSubItem);
			this.SdkListView.MouseDown += new System.Windows.Forms.MouseEventHandler(this.SdkListView_MouseDown);
			this.SdkListView.MouseLeave += new System.EventHandler(this.SdkListView_MouseLeave);
			this.SdkListView.MouseMove += new System.Windows.Forms.MouseEventHandler(this.SdkListView_MouseMove);
			// 
			// columnHeader2
			// 
			this.columnHeader2.Text = "Requirements";
			this.columnHeader2.Width = 377;
			// 
			// columnHeader3
			// 
			this.columnHeader3.Text = "Links";
			this.columnHeader3.Width = 158;
			// 
			// SdkInfoWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.ClientSize = new System.Drawing.Size(600, 451);
			this.Controls.Add(this.SdkListView);
			this.Controls.Add(this.OkBtn);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "SdkInfoWindow";
			this.ShowIcon = false;
			this.ShowInTaskbar = false;
			this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "SDK Info";
			this.ResumeLayout(false);

		}

		#endregion
		private System.Windows.Forms.Button OkBtn;
		private CustomListViewControl SdkListView;
		private System.Windows.Forms.ColumnHeader columnHeader2;
		private System.Windows.Forms.ColumnHeader columnHeader3;
	}
}
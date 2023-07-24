namespace Perforce.sln.bld.gui
{
    partial class DepotPathDlg
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
			System.Windows.Forms.TreeNode treeNode1 = new System.Windows.Forms.TreeNode("Node1");
			System.Windows.Forms.TreeNode treeNode2 = new System.Windows.Forms.TreeNode("Depot", 0, 24, new System.Windows.Forms.TreeNode[] {
            treeNode1});
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(DepotPathDlg));
			this.mDepotTreeView = new System.Windows.Forms.TreeView();
			this.DepotTreeViewImageList = new System.Windows.Forms.ImageList(this.components);
			this.mSolutionPathTxt = new System.Windows.Forms.TextBox();
			this.mOkBtn = new System.Windows.Forms.Button();
			this.mCancelBtn = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// mDepotTreeView
			// 
			this.mDepotTreeView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
						| System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.mDepotTreeView.ImageIndex = 0;
			this.mDepotTreeView.ImageList = this.DepotTreeViewImageList;
			this.mDepotTreeView.Location = new System.Drawing.Point(0, 0);
			this.mDepotTreeView.Name = "mDepotTreeView";
			treeNode1.Name = "Node1";
			treeNode1.Text = "Node1";
			treeNode2.ImageIndex = 0;
			treeNode2.Name = "Node0";
			treeNode2.SelectedImageIndex = 24;
			treeNode2.Text = "Depot";
			this.mDepotTreeView.Nodes.AddRange(new System.Windows.Forms.TreeNode[] {
            treeNode2});
			this.mDepotTreeView.SelectedImageIndex = 0;
			this.mDepotTreeView.Size = new System.Drawing.Size(304, 403);
			this.mDepotTreeView.TabIndex = 2;
			this.mDepotTreeView.BeforeExpand += new System.Windows.Forms.TreeViewCancelEventHandler(this.mDepotTreeView_BeforeExpand);
			this.mDepotTreeView.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.mDepotTreeView_AfterSelect);
			// 
			// DepotTreeViewImageList
			// 
			this.DepotTreeViewImageList.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("DepotTreeViewImageList.ImageStream")));
			this.DepotTreeViewImageList.TransparentColor = System.Drawing.Color.Transparent;
			this.DepotTreeViewImageList.Images.SetKeyName(0, "depot.png");
			this.DepotTreeViewImageList.Images.SetKeyName(1, "folder.png");
			this.DepotTreeViewImageList.Images.SetKeyName(2, "GreyFile.png");
			// 
			// mSolutionPathTxt
			// 
			this.mSolutionPathTxt.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.mSolutionPathTxt.Location = new System.Drawing.Point(0, 401);
			this.mSolutionPathTxt.Name = "mSolutionPathTxt";
			this.mSolutionPathTxt.Size = new System.Drawing.Size(304, 20);
			this.mSolutionPathTxt.TabIndex = 3;
			this.mSolutionPathTxt.TextChanged += new System.EventHandler(this.mSolutionPathTxt_TextChanged);
			// 
			// mOkBtn
			// 
			this.mOkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.mOkBtn.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.mOkBtn.Enabled = false;
			this.mOkBtn.Location = new System.Drawing.Point(136, 427);
			this.mOkBtn.Name = "mOkBtn";
			this.mOkBtn.Size = new System.Drawing.Size(75, 23);
			this.mOkBtn.TabIndex = 4;
			this.mOkBtn.Text = "OK";
			this.mOkBtn.UseVisualStyleBackColor = true;
			this.mOkBtn.Click += new System.EventHandler(this.mOkBtn_Click);
			// 
			// mCancelBtn
			// 
			this.mCancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.mCancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.mCancelBtn.Location = new System.Drawing.Point(217, 427);
			this.mCancelBtn.Name = "mCancelBtn";
			this.mCancelBtn.Size = new System.Drawing.Size(75, 23);
			this.mCancelBtn.TabIndex = 5;
			this.mCancelBtn.Text = "Cancel";
			this.mCancelBtn.UseVisualStyleBackColor = true;
			// 
			// DepotPathDlg
			// 
			this.AcceptButton = this.mOkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.mCancelBtn;
			this.ClientSize = new System.Drawing.Size(304, 462);
			this.Controls.Add(this.mCancelBtn);
			this.Controls.Add(this.mOkBtn);
			this.Controls.Add(this.mSolutionPathTxt);
			this.Controls.Add(this.mDepotTreeView);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MinimumSize = new System.Drawing.Size(320, 300);
			this.Name = "DepotPathDlg";
			this.Text = "Choose Solution in Depot";
			this.ResumeLayout(false);
			this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TreeView mDepotTreeView;
        private System.Windows.Forms.TextBox mSolutionPathTxt;
        private System.Windows.Forms.Button mOkBtn;
        private System.Windows.Forms.Button mCancelBtn;
        private System.Windows.Forms.ImageList DepotTreeViewImageList;
    }
}

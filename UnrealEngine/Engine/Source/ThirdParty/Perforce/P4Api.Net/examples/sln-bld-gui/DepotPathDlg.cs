using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;
using Perforce;

namespace Perforce.sln.bld.gui
{
    public partial class DepotPathDlg : Form
    {
        public DepotPathDlg(P4.Repository rep)
        {
            InitializeComponent();

            Init(rep);
        }

        public String SelectedFile
        {
            get
            {
                return mSolutionPathTxt.Text;
            }
        }

		private P4.P4Server pServer;

		public void Init(P4.Repository rep)
        {
            if (rep == null)
            {
                mDepotTreeView.Enabled = false;
                return;
            }
            // Initialize the depot tree view
			P4.P4Directory root = new P4.P4Directory(rep, null, "depot", "//depot", null, null);
            TreeNode rootNode = new TreeNode("Depot");
            rootNode.Tag = root;
            rootNode.ImageIndex = 0;
            rootNode.SelectedImageIndex = 0;
            rootNode.Nodes.Add(new TreeNode("empty"));
            mDepotTreeView.Nodes.Clear();
            mDepotTreeView.Nodes.Add(rootNode);
        }

        private void mDepotTreeView_BeforeExpand(object sender, TreeViewCancelEventArgs e)
        {
            // insanity check, should never be null
            if (e.Node == null)
            {
                e.Cancel = true;
                return;
            }

            TreeNode node = e.Node;
            // clear any old data
            node.Nodes.Clear();

			P4.P4Directory p4Dir = (P4.P4Directory)node.Tag;

            if (String.IsNullOrEmpty(p4Dir.DepotPath) || !p4Dir.Expand())
            {
                e.Cancel = true;
                return;
            }

            if ((p4Dir.Files != null) && (p4Dir.Files.Count > 0))
            {
				foreach (P4.FileMetaData file in p4Dir.Files)
                {
                    if (Path.GetExtension(file.DepotPath.Path) == ".sln")
                    {
                        TreeNode child = new TreeNode(file.DepotPath.Path);
                        child.Tag = file;
                        child.ImageIndex = 2;
                        child.SelectedImageIndex = 2;
                        e.Node.Nodes.Add(child);
                    }
                }
            }

            if ((p4Dir.Subdirectories != null) && (p4Dir.Subdirectories.Count > 0))
            {
				foreach (P4.P4Directory p4SubDir in p4Dir.Subdirectories)
                {
                    if (!p4SubDir.InDepot)
                        continue;

                    TreeNode child = new TreeNode(p4SubDir.Name);
                    child.Tag = p4SubDir;
                    child.ImageIndex = 1;
                    child.SelectedImageIndex = 1;
                    child.Nodes.Add(new TreeNode("<empty>"));
                    e.Node.Nodes.Add(child);
                }
            }
        }

        private void mDepotTreeView_AfterSelect(object sender, TreeViewEventArgs e)
        {
            Object obj  = mDepotTreeView.SelectedNode.Tag;
			if (obj is P4.FileMetaData)
			{
				mSolutionPathTxt.Text = mDepotTreeView.SelectedNode.Text.ToString();
			}
			else
			{
				mSolutionPathTxt.Text = string.Empty;
			}
        }

		private void mOkBtn_Click(object sender, EventArgs e)
		{

		}

		private void mSolutionPathTxt_TextChanged(object sender, EventArgs e)
		{
			string path = mSolutionPathTxt.Text;
			
			mOkBtn.Enabled = !string.IsNullOrEmpty(path);
		}
    }
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics.CodeAnalysis;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

#nullable enable

namespace UnrealGameSync
{
	partial class SelectProjectFromWorkspaceWindow : Form
	{
		static class EnumerateWorkspaceProjectsTask
		{
			static readonly string[] Patterns =
			{
				"....uproject",
				"....uprojectdirs",
			};

			public static async Task<List<string>> RunAsync(IPerforceConnection perforce, string clientName, CancellationToken cancellationToken)
			{
				ClientRecord clientSpec = await perforce.GetClientAsync(perforce.Settings.ClientName, cancellationToken);

				string clientRoot = clientSpec.Root.TrimEnd(Path.DirectorySeparatorChar);
				if (String.IsNullOrEmpty(clientRoot))
				{
					throw new UserErrorException($"Client '{clientName}' does not have a valid root directory.");
				}

				List<FStatRecord> fileRecords = new List<FStatRecord>();
				foreach (string pattern in Patterns)
				{
					string filter = String.Format("//{0}/{1}", clientName, pattern);

					List<FStatRecord> wildcardFileRecords = await perforce.FStatAsync(filter, cancellationToken).ToListAsync(cancellationToken);
					wildcardFileRecords.RemoveAll(x => x.HeadAction == FileAction.Delete || x.HeadAction == FileAction.MoveDelete);

					fileRecords.AddRange(wildcardFileRecords);
				}

				string clientPrefix = clientRoot;
				if (!clientPrefix.EndsWith(Path.DirectorySeparatorChar))
				{
					clientPrefix += Path.DirectorySeparatorChar;
				}

				List<string> paths = new List<string>();
				foreach (FStatRecord fileRecord in fileRecords)
				{
					if (fileRecord.ClientFile != null && fileRecord.ClientFile.StartsWith(clientPrefix, StringComparison.OrdinalIgnoreCase))
					{
						paths.Add(fileRecord.ClientFile.Substring(clientRoot.Length).Replace(Path.DirectorySeparatorChar, '/'));
					}
				}

				return paths;
			}
		}

		[DllImport("Shell32.dll", EntryPoint = "ExtractIconExW", CharSet = CharSet.Unicode, ExactSpelling = true, CallingConvention = CallingConvention.StdCall)]
		private static extern int ExtractIconEx(string sFile, int iIndex, IntPtr piLargeVersion, out IntPtr piSmallVersion, int amountIcons);

		readonly List<string> _projectFiles;
		string _selectedProjectFile;

		class ProjectNode
		{
			public string FullName;
			public string Folder;
			public string Name;

			public ProjectNode(string fullName)
			{
				FullName = fullName;

				int slashIdx = fullName.LastIndexOf('/');
				Folder = fullName.Substring(0, slashIdx);
				Name = fullName.Substring(slashIdx + 1);
			}
		}

		public SelectProjectFromWorkspaceWindow(string workspaceName, List<string> projectFiles, string selectedProjectFile)
		{
			InitializeComponent();
			Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));

			_projectFiles = projectFiles;
			_selectedProjectFile = selectedProjectFile;

			// Make the image strip containing icons for nodes in the tree
			IntPtr folderIconPtr;
			ExtractIconEx("imageres.dll", 3, IntPtr.Zero, out folderIconPtr, 1);

			Icon[] icons = new Icon[] { Icon.FromHandle(folderIconPtr), Properties.Resources.Icon };

			Bitmap typeImageListBitmap = new Bitmap(icons.Length * 16, 16, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
			using (Graphics graphics = Graphics.FromImage(typeImageListBitmap))
			{
				for (int iconIdx = 0; iconIdx < icons.Length; iconIdx++)
				{
					graphics.DrawIcon(icons[iconIdx], new Rectangle(iconIdx * 16, 0, 16, 16));
				}
			}

			ImageList typeImageList = new ImageList();
			typeImageList.ImageSize = new Size(16, 16);
			typeImageList.ColorDepth = ColorDepth.Depth32Bit;
			typeImageList.Images.AddStrip(typeImageListBitmap);
			ProjectTreeView.ImageList = typeImageList;

			// Create the root node
			TreeNode rootNode = new TreeNode();
			rootNode.Text = workspaceName;
			rootNode.Expand();
			ProjectTreeView.Nodes.Add(rootNode);

			// Populate the tree
			Populate();
		}

		private void Populate()
		{
			// Clear out the existing nodes
			TreeNode rootNode = ProjectTreeView.Nodes[0];
			rootNode.Nodes.Clear();

			// Filter the project files
			List<string> filteredProjectFiles = new List<string>(_projectFiles);
			if (!ShowProjectDirsFiles.Checked)
			{
				filteredProjectFiles.RemoveAll(x => x.EndsWith(".uprojectdirs", StringComparison.OrdinalIgnoreCase));
			}

			// Sort by paths, then files
			List<ProjectNode> projectNodes = filteredProjectFiles.Select(x => new ProjectNode(x)).OrderBy(x => x.Folder).ThenBy(x => x.Name).ToList();

			// Add the folders for each project
			TreeNode[] projectParentNodes = new TreeNode[projectNodes.Count];
			for (int idx = 0; idx < projectNodes.Count; idx++)
			{
				TreeNode parentNode = rootNode;
				if (projectNodes[idx].Folder.Length > 0)
				{
					string[] fragments = projectNodes[idx].Folder.Split(new char[] { '/' }, StringSplitOptions.RemoveEmptyEntries);
					foreach (string fragment in fragments)
					{
						parentNode = FindOrAddChildNode(parentNode, fragment, 0);
					}
				}
				projectParentNodes[idx] = parentNode;
			}

			// Add the actual project nodes themselves
			for (int idx = 0; idx < projectNodes.Count; idx++)
			{
				TreeNode node = FindOrAddChildNode(projectParentNodes[idx], projectNodes[idx].Name, 1);
				node.Tag = projectNodes[idx].FullName;

				if (String.Equals(projectNodes[idx].FullName, _selectedProjectFile, StringComparison.OrdinalIgnoreCase))
				{
					ProjectTreeView.SelectedNode = node;
					for (TreeNode parentNode = node.Parent; parentNode != rootNode; parentNode = parentNode.Parent)
					{
						parentNode.Expand();
					}
				}
			}
		}

		static TreeNode FindOrAddChildNode(TreeNode parentNode, string text, int imageIndex)
		{
			foreach (TreeNode? childNode in parentNode.Nodes)
			{
				if (childNode != null && String.Equals(childNode.Text, text, StringComparison.OrdinalIgnoreCase))
				{
					return childNode;
				}
			}

			TreeNode nextNode = new TreeNode(text);
			nextNode.ImageIndex = imageIndex;
			nextNode.SelectedImageIndex = imageIndex;
			parentNode.Nodes.Add(nextNode);
			return nextNode;
		}

		public static bool ShowModal(IWin32Window owner, IPerforceSettings perforce, string workspaceName, string workspacePath, IServiceProvider serviceProvider, [NotNullWhen(true)] out string? newWorkspacePath)
		{
			perforce = new PerforceSettings(perforce) { ClientName = workspaceName };

			ILogger logger = serviceProvider.GetRequiredService<ILogger<SelectProjectFromWorkspaceWindow>>();

			ModalTask<List<string>>? pathsTask = PerforceModalTask.Execute(owner, "Finding Projects", "Finding projects, please wait...", perforce, (p, c) => EnumerateWorkspaceProjectsTask.RunAsync(p, workspaceName, c), logger);
			if (pathsTask == null || !pathsTask.Succeeded)
			{
				newWorkspacePath = null;
				return false;
			}

			using SelectProjectFromWorkspaceWindow selectProjectWindow = new SelectProjectFromWorkspaceWindow(workspaceName, pathsTask.Result, workspacePath);
			if (selectProjectWindow.ShowDialog() == DialogResult.OK && !String.IsNullOrEmpty(selectProjectWindow._selectedProjectFile))
			{
				newWorkspacePath = selectProjectWindow._selectedProjectFile;
				return true;
			}
			else
			{
				newWorkspacePath = null;
				return false;
			}
		}

		private void ProjectTreeView_AfterSelect(object sender, TreeViewEventArgs e)
		{
			OkBtn.Enabled = (e.Node?.Tag != null);
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			if (ProjectTreeView.SelectedNode != null && ProjectTreeView.SelectedNode.Tag != null)
			{
				_selectedProjectFile = (string)ProjectTreeView.SelectedNode.Tag;
				DialogResult = DialogResult.OK;
				Close();
			}
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void ShowProjectDirsFiles_CheckedChanged(object sender, EventArgs e)
		{
			Populate();
		}
	}
}

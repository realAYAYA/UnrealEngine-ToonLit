// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;

namespace UnrealGameSync
{
	partial class CleanWorkspaceWindow : Form
	{
		enum TreeNodeAction
		{
			Sync,
			Delete,
		}

		class TreeNodeData
		{
			public TreeNodeAction Action;
			public FileInfo? File;
			public FolderToClean? Folder;
			public int NumFiles;
			public int NumSelectedFiles;
			public int NumEmptySelectedFiles;
			public int NumMissingSelectedFiles;
			public int NumDefaultSelectedFiles;
		}

		enum SelectionType
		{
			All,
			SafeToDelete,
			Missing,
			Empty,
			None,
		}

		static readonly CheckBoxState[] CheckBoxStates = 
		{ 
			CheckBoxState.UncheckedNormal, 
			CheckBoxState.MixedNormal, 
			CheckBoxState.CheckedNormal 
		};

		IPerforceSettings _perforceSettings;
		FolderToClean _rootFolderToClean;

		static readonly string[] SafeToDeleteFolders =
		{
			"/binaries/",
			"/intermediate/",
			"/build/receipts/",
			"/.vs/",
			"/automationtool/saved/rules/",
			"/saved/logs/",
	
			"/bin/debug/",
			"/bin/development/",
			"/bin/release/",
			"/obj/debug/",
			"/obj/development/",
			"/obj/release/",

			"/bin/x86/debug/",
			"/bin/x86/development/",
			"/bin/x86/release/",
			"/obj/x86/debug/",
			"/obj/x86/development/",
			"/obj/x86/release/",

			"/bin/x64/debug/",
			"/bin/x64/development/",
			"/bin/x64/release/",
			"/obj/x64/debug/",
			"/obj/x64/development/",
			"/obj/x64/release/",
		};

		static readonly string[] SafeToDeleteExtensions =
		{
			".pdb",
			".obj",
			".sdf",
			".suo",
			".sln",
			".csproj.user",
			".csproj.references",
		};

		IReadOnlyList<string> _extraSafeToDeleteFolders;
		IReadOnlyList<string> _extraSafeToDeleteExtensions;
		ILogger _logger;

		[DllImport("Shell32.dll", EntryPoint = "ExtractIconExW", CharSet = CharSet.Unicode, ExactSpelling = true, CallingConvention = CallingConvention.StdCall)]
		private static extern int ExtractIconEx(string sFile, int iIndex, IntPtr piLargeVersion, out IntPtr piSmallVersion, int amountIcons);

		private CleanWorkspaceWindow(IPerforceSettings perforceSettings, FolderToClean rootFolderToClean, string[] extraSafeToDeleteFolders, string[] extraSafeToDeleteExtensions, ILogger<CleanWorkspaceWindow> logger)
		{
			this._perforceSettings = perforceSettings;
			this._rootFolderToClean = rootFolderToClean;
			this._extraSafeToDeleteFolders = extraSafeToDeleteFolders.Select(x => x.Trim().Replace('\\', '/').Trim('/')).Where(x => x.Length > 0).Select(x => String.Format("/{0}/", x.ToLowerInvariant())).ToArray();
			this._extraSafeToDeleteExtensions = extraSafeToDeleteExtensions.Select(x => x.Trim().ToLowerInvariant()).Where(x => x.Length > 0).ToArray();
			this._logger = logger;

			InitializeComponent();
		}

		public static void DoClean(IWin32Window owner, IPerforceSettings perforceSettings, DirectoryReference localRootPath, string clientRootPath, IReadOnlyList<string> syncPaths, string[] extraSafeToDeleteFolders, string[] extraSafeToDeleteExtensions, ILogger<CleanWorkspaceWindow> logger)
		{
			// Figure out which folders to clean
			FolderToClean rootFolderToClean = new FolderToClean(localRootPath.ToDirectoryInfo());
			using(FindFoldersToCleanTask queryWorkspace = new FindFoldersToCleanTask(perforceSettings, rootFolderToClean, clientRootPath, syncPaths, logger))
			{
				ModalTask? result = ModalTask.Execute(owner, "Clean Workspace", "Querying files in Perforce, please wait...", x => queryWorkspace.RunAsync(x), ModalTaskFlags.None);
				if (result == null || !result.Succeeded)
				{
					return;
				}
			}

			// If there's nothing to delete, don't bother displaying the dialog at all
			if(rootFolderToClean.FilesToDelete.Count == 0 && rootFolderToClean.NameToSubFolder.Count == 0)
			{
				MessageBox.Show("You have no local files which are not in Perforce.", "Workspace Clean", MessageBoxButtons.OK);
				return;
			}

			// Populate the tree
			CleanWorkspaceWindow cleanWorkspace = new CleanWorkspaceWindow(perforceSettings, rootFolderToClean, extraSafeToDeleteFolders, extraSafeToDeleteExtensions, logger);
			cleanWorkspace.ShowDialog();
		}

		private void CleanWorkspaceWindow_Load(object sender, EventArgs e)
		{
			IntPtr folderIconPtr;
			ExtractIconEx("imageres.dll", 3, IntPtr.Zero, out folderIconPtr, 1);

			IntPtr fileIconPtr;
			ExtractIconEx("imageres.dll", 2, IntPtr.Zero, out fileIconPtr, 1);

			Icon[] icons = new Icon[]{ Icon.FromHandle(folderIconPtr), Icon.FromHandle(fileIconPtr) };

			Size largestIconSize = Size.Empty;
			foreach(Icon icon in icons)
			{
				largestIconSize = new Size(Math.Max(largestIconSize.Width, icon.Width), Math.Max(largestIconSize.Height, icon.Height));
			}

			Size largestCheckBoxSize = Size.Empty;
			using(Graphics graphics = Graphics.FromHwnd(IntPtr.Zero))
			{
				foreach(CheckBoxState state in CheckBoxStates)
				{
					Size checkBoxSize = CheckBoxRenderer.GetGlyphSize(graphics, state);
					largestCheckBoxSize = new Size(Math.Max(largestCheckBoxSize.Width, checkBoxSize.Width), Math.Max(largestCheckBoxSize.Height, checkBoxSize.Height));
				}
			}

			Size imageSize = new Size(largestCheckBoxSize.Width + largestIconSize.Width, Math.Max(largestIconSize.Height, largestCheckBoxSize.Height));

			Bitmap typeImageListBitmap = new Bitmap(icons.Length * 3 * imageSize.Width, imageSize.Height, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
			using(Graphics graphics = Graphics.FromImage(typeImageListBitmap))
			{
				int minX = 0;
				for(int iconIdx = 0; iconIdx < icons.Length; iconIdx++)
				{
					for(int stateIdx = 0; stateIdx < 3; stateIdx++)
					{
						Size checkBoxSize = CheckBoxRenderer.GetGlyphSize(graphics, CheckBoxStates[stateIdx]);
						CheckBoxRenderer.DrawCheckBox(graphics, new Point(minX + (largestCheckBoxSize.Width - checkBoxSize.Width) / 2, (largestCheckBoxSize.Height - checkBoxSize.Height) / 2), CheckBoxStates[stateIdx]);

						Size iconSize = icons[iconIdx].Size;
						graphics.DrawIcon(icons[iconIdx], minX + largestCheckBoxSize.Width + (largestIconSize.Width - iconSize.Width) / 2, (largestIconSize.Height - iconSize.Height) / 2);

						minX += imageSize.Width;
					}
				}
			}

			ImageList typeImageList = new ImageList();
			typeImageList.ImageSize = imageSize;
			typeImageList.ColorDepth = ColorDepth.Depth32Bit;
			typeImageList.Images.AddStrip(typeImageListBitmap);
			TreeView.ImageList = typeImageList;

			TreeNode node = BuildTreeViewStructure(_rootFolderToClean, "/", false, 0);
			node.Text = _rootFolderToClean.Directory.FullName;
			TreeView.Nodes.Add(node);
		}

		private TreeNode BuildTreeViewStructure(FolderToClean folder, string folderPath, bool parentFolderSelected, int depth)
		{
			bool selectFolder = parentFolderSelected || IsSafeToDeleteFolder(folderPath) || folder.EmptyLeaf;

			TreeNodeData folderNodeData = new TreeNodeData();
			folderNodeData.Folder = folder;

			TreeNode folderNode = new TreeNode();
			folderNode.Text = folder.Name;
			folderNode.Tag = folderNodeData;

			foreach(FolderToClean subFolder in folder.NameToSubFolder.OrderBy(x => x.Key).Select(x => x.Value))
			{
				TreeNode childNode = BuildTreeViewStructure(subFolder, folderPath + subFolder.Name.ToLowerInvariant() + "/", selectFolder, depth + 1);
				folderNode.Nodes.Add(childNode);

				TreeNodeData childNodeData = (TreeNodeData)childNode.Tag;
				folderNodeData.NumFiles += childNodeData.NumFiles;
				folderNodeData.NumSelectedFiles += childNodeData.NumSelectedFiles;
				folderNodeData.NumEmptySelectedFiles += childNodeData.NumEmptySelectedFiles;
				folderNodeData.NumMissingSelectedFiles += childNodeData.NumMissingSelectedFiles;
				folderNodeData.NumDefaultSelectedFiles += childNodeData.NumDefaultSelectedFiles;
			}

			foreach(FileInfo file in folder.FilesToSync.OrderBy(x => x.Name))
			{
				TreeNodeData fileNodeData = new TreeNodeData();
				fileNodeData.Action = TreeNodeAction.Sync;
				fileNodeData.File = file;
				fileNodeData.NumFiles = 1;
				fileNodeData.NumSelectedFiles = 1;
				fileNodeData.NumEmptySelectedFiles = 0;
				fileNodeData.NumMissingSelectedFiles = 1;
				fileNodeData.NumDefaultSelectedFiles = fileNodeData.NumSelectedFiles;

				TreeNode fileNode = new TreeNode();
				fileNode.Text = file.Name + " (sync)";
				fileNode.Tag = fileNodeData;
				folderNode.Nodes.Add(fileNode);

				UpdateImage(fileNode);

				folderNodeData.NumFiles++;
				folderNodeData.NumSelectedFiles += fileNodeData.NumSelectedFiles;
				folderNodeData.NumEmptySelectedFiles += fileNodeData.NumEmptySelectedFiles;
				folderNodeData.NumMissingSelectedFiles += fileNodeData.NumMissingSelectedFiles;
				folderNodeData.NumDefaultSelectedFiles += fileNodeData.NumDefaultSelectedFiles;
			}

			foreach(FileInfo file in folder.FilesToDelete.OrderBy(x => x.Name))
			{
				string name = file.Name.ToLowerInvariant();

				bool selectFile = selectFolder || IsSafeToDeleteFile(folderPath, file.Name.ToLowerInvariant());

				TreeNodeData fileNodeData = new TreeNodeData();
				fileNodeData.Action = TreeNodeAction.Delete;
				fileNodeData.File = file;
				fileNodeData.NumFiles = 1;
				fileNodeData.NumSelectedFiles = selectFile? 1 : 0;
				fileNodeData.NumEmptySelectedFiles = 0;
				fileNodeData.NumMissingSelectedFiles = 0;
				fileNodeData.NumDefaultSelectedFiles = fileNodeData.NumSelectedFiles;

				TreeNode fileNode = new TreeNode();
				fileNode.Text = file.Name;
				fileNode.Tag = fileNodeData;
				folderNode.Nodes.Add(fileNode);

				UpdateImage(fileNode);

				folderNodeData.NumFiles++;
				folderNodeData.NumSelectedFiles += fileNodeData.NumSelectedFiles;
				folderNodeData.NumEmptySelectedFiles += fileNodeData.NumEmptySelectedFiles;
				folderNodeData.NumMissingSelectedFiles += fileNodeData.NumMissingSelectedFiles;
				folderNodeData.NumDefaultSelectedFiles += fileNodeData.NumDefaultSelectedFiles;
			}

			if(folderNodeData.Folder.EmptyLeaf)
			{
				folderNodeData.NumFiles++;
				folderNodeData.NumSelectedFiles++;
				folderNodeData.NumEmptySelectedFiles++;
				folderNodeData.NumDefaultSelectedFiles++;
			}

			if(folderNodeData.NumSelectedFiles > 0 && !folderNodeData.Folder.EmptyAfterClean && depth < 2)
			{
				folderNode.Expand();
			}
			else
			{
				folderNode.Collapse();
			}

			UpdateImage(folderNode);
			return folderNode;
		}

		private bool IsSafeToDeleteFolder(string folderPath)
		{
			return SafeToDeleteFolders.Any(x => folderPath.EndsWith(x)) || _extraSafeToDeleteFolders.Any(x => folderPath.EndsWith(x));
		}

		private bool IsSafeToDeleteFile(string folderPath, string name)
		{
			return SafeToDeleteExtensions.Any(x => name.EndsWith(x)) || _extraSafeToDeleteExtensions.Any(x => name.EndsWith(x));
		}

		private void TreeView_DrawNode(object sender, DrawTreeNodeEventArgs e)
		{
			e.Graphics.DrawLine(Pens.Black, new Point(e.Bounds.Left, e.Bounds.Top), new Point(e.Bounds.Right, e.Bounds.Bottom));
		}

		private void TreeView_NodeMouseClick(object sender, TreeNodeMouseClickEventArgs e)
		{
			TreeNode node = e.Node;
			if(e.Button == System.Windows.Forms.MouseButtons.Right)
			{
				TreeView.SelectedNode = e.Node;
				FolderContextMenu.Tag = e.Node;
				FolderContextMenu.Show(TreeView.PointToScreen(e.Location));
			}
			else if(e.X >= e.Node.Bounds.Left - 32 && e.X < e.Node.Bounds.Left - 16)
			{
				TreeNodeData nodeData = (TreeNodeData)node.Tag;
				SetSelected(node, (nodeData.NumSelectedFiles == 0)? SelectionType.All : SelectionType.None);
			}
		}

		private void SetSelected(TreeNode parentNode, SelectionType type)
		{
			TreeNodeData parentNodeData = (TreeNodeData)parentNode.Tag;

			int prevNumSelectedFiles = parentNodeData.NumSelectedFiles;
			SetSelectedOnChildren(parentNode, type);

			int deltaNumSelectedFiles = parentNodeData.NumSelectedFiles - prevNumSelectedFiles;
			if(deltaNumSelectedFiles != 0)
			{
				for(TreeNode nextParentNode = parentNode.Parent; nextParentNode != null; nextParentNode = nextParentNode.Parent)
				{
					TreeNodeData nextParentNodeData = (TreeNodeData)nextParentNode.Tag;
					nextParentNodeData.NumSelectedFiles += deltaNumSelectedFiles;
					UpdateImage(nextParentNode);
				}
			}
		}

		private void SetSelectedOnChildren(TreeNode parentNode, SelectionType type)
		{
			TreeNodeData parentNodeData = (TreeNodeData)parentNode.Tag;

			int newNumSelectedFiles = 0;
			switch(type)
			{
				case SelectionType.All:
					newNumSelectedFiles = parentNodeData.NumFiles;
					break;
				case SelectionType.Empty:
					newNumSelectedFiles = parentNodeData.NumEmptySelectedFiles;
					break;
				case SelectionType.Missing:
					newNumSelectedFiles = parentNodeData.NumMissingSelectedFiles;
					break;
				case SelectionType.SafeToDelete:
					newNumSelectedFiles = parentNodeData.NumDefaultSelectedFiles;
					break;
				case SelectionType.None:
					newNumSelectedFiles = 0;
					break;
			}

			if(newNumSelectedFiles != parentNodeData.NumSelectedFiles)
			{
				foreach(TreeNode? childNode in parentNode.Nodes)
				{
					if (childNode != null)
					{
						SetSelectedOnChildren(childNode, type);
					}
				}
				parentNodeData.NumSelectedFiles = newNumSelectedFiles;
				UpdateImage(parentNode);
			}
		}

		private void UpdateImage(TreeNode node)
		{
			TreeNodeData nodeData = (TreeNodeData)node.Tag;
			int imageIndex = (nodeData.Folder != null)? 0 : 3;
			imageIndex += (nodeData.NumSelectedFiles == 0)? 0 : (nodeData.NumSelectedFiles < nodeData.NumFiles || (nodeData.Folder != null && !nodeData.Folder.EmptyAfterClean))? 1 : 2;
			node.ImageIndex = imageIndex;
			node.SelectedImageIndex = imageIndex;
		}

		private void CleanBtn_Click(object sender, EventArgs e)
		{
			List<FileInfo> filesToSync = new List<FileInfo>();
			List<FileInfo> filesToDelete = new List<FileInfo>();
			List<DirectoryInfo> directoriesToDelete = new List<DirectoryInfo>();
			foreach(TreeNode? rootNode in TreeView.Nodes)
			{
				if (rootNode != null)
				{
					FindSelection(rootNode, filesToSync, filesToDelete, directoriesToDelete);
				}
			}

			ModalTask? result = ModalTask.Execute(this, "Clean Workspace", "Cleaning files, please wait...", x => DeleteFilesTask.RunAsync(_perforceSettings, filesToSync, filesToDelete, directoriesToDelete, _logger, x), ModalTaskFlags.Quiet);
			if(result != null && result.Failed)
			{
				FailedToDeleteWindow failedToDelete = new FailedToDeleteWindow();
				failedToDelete.FileList.Text = result.Error;
				failedToDelete.FileList.SelectionStart = 0;
				failedToDelete.FileList.SelectionLength = 0;
				failedToDelete.ShowDialog();
			}
		}

		private void FindSelection(TreeNode node, List<FileInfo> filesToSync, List<FileInfo> filesToDelete, List<DirectoryInfo> directoriesToDelete)
		{
			TreeNodeData nodeData = (TreeNodeData)node.Tag;
			if(nodeData.File != null)
			{
				if(nodeData.NumSelectedFiles > 0)
				{
					if(nodeData.Action == TreeNodeAction.Delete)
					{
						filesToDelete.Add(nodeData.File);
					}
					else
					{
						filesToSync.Add(nodeData.File);
					}
				}
			}
			else
			{
				foreach(TreeNode? childNode in node.Nodes)
				{
					if (childNode != null)
					{
						FindSelection(childNode, filesToSync, filesToDelete, directoriesToDelete);
					}
				}
				if(nodeData.Folder != null && nodeData.Folder.EmptyAfterClean && nodeData.NumSelectedFiles == nodeData.NumFiles)
				{
					directoriesToDelete.Add(nodeData.Folder.Directory);
				}
			}
		}

		private void SelectAllBtn_Click(object sender, EventArgs e)
		{
			foreach(TreeNode? node in TreeView.Nodes)
			{
				if (node != null)
				{
					SetSelected(node, SelectionType.All);
				}
			}
		}

		private void SelectMissingBtn_Click(object sender, EventArgs e)
		{
			foreach(TreeNode? node in TreeView.Nodes)
			{
				if (node != null)
				{
					SetSelected(node, SelectionType.Missing);
				}
			}
		}

		private void FolderContextMenu_SelectAll_Click(object sender, EventArgs e)
		{
			TreeNode node = (TreeNode)FolderContextMenu.Tag;
			SetSelected(node, SelectionType.All);
		}

		private void FolderContextMenu_SelectSafeToDelete_Click(object sender, EventArgs e)
		{
			TreeNode node = (TreeNode)FolderContextMenu.Tag;
			SetSelected(node, SelectionType.SafeToDelete);
		}

		private void FolderContextMenu_SelectEmptyFolder_Click(object sender, EventArgs e)
		{
			TreeNode node = (TreeNode)FolderContextMenu.Tag;
			SetSelected(node, SelectionType.Empty);
		}

		private void FolderContextMenu_SelectNone_Click(object sender, EventArgs e)
		{
			TreeNode node = (TreeNode)FolderContextMenu.Tag;
			SetSelected(node, SelectionType.None);
		}

		private void FolderContextMenu_OpenWithExplorer_Click(object sender, EventArgs e)
		{
			TreeNode node = (TreeNode)FolderContextMenu.Tag;
			TreeNodeData nodeData = (TreeNodeData)node.Tag;

			if (nodeData.Folder != null)
			{
				Process.Start("explorer.exe", String.Format("\"{0}\"", nodeData.Folder.Directory.FullName));
			}
			else if (nodeData.File != null)
			{
				Process.Start("explorer.exe", String.Format("\"{0}\"", nodeData.File.Directory.FullName));
			}
		}
	}
}

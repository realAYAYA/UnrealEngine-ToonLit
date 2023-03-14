// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Concurrent;
using System.Collections.Generic;
using EpicGames.Core;

namespace EpicGames.Perforce.Managed
{
	class RemoveTransaction
	{
		readonly Dictionary<FileContentId, CachedFileInfo> _contentIdToTrackedFile;

		public WorkspaceDirectoryInfo _newWorkspaceRootDir;
		public StreamSnapshot _streamSnapshot;
		public ConcurrentDictionary<FileContentId, WorkspaceFileInfo> _filesToMove = new ConcurrentDictionary<FileContentId, WorkspaceFileInfo>();
		public ConcurrentQueue<WorkspaceFileInfo> _filesToDelete = new ConcurrentQueue<WorkspaceFileInfo>();
		public ConcurrentQueue<WorkspaceDirectoryInfo> _directoriesToDelete = new ConcurrentQueue<WorkspaceDirectoryInfo>();

		public RemoveTransaction(WorkspaceDirectoryInfo workspaceRootDir, StreamSnapshot streamSnapshot, Dictionary<FileContentId, CachedFileInfo> contentIdToTrackedFile)
		{
			_newWorkspaceRootDir = new WorkspaceDirectoryInfo(workspaceRootDir.GetLocation());
			_streamSnapshot = streamSnapshot;
			_contentIdToTrackedFile = contentIdToTrackedFile;

			using (ThreadPoolWorkQueue queue = new ThreadPoolWorkQueue())
			{
				queue.Enqueue(() => Merge(workspaceRootDir, _newWorkspaceRootDir, streamSnapshot.Root, queue));
			}
		}

		void Merge(WorkspaceDirectoryInfo workspaceDir, WorkspaceDirectoryInfo newWorkspaceDir, StreamTreeRef streamTreeRef, ThreadPoolWorkQueue queue)
		{
			StreamTree streamDir = _streamSnapshot.Lookup(streamTreeRef);

			// Update all the subdirectories
			foreach (WorkspaceDirectoryInfo workspaceSubDir in workspaceDir.NameToSubDirectory.Values)
			{
				StreamTreeRef? streamSubTreeRef;
				if (streamDir.NameToTree.TryGetValue(workspaceSubDir.Name, out streamSubTreeRef))
				{
					MergeSubDirectory(workspaceSubDir.Name, newWorkspaceDir, workspaceSubDir, streamSubTreeRef, queue);
				}
				else
				{
					RemoveDirectory(workspaceSubDir, queue);
				}
			}

			// Update the staged files
			foreach (WorkspaceFileInfo workspaceFile in workspaceDir.NameToFile.Values)
			{
				StreamFile? streamFile;
				if (streamDir != null && streamDir.NameToFile.TryGetValue(workspaceFile.Name, out streamFile) && streamFile.ContentId.Equals(workspaceFile.ContentId))
				{
					newWorkspaceDir.NameToFile.Add(workspaceFile.Name, workspaceFile);
				}
				else
				{
					RemoveFile(workspaceFile);
				}
			}
		}

		void MergeSubDirectory(Utf8String name, WorkspaceDirectoryInfo newWorkspaceDir, WorkspaceDirectoryInfo workspaceSubDir, StreamTreeRef streamSubTreeRef, ThreadPoolWorkQueue queue)
		{
			WorkspaceDirectoryInfo newWorkspaceSubDir = new WorkspaceDirectoryInfo(newWorkspaceDir, name, streamSubTreeRef);
			newWorkspaceDir.NameToSubDirectory.Add(newWorkspaceSubDir.Name, newWorkspaceSubDir);
			queue.Enqueue(() => Merge(workspaceSubDir, newWorkspaceSubDir, streamSubTreeRef, queue));
		}

		void RemoveDirectory(WorkspaceDirectoryInfo workspaceDir, ThreadPoolWorkQueue queue)
		{
			_directoriesToDelete.Enqueue(workspaceDir);

			foreach (WorkspaceDirectoryInfo workspaceSubDir in workspaceDir.NameToSubDirectory.Values)
			{
				queue.Enqueue(() => RemoveDirectory(workspaceSubDir, queue));
			}
			foreach (WorkspaceFileInfo workspaceFile in workspaceDir.NameToFile.Values)
			{
				RemoveFile(workspaceFile);
			}
		}

		void RemoveFile(WorkspaceFileInfo workspaceFile)
		{
			if (_contentIdToTrackedFile.ContainsKey(workspaceFile.ContentId) || !_filesToMove.TryAdd(workspaceFile.ContentId, workspaceFile))
			{
				_filesToDelete.Enqueue(workspaceFile);
			}
		}
	}
}

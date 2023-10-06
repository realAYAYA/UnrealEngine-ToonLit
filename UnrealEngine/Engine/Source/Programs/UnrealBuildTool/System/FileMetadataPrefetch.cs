// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Threading;
using EpicGames.Core;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Prefetches metadata from the filesystem, by populating FileItem and DirectoryItem objects for requested directory trees. Since 
	/// </summary>
	static class FileMetadataPrefetch
	{
		/// <summary>
		/// Queue for tasks added to the thread pool
		/// </summary>
		static ThreadPoolWorkQueue Queue = new ThreadPoolWorkQueue();

		/// <summary>
		/// Used to cancel any queued tasks
		/// </summary>
		static CancellationTokenSource CancelSource = new CancellationTokenSource();

		/// <summary>
		/// The cancellation token
		/// </summary>
		static CancellationToken CancelToken = CancelSource.Token;

		/// <summary>
		/// Set of all the directory trees that have been queued up, to save adding any more than once.
		/// </summary>
		static HashSet<DirectoryReference> QueuedDirectories = new HashSet<DirectoryReference>();

		/// <summary>
		/// Enqueue the engine directory for prefetching
		/// </summary>
		public static void QueueEngineDirectory()
		{
			lock (QueuedDirectories)
			{
				if (QueuedDirectories.Add(Unreal.EngineDirectory))
				{
					Enqueue(() => ScanEngineDirectory());
				}
			}
		}

		/// <summary>
		/// Enqueue a project directory for prefetching
		/// </summary>
		/// <param name="ProjectDirectory">The project directory to prefetch</param>
		public static void QueueProjectDirectory(DirectoryReference ProjectDirectory)
		{
			lock (QueuedDirectories)
			{
				if (QueuedDirectories.Add(ProjectDirectory))
				{
					Enqueue(() => ScanProjectDirectory(DirectoryItem.GetItemByDirectoryReference(ProjectDirectory)));
				}
			}
		}

		/// <summary>
		/// Enqueue a directory tree for prefetching
		/// </summary>
		/// <param name="Directory">Directory to start searching from</param>
		public static void QueueDirectoryTree(DirectoryReference Directory)
		{
			lock (QueuedDirectories)
			{
				if (QueuedDirectories.Add(Directory))
				{
					Enqueue(() => ScanDirectoryTree(DirectoryItem.GetItemByDirectoryReference(Directory)));
				}
			}
		}

		/// <summary>
		/// Wait for the prefetcher to complete all reqeusted tasks
		/// </summary>
		public static void Wait()
		{
			Queue.Wait();
		}

		/// <summary>
		/// Stop prefetching items, and cancel all pending tasks. synchronous.
		/// </summary>
		public static void Stop()
		{
			CancelSource.Cancel();
			Queue.Wait();
		}

		/// <summary>
		/// Enqueue a task which checks for the cancellation token first
		/// </summary>
		/// <param name="Action">Action to enqueue</param>
		static void Enqueue(System.Action Action)
		{
			Queue.Enqueue(() => { if (!CancelToken.IsCancellationRequested) { Action(); } });
		}

		/// <summary>
		/// Scans the engine directory, adding tasks for subdirectories
		/// </summary>
		static void ScanEngineDirectory()
		{
			foreach (DirectoryReference ExtensionDir in Unreal.GetExtensionDirs(Unreal.EngineDirectory))
			{
				DirectoryItem BaseDirectory = DirectoryItem.GetItemByDirectoryReference(ExtensionDir);
				BaseDirectory.CacheDirectories();

				DirectoryItem BasePluginsDirectory = DirectoryItem.Combine(BaseDirectory, "Plugins");
				Enqueue(() => ScanPluginFolder(BasePluginsDirectory));

				DirectoryItem BaseSourceDirectory = DirectoryItem.Combine(BaseDirectory, "Source");
				BaseSourceDirectory.CacheDirectories();

				DirectoryItem BaseSourceRuntimeDirectory = DirectoryItem.Combine(BaseSourceDirectory, "Runtime");
				Enqueue(() => ScanDirectoryTree(BaseSourceRuntimeDirectory));

				DirectoryItem BaseSourceDeveloperDirectory = DirectoryItem.Combine(BaseSourceDirectory, "Developer");
				Enqueue(() => ScanDirectoryTree(BaseSourceDeveloperDirectory));

				DirectoryItem BaseSourceEditorDirectory = DirectoryItem.Combine(BaseSourceDirectory, "Editor");
				Enqueue(() => ScanDirectoryTree(BaseSourceEditorDirectory));
			}
		}

		/// <summary>
		/// Scans a project directory, adding tasks for subdirectories
		/// </summary>
		/// <param name="ProjectDirectory">The project directory to search</param>
		static void ScanProjectDirectory(DirectoryItem ProjectDirectory)
		{
			foreach (DirectoryReference ExtensionDir in Unreal.GetExtensionDirs(ProjectDirectory.Location))
			{
				DirectoryItem BaseDirectory = DirectoryItem.GetItemByDirectoryReference(ExtensionDir);
				BaseDirectory.CacheDirectories();

				DirectoryItem BasePluginsDirectory = DirectoryItem.Combine(BaseDirectory, "Plugins");
				Enqueue(() => ScanPluginFolder(BasePluginsDirectory));

				DirectoryItem BaseSourceDirectory = DirectoryItem.Combine(BaseDirectory, "Source");
				Enqueue(() => ScanDirectoryTree(BaseSourceDirectory));
			}
		}

		/// <summary>
		/// Scans a plugin parent directory, adding tasks for subdirectories
		/// </summary>
		/// <param name="Directory">The directory which may contain plugin directories</param>
		static void ScanPluginFolder(DirectoryItem Directory)
		{
			if (CancelToken.IsCancellationRequested || Directory.TryGetFile(".ubtignore", out FileItem? _))
			{
				return;
			}

			foreach (DirectoryItem SubDirectory in Directory.EnumerateDirectories())
			{
				if (SubDirectory.EnumerateFiles().Any((fi) => fi.HasExtension(".uplugin")))
				{
					Enqueue(() => ScanDirectoryTree(DirectoryItem.Combine(SubDirectory, "Source")));
				}
				else if (!SubDirectory.TryGetFile(".ubtignore", out FileItem? OutFile))
				{
					Enqueue(() => ScanPluginFolder(SubDirectory));
				}
			}
		}

		/// <summary>
		/// Scans an arbitrary directory tree
		/// </summary>
		/// <param name="Directory">Root of the directory tree</param>
		static void ScanDirectoryTree(DirectoryItem Directory)
		{
			if (CancelToken.IsCancellationRequested || Directory.TryGetFile(".ubtignore", out FileItem? _))
			{
				return;
			}

			foreach (DirectoryItem SubDirectory in Directory.EnumerateDirectories())
			{
				Enqueue(() => ScanDirectoryTree(SubDirectory));
			}
		}
	}
}

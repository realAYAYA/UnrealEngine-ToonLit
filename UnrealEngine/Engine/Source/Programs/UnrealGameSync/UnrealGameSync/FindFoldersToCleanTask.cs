// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class FolderToClean
	{
		public DirectoryInfo Directory;
		public Dictionary<string, FolderToClean> NameToSubFolder = new Dictionary<string, FolderToClean>(StringComparer.InvariantCultureIgnoreCase);
		public Dictionary<string, FileInfo> NameToFile = new Dictionary<string, FileInfo>(StringComparer.InvariantCultureIgnoreCase);
		public List<FileInfo> FilesToDelete = new List<FileInfo>();
		public List<FileInfo> FilesToSync = new List<FileInfo>();
		public bool EmptyLeaf = false;
		public bool EmptyAfterClean = true;

		public FolderToClean(DirectoryInfo inDirectory)
		{
			Directory = inDirectory;
		}

		public string Name
		{
			get { return Directory.Name; }
		}

		public override string ToString()
		{
			return Directory.FullName;
		}
	}

	class FindFoldersToCleanTask : IDisposable
	{
		class PerforceHaveFolder
		{
			public Dictionary<string, PerforceHaveFolder> NameToSubFolder = new Dictionary<string,PerforceHaveFolder>(StringComparer.OrdinalIgnoreCase);
			public Dictionary<string, FStatRecord> NameToFile = new Dictionary<string, FStatRecord>(StringComparer.OrdinalIgnoreCase);
		}

		IPerforceSettings _perforceSettings;
		string _clientRootPath;
		IReadOnlyList<string> _syncPaths;
		ILogger _logger;
		FolderToClean _rootFolderToClean;

		int _remainingFoldersToScan;
		ManualResetEvent _finishedScan = new ManualResetEvent(false);
		bool _abortScan;
		string? _scanError;

		public List<string> FileNames = new List<string>();

		public FindFoldersToCleanTask(IPerforceSettings inPerforceSettings, FolderToClean inRootFolderToClean, string inClientRootPath, IReadOnlyList<string> inSyncPaths, ILogger logger)
		{
			_perforceSettings = inPerforceSettings;
			_clientRootPath = inClientRootPath.TrimEnd('/') + "/";
			_syncPaths = new List<string>(inSyncPaths);
			this._logger = logger;
			_rootFolderToClean = inRootFolderToClean;
			_finishedScan = new ManualResetEvent(true);
		}

		void QueueFolderToPopulate(FolderToClean folder)
		{
			if(Interlocked.Increment(ref _remainingFoldersToScan) == 1)
			{
				_finishedScan.Reset();
			}
			ThreadPool.QueueUserWorkItem(x => PopulateFolder(folder));
		}

		void PopulateFolder(FolderToClean folder)
		{
			if(!_abortScan)
			{
				try
				{
					if ((folder.Directory.Attributes & FileAttributes.ReparsePoint) == 0)
					{
						foreach (DirectoryInfo subDirectory in folder.Directory.EnumerateDirectories())
						{
							FolderToClean subFolder = new FolderToClean(subDirectory);
							folder.NameToSubFolder[subFolder.Name] = subFolder;
							QueueFolderToPopulate(subFolder);
						}
						foreach (FileInfo file in folder.Directory.EnumerateFiles())
						{
							FileAttributes attributes = file.Attributes; // Force the value to be cached.
							folder.NameToFile[file.Name] = file;
						}
					}
				}
				catch (Exception ex)
				{
					string newError = String.Format("Unable to enumerate contents of {0} due to an error:\n\n{1}", folder.Directory.FullName, ex);
					Interlocked.CompareExchange(ref _scanError, newError, null);
					_abortScan = true;
				}
			}

			if(Interlocked.Decrement(ref _remainingFoldersToScan) == 0)
			{
				_finishedScan.Set();
			}
		}

		void MergeTrees(FolderToClean localFolder, PerforceHaveFolder? perforceFolder, HashSet<string> openClientPaths, string? perforceConfigFile)
		{
			if(perforceFolder == null)
			{
				// Loop through all the local sub-folders
				foreach(FolderToClean localSubFolder in localFolder.NameToSubFolder.Values)
				{
					MergeTrees(localSubFolder, null, openClientPaths, perforceConfigFile);
				}

				// Delete everything
				localFolder.FilesToDelete.AddRange(localFolder.NameToFile.Values);
			}
			else
			{
				// Loop through all the local sub-folders
				foreach(FolderToClean localSubFolder in localFolder.NameToSubFolder.Values)
				{
					PerforceHaveFolder? perforceSubFolder;
					perforceFolder.NameToSubFolder.TryGetValue(localSubFolder.Name, out perforceSubFolder);
					MergeTrees(localSubFolder, perforceSubFolder, openClientPaths, perforceConfigFile);
				}

				// Also merge all the Perforce folders that no longer exist
				foreach(KeyValuePair<string, PerforceHaveFolder> perforceSubFolderPair in perforceFolder.NameToSubFolder)
				{
					FolderToClean? localSubFolder;
					if(!localFolder.NameToSubFolder.TryGetValue(perforceSubFolderPair.Key, out localSubFolder))
					{
						localSubFolder = new FolderToClean(new DirectoryInfo(Path.Combine(localFolder.Directory.FullName, perforceSubFolderPair.Key)));
						MergeTrees(localSubFolder, perforceSubFolderPair.Value, openClientPaths, perforceConfigFile);
						localFolder.NameToSubFolder.Add(localSubFolder.Name, localSubFolder);
					}
				}

				// Find all the files that need to be re-synced
				foreach(KeyValuePair<string, FStatRecord> filePair in perforceFolder.NameToFile)
				{
					FileInfo? localFile;
					if(!localFolder.NameToFile.TryGetValue(filePair.Key, out localFile))
					{
						localFolder.FilesToSync.Add(new FileInfo(Path.Combine(localFolder.Directory.FullName, filePair.Key)));
					}
					else if(IsFileTypeWritable(filePair.Value?.HeadType ?? filePair.Value?.Type) && (localFile.Attributes & FileAttributes.ReadOnly) == 0 && !openClientPaths.Contains(filePair.Value?.ClientFile ?? ""))
					{
						localFolder.FilesToSync.Add(localFile);
					}
				}

				// Find all the files that should be deleted
				foreach(FileInfo localFileInfo in localFolder.NameToFile.Values)
				{
					if(!perforceFolder.NameToFile.ContainsKey(localFileInfo.Name) && !openClientPaths.Contains(localFileInfo.FullName))
					{
						localFolder.FilesToDelete.Add(localFileInfo);
					}
				}
			}

			// Remove any config files
			if(perforceConfigFile != null)
			{
				localFolder.FilesToDelete.RemoveAll(x => String.Compare(x.Name, perforceConfigFile, StringComparison.OrdinalIgnoreCase) == 0);
			}

			// Figure out if this folder is just an empty directory that needs to be removed
			localFolder.EmptyLeaf = localFolder.NameToFile.Count == 0 && localFolder.NameToSubFolder.Count == 0 && localFolder.FilesToSync.Count == 0;

			// Figure out if it the folder will be empty after the clean operation
			localFolder.EmptyAfterClean = localFolder.NameToSubFolder.Values.All(x => x.EmptyAfterClean) && localFolder.FilesToDelete.Count == localFolder.NameToFile.Count && localFolder.FilesToSync.Count == 0;
		}

		static bool IsFileTypeWritable(string? type)
		{
			if (type != null)
			{
				int idx = type.IndexOf('+');
				if (idx != -1)
				{
					return type.IndexOf('w', idx) != -1;
				}
			}
			return true;
		}

		void RemoveEmptyFolders(FolderToClean folder)
		{
			foreach(FolderToClean subFolder in folder.NameToSubFolder.Values)
			{
				RemoveEmptyFolders(subFolder);
			}

			folder.NameToSubFolder = folder.NameToSubFolder.Values.Where(x => x.NameToSubFolder.Count > 0 || x.FilesToSync.Count > 0 || x.FilesToDelete.Count > 0 || x.EmptyLeaf).ToDictionary(x => x.Name, x => x, StringComparer.InvariantCultureIgnoreCase);
		}

		public void Dispose()
		{
			_abortScan = true;

			if(_finishedScan != null)
			{
				_finishedScan.WaitOne();
				_finishedScan.Dispose();
				_finishedScan = null!;
			}
		}

		public async Task RunAsync(CancellationToken cancellationToken)
		{
			using IPerforceConnection perforceClient = await PerforceConnection.CreateAsync(_perforceSettings, _logger);

			_logger.LogInformation("Finding files in workspace...");
			_logger.LogInformation("");

			// Clear the current error
			_scanError = null;

			// Start enumerating all the files that exist locally
			foreach(string syncPath in _syncPaths)
			{
				Debug.Assert(syncPath.StartsWith(_clientRootPath));
				if(syncPath.StartsWith(_clientRootPath, StringComparison.InvariantCultureIgnoreCase))
				{
					string[] fragments = syncPath.Substring(_clientRootPath.Length).Split('/');

					FolderToClean syncFolder = _rootFolderToClean;
					for(int idx = 0; idx < fragments.Length - 1; idx++)
					{
						FolderToClean? nextSyncFolder;
						if(!syncFolder.NameToSubFolder.TryGetValue(fragments[idx], out nextSyncFolder))
						{
							nextSyncFolder = new FolderToClean(new DirectoryInfo(Path.Combine(syncFolder.Directory.FullName, fragments[idx])));
							syncFolder.NameToSubFolder[nextSyncFolder.Name] = nextSyncFolder;
						}
						syncFolder = nextSyncFolder;
					}

					string wildcard = fragments[fragments.Length - 1];
					if(wildcard == "...")
					{
						QueueFolderToPopulate(syncFolder);
					}
					else
					{
						if(syncFolder.Directory.Exists)
						{
							foreach(FileInfo file in syncFolder.Directory.EnumerateFiles(wildcard))
							{
								syncFolder.NameToFile[file.Name] = file;
							}
						}
					}
				}
			}

			// Get the prefix for any local file
			string localRootPrefix = _rootFolderToClean.Directory.FullName.Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;

			// Query the have table and build a separate tree from it
			PerforceHaveFolder rootHaveFolder = new PerforceHaveFolder();
			foreach(string syncPath in _syncPaths)
			{
				List<FStatRecord> fileRecords = await perforceClient.FStatAsync($"{syncPath}#have", cancellationToken).ToListAsync(cancellationToken);
				foreach(FStatRecord fileRecord in fileRecords)
				{
					if(fileRecord.ClientFile == null || !fileRecord.ClientFile.StartsWith(localRootPrefix, StringComparison.InvariantCultureIgnoreCase))
					{
						throw new UserErrorException($"Failed to get have table; file '{fileRecord.ClientFile}' doesn't start with root path ('{_rootFolderToClean.Directory.FullName}')");
					}

					string[] tokens = fileRecord.ClientFile.Substring(localRootPrefix.Length).Split('/', '\\');

					PerforceHaveFolder fileFolder = rootHaveFolder;
					for(int idx = 0; idx < tokens.Length - 1; idx++)
					{
						PerforceHaveFolder? nextFileFolder;
						if(!fileFolder.NameToSubFolder.TryGetValue(tokens[idx], out nextFileFolder))
						{
							nextFileFolder = new PerforceHaveFolder();
							fileFolder.NameToSubFolder.Add(tokens[idx], nextFileFolder);
						}
						fileFolder = nextFileFolder;
					}
					fileFolder.NameToFile[tokens[tokens.Length - 1]] = fileRecord;
				}
			}

			// Find all the files which are currently open for edit. We don't want to force sync these.
			List<OpenedRecord> openFileRecords = await perforceClient.OpenedAsync(OpenedOptions.None, -1, null, null, -1, "//...", cancellationToken).ToListAsync(cancellationToken);

			// Build a set of all the open local files
			HashSet<string> openLocalFiles = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);
			foreach (OpenedRecord openFileRecord in openFileRecords)
			{
				if(!openFileRecord.ClientFile.StartsWith(_clientRootPath, StringComparison.InvariantCultureIgnoreCase))
				{
					throw new UserErrorException($"Failed to get open file list; file '{openFileRecord.ClientFile}' doesn't start with client root path ('{_clientRootPath}')");
				}
				openLocalFiles.Add(localRootPrefix + PerforceUtils.UnescapePath(openFileRecord.ClientFile).Substring(_clientRootPath.Length).Replace('/', Path.DirectorySeparatorChar));
			}

			// Wait to finish scanning the directory
			_finishedScan.WaitOne();

			// Check if there was an error
			if (_scanError != null)
			{
				throw new UserErrorException(_scanError);
			}

			// Find the value of the P4CONFIG variable
			string? perforceConfigFile = PerforceEnvironment.Default.GetValue("P4CONFIG");

			// Merge the trees
			MergeTrees(_rootFolderToClean, rootHaveFolder, openLocalFiles, perforceConfigFile);

			// Remove all the empty folders
			RemoveEmptyFolders(_rootFolderToClean);
		}
	}
}

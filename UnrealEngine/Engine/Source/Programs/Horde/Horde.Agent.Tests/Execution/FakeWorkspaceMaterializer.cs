// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Execution;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Tests.Execution;

class FakeWorkspaceMaterializer : IWorkspaceMaterializer
{
	private bool _isInitialized;
	private DirectoryReference? _rootDir;
	private readonly Dictionary<int, Dictionary<string, string>> _changeToFiles = new();

	public void SetFile(int changeNum, string path, string content)
	{
		if (path.Contains("..", StringComparison.Ordinal))
		{
			throw new ArgumentException("Cannot contain '..'");
		}
		if (path.Contains(':', StringComparison.Ordinal))
		{
			throw new ArgumentException("Cannot contain ':'");
		}
		path = path.Replace("\\", "/", StringComparison.Ordinal);

		if (!_changeToFiles.TryGetValue(changeNum, out Dictionary<string, string>? pathToContent))
		{
			pathToContent = new();
			_changeToFiles[changeNum] = pathToContent;
		}

		pathToContent[path] = content;
	}

	/// <inheritdoc/>
	public void Dispose()
	{
	}

	/// <inheritdoc/>
	public Task<WorkspaceMaterializerSettings> InitializeAsync(ILogger logger, CancellationToken cancellationToken)
	{
		if (_isInitialized)
		{
			throw new WorkspaceMaterializationException("Already initialized");
		}

		_rootDir = new DirectoryReference(Path.Join(Path.GetTempPath(), "horde-fakeworkspace-" + Guid.NewGuid().ToString()[..8]));
		Directory.CreateDirectory(_rootDir.FullName);
		_isInitialized = true;

		return GetSettingsAsync(cancellationToken);
	}

	/// <inheritdoc/>
	public Task FinalizeAsync(CancellationToken cancellationToken)
	{
		if (!_isInitialized || _rootDir == null)
		{
			throw new WorkspaceMaterializationException("Cannot finalize before initialization");
		}

		if (Directory.Exists(_rootDir.FullName))
		{
			Directory.Delete(_rootDir.FullName, true);
		}

		return Task.CompletedTask;
	}

	/// <inheritdoc/>
	public Task<WorkspaceMaterializerSettings> GetSettingsAsync(CancellationToken cancellationToken)
	{
		if (!_isInitialized || _rootDir == null)
		{
			throw new WorkspaceMaterializationException("Cannot get settings before initialization");
		}

		return Task.FromResult(new WorkspaceMaterializerSettings(_rootDir, "fakeWorkspaceIdentifier", "fakeWorkspaceStreamRoot", new Dictionary<string, string>(), false));
	}

	/// <inheritdoc/>
	public Task SyncAsync(int changeNum, int preflightChangeNum, SyncOptions options, CancellationToken cancellationToken)
	{
		if (!_isInitialized || _rootDir == null)
		{
			throw new Exception("Cannot sync before initialization");
		}

		if (changeNum == IWorkspaceMaterializer.LatestChangeNumber)
		{
			changeNum = _changeToFiles.Keys.Max();
		}

		if (!_changeToFiles.ContainsKey(changeNum))
		{
			throw new WorkspaceMaterializationException($"Change {changeNum} could not be found");
		}

		if (options.RemoveUntracked)
		{
			DeleteAllFiles(_rootDir.FullName);
		}

		WriteChangesToDisk(changeNum);

		if (preflightChangeNum > 0)
		{
			WriteChangesToDisk(preflightChangeNum);
		}

		return Task.CompletedTask;
	}

	private void WriteChangesToDisk(int changeNum)
	{
		foreach ((string relativeFilePath, string content) in _changeToFiles[changeNum])
		{
			string absFilePath = Path.GetFullPath(relativeFilePath, _rootDir!.FullName);
			string absDirPath = Path.GetDirectoryName(absFilePath)!;
			Directory.CreateDirectory(absDirPath);
			File.WriteAllText(absFilePath, content);
		}
	}

	private static void DeleteAllFiles(string dirPath)
	{
		DirectoryInfo di = new(dirPath);

		foreach (FileInfo file in di.EnumerateFiles())
		{
			file.Delete();
		}
		foreach (DirectoryInfo dir in di.EnumerateDirectories())
		{
			dir.Delete(true);
		}
	}
}
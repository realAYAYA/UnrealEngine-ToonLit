// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Horde.Agent.Execution;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests.Execution;

[TestClass]
public class FakeWorkspaceMaterializerTest
{
	private readonly FakeWorkspaceMaterializer _wm = new ();
	private readonly WorkspaceMaterializerSettings _settings;

	public FakeWorkspaceMaterializerTest()
	{
		_settings = _wm.InitializeAsync(CancellationToken.None).Result;
	}

	[TestCleanup]
	public async Task TestCleanup()
	{
		await _wm.FinalizeAsync(CancellationToken.None);
	}

	[TestMethod]
	public async Task SingleFile()
	{
		_wm.SetFile(1, "readme.txt", "hello");
		await _wm.SyncAsync(1, new SyncOptions(), CancellationToken.None);
		AssertFile("readme.txt", "hello");
	}
	
	[TestMethod]
	public async Task SubDir()
	{
		_wm.SetFile(1, "foo/bar/baz.txt", "fortnite");
		await _wm.SyncAsync(1, new SyncOptions(), CancellationToken.None);
		AssertFile("foo/bar/baz.txt", "fortnite");
	}
	
	[TestMethod]
	public async Task FilesAreKeptBetweenChangelists()
	{
		_wm.SetFile(1, "foo/bar/baz.txt", "fortnite");
		_wm.SetFile(2, "foo/main.cpp", "main");
		
		await _wm.SyncAsync(1, new SyncOptions(), CancellationToken.None);
		AssertFile("foo/bar/baz.txt", "fortnite");
		AssertFileDoesNotExist("foo/main.cpp");
		
		await _wm.SyncAsync(2, new SyncOptions(), CancellationToken.None);
		AssertFile("foo/bar/baz.txt", "fortnite");
		AssertFile("foo/main.cpp", "main");
	}
	
	[TestMethod]
	public async Task RemoveUntrackedFiles()
	{
		_wm.SetFile(1, "foo/bar/baz.txt", "fortnite");
		_wm.SetFile(2, "foo/main.cpp", "main");
		File.WriteAllText(Path.Join(_settings.DirectoryPath.FullName, "external.txt"), "external");
		
		await _wm.SyncAsync(1, new SyncOptions(), CancellationToken.None);
		AssertFile("foo/bar/baz.txt", "fortnite");
		AssertFile("external.txt", "external");
		AssertFileDoesNotExist("foo/main.cpp");
		
		await _wm.SyncAsync(2, new SyncOptions { RemoveUntracked = true }, CancellationToken.None);
		AssertFileDoesNotExist("foo/bar/baz.txt");
		AssertFileDoesNotExist("external.txt");
		AssertFile("foo/main.cpp", "main");
	}

	private void AssertFile(string relativePath, string expectedContent) { Assert.AreEqual(expectedContent, File.ReadAllText(GetAbsPath(relativePath))); }
	private void AssertFileExists(string relativePath) { Assert.IsTrue(File.Exists(GetAbsPath(relativePath))); }

	private void AssertFileDoesNotExist(string relativePath)
	{
		Assert.IsFalse(File.Exists(GetAbsPath(relativePath)), $"File {relativePath} exists");
	}
	
	private string GetAbsPath(string relativeFilePath) { return Path.Join(_settings.DirectoryPath.FullName, relativeFilePath); }
}

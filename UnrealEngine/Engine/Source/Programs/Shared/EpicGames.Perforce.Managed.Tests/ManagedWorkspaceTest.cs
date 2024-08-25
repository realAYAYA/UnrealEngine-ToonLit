// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce.Fixture;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Perforce.Managed.Tests;

[TestClass]
[System.Diagnostics.CodeAnalysis.SuppressMessage("Naming", "CA1707:Identifiers should not contain underscores")]
public class ManagedWorkspaceTest : BasePerforceFixtureTest
{
	private string SyncDir => Path.Join(TempDir.FullName, "Sync");
	private string CacheDir => Path.Join(TempDir.FullName, "Cache");
	private string StreamName => Fixture.StreamFooMain.Root;
	private StreamFixture Stream => Fixture.StreamFooMain;

	private readonly ILogger<ManagedWorkspace> _mwLogger;

	public ManagedWorkspaceTest()
	{
		_mwLogger = LoggerFactory.CreateLogger<ManagedWorkspace>();
	}

	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task SyncSingleChangelistAsync(bool useHaveTable)
	{
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(useHaveTable);
		await AssertHaveTableFileCountAsync(0);

		await SyncAsync(ws, 6);
		Stream.GetChangelist(6).AssertDepotFiles(SyncDir);
		await Stream.GetChangelist(6).AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}

	[TestMethod]
	public async Task SyncBackwardsToOlderChangelistRemoveUntrackedAsync()
	{
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(true);

		await SyncAsync(ws, 6, removeUntracked: false);
		Stream.GetChangelist(6).AssertDepotFiles(SyncDir);
		await Stream.GetChangelist(6).AssertHaveTableAsync(PerforceConnection);

		await SyncAsync(ws, 7, removeUntracked: false);
		Stream.GetChangelist(7).AssertDepotFiles(SyncDir);
		await Stream.GetChangelist(7).AssertHaveTableAsync(PerforceConnection);

		await SyncAsync(ws, 6, removeUntracked: false);
		Stream.GetChangelist(6).AssertDepotFiles(SyncDir);
		await Stream.GetChangelist(6).AssertHaveTableAsync(PerforceConnection);
	}

	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task SyncBackwardsToOlderChangelistAsync(bool useHaveTable)
	{
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(useHaveTable);

		await SyncAsync(ws, 6);
		Stream.GetChangelist(6).AssertDepotFiles(SyncDir);
		await Stream.GetChangelist(6).AssertHaveTableAsync(PerforceConnection, useHaveTable);

		await SyncAsync(ws, 7);
		Stream.GetChangelist(7).AssertDepotFiles(SyncDir);
		await Stream.GetChangelist(7).AssertHaveTableAsync(PerforceConnection, useHaveTable);

		// Go back one changelist
		await SyncAsync(ws, 6);
		Stream.GetChangelist(6).AssertDepotFiles(SyncDir);
		await Stream.GetChangelist(6).AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}

	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task SyncUsingCacheFilesAsync(bool useHaveTable)
	{
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(useHaveTable);

		FileReference GetCacheFilePath(int changeNumber)
		{
			return new FileReference(Path.Join(TempDir.FullName, $"CacheFile-{changeNumber}.bin"));
		}

		// Sync and create a new cache file per change number
		foreach (ChangelistFixture cl in Stream.Changelists)
		{
			await SyncAsync(ws, cl.Number, cacheFile: GetCacheFilePath(cl.Number));
			cl.AssertDepotFiles(SyncDir);
			await cl.AssertHaveTableAsync(PerforceConnection, useHaveTable);
		}

		// Sync again but using the cache files created above
		foreach (ChangelistFixture cl in Stream.Changelists.Reverse())
		{
			await SyncAsync(ws, cl.Number, cacheFile: GetCacheFilePath(cl.Number));
			cl.AssertDepotFiles(SyncDir);
			await cl.AssertHaveTableAsync(PerforceConnection, useHaveTable);
		}
	}

	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task SyncWithViewExclusivePathFirstAsync(bool useHaveTable)
	{
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(useHaveTable);
		ChangelistFixture cl = Stream.GetChangelist(6);

		List<string> view = new() { "-/Data/...", "-/shared.h", };
		await ws.SyncAsync(PerforceConnection, StreamName, cl.Number, view, true, false, null, CancellationToken.None);

		List<DepotFileFixture> filtered = cl.StreamFiles
			.Where(x => !x.DepotFile.Contains("shared.h", StringComparison.Ordinal))
			.Where(x => !x.DepotFile.Contains("Data/", StringComparison.Ordinal)).ToList();

		ChangelistFixture clViewApplied = new(cl.Number, cl.Description, filtered, cl.IsShelved);
		clViewApplied.AssertDepotFiles(SyncDir);
		await clViewApplied.AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}

	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task SyncWithViewInclusivePathFirstAsync(bool useHaveTable)
	{
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(useHaveTable);
		ChangelistFixture cl = Stream.GetChangelist(6);

		List<string> view = new() { "/...", "-/Data/..." };
		await ws.SyncAsync(PerforceConnection, StreamName, cl.Number, view, true, false, null, CancellationToken.None);

		List<DepotFileFixture> filtered = cl.StreamFiles
			.Where(x => !x.DepotFile.Contains("Data/", StringComparison.Ordinal)).ToList();

		ChangelistFixture clViewApplied = new(cl.Number, cl.Description, filtered, cl.IsShelved);
		clViewApplied.AssertDepotFiles(SyncDir);
		await clViewApplied.AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}

	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task PopulateAsync(bool useHaveTable)
	{
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(useHaveTable);
		List<PopulateRequest> populateRequests = new() { new PopulateRequest(PerforceConnection, StreamName, new List<string>()) };
		await ws.PopulateAsync(populateRequests, false, CancellationToken.None);
		Stream.LatestChangelist.AssertDepotFiles(SyncDir);
		await Stream.LatestChangelist.AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}

	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task PopulateWithViewAsync(bool useHaveTable)
	{
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(useHaveTable);
		List<string> view = new() { "-/Data/...", "-/shared.h" };
		ChangelistFixture cl = Stream.LatestChangelist;
		List<DepotFileFixture> filtered = cl.StreamFiles
			.Where(x => !x.DepotFile.Contains("shared.h", StringComparison.Ordinal))
			.Where(x => !x.DepotFile.Contains("Data/", StringComparison.Ordinal)).ToList();

		List<PopulateRequest> populateRequests = new() { new PopulateRequest(PerforceConnection, StreamName, view) };
		await ws.PopulateAsync(populateRequests, false, CancellationToken.None);

		ChangelistFixture clViewApplied = new(cl.Number, cl.Description, filtered, cl.IsShelved);
		clViewApplied.AssertDepotFiles(SyncDir);
		await clViewApplied.AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}

	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task UnshelveAsync(bool useHaveTable)
	{
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(useHaveTable);
		await SyncAsync(ws, 7);
		await ws.UnshelveAsync(PerforceConnection, 8, CancellationToken.None);
		Stream.GetChangelist(8).AssertDepotFiles(SyncDir);

		// Have-table still correspond to CL 7 as CL 8 is shelved, only p4 printed to workspace
		await Stream.GetChangelist(7).AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}

	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task Caching_SyncingNewChangelist_UnusedFilesMovedToCacheAsync(bool useHaveTable)
	{
		// Arrange
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(useHaveTable);
		await SyncAsync(ws, 7);

		// Act
		await SyncAsync(ws, 1);

		// Assert - all files from CL 7 are moved to the cache
		PerforceFixture.AssertCacheEquals(CacheDir, Stream.GetChangelist(7).StreamFiles.Select(x => x.Digest).ToArray());
	}

	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task Caching_SyncingWithCachedData_FilesPopulatedFromCacheAsync(bool useHaveTable)
	{
		// Arrange
		ManagedWorkspace ws = await CreateManagedWorkspaceAsync(useHaveTable);
		await SyncAsync(ws, 7);
		await SyncAsync(ws, 1);

		// Act
		await SyncAsync(ws, 7);

		// Assert - cache becomes empty
		PerforceFixture.AssertCacheEquals(CacheDir, Array.Empty<string>());
	}

	[TestMethod]
	public async Task FixtureMetadataMatchesServerMetadataAsync()
	{
		FStatOptions options = FStatOptions.IncludeFileSizes;

		string fileSpec = "//Foo/Main/...";
		List<ChangesRecord> submittedChanges = await PerforceConnection.GetChangesAsync(ChangesOptions.None, -1, ChangeStatus.Submitted, fileSpec);
		List<ChangesRecord> shelvedChanges = await PerforceConnection.GetChangesAsync(ChangesOptions.None, -1, ChangeStatus.Shelved, fileSpec);
		foreach (ChangesRecord cr in submittedChanges.Concat(shelvedChanges).OrderBy(x => x.Number))
		{
			HashSet<(string clientFile, int rev, long size, string digest)> depotFiles = new();
			List<FStatRecord> fstatRecords = await PerforceConnection.FStatAsync(options, fileSpec + "@" + cr.Number).ToListAsync();
			foreach (FStatRecord fsr in fstatRecords)
			{
				if (fsr.HeadAction is FileAction.Add or FileAction.MoveAdd or FileAction.Edit)
				{
					depotFiles.Add((fsr.DepotFile!, fsr.HeadRevision, fsr.FileSize, fsr.Digest!));
				}
			}

			HashSet<(string clientFile, int rev, long size, string digest)> fixtureFiles = new();
			ChangelistFixture changelist = Stream.GetChangelist(cr.Number);
			if (changelist.IsShelved)
			{
				continue;
			}

			foreach (DepotFileFixture depotFile in changelist.StreamFiles)
			{
				fixtureFiles.Add((depotFile.DepotFile, depotFile.Revision, depotFile.Size, depotFile.Digest));
			}

			Assert.AreEqual(depotFiles.Count, fixtureFiles.Count);
			foreach ((string clientFile, int rev, long size, string digest) tuple in fixtureFiles)
			{
				if (!depotFiles.Contains(tuple))
				{
					Assert.Fail("File in fixtures does not exist in depot: " + tuple);
				}
			}
		}
	}

	[TestMethod]
	public async Task ReusePerforceClientWithoutHaveTableAsync()
	{
		// Sync with have-table as normal
		ManagedWorkspace wsWithHave = await CreateManagedWorkspaceAsync(true);
		await wsWithHave.SetupAsync(PerforceConnection, StreamName, CancellationToken.None);
		ChangelistFixture cl = Stream.GetChangelist(6);
		await SyncAsync(wsWithHave, cl.Number);
		cl.AssertDepotFiles(SyncDir);
		await cl.AssertHaveTableAsync(PerforceConnection);

		// Create a new ManagedWorkspace without have-table but re-use same Perforce connection
		// The have-table remnants from above should not interfere with this workspace
		ManagedWorkspace wsWithoutHave = await CreateManagedWorkspaceAsync(false);
		await wsWithoutHave.SetupAsync(PerforceConnection, StreamName, CancellationToken.None);
		await SyncAsync(wsWithoutHave, cl.Number);
		await AssertHaveTableFileCountAsync(0);
		await cl.AssertHaveTableAsync(PerforceConnection, useHaveTable: false);
		cl.AssertDepotFiles(SyncDir);
	}

	private async Task<ManagedWorkspace> CreateManagedWorkspaceAsync(bool useHaveTable)
	{
		ManagedWorkspaceOptions options = new() { UseHaveTable = useHaveTable };
		ManagedWorkspace ws = await ManagedWorkspace.CreateAsync(Environment.MachineName, TempDir, options, _mwLogger, CancellationToken.None);
		await ws.SetupAsync(PerforceConnection, StreamName, CancellationToken.None);
		return ws;
	}

	private async Task SyncAsync(ManagedWorkspace managedWorkspace, int changeNumber, FileReference? cacheFile = null, bool removeUntracked = true)
	{
		await managedWorkspace.SyncAsync(PerforceConnection, StreamName, changeNumber, Array.Empty<string>(), removeUntracked, false, cacheFile, CancellationToken.None);
	}

	private async Task AssertHaveTableFileCountAsync(int expected)
	{
		List<HaveRecord> haveRecords = await PerforceConnection.HaveAsync(new FileSpecList(), CancellationToken.None).ToListAsync();

		if (haveRecords.Count != expected)
		{
			Console.WriteLine("Have table contains:");
			foreach (HaveRecord haveRecord in haveRecords)
			{
				Console.WriteLine(haveRecord.DepotFile + "#" + haveRecord.HaveRev);
			}
			Assert.Fail($"Actual have table file count does not match expected count. Actual={haveRecords.Count} Expected={expected}");
		}
	}
}
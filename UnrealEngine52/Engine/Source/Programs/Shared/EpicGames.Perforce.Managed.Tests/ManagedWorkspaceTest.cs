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
public class ManagedWorkspaceTest : BasePerforceFixtureTest
{
	private string SyncDir => Path.Join(TempDir.FullName, "Sync");
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
	public async Task SyncSingleChangelist(bool useHaveTable)
	{
		ManagedWorkspace ws = await GetManagedWorkspace(useHaveTable);
		await AssertHaveTableFileCount(0);

		await SyncAsync(ws, 6);
		Stream.GetChangelist(6).AssertDepotFiles(SyncDir);
		await Stream.GetChangelist(6).AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}
	
	[TestMethod]
	public async Task SyncBackwardsToOlderChangelistRemoveUntracked()
	{
		ManagedWorkspace ws = await GetManagedWorkspace(true);

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
	public async Task SyncBackwardsToOlderChangelist(bool useHaveTable)
	{
		ManagedWorkspace ws = await GetManagedWorkspace(useHaveTable);

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
	public async Task SyncUsingCacheFiles(bool useHaveTable)
	{
		ManagedWorkspace ws = await GetManagedWorkspace(useHaveTable);

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
	public async Task SyncWithViewExclusivePathFirst(bool useHaveTable)
	{
		ManagedWorkspace ws = await GetManagedWorkspace(useHaveTable);
		ChangelistFixture cl = Stream.GetChangelist(6);

		List<string> view = new() { "-/Data/...", "-/shared.h", };
		await ws.SyncAsync(PerforceConnection, StreamName, cl.Number, view, true, false, null, CancellationToken.None);

		List<DepotFileFixture> filtered = cl.StreamFiles
			.Where(x => !x.DepotFile.Contains("shared.h", StringComparison.Ordinal))
			.Where(x => !x.DepotFile.Contains("Data/", StringComparison.Ordinal)).ToList();

		ChangelistFixture clViewApplied = new (cl.Number, cl.Description, filtered, cl.IsShelved);
		clViewApplied.AssertDepotFiles(SyncDir);
		await clViewApplied.AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}
	
	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task SyncWithViewInclusivePathFirst(bool useHaveTable)
	{
		ManagedWorkspace ws = await GetManagedWorkspace(useHaveTable);
		ChangelistFixture cl = Stream.GetChangelist(6);

		List<string> view = new() { "/...", "-/Data/..." };
		await ws.SyncAsync(PerforceConnection, StreamName, cl.Number, view, true, false, null, CancellationToken.None);

		List<DepotFileFixture> filtered = cl.StreamFiles
			.Where(x => !x.DepotFile.Contains("Data/", StringComparison.Ordinal)).ToList();

		ChangelistFixture clViewApplied = new (cl.Number, cl.Description, filtered, cl.IsShelved);
		clViewApplied.AssertDepotFiles(SyncDir);
		await clViewApplied.AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}
	
	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task Populate(bool useHaveTable)
	{
		ManagedWorkspace ws = await GetManagedWorkspace(useHaveTable);
		List<PopulateRequest> populateRequests = new () { new PopulateRequest(PerforceConnection, StreamName, new List<string>()) };
		await ws.PopulateAsync(populateRequests, false, CancellationToken.None);
		Stream.LatestChangelist.AssertDepotFiles(SyncDir);
		await Stream.LatestChangelist.AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}
	
	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task PopulateWithView(bool useHaveTable)
	{
		ManagedWorkspace ws = await GetManagedWorkspace(useHaveTable);
		List<string> view = new () { "-/Data/...", "-/shared.h" };
		ChangelistFixture cl = Stream.LatestChangelist;
		List<DepotFileFixture> filtered = cl.StreamFiles
			.Where(x => !x.DepotFile.Contains("shared.h", StringComparison.Ordinal))
			.Where(x => !x.DepotFile.Contains("Data/", StringComparison.Ordinal)).ToList();

		List<PopulateRequest> populateRequests = new () { new PopulateRequest(PerforceConnection, StreamName, view) };
		await ws.PopulateAsync(populateRequests, false, CancellationToken.None);
		
		ChangelistFixture clViewApplied = new (cl.Number, cl.Description, filtered, cl.IsShelved);
		clViewApplied.AssertDepotFiles(SyncDir);
		await clViewApplied.AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}
	
	[TestMethod]
	[DataRow(true, DisplayName = "With have-table")]
	[DataRow(false, DisplayName = "Without have-table")]
	public async Task Unshelve(bool useHaveTable)
	{
		ManagedWorkspace ws = await GetManagedWorkspace(useHaveTable);
		await SyncAsync(ws, 7);
		await ws.UnshelveAsync(PerforceConnection, 8, CancellationToken.None);
		Stream.GetChangelist(8).AssertDepotFiles(SyncDir);
		
		// Have-table still correspond to CL 7 as CL 8 is shelved, only p4 printed to workspace
		await Stream.GetChangelist(7).AssertHaveTableAsync(PerforceConnection, useHaveTable);
	}
	
	[TestMethod]
	public async Task ReusePerforceClientWithoutHaveTable()
	{
		// Sync with have-table as normal
		ManagedWorkspace wsWithHave = await GetManagedWorkspace(true);
		await wsWithHave.SetupAsync(PerforceConnection, StreamName, CancellationToken.None);
		ChangelistFixture cl = Stream.GetChangelist(6);
		await SyncAsync(wsWithHave, cl.Number);
		cl.AssertDepotFiles(SyncDir);
		await cl.AssertHaveTableAsync(PerforceConnection);

		// Create a new ManagedWorkspace without have-table but re-use same Perforce connection
		// The have-table remnants from above should not interfere with this workspace
		ManagedWorkspace wsWithoutHave = await GetManagedWorkspace(false);
		await wsWithoutHave.SetupAsync(PerforceConnection, StreamName, CancellationToken.None);
		await SyncAsync(wsWithoutHave, cl.Number);
		await AssertHaveTableFileCount(0);
		await cl.AssertHaveTableAsync(PerforceConnection, useHaveTable: false);
		cl.AssertDepotFiles(SyncDir);
	}

	private async Task<ManagedWorkspace> GetManagedWorkspace(bool useHaveTable)
	{
		ManagedWorkspace ws = await ManagedWorkspace.CreateAsync(Environment.MachineName, TempDir, useHaveTable, _mwLogger, CancellationToken.None);
		await ws.SetupAsync(PerforceConnection, StreamName, CancellationToken.None);
		return ws;
	}
	
	private async Task SyncAsync(ManagedWorkspace managedWorkspace, int changeNumber, FileReference? cacheFile = null, bool removeUntracked = true)
	{
		await managedWorkspace.SyncAsync(PerforceConnection, StreamName, changeNumber, Array.Empty<string>(), removeUntracked, false, cacheFile, CancellationToken.None);
	}
	
	private async Task AssertHaveTableFileCount(int expected)
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

	private void DumpMetaDir()
	{
		Console.WriteLine("Meta dir: --------------------------");
		foreach (string path in Directory.GetFiles(TempDir.FullName, "*", SearchOption.AllDirectories))
		{
			Console.WriteLine(Path.GetRelativePath(TempDir.FullName, path));
		}
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Server.Server;
using Horde.Server.Storage;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Storage
{
	[TestClass]
	public sealed class GcServiceTests : TestSetup
	{
		[TestMethod]
		public async Task CreateBasicTreeAsync()
		{
			await StorageService.StartAsync(CancellationToken.None);

			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Storage.Backends.Add(new BackendConfig { Id = new BackendId("default-backend"), Type = StorageBackendType.Memory });
			globalConfig.Storage.Namespaces.Add(new NamespaceConfig { Id = new NamespaceId("default"), Backend = new BackendId("default-backend"), GcDelayHrs = 0.0 });
			SetConfig(globalConfig);

			using IStorageClient store = StorageService.CreateClient(new NamespaceId("default"));

			Random random = new Random(0);
			IBlobRef[] blobs = await CreateTestDataAsync(store, 30, 50, 30, 5, random);

			HashSet<IBlobHandle> roots = new HashSet<IBlobHandle>();
			for (int idx = 0; idx < 10; idx++)
			{
				int blobIdx = (int)(random.NextDouble() * blobs.Length);
				if (roots.Add(blobs[blobIdx]))
				{
					IBlobRef handle = blobs[blobIdx];
					await store.WriteRefAsync(new RefName($"ref-{idx}"), handle);
				}
			}

			HashSet<BlobLocator> nodes = await FindNodesAsync(store, roots);

			await Clock.AdvanceAsync(TimeSpan.FromDays(1.0));

			IObjectStore backend = ServiceProvider.GetRequiredService<IObjectStoreFactory>().CreateObjectStore(globalConfig.Storage.Backends[0]);

			ObjectKey[] remaining = await backend.EnumerateAsync().ToArrayAsync();
			Assert.AreEqual(nodes.Count, remaining.Length);

			HashSet<ObjectKey> nodePaths = new HashSet<ObjectKey>(nodes.Select(x => StorageService.GetObjectKey(x.BaseLocator)));
			Assert.IsTrue(remaining.All(x => nodePaths.Contains(x)));
		}

		static async Task<HashSet<BlobLocator>> FindNodesAsync(IStorageClient store, IEnumerable<IBlobHandle> roots)
		{
			HashSet<BlobLocator> nodes = new HashSet<BlobLocator>();
			await FindNodesAsync(store, roots, nodes);
			return nodes;
		}

		static async Task FindNodesAsync(IStorageClient store, IEnumerable<IBlobHandle> roots, HashSet<BlobLocator> nodes)
		{
			foreach (IBlobHandle root in roots)
			{
				BlobLocator locator = root.GetLocator();
				if (nodes.Add(locator))
				{
					BlobData data = await root.ReadBlobDataAsync();
					await FindNodesAsync(store, data.Imports, nodes);
				}
			}
		}

		static async ValueTask<IBlobRef[]> CreateTestDataAsync(IStorageClient store, int numRoots, int numInterior, int numLeaves, int avgChildren, Random random)
		{
			int firstRoot = 0;
			int firstInterior = firstRoot + numRoots;
			int firstLeaf = firstInterior + numInterior;
			int numNodes = firstLeaf + numLeaves;

			List<int>[] children = new List<int>[numNodes];
			for (int idx = 0; idx < numNodes; idx++)
			{
				children[idx] = new List<int>();
			}

			double maxParents = ((numRoots + numInterior) * avgChildren) / (numInterior + numLeaves);
			for (int idx = numRoots; idx < numNodes; idx++)
			{
				int numParents = 1 + (int)(random.NextDouble() * maxParents);
				for (; numParents > 0; numParents--)
				{
					int parentIdx = Math.Min((int)(random.NextDouble() * Math.Min(idx, numRoots + numInterior)), idx - 1);
					children[parentIdx].Add(idx);
				}
			}

			BlobType blobType = new BlobType(Guid.Parse("{AFDF76A7-4DEE-5333-F5B5-37B8451251CA}"), 0);

			IBlobRef[] handles = new IBlobRef[children.Length];
			for (int idx = numNodes - 1; idx >= 0; idx--)
			{
				IBlobRef handle;
				await using (IBlobWriter writer = store.CreateBlobWriter("gctest"))
				{
					List<IBlobRef> imports = children[idx].ConvertAll(x => handles[x]);
					foreach (IBlobRef import in imports)
					{
						writer.WriteBlobRef(import);
					}
					handle = await writer.CompleteAsync<object>(blobType);
				}
				handles[idx] = handle;
			}

			return handles;
		}
	}
}

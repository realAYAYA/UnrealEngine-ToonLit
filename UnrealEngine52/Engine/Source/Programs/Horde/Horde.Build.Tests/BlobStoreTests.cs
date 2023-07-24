// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Driver;
using EpicGames.Horde.Storage;
using System.Buffers;
using System.Threading;
using Horde.Build.Server;
using Horde.Build.Storage;
using Horde.Build.Utilities;

namespace Horde.Build.Tests
{
	using BackendId = StringId<IStorageBackend>;

	[TestClass]
	public class BlobStoreTests : TestSetup
	{
		async Task<IStorageClient> CreateStorageClientAsync()
		{
			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Storage.Backends.Add(new BackendConfig { Id = new BackendId("default-backend"), Type = StorageBackendType.Memory });
			globalConfig.Storage.Namespaces.Add(new NamespaceConfig { Id = new NamespaceId("default"), Backend = new BackendId("default-backend"), GcDelayHrs = 0.0 });
			SetConfig(globalConfig);

			return await StorageService.GetClientAsync(new NamespaceId("default"), CancellationToken.None);
		}

		static byte[] CreateTestData(int length, int seed)
		{
			byte[] data = new byte[length];
			new Random(seed).NextBytes(data);
			return data;
		}

		class Blob
		{
			public ReadOnlyMemory<byte> Data { get; set; } = ReadOnlyMemory<byte>.Empty;
			public List<BlobLocator> References { get; set; } = new List<BlobLocator>();
		}

		static Bundle CreateTestBundle(ReadOnlyMemory<byte> data, IReadOnlyList<BlobLocator> refs)
		{
			List<BundleType> types = new List<BundleType>();
			types.Add(new BundleType(Guid.Parse("{AFDF76A7-5333-4DEE-B837-B5F5CA511245}"), 1));

			List<BundleImport> imports = new List<BundleImport>();

			Dictionary<BlobLocator, int> locatorToIndex = new Dictionary<BlobLocator, int>();
			foreach (BlobLocator locator in refs)
			{
				int index;
				if (!locatorToIndex.TryGetValue(locator, out index))
				{
					BundleImport import = new BundleImport(locator, new int[] { 0 });
					imports.Add(import);
					locatorToIndex.Add(locator, imports.Count - 1);
				}
			}

			List<BundleExport> exports = new List<BundleExport>();
			exports.Add(new BundleExport(0, IoHash.Compute(data.Span), (int)data.Length, refs.Select(x => locatorToIndex[x]).ToArray()));

			List<BundlePacket> packets = new List<BundlePacket>();
			packets.Add(new BundlePacket((int)data.Length, (int)data.Length));

			BundleHeader header = new BundleHeader(BundleCompressionFormat.None, types, imports, exports, packets);
			return new Bundle(header, new[] { data });
		}

		static async Task<Blob> ReadBlobAsync(IStorageClient store, BlobLocator locator)
		{
			Bundle bundle = await store.ReadBundleAsync(locator);
			return ExtractBlobFromBundle(bundle);
		}

		static Blob ExtractBlobFromBundle(Bundle bundle)
		{
			Blob blob = new Blob();
			blob.Data = bundle.Packets[0];
			blob.References = bundle.Header.Exports[0].References.Select(x => bundle.Header.Imports[x].Locator).ToList();
			return blob;
		}

		[TestMethod]
		public async Task LeafTest()
		{
			IStorageClient store = await CreateStorageClientAsync();

			byte[] input = CreateTestData(256, 0);
			BlobLocator locator = await store.WriteBundleAsync(CreateTestBundle(input, Array.Empty<BlobLocator>()));

			Bundle outputBundle = await store.ReadBundleAsync(locator);
			Assert.IsTrue(outputBundle.Packets[0].Span.SequenceEqual(input));
		}

		[TestMethod]
		public async Task ReferenceTest()
		{
			IStorageClient store = await CreateStorageClientAsync();

			byte[] input1 = CreateTestData(256, 1);
			Bundle bundle1 = CreateTestBundle(input1, Array.Empty<BlobLocator>());
			BlobLocator locator1 = await store.WriteBundleAsync(bundle1);
			Blob blob1 = await ReadBlobAsync(store, locator1);
			Assert.IsTrue(blob1.Data.Span.SequenceEqual(input1));
			Assert.IsTrue(blob1.References.SequenceEqual(Array.Empty<BlobLocator>()));

			byte[] input2 = CreateTestData(256, 2);
			Bundle bundle2 = CreateTestBundle(input2, new BlobLocator[] { locator1 });
			BlobLocator locator2 = await store.WriteBundleAsync(bundle2);
			Blob blob2 = await ReadBlobAsync(store, locator2);
			Assert.IsTrue(blob2.Data.Span.SequenceEqual(input2));
			Assert.IsTrue(blob2.References.SequenceEqual(new BlobLocator[] { locator1 }));

			byte[] input3 = CreateTestData(256, 3);
			Bundle bundle3 = CreateTestBundle(input3, new BlobLocator[] { locator1, locator2, locator1 });
			BlobLocator locator3 = await store.WriteBundleAsync(bundle3);
			Blob blob3 = await ReadBlobAsync(store, locator3);
			Assert.IsTrue(blob3.Data.Span.SequenceEqual(input3));
			Assert.IsTrue(blob3.References.SequenceEqual(new BlobLocator[] { locator1, locator2, locator1 }));

			for(int idx = 0; idx < 2; idx++)
			{
				RefName refName = new RefName("hello");
				await store.WriteRefTargetAsync(refName, new NodeHandle(bundle3.Header.Exports[0].Hash, locator3, 0));
				NodeHandle refTarget = await store.ReadRefTargetAsync(refName);
				Assert.AreEqual(locator3, refTarget.Locator.Blob);
			}

			RefName refName2 = new RefName("hello2");

			NodeHandle refTargetId2 = await store.WriteRefAsync(refName2, CreateTestBundle(input3, new BlobLocator[] { locator1, locator2 }), 0);
			Blob refTarget2 = await ReadBlobAsync(store, refTargetId2.Locator.Blob);
			Assert.IsTrue(refTarget2.Data.Span.SequenceEqual(input3));
			Assert.IsTrue(refTarget2.References.SequenceEqual(new BlobLocator[] { locator1, locator2 }));
		}

		[TestMethod]
		public async Task RefExpiryTest()
		{
			IStorageClient store = await CreateStorageClientAsync();

			Bundle bundle1 = CreateTestBundle(new byte[] { 1, 2, 3 }, Array.Empty<BlobLocator>());
			BlobLocator locator1 = await store.WriteBundleAsync(bundle1);
			NodeHandle target = new NodeHandle(bundle1.Header.Exports[0].Hash, locator1, 0);

			await store.WriteRefTargetAsync("test-ref-1", target);
			await store.WriteRefTargetAsync("test-ref-2", target, new RefOptions { Lifetime = TimeSpan.FromMinutes(30.0), Extend = true });
			await store.WriteRefTargetAsync("test-ref-3", target, new RefOptions { Lifetime = TimeSpan.FromMinutes(30.0), Extend = false });

			Assert.AreEqual(target.Locator, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(target.Locator, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(target.Locator, await TryReadRefTargetAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(25.0));

			Assert.AreEqual(target.Locator, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(target.Locator, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(target.Locator, await TryReadRefTargetAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(25.0));

			Assert.AreEqual(target.Locator, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(target.Locator, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(default, await TryReadRefTargetAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(35.0));

			Assert.AreEqual(target.Locator, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(default, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(default, await TryReadRefTargetAsync(store, "test-ref-3"));
		}

		static async Task<NodeLocator> TryReadRefTargetAsync(IStorageClient store, RefName name)
		{
			NodeHandle? handle = await store.TryReadRefTargetAsync(name);
			return handle?.Locator ?? default;
		}
	}
}

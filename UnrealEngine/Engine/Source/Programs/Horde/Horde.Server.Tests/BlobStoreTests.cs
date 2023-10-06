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
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;

namespace Horde.Server.Tests
{
	[TestClass]
	public class BlobStoreTests : TestSetup
	{
		async Task<IStorageClientImpl> CreateStorageClientAsync()
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
			List<BlobType> types = new List<BlobType>();
			types.Add(new BlobType(Guid.Parse("{AFDF76A7-5333-4DEE-B837-B5F5CA511245}"), 1));

			List<BlobLocator> imports = new List<BlobLocator>(refs);

			List<BundleExportRef> exportRefs = new List<BundleExportRef>();
			for(int idx = 0; idx < refs.Count; idx++)
			{
				exportRefs.Add(new BundleExportRef(idx, 0, IoHash.Zero));
			}

			List<BundleExport> exports = new List<BundleExport>();
			exports.Add(new BundleExport(0, IoHash.Compute(data.Span), 0, 0, (int)data.Length, exportRefs));

			List<BundlePacket> packets = new List<BundlePacket>();
			packets.Add(new BundlePacket(BundleCompressionFormat.None, 0, (int)data.Length, (int)data.Length));

			BundleHeader header = BundleHeader.Create(types, imports, exports, packets);
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
			blob.References = bundle.Header.Imports.ToList();
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
			IStorageClientImpl store = await CreateStorageClientAsync();

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
				await store.WriteRefTargetAsync(refName, new NodeLocator(bundle3.Header.Exports[0].Hash, locator3, 0));
				BlobHandle refTarget = await store.ReadRefTargetAsync(refName);
				Assert.AreEqual(locator3, refTarget.GetLocator().Blob);
			}
		}

		[TestMethod]
		public async Task RefExpiryTest()
		{
			IStorageClientImpl store = await CreateStorageClientAsync();

			Bundle bundle1 = CreateTestBundle(new byte[] { 1, 2, 3 }, Array.Empty<BlobLocator>());
			BlobLocator locator1 = await store.WriteBundleAsync(bundle1);
			NodeLocator target = new NodeLocator(bundle1.Header.Exports[0].Hash, locator1, 0);

			await store.WriteRefTargetAsync("test-ref-1", target);
			await store.WriteRefTargetAsync("test-ref-2", target, new RefOptions { Lifetime = TimeSpan.FromMinutes(30.0), Extend = true });
			await store.WriteRefTargetAsync("test-ref-3", target, new RefOptions { Lifetime = TimeSpan.FromMinutes(30.0), Extend = false });

			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(25.0));

			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(25.0));

			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(default, await TryReadRefTargetAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(35.0));

			Assert.AreEqual(target, await TryReadRefTargetAsync(store, "test-ref-1"));
			Assert.AreEqual(default, await TryReadRefTargetAsync(store, "test-ref-2"));
			Assert.AreEqual(default, await TryReadRefTargetAsync(store, "test-ref-3"));
		}

		static async Task<NodeLocator> TryReadRefTargetAsync(IStorageClient store, RefName name)
		{
			BlobHandle? handle = await store.TryReadRefTargetAsync(name);
			if (handle == null)
			{
				return default;
			}
			return handle.GetLocator();
		}
	}
}


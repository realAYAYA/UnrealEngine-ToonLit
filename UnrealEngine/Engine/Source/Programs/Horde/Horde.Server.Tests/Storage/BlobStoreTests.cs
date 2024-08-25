// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Horde.Server.Server;
using Horde.Server.Storage;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Storage
{
	[TestClass]
	public class BlobStoreTests : TestSetup
	{
		static readonly BlobType s_blobType = new BlobType("{AFDF76A7-4DEE-5333-F5B5-37B8451251CA}", 1);

		IStorageClient CreateStorageClient()
		{
			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Storage.Backends.Add(new BackendConfig { Id = new BackendId("default-backend"), Type = StorageBackendType.Memory });
			globalConfig.Storage.Namespaces.Add(new NamespaceConfig { Id = new NamespaceId("default"), Backend = new BackendId("default-backend"), GcDelayHrs = 0.0 });
			SetConfig(globalConfig);

			return StorageService.CreateClient(new NamespaceId("default"));
		}

		static byte[] CreateTestData(int length, int seed)
		{
			byte[] data = new byte[length];
			new Random(seed).NextBytes(data);
			return data;
		}

		class Blob
		{
			public ReadOnlyMemory<byte> Data { get; set; }
			public List<BlobRefValue> References { get; set; }

			public Blob() : this(ReadOnlyMemory<byte>.Empty, new List<BlobRefValue>())
			{
			}

			public Blob(ReadOnlyMemory<byte> data, IEnumerable<BlobRefValue> references)
			{
				Data = data;
				References = new List<BlobRefValue>(references);
			}
		}

		static async ValueTask<Blob> ReadBlobAsync(IStorageClient store, BlobRefValue locator)
		{
			using (BlobData blobData = await store.CreateBlobRef(locator.Hash, locator.Locator).ReadBlobDataAsync())
			{
				BlobReader reader = new BlobReader(blobData, null);
				ReadOnlyMemory<byte> data = reader.ReadVariableLengthBytes();

				List<IBlobRef> handles = new List<IBlobRef>();
				while (reader.RemainingMemory.Length > 0)
				{
					handles.Add(reader.ReadBlobRef<object>());
				}

				List<BlobRefValue> locators = handles.ConvertAll(x => x.GetRefValue());
				return new Blob(data, locators);
			}
		}

		static async ValueTask<BlobRefValue> WriteBlobAsync(IStorageClient store, Blob blob)
		{
			await using IBlobWriter writer = store.CreateBlobWriter();
			writer.WriteVariableLengthBytes(blob.Data.Span);

			foreach (BlobRefValue reference in blob.References)
			{
				writer.WriteBlobRef(store.CreateBlobRef(reference));
			}

			IBlobRef blobRef = await writer.CompleteAsync(s_blobType);
			await blobRef.FlushAsync();

			return new BlobRefValue(blobRef.Hash, blobRef.GetLocator());
		}

		[TestMethod]
		public async Task LeafTestAsync()
		{
			using IStorageClient store = CreateStorageClient();

			byte[] input = CreateTestData(256, 0);

			Blob inputBlob = new Blob(input, Array.Empty<BlobRefValue>());
			BlobRefValue locator = await WriteBlobAsync(store, inputBlob);
			Blob outputBlob = await ReadBlobAsync(store, locator);

			Assert.IsTrue(outputBlob.Data.Span.SequenceEqual(input));
			Assert.AreEqual(0, outputBlob.References.Count);
		}

		[TestMethod]
		public async Task ReferenceTestAsync()
		{
			using IStorageClient store = CreateStorageClient();

			byte[] input1 = CreateTestData(256, 1);
			BlobRefValue locator1 = await WriteBlobAsync(store, new Blob(input1, Array.Empty<BlobRefValue>()));
			Blob blob1 = await ReadBlobAsync(store, locator1);
			Assert.IsTrue(blob1.Data.Span.SequenceEqual(input1));
			Assert.IsTrue(blob1.References.SequenceEqual(Array.Empty<BlobRefValue>()));

			byte[] input2 = CreateTestData(256, 2);
			BlobRefValue locator2 = await WriteBlobAsync(store, new Blob(input2, new BlobRefValue[] { locator1 }));
			Blob blob2 = await ReadBlobAsync(store, locator2);
			Assert.IsTrue(blob2.Data.Span.SequenceEqual(input2));
			Assert.IsTrue(blob2.References.SequenceEqual(new[] { locator1 }));

			byte[] input3 = CreateTestData(256, 3);
			BlobRefValue locator3 = await WriteBlobAsync(store, new Blob(input3, new BlobRefValue[] { locator1, locator2, locator1 }));
			Blob blob3 = await ReadBlobAsync(store, locator3);
			Assert.IsTrue(blob3.Data.Span.SequenceEqual(input3));
			Assert.IsTrue(blob3.References.SequenceEqual(new[] { locator1, locator2, locator1 }));

			for (int idx = 0; idx < 2; idx++)
			{
				RefName refName = new RefName("hello");
				await store.WriteRefAsync(refName, store.CreateBlobRef(locator3));
				IBlobRef refTarget = await store.ReadRefAsync(refName);
				Assert.AreEqual(locator3.Locator, refTarget.GetLocator());
			}
		}

		[TestMethod]
		public async Task RefExpiryTestAsync()
		{
			using IStorageClient store = CreateStorageClient();

			Blob blob1 = new Blob(new byte[] { 1, 2, 3 }, Array.Empty<BlobRefValue>());
			BlobRefValue target = await WriteBlobAsync(store, blob1);

			await store.WriteRefAsync("test-ref-1", store.CreateBlobRef(target));
			await store.WriteRefAsync("test-ref-2", store.CreateBlobRef(target), new RefOptions { Lifetime = TimeSpan.FromMinutes(30.0), Extend = true });
			await store.WriteRefAsync("test-ref-3", store.CreateBlobRef(target), new RefOptions { Lifetime = TimeSpan.FromMinutes(30.0), Extend = false });

			Assert.AreEqual(target, await TryReadRefValueAsync(store, "test-ref-1"));
			Assert.AreEqual(target, await TryReadRefValueAsync(store, "test-ref-2"));
			Assert.AreEqual(target, await TryReadRefValueAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(25.0));

			Assert.AreEqual(target, await TryReadRefValueAsync(store, "test-ref-1"));
			Assert.AreEqual(target, await TryReadRefValueAsync(store, "test-ref-2"));
			Assert.AreEqual(target, await TryReadRefValueAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(25.0));

			Assert.AreEqual(target, await TryReadRefValueAsync(store, "test-ref-1"));
			Assert.AreEqual(target, await TryReadRefValueAsync(store, "test-ref-2"));
			Assert.AreEqual(default, await TryReadRefValueAsync(store, "test-ref-3"));

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(35.0));

			Assert.AreEqual(target, await TryReadRefValueAsync(store, "test-ref-1"));
			Assert.AreEqual(default, await TryReadRefValueAsync(store, "test-ref-2"));
			Assert.AreEqual(default, await TryReadRefValueAsync(store, "test-ref-3"));
		}

		static async Task<BlobRefValue?> TryReadRefValueAsync(IStorageClient store, RefName name)
		{
			IBlobRef? handle = await store.TryReadRefAsync(name);
			if (handle == null)
			{
				return default;
			}
			return handle.GetRefValue();
		}
	}
}


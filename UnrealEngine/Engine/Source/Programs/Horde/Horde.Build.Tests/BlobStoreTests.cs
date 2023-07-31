// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs;
using Horde.Build.Issues;
using Horde.Build.Logs;
using Horde.Build.Users;
using Horde.Build.Projects;
using Horde.Build.Streams;
using Horde.Build.Server;
using Horde.Build.Tests.Stubs.Services;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Bson;
using MongoDB.Driver;
using Moq;
using Horde.Build.Issues.Handlers;
using System.Threading;
using EpicGames.Horde.Storage;
using Horde.Build.Storage;
using Horde.Build.Storage.Backends;
using Microsoft.Extensions.Caching.Memory;
using System.Buffers;

namespace Horde.Build.Tests
{
	[TestClass]
	public class BlobStoreTests : TestSetup
	{
		IBlobStore CreateBlobStore()
		{
			return new BasicBlobStore(MongoService, new TransientStorageBackend(), ServiceProvider.GetRequiredService<IMemoryCache>(), ServiceProvider.GetRequiredService<ILogger<BasicBlobStore>>());
		}

		static byte[] CreateTestData(int length, int seed)
		{
			byte[] data = new byte[length];
			new Random(seed).NextBytes(data);
			return data;
		}

		[TestMethod]
		public async Task LeafTest()
		{
			IBlobStore store = CreateBlobStore();

			byte[] input = CreateTestData(256, 0);

			BlobId blobId = await store.WriteBlobAsync(new ReadOnlySequence<byte>(input), Array.Empty<BlobId>());

			IBlob blob = await store.ReadBlobAsync(blobId);
			Assert.IsTrue(blob.Data.Span.SequenceEqual(input));
		}

		[TestMethod]
		public async Task ReferenceTest()
		{
			IBlobStore store = CreateBlobStore();

			byte[] input1 = CreateTestData(256, 1);
			BlobId blobId1 = await store.WriteBlobAsync(new ReadOnlySequence<byte>(input1), Array.Empty<BlobId>());
			IBlob blob1 = await store.ReadBlobAsync(blobId1);
			Assert.IsTrue(blob1.Data.Span.SequenceEqual(input1));
			Assert.IsTrue(blob1.References.SequenceEqual(Array.Empty<BlobId>()));

			byte[] input2 = CreateTestData(256, 2);
			BlobId blobId2 = await store.WriteBlobAsync(new ReadOnlySequence<byte>(input2), new BlobId[] { blob1.Id });
			IBlob blob2 = await store.ReadBlobAsync(blobId2);
			Assert.IsTrue(blob2.Data.Span.SequenceEqual(input2));
			Assert.IsTrue(blob2.References.SequenceEqual(new BlobId[] { blob1.Id }));

			byte[] input3 = CreateTestData(256, 3);
			BlobId blobId3 = await store.WriteBlobAsync(new ReadOnlySequence<byte>(input3), new BlobId[] { blob1.Id, blob2.Id, blob1.Id });
			IBlob blob3 = await store.ReadBlobAsync(blobId3);
			Assert.IsTrue(blob3.Data.Span.SequenceEqual(input3));
			Assert.IsTrue(blob3.References.SequenceEqual(new BlobId[] { blob1.Id, blob2.Id, blob1.Id }));

			for(int idx = 0; idx < 2; idx++)
			{
				RefName refName = new RefName("hello");
				await store.WriteRefTargetAsync(refName, blob3.Id);
				BlobId refTargetId = await store.ReadRefIdAsync(refName);
				Assert.AreEqual(blob3.Id, refTargetId);
			}

			RefName refName2 = new RefName("hello2");

			BlobId refTargetId2 = await store.WriteRefAsync(refName2, new ReadOnlySequence<byte>(input3), new BlobId[] { blob1.Id, blob2.Id });
			IBlob refTarget2 = await store.ReadBlobAsync(refTargetId2);
			Assert.IsTrue(refTarget2.Data.Span.SequenceEqual(input3));
			Assert.IsTrue(refTarget2.References.SequenceEqual(new BlobId[] { blob1.Id, blob2.Id }));

			refTarget2 = await store.ReadRefAsync(refName2);
			Assert.IsTrue(refTarget2.Data.Span.SequenceEqual(input3));
			Assert.IsTrue(refTarget2.References.SequenceEqual(new BlobId[] { blob1.Id, blob2.Id }));
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	[TestClass]
	public class StorageTests : TestSetup
	{
		[TestMethod]
		public async Task BlobCollectionTest()
		{
			NamespaceId namespaceId = new NamespaceId("test-ns");

			IStorageClient storageClient = ServiceProvider.GetRequiredService<IStorageClient>();

			byte[] testData = Encoding.UTF8.GetBytes("Hello world");
			IoHash hash = IoHash.Compute(testData);

			await Assert.ThrowsExceptionAsync<BlobNotFoundException>(() => storageClient.ReadBlobAsync(namespaceId, hash));

			IoHash returnedHash = await storageClient.WriteBlobFromMemoryAsync(namespaceId, testData);
			Assert.AreEqual(hash, returnedHash);
			Assert.IsNotNull(await storageClient.ReadBlobToMemoryAsync(namespaceId, hash));

			ReadOnlyMemory<byte> storedData = await storageClient.ReadBlobToMemoryAsync(namespaceId, hash);
			Assert.IsTrue(testData.AsSpan().SequenceEqual(storedData.Span));

			NamespaceId otherNamespaceId = new NamespaceId("other-ns");
			await Assert.ThrowsExceptionAsync<BlobNotFoundException>(() => storageClient.ReadBlobAsync(otherNamespaceId, hash));
		}

		[TestMethod]
		public async Task ObjectCollectionTest()
		{
			NamespaceId namespaceId = new NamespaceId("test-ns");

			IStorageClient storageClient = ServiceProvider.GetRequiredService<IStorageClient>();

			CbObject objectD = CbObject.Build(writer =>
			{
				writer.WriteString("hello", "field");
			});
			IoHash hashD = objectD.GetHash();

			await Assert.ThrowsExceptionAsync<BlobNotFoundException>(() => storageClient.ReadBlobToMemoryAsync(namespaceId, hashD));

			await storageClient.WriteBlobFromMemoryAsync(namespaceId, hashD, objectD.GetView());

			CbObject returnedObjectD = new CbObject(await storageClient.ReadBlobToMemoryAsync(namespaceId, hashD));
			Assert.IsTrue(returnedObjectD!.GetView().Span.SequenceEqual(objectD.GetView().Span));
		}

		[TestMethod]
		public async Task RefCollectionTest()
		{
			IStorageClient storageClient = ServiceProvider.GetRequiredService<IStorageClient>();

			byte[] blobA = Encoding.UTF8.GetBytes("This is blob A");
			IoHash hashA = IoHash.Compute(blobA);

			byte[] blobB = Encoding.UTF8.GetBytes("This is blob B");
			IoHash hashB = IoHash.Compute(blobB);

			byte[] blobC = Encoding.UTF8.GetBytes("This is blob C");
			IoHash hashC = IoHash.Compute(blobC);

			CbObject objectD = CbObject.Build(writer =>
			{
				writer.WriteBinaryAttachment("A", hashA);
				writer.WriteBinaryAttachment("B", hashB);
				writer.WriteBinaryAttachment("C", hashC);
			});
			IoHash hashD = objectD.GetHash();

			CbObject objectE = CbObject.Build(writer =>
			{
				writer.WriteBinaryAttachment("A", hashA);
				writer.WriteObjectAttachment("D", hashD);
			});
			IoHash hashE = objectE.GetHash();

			NamespaceId namespaceId = new NamespaceId("test-ns");
			BucketId bucketId = new BucketId("test-bkt");
			RefId refId = new RefId("refname");

			// Check that setting the ref to E fails with the correct missing hashes
			List<IoHash> missingHashes = await storageClient.TrySetRefAsync(namespaceId, bucketId, refId, objectE);
			Assert.AreEqual(2, missingHashes.Count);
			Assert.IsTrue(missingHashes.Contains(hashA));
			Assert.IsTrue(missingHashes.Contains(hashD));

			await Assert.ThrowsExceptionAsync<RefNotFoundException>(() => storageClient.GetRefAsync(namespaceId, bucketId, refId));

			// Add object D, and check that changes the missing hashes to just the blobs
			await storageClient.WriteBlobFromMemoryAsync(namespaceId, hashD, objectD.GetView());

			missingHashes = await storageClient.TryFinalizeRefAsync(namespaceId, bucketId, refId, hashE);
			Assert.AreEqual(3, missingHashes.Count);
			Assert.IsTrue(missingHashes.Contains(hashA));
			Assert.IsTrue(missingHashes.Contains(hashB));
			Assert.IsTrue(missingHashes.Contains(hashC));

			await Assert.ThrowsExceptionAsync<RefNotFoundException>(() => storageClient.GetRefAsync(namespaceId, bucketId, refId));

			// Add blobs A, B and C and check that the object can be finalized
			await storageClient.WriteBlobFromMemoryAsync(namespaceId, hashA, blobA);
			await storageClient.WriteBlobFromMemoryAsync(namespaceId, hashB, blobB);
			await storageClient.WriteBlobFromMemoryAsync(namespaceId, hashC, blobC);

			missingHashes = await storageClient.TryFinalizeRefAsync(namespaceId, bucketId, refId, hashE);
			Assert.AreEqual(0, missingHashes.Count);

			IRef refValue = await storageClient.GetRefAsync(namespaceId, bucketId, refId);
			Assert.IsNotNull(refValue);

			// Add a new ref to existing objects and check it finalizes correctly 
			RefId refId2 = new RefId("refname");

			missingHashes = await storageClient.TrySetRefAsync(namespaceId, bucketId, refId2, objectE);
			Assert.AreEqual(0, missingHashes.Count);

			refValue = await storageClient.GetRefAsync(namespaceId, bucketId, refId2);
			Assert.IsNotNull(refValue);
		}
	}
}

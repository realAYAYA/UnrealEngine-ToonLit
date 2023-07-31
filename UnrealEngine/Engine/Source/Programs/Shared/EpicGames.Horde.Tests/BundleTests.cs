// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Impl;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Serialization;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Buffers;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public sealed class BundleTests : IDisposable
	{
		readonly InMemoryBlobStore _blobStore;
		readonly ITreeStore _treeStore;

		public BundleTests()
		{
			_blobStore = new InMemoryBlobStore();
			_treeStore = new BundleStore(_blobStore, new BundleOptions());
		}

		public void Dispose()
		{
			_treeStore.Dispose();
		}

		[TestMethod]
		public void BuzHashTests()
		{
			byte[] data = new byte[4096];
			new Random(0).NextBytes(data);

			const int WindowSize = 128;

			uint rollingHash = 0;
			for (int maxIdx = 0; maxIdx < data.Length + WindowSize; maxIdx++)
			{
				int minIdx = maxIdx - WindowSize;

				if (maxIdx < data.Length)
				{
					rollingHash = BuzHash.Add(rollingHash, data[maxIdx]);
				}

				int length = Math.Min(maxIdx + 1, data.Length) - Math.Max(minIdx, 0);
				uint cleanHash = BuzHash.Add(0, data.AsSpan(Math.Max(minIdx, 0), length));
				Assert.AreEqual(rollingHash, cleanHash);

				if (minIdx >= 0)
				{
					rollingHash = BuzHash.Sub(rollingHash, data[minIdx], length);
				}
			}
		}

		[TestMethod]
		public async Task BasicChunkingTests()
		{
			RefName refName = new RefName("test");
			ITreeWriter writer = _treeStore.CreateTreeWriter(refName.Text);

			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new ChunkingOptionsForNodeType(8, 8, 8);

			FileNode node = new LeafFileNode();
			node = await node.AppendAsync(new byte[7], options, writer, CancellationToken.None);
			Assert.IsTrue(node is LeafFileNode);
			Assert.AreEqual(7, ((LeafFileNode)node).Data.Length);

			node = new LeafFileNode();
			node = await node.AppendAsync(new byte[8], options, writer, CancellationToken.None);
			Assert.IsTrue(node is LeafFileNode);
			Assert.AreEqual(8, ((LeafFileNode)node).Data.Length);

			node = new LeafFileNode();
			node = await node.AppendAsync(new byte[9], options, writer, CancellationToken.None);
			Assert.IsTrue(node is InteriorFileNode);
			Assert.AreEqual(2, node.GetReferences().Count);

			FileNode? childNode1 = await ((TreeNodeRef<FileNode>)node.GetReferences()[0]).ExpandAsync();
			Assert.IsNotNull(childNode1);
			Assert.IsTrue(childNode1 is LeafFileNode);
			Assert.AreEqual(8, ((LeafFileNode)childNode1!).Data.Length);

			FileNode? childNode2 = await ((TreeNodeRef<FileNode>)node.GetReferences()[1]).ExpandAsync();
			Assert.IsNotNull(childNode2);
			Assert.IsTrue(childNode2 is LeafFileNode);
			Assert.AreEqual(1, ((LeafFileNode)childNode2!).Data.Length);
		}

		[TestMethod]
		public void SerializationTests()
		{
			BundleHeader oldHeader;
			{
				List<BundleImport> imports = new List<BundleImport>();
				imports.Add(new BundleImport(new BlobId("import1"), 10, new (int, IoHash)[] { (5, IoHash.Compute(Encoding.UTF8.GetBytes("blob1"))) }));
				imports.Add(new BundleImport(new BlobId("import2"), 20, new (int, IoHash)[] { (6, IoHash.Compute(Encoding.UTF8.GetBytes("blob2"))) }));

				List<BundleExport> exports = new List<BundleExport>();
				exports.Add(new BundleExport(IoHash.Compute(Encoding.UTF8.GetBytes("export1")), 2, new int[] { 1, 2 }));
				exports.Add(new BundleExport(IoHash.Compute(Encoding.UTF8.GetBytes("export2")), 3, new int[] { 3 }));

				List<BundlePacket> packets = new List<BundlePacket>();
				packets.Add(new BundlePacket(20, 40));
				packets.Add(new BundlePacket(10, 20));

				oldHeader = new BundleHeader(BundleCompressionFormat.LZ4, imports, exports, packets);
			}

			ByteArrayBuilder writer = new ByteArrayBuilder();
			oldHeader.Write(writer);
			byte[] serializedData = writer.ToByteArray();

			BundleHeader newHeader = new BundleHeader(new MemoryReader(serializedData));
				
			Assert.AreEqual(oldHeader.Imports.Count, newHeader.Imports.Count);
			for (int idx = 0; idx < oldHeader.Imports.Count; idx++)
			{
				BundleImport oldImport = oldHeader.Imports[idx];
				BundleImport newImport = newHeader.Imports[idx];

				Assert.AreEqual(oldImport.BlobId, newImport.BlobId);
				Assert.AreEqual(oldImport.ExportCount, newImport.ExportCount);
				Assert.AreEqual(oldImport.Exports.Count, newImport.Exports.Count);

				for (int importIdx = 0; importIdx < oldImport.Exports.Count; importIdx++)
				{
					(int Index, IoHash Hash) oldExport = oldImport.Exports[importIdx];
					(int Index, IoHash Hash) newExport = newImport.Exports[importIdx];

					Assert.AreEqual(oldExport.Hash, newExport.Hash);
					Assert.AreEqual(oldExport.Index, newExport.Index);
				}
			}

			Assert.AreEqual(oldHeader.Exports.Count, newHeader.Exports.Count);
			for (int idx = 0; idx < oldHeader.Exports.Count; idx++)
			{
				BundleExport oldExport = oldHeader.Exports[idx];
				BundleExport newExport = newHeader.Exports[idx];

				Assert.AreEqual(oldExport.Hash, newExport.Hash);
				Assert.AreEqual(oldExport.Length, newExport.Length);
				Assert.IsTrue(oldExport.References.SequenceEqual(newExport.References));
			}

			Assert.AreEqual(oldHeader.Packets.Count, newHeader.Packets.Count);
			for (int idx = 0; idx < oldHeader.Packets.Count; idx++)
			{
				BundlePacket oldPacket = oldHeader.Packets[idx];
				BundlePacket newPacket = newHeader.Packets[idx];
				Assert.AreEqual(oldPacket.DecodedLength, newPacket.DecodedLength);
				Assert.AreEqual(oldPacket.EncodedLength, newPacket.EncodedLength);
			}
		}

		[TestMethod]
		public async Task BasicTestDirectory()
		{
			InMemoryBlobStore blobStore = new InMemoryBlobStore();
			using ITreeStore treeStore = new BundleStore(blobStore, new BundleOptions());

			DirectoryNode root = new DirectoryNode(DirectoryFlags.None);
			DirectoryNode node = root.AddDirectory("hello");
			DirectoryNode node2 = node.AddDirectory("world");

			RefName refName = new RefName("testref");
			await treeStore.WriteTreeAsync(refName, root);

			// Should be stored inline
			Assert.AreEqual(1, blobStore.Refs.Count);
			Assert.AreEqual(1, blobStore.Blobs.Count);

			// Check the ref
			IBlob blob = await blobStore.ReadRefAsync(refName);
			Bundle bundle = new Bundle(new MemoryReader(blob.Data));
			Assert.AreEqual(0, bundle.Header.Imports.Count);
			Assert.AreEqual(3, bundle.Header.Exports.Count);

			// Create a new bundle and read it back in again
			DirectoryNode newRoot = await treeStore.ReadTreeAsync<DirectoryNode>(refName);

			Assert.AreEqual(0, newRoot.Files.Count);
			Assert.AreEqual(1, newRoot.Directories.Count);
			DirectoryNode? outputNode = await newRoot.FindDirectoryAsync("hello", CancellationToken.None);
			Assert.IsNotNull(outputNode);

			Assert.AreEqual(0, outputNode!.Files.Count);
			Assert.AreEqual(1, outputNode!.Directories.Count);
			DirectoryNode? outputNode2 = await outputNode.FindDirectoryAsync("world", CancellationToken.None);
			Assert.IsNotNull(outputNode2);

			Assert.AreEqual(0, outputNode2!.Files.Count);
			Assert.AreEqual(0, outputNode2!.Directories.Count);
		}

		[TestMethod]
		public async Task DedupTests()
		{
			BundleOptions options = new BundleOptions();
			options.MaxBlobSize = 1;

			InMemoryBlobStore blobStore = new InMemoryBlobStore();
			using ITreeStore treeStore = new BundleStore(blobStore, options);

			DirectoryNode root = new DirectoryNode(DirectoryFlags.None);
			root.AddDirectory("node1");
			root.AddDirectory("node2");
			root.AddDirectory("node3");

			Assert.AreEqual(0, blobStore.Refs.Count);
			Assert.AreEqual(0, blobStore.Blobs.Count);

			RefName refName = new RefName("ref");
			await treeStore.WriteTreeAsync(refName, root);

			Assert.AreEqual(1, blobStore.Refs.Count);
			Assert.AreEqual(2, blobStore.Blobs.Count);
		}

		[TestMethod]
		public async Task ReloadTests()
		{
			BundleOptions options = new BundleOptions();
			options.MaxBlobSize = 1;

			InMemoryBlobStore blobStore = new InMemoryBlobStore();

			RefName refName = new RefName("ref");

			using (ITreeStore oldBundle = new BundleStore(blobStore, options))
			{
				DirectoryNode root = new DirectoryNode(DirectoryFlags.None);

				DirectoryNode node1 = root.AddDirectory("node1");
				DirectoryNode node2 = node1.AddDirectory("node2");
				DirectoryNode node3 = node2.AddDirectory("node3");
				DirectoryNode node4 = node3.AddDirectory("node4");

				await oldBundle.WriteTreeAsync(refName, root);

				Assert.AreEqual(1, blobStore.Refs.Count);
				Assert.AreEqual(5, blobStore.Blobs.Count);
			}

			using (ITreeStore newBundle = new BundleStore(blobStore, options))
			{
				DirectoryNode root = await newBundle.ReadTreeAsync<DirectoryNode>(refName);

				DirectoryNode? newNode1 = await root.FindDirectoryAsync("node1", CancellationToken.None);
				Assert.IsNotNull(newNode1);

				DirectoryNode? newNode2 = await newNode1!.FindDirectoryAsync("node2", CancellationToken.None);
				Assert.IsNotNull(newNode2);

				DirectoryNode? newNode3 = await newNode2!.FindDirectoryAsync("node3", CancellationToken.None);
				Assert.IsNotNull(newNode3);

				DirectoryNode? newNode4 = await newNode3!.FindDirectoryAsync("node4", CancellationToken.None);
				Assert.IsNotNull(newNode4);
			}
		}
		/*
		[TestMethod]
		public async Task CompactTest()
		{
			BundleOptions options = new BundleOptions();
			options.MaxBlobSize = 1024 * 1024;
			options.MaxInlineBlobSize = 1;

			using Bundle<DirectoryNode> newBundle = Bundle.Create<DirectoryNode>(_storageClient, _namespaceId, new DirectoryNode(), options, _cache);

			DirectoryNode node1 = newBundle.Root.AddDirectory("node1");
			DirectoryNode node2 = node1.AddDirectory("node2");
			DirectoryNode node3 = node2.AddDirectory("node3");
			DirectoryNode node4 = newBundle.Root.AddDirectory("node4"); // same contents as node 3

			RefId refId1 = new RefId("ref1");
			await newBundle.WriteAsync(_bucketId, refId1, CbObject.Empty, false);

			Assert.AreEqual(1, _storageClient.Refs.Count);
			Assert.AreEqual(1, _storageClient.Blobs.Count);

			IRef ref1 = _storageClient.Refs[(_namespaceId, _bucketId, refId1)];

			BundleRoot root1 = CbSerializer.Deserialize<BundleRoot>(ref1.Value);
			BundleObject rootObject1 = root1.Object;
			Assert.AreEqual(1, rootObject1.Exports.Count);
			Assert.AreEqual(1, rootObject1.ImportObjects.Count);

			IoHash leafHash1 = rootObject1.ImportObjects[0].Object.Hash;
			BundleObject leafObject1 = CbSerializer.Deserialize<BundleObject>(_storageClient.Blobs[(_namespaceId, leafHash1)]);
			Assert.AreEqual(3, leafObject1.Exports.Count); // node1 + node2 + node3 (== node4)
			Assert.AreEqual(0, leafObject1.ImportObjects.Count);

			// Remove one of the nodes from the root without compacting. the existing blob should be reused.
			newBundle.Root.DeleteDirectory("node1");

			RefId refId2 = new RefId("ref2");
			await newBundle.WriteAsync(_bucketId, refId2, CbObject.Empty, false);

			IRef ref2 = _storageClient.Refs[(_namespaceId, _bucketId, refId2)];

			BundleRoot root2 = CbSerializer.Deserialize<BundleRoot>(ref2.Value);
			BundleObject rootObject2 = root2.Object;
			Assert.AreEqual(1, rootObject2.Exports.Count);
			Assert.AreEqual(1, rootObject2.ImportObjects.Count);

			IoHash leafHash2 = rootObject2.ImportObjects[0].Object.Hash;
			Assert.AreEqual(leafHash1, leafHash2);
			Assert.AreEqual(3, leafObject1.Exports.Count); // unused: node1 + node2 + node3, used: node4 (== node3)
			Assert.AreEqual(0, leafObject1.ImportObjects.Count);

			// Repack it and check that we make a new object
			RefId refId3 = new RefId("ref3");
			await newBundle.WriteAsync(_bucketId, refId3, CbObject.Empty, true);

			IRef ref3 = _storageClient.Refs[(_namespaceId, _bucketId, refId3)];

			BundleRoot root3 = CbSerializer.Deserialize<BundleRoot>(ref3.Value);
			BundleObject rootObject3 = root3.Object;
			Assert.AreEqual(1, rootObject3.Exports.Count);
			Assert.AreEqual(1, rootObject3.ImportObjects.Count);

			IoHash leafHash3 = rootObject3.ImportObjects[0].Object.Hash;
			Assert.AreNotEqual(leafHash1, leafHash3);

			BundleObject leafObject3 = CbSerializer.Deserialize<BundleObject>(_storageClient.Blobs[(_namespaceId, leafHash3)]);
			Assert.AreEqual(1, leafObject3.Exports.Count);
			Assert.AreEqual(0, leafObject3.ImportObjects.Count);
		}
*/
		[TestMethod]
		public async Task CoreAppendTest()
		{
			ITreeWriter writer = _treeStore.CreateTreeWriter();

			byte[] data = new byte[4096];
			new Random(0).NextBytes(data);

			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new ChunkingOptionsForNodeType(16, 64, 256);

			FileNode node = new LeafFileNode();
			for (int idx = 0; idx < data.Length; idx++)
			{
				node = await node.AppendAsync(data.AsMemory(idx, 1), options, writer, CancellationToken.None);

				byte[] outputData = await node.ToByteArrayAsync(CancellationToken.None);
				Assert.IsTrue(data.AsMemory(0, idx + 1).Span.SequenceEqual(outputData.AsSpan(0, idx + 1)));
			}
		}

		[TestMethod]
		public async Task FixedSizeChunkingTests()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new ChunkingOptionsForNodeType(64, 64, 64);
			options.InteriorOptions = new ChunkingOptionsForNodeType(IoHash.NumBytes * 4, IoHash.NumBytes * 4, IoHash.NumBytes * 4);

			await ChunkingTests(options);
		}

		[TestMethod]
		public async Task VariableSizeChunkingTests()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new ChunkingOptionsForNodeType(32, 64, 96);
			options.InteriorOptions = new ChunkingOptionsForNodeType(IoHash.NumBytes * 1, IoHash.NumBytes * 4, IoHash.NumBytes * 12);

			await ChunkingTests(options);
		}

		async Task ChunkingTests(ChunkingOptions options)
		{
			ITreeWriter writer = _treeStore.CreateTreeWriter();

			byte[] data = new byte[4096];
			new Random(0).NextBytes(data);

			for (int idx = 0; idx < data.Length; idx++)
			{
				data[idx] = (byte)idx;
			}

			FileNode root = new LeafFileNode();

			const int NumIterations = 100;
			for (int idx = 0; idx < NumIterations; idx++)
			{
				root = await root.AppendAsync(data, options, writer, CancellationToken.None);
			}

			byte[] result = await root.ToByteArrayAsync(CancellationToken.None);
			Assert.AreEqual(NumIterations * data.Length, result.Length);

			for (int idx = 0; idx < NumIterations; idx++)
			{
				ReadOnlyMemory<byte> spanData = result.AsMemory(idx * data.Length, data.Length);
				Assert.IsTrue(spanData.Span.SequenceEqual(data));
			}

			await CheckSizes(root, options, true);
		}

		async Task CheckSizes(FileNode node, ChunkingOptions options, bool rightmost)
		{
			if (node is LeafFileNode leafNode)
			{
				Assert.IsTrue(rightmost || leafNode.Data.Length >= options.LeafOptions.MinSize);
				Assert.IsTrue(leafNode.Data.Length <= options.LeafOptions.MaxSize);
			}
			else
			{
				InteriorFileNode interiorNode = (InteriorFileNode)node;

				Assert.IsTrue(rightmost || interiorNode.Children.Count >= options.InteriorOptions.MinSize);
				Assert.IsTrue(interiorNode.Children.Count <= options.InteriorOptions.MaxSize);

				int childCount = interiorNode.Children.Count;
				for (int idx = 0; idx < childCount; idx++)
				{
					FileNode childNode = await interiorNode.Children[idx].ExpandAsync(CancellationToken.None);
					await CheckSizes(childNode, options, idx == childCount - 1);
				}
			}
		}

		[TestMethod]
		public async Task SpillTestAsync()
		{
			BundleOptions options = new BundleOptions();
			options.MaxBlobSize = 1;

			using BundleStore treeStore = new BundleStore(_blobStore, options);

			RefName refName = new RefName("ref");
			ITreeWriter writer = treeStore.CreateTreeWriter(refName.Text);

			DirectoryNode root = new DirectoryNode(DirectoryFlags.None);

			long totalLength = 0;
			for (int idxA = 0; idxA < 10; idxA++)
			{
				DirectoryNode nodeA = root.AddDirectory($"{idxA}");
				for (int idxB = 0; idxB < 10; idxB++)
				{
					DirectoryNode nodeB = nodeA.AddDirectory($"{idxB}");
					for (int idxC = 0; idxC < 10; idxC++)
					{
						DirectoryNode nodeC = nodeB.AddDirectory($"{idxC}");
						for (int idxD = 0; idxD < 10; idxD++)
						{
							FileEntry file = nodeC.AddFile($"{idxD}", FileEntryFlags.None);
							byte[] data = Encoding.UTF8.GetBytes($"This is file {idxA}/{idxB}/{idxC}/{idxD}");
							totalLength += data.Length;
							await file.AppendAsync(data, new ChunkingOptions(), writer, CancellationToken.None);
						}
					}

					int oldWorkingSetSize = GetWorkingSetSize(root);
					await treeStore.WriteTreeAsync(refName, root);
					int newWorkingSetSize = GetWorkingSetSize(root);
					Assert.IsTrue(newWorkingSetSize <= oldWorkingSetSize);
					Assert.IsTrue(newWorkingSetSize <= 20);
				}
			}

			Assert.IsTrue(_blobStore.Blobs.Count > 0);
			Assert.IsTrue(_blobStore.Refs.Count == 1);

			await treeStore.WriteTreeAsync(refName, root, CancellationToken.None);

			Assert.AreEqual(totalLength, root.Length);

			Assert.IsTrue(_blobStore.Blobs.Count > 0);
			Assert.IsTrue(_blobStore.Refs.Count == 1);
		}

		int GetWorkingSetSize(TreeNode node)
		{
			int size = 0;
			foreach (TreeNodeRef nodeRef in node.GetReferences())
			{
				if (nodeRef.Node != null)
				{
					size += 1 + GetWorkingSetSize(nodeRef.Node);
				}
			}
			return size;
		}
	}
}

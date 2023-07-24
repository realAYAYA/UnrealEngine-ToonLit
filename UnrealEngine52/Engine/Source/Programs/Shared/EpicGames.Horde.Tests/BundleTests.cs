// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
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
		readonly IMemoryCache _cache;
		readonly MemoryStorageClient _storage;

		public BundleTests()
		{
			_cache = new MemoryCache(new MemoryCacheOptions());
			_storage = new MemoryStorageClient();
		}

		public void Dispose()
		{
			_cache.Dispose();
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
			TreeReader reader = new TreeReader(_storage, null, NullLogger.Instance);
			using TreeWriter writer = new TreeWriter(_storage, new TreeOptions(), refName.Text);

			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new ChunkingOptionsForNodeType(8, 8, 8);

			FileNodeWriter fileNodeWriter = new FileNodeWriter(writer, options);

			NodeHandle handle;
			FileNode node;

			handle = await fileNodeWriter.CreateAsync(new byte[7], CancellationToken.None);
			node = await reader.ReadNodeAsync<FileNode>(handle.Locator);
			Assert.IsTrue(node is LeafFileNode);
			Assert.AreEqual(7, ((LeafFileNode)node).Data.Length);

			handle = await fileNodeWriter.CreateAsync(new byte[8], CancellationToken.None);
			node = await reader.ReadNodeAsync<FileNode>(handle.Locator);
			Assert.IsTrue(node is LeafFileNode);
			Assert.AreEqual(8, ((LeafFileNode)node).Data.Length);

			handle = await fileNodeWriter.CreateAsync(new byte[9], CancellationToken.None);
			node = await reader.ReadNodeAsync<FileNode>(handle.Locator);
			Assert.IsTrue(node is InteriorFileNode);
			Assert.AreEqual(2, ((InteriorFileNode)node).Children.Count);

			FileNode? childNode1 = await ((TreeNodeRef<FileNode>)((InteriorFileNode)node).Children[0]).ExpandAsync(reader);
			Assert.IsNotNull(childNode1);
			Assert.IsTrue(childNode1 is LeafFileNode);
			Assert.AreEqual(8, ((LeafFileNode)childNode1!).Data.Length);

			FileNode? childNode2 = await ((TreeNodeRef<FileNode>)((InteriorFileNode)node).Children[1]).ExpandAsync(reader);
			Assert.IsNotNull(childNode2);
			Assert.IsTrue(childNode2 is LeafFileNode);
			Assert.AreEqual(1, ((LeafFileNode)childNode2!).Data.Length);
		}

		[TestMethod]
		public void SerializationTests()
		{
			BundleHeader oldHeader;
			{
				List<BundleType> types = new List<BundleType>();
				types.Add(new BundleType(Guid.NewGuid(), 0));

				List<BundleImport> imports = new List<BundleImport>();
				imports.Add(new BundleImport(new BlobLocator("import1"), new[] { 5 }));
				imports.Add(new BundleImport(new BlobLocator("import2"), new[] { 6 }));

				List<BundleExport> exports = new List<BundleExport>();
				exports.Add(new BundleExport(0, IoHash.Compute(Encoding.UTF8.GetBytes("export1")), 2, new int[] { 1, 2 }));
				exports.Add(new BundleExport(0, IoHash.Compute(Encoding.UTF8.GetBytes("export2")), 3, new int[] { 3 }));

				List<BundlePacket> packets = new List<BundlePacket>();
				packets.Add(new BundlePacket(20, 40));
				packets.Add(new BundlePacket(10, 20));

				oldHeader = new BundleHeader(BundleCompressionFormat.LZ4, types, imports, exports, packets);
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

				Assert.AreEqual(oldImport.Locator, newImport.Locator);
				Assert.AreEqual(oldImport.Exports.Count, newImport.Exports.Count);

				for (int importIdx = 0; importIdx < oldImport.Exports.Count; importIdx++)
				{
					int oldExportIdx = oldImport.Exports[importIdx];
					int newExportIdx = newImport.Exports[importIdx];

					Assert.AreEqual(oldExportIdx, newExportIdx);
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
			MemoryStorageClient store = _storage;
			TreeReader reader = new TreeReader(store, null, NullLogger.Instance);

			DirectoryNode root = new DirectoryNode(DirectoryFlags.None);
			DirectoryNode node = root.AddDirectory("hello");
			DirectoryNode node2 = node.AddDirectory("world");

			RefName refName = new RefName("testref");
			await store.WriteNodeAsync(refName, root);

			// Should be stored inline
			Assert.AreEqual(1, store.Refs.Count);
			Assert.AreEqual(1, store.Blobs.Count);

			// Check the ref
			NodeHandle refTarget = await store.ReadRefTargetAsync(refName);
			Bundle bundle = await store.ReadBundleAsync(refTarget.Locator.Blob);
			Assert.AreEqual(0, bundle.Header.Imports.Count);
			Assert.AreEqual(3, bundle.Header.Exports.Count);

			// Create a new bundle and read it back in again
			DirectoryNode newRoot = await reader.ReadNodeAsync<DirectoryNode>(refName);

			Assert.AreEqual(0, newRoot.Files.Count);
			Assert.AreEqual(1, newRoot.Directories.Count);
			DirectoryNode? outputNode = await newRoot.FindDirectoryAsync(reader, "hello", CancellationToken.None);
			Assert.IsNotNull(outputNode);

			Assert.AreEqual(0, outputNode!.Files.Count);
			Assert.AreEqual(1, outputNode!.Directories.Count);
			DirectoryNode? outputNode2 = await outputNode.FindDirectoryAsync(reader, "world", CancellationToken.None);
			Assert.IsNotNull(outputNode2);

			Assert.AreEqual(0, outputNode2!.Files.Count);
			Assert.AreEqual(0, outputNode2!.Directories.Count);
		}

		[TestMethod]
		public async Task DedupTests()
		{
			TreeOptions options = new TreeOptions();
			options.MaxBlobSize = 1;

			DirectoryNode root = new DirectoryNode(DirectoryFlags.None);
			root.AddDirectory("node1");
			root.AddDirectory("node2");
			root.AddDirectory("node3");

			Assert.AreEqual(0, _storage.Refs.Count);
			Assert.AreEqual(0, _storage.Blobs.Count);

			RefName refName = new RefName("ref");
			await _storage.WriteNodeAsync(refName, root, options);

			Assert.AreEqual(1, _storage.Refs.Count);
			Assert.AreEqual(2, _storage.Blobs.Count);
		}

		[TestMethod]
		public async Task ReloadTests()
		{
			TreeOptions options = new TreeOptions();
			options.MaxBlobSize = 1;

			RefName refName = new RefName("ref");

			{
				DirectoryNode root = new DirectoryNode(DirectoryFlags.None);

				DirectoryNode node1 = root.AddDirectory("node1");
				DirectoryNode node2 = node1.AddDirectory("node2");
				DirectoryNode node3 = node2.AddDirectory("node3");
				DirectoryNode node4 = node3.AddDirectory("node4");

				await _storage.WriteNodeAsync(refName, root, options);

				Assert.AreEqual(1, _storage.Refs.Count);
				Assert.AreEqual(5, _storage.Blobs.Count);
			}

			{
				TreeReader reader = new TreeReader(_storage, null, NullLogger.Instance);

				DirectoryNode root = await reader.ReadNodeAsync<DirectoryNode>(refName);

				DirectoryNode? newNode1 = await root.FindDirectoryAsync(reader, "node1", CancellationToken.None);
				Assert.IsNotNull(newNode1);

				DirectoryNode? newNode2 = await newNode1!.FindDirectoryAsync(reader, "node2", CancellationToken.None);
				Assert.IsNotNull(newNode2);

				DirectoryNode? newNode3 = await newNode2!.FindDirectoryAsync(reader, "node3", CancellationToken.None);
				Assert.IsNotNull(newNode3);

				DirectoryNode? newNode4 = await newNode3!.FindDirectoryAsync(reader, "node4", CancellationToken.None);
				Assert.IsNotNull(newNode4);
			}
		}
	}
}

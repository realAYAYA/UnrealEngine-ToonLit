// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Buffers;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Threading;
using System.Text;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging.Abstractions;
using System.Reflection;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class BundleStoreTests
	{
		[TestMethod]
		public async Task CreateBundlesManuallyAsync()
		{
			Bundle a = CreateBundleManually();
			Bundle b = await CreateBundleNormalAsync();

			byte[] bytesA = a.AsSequence().ToArray();
			byte[] bytesB = b.AsSequence().ToArray();
			Assert.IsTrue(bytesA.SequenceEqual(bytesB));
		}

		[NodeType("{F63606D4-5DBB-4061-A655-6F444F65229E}")]
		class TextNode : Node
		{
			public string Text { get; }

			public TextNode(string text) => Text = text;

			public TextNode(NodeReader reader)
			{
				Text = reader.ReadString();
			}

			public override void Serialize(NodeWriter writer)
			{
				writer.WriteString(Text);
			}
		}

		public BundleStoreTests()
		{
			Node.RegisterTypesFromAssembly(Assembly.GetExecutingAssembly());
		}

		static async Task<Bundle> CreateBundleNormalAsync()
		{
			MemoryStorageClient store = new MemoryStorageClient();
			await using IStorageWriter writer = store.CreateWriter(options: new BundleOptions { CompressionFormat = BundleCompressionFormat.None });

			TextNode node = new TextNode("Hello world");
			BlobHandle handle = await writer.FlushAsync(node, CancellationToken.None);

			return await store.ReadBundleAsync(handle.GetLocator().Blob);
		}

		static Bundle CreateBundleManually()
		{
			ArrayMemoryWriter payloadWriter = new ArrayMemoryWriter(200);
			payloadWriter.WriteString("Hello world");
			byte[] payload = payloadWriter.WrittenMemory.ToArray();

			List<BlobType> types = new List<BlobType>();
			types.Add(new BlobType(Guid.Parse("F63606D4-5DBB-4061-A655-6F444F65229E"), 1));

			List<BundleExport> exports = new List<BundleExport>();
			exports.Add(new BundleExport(0, IoHash.Compute(payload), 0, 0, payload.Length, Array.Empty<BundleExportRef>()));

			List<BundlePacket> packets = new List<BundlePacket>();
			packets.Add(new BundlePacket(BundleCompressionFormat.None, 0, payload.Length, payload.Length));

			BundleHeader header = BundleHeader.Create(types, Array.Empty<BlobLocator>(), exports, packets);
			return new Bundle(header, new List<ReadOnlyMemory<byte>> { payload });
		}

		[TestMethod]
		public async Task TestTreeAsync()
		{
			using IMemoryCache cache = new MemoryCache(new MemoryCacheOptions());

			MemoryStorageClient blobStore = new MemoryStorageClient();
			await TestTreeAsync(blobStore, new BundleOptions { MaxBlobSize = 1024 * 1024 });

			Assert.AreEqual(1, blobStore.Blobs.Count);
			Assert.AreEqual(1, blobStore.Refs.Count);
		}

		[TestMethod]
		public async Task TestTreeSeparateBlobsAsync()
		{
			using IMemoryCache cache = new MemoryCache(new MemoryCacheOptions());

			MemoryStorageClient blobStore = new MemoryStorageClient();
			await TestTreeAsync(blobStore, new BundleOptions { MaxBlobSize = 1 });

			Assert.AreEqual(5, blobStore.Blobs.Count);
			Assert.AreEqual(1, blobStore.Refs.Count);
		}

		[NodeType("{F63606D4-5DBB-4061-A655-6F444F65229F}")]
		class SimpleNode : Node
		{
			public ReadOnlySequence<byte> Data { get; }
			public IReadOnlyList<NodeRef<SimpleNode>> Refs { get; }

			public SimpleNode(ReadOnlySequence<byte> data, IReadOnlyList<NodeRef<SimpleNode>> refs)
			{
				Data = data;
				Refs = refs;
			}

			public SimpleNode(NodeReader reader)
			{
				Data = new ReadOnlySequence<byte>(reader.ReadVariableLengthBytes());
				Refs = reader.ReadVariableLengthArray(() => reader.ReadNodeRef<SimpleNode>());
			}

			public override void Serialize(NodeWriter writer)
			{
				writer.WriteVariableLengthBytes(Data);
				writer.WriteVariableLengthArray(Refs, x => writer.WriteNodeRef(x));
			}
		}

		static async Task TestTreeAsync(MemoryStorageClient store, BundleOptions options)
		{
			// Generate a tree
			{
				await using IStorageWriter writer = store.CreateWriter(new RefName("test"), options);

				SimpleNode node1 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 1 }), Array.Empty<NodeRef<SimpleNode>>());
				SimpleNode node2 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 2 }), new[] { await writer.WriteNodeAsync(node1) });
				SimpleNode node3 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 3 }), new[] { await writer.WriteNodeAsync(node2) });
				SimpleNode node4 = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 4 }), Array.Empty<NodeRef<SimpleNode>>());

				SimpleNode root = new SimpleNode(new ReadOnlySequence<byte>(new byte[] { 5 }), new[] { await writer.WriteNodeAsync(node4), await writer.WriteNodeAsync(node3) });

				await store.WriteRefTargetAsync(new RefName("test"), await writer.WriteNodeAsync(root));

				BundleReader reader = new BundleReader(store, null, NullLogger.Instance);
				await CheckTree(root);
			}

			// Check we can read it back in
			{
				SimpleNode root = await store.ReadNodeAsync<SimpleNode>(new RefName("test"));
				await CheckTree(root);
			}
		}

		static async Task CheckTree(SimpleNode root)
		{
			SimpleNode node5 = root;
			byte[] data5 = node5.Data.ToArray();
			Assert.IsTrue(data5.SequenceEqual(new byte[] { 5 }));
			IReadOnlyList<NodeRef<SimpleNode>> refs5 = node5.Refs;
			Assert.AreEqual(2, refs5.Count);

			SimpleNode node4 = await refs5[0].ExpandAsync();
			byte[] data4 = node4.Data.ToArray();
			Assert.IsTrue(data4.SequenceEqual(new byte[] { 4 }));
			IReadOnlyList<NodeRef<SimpleNode>> refs4 = node4.Refs;
			Assert.AreEqual(0, refs4.Count);

			SimpleNode node3 = await refs5[1].ExpandAsync();
			byte[] data3 = node3.Data.ToArray();
			Assert.IsTrue(data3.SequenceEqual(new byte[] { 3 }));
			IReadOnlyList<NodeRef<SimpleNode>> refs3 = node3.Refs;
			Assert.AreEqual(1, refs3.Count);

			SimpleNode node2 = await refs3[0].ExpandAsync();
			byte[] data2 = node2.Data.ToArray();
			Assert.IsTrue(data2.SequenceEqual(new byte[] { 2 }));
			IReadOnlyList<NodeRef<SimpleNode>> refs2 = node2.Refs;
			Assert.AreEqual(1, refs2.Count);

			SimpleNode node1 = await refs2[0].ExpandAsync();
			byte[] data1 = node1.Data.ToArray();
			Assert.IsTrue(data1.SequenceEqual(new byte[] { 1 }));
			IReadOnlyList<NodeRef<SimpleNode>> refs1 = node1.Refs;
			Assert.AreEqual(0, refs1.Count);
		}

		[TestMethod]
		public async Task SimpleNodeAsync()
		{
			MemoryStorageClient store = new MemoryStorageClient();

			RefName refName = new RefName("test");
			await store.WriteNodeAsync(refName, new SimpleNode(new ReadOnlySequence<byte>(new byte[] { (byte)123 }), Array.Empty<NodeRef<SimpleNode>>()));

			SimpleNode node = await store.ReadNodeAsync<SimpleNode>(refName);

			Assert.AreEqual(123, node.Data.FirstSpan[0]);
		}

		[TestMethod]
		public async Task DirectoryNodesAsync()
		{
			MemoryStorageClient store = new MemoryStorageClient();

			// Generate a tree
			{
				await using (IStorageWriter writer = store.CreateWriter(new RefName("test")))
				{
					DirectoryNode world = new DirectoryNode();
					NodeRef<DirectoryNode> worldRef = await writer.WriteNodeAsync(world);

					DirectoryNode hello = new DirectoryNode();
					hello.AddDirectory(new DirectoryEntry("world", 0, worldRef));
					NodeRef<DirectoryNode> helloRef = await writer.WriteNodeAsync(hello);

					DirectoryNode root = new DirectoryNode(DirectoryFlags.None);
					root.AddDirectory(new DirectoryEntry("hello", 0, helloRef));
					NodeRef<DirectoryNode> rootRef = await writer.WriteNodeAsync(root);

					await writer.FlushAsync();

					await store.WriteRefTargetAsync(new RefName("test"), rootRef);
				}
			}

			// Check we can read it back in
			{
				DirectoryNode root = await store.ReadNodeAsync<DirectoryNode>(new RefName("test"));
				await CheckDirectoryTreeAsync(root);
			}
		}

		static async Task CheckDirectoryTreeAsync(DirectoryNode root)
		{
			Assert.AreEqual(1, root.Directories.Count);
			Assert.AreEqual("hello", root.Directories.First().Name);

			DirectoryNode hello = await root.Directories.First().ExpandAsync(CancellationToken.None);
			Assert.AreEqual(1, hello.Directories.Count);
			Assert.AreEqual("world", hello.Directories.First().Name);

			DirectoryNode world = await hello.Directories.First().ExpandAsync(CancellationToken.None);
			Assert.AreEqual(0, world.Directories.Count);
		}

		[TestMethod]
		public async Task FileNodesAsync()
		{
			using IMemoryCache cache = new MemoryCache(new MemoryCacheOptions());
			MemoryStorageClient store = new MemoryStorageClient();
			BundleReader reader = new BundleReader(store, null, NullLogger.Instance);

			// Generate a tree
			{
				await using IStorageWriter writer = store.CreateWriter();

				ChunkedDataWriter fileWriter = new ChunkedDataWriter(writer, new ChunkingOptions());
				NodeRef<ChunkedDataNode> fileHandle = await fileWriter.CreateAsync(Encoding.UTF8.GetBytes("world"), CancellationToken.None);

				List<FileUpdate> fileUpdates = new List<FileUpdate>();
				fileUpdates.Add(new FileUpdate("hello/world", FileEntryFlags.None, fileWriter.Length, fileHandle));

				DirectoryNode root = new DirectoryNode();
				await root.UpdateAsync(fileUpdates, writer);

				NodeRef<DirectoryNode> rootRef = await writer.WriteNodeAsync(root);
				await store.WriteRefTargetAsync(new RefName("test"), rootRef);

				await CheckFileTreeAsync(root);
			}

			// Check we can read it back in
			{
				DirectoryNode root = await store.ReadNodeAsync<DirectoryNode>(new RefName("test"));
				await CheckFileTreeAsync(root);
			}
		}

		static async Task CheckFileTreeAsync(DirectoryNode root)
		{
			Assert.AreEqual(1, root.Directories.Count);
			Assert.AreEqual("hello", root.Directories.First().Name);

			DirectoryNode hello = await root.Directories.First().ExpandAsync();
			Assert.AreEqual(0, hello.Directories.Count);
			Assert.AreEqual(1, hello.Files.Count);
			Assert.AreEqual("world", hello.Files.First().Name);

			ChunkedDataNode world = await hello.Files.First().ExpandAsync();

			byte[] worldData = await GetFileDataAsync(world);
			Assert.IsTrue(worldData.SequenceEqual(Encoding.UTF8.GetBytes("world")));
		}

		[TestMethod]
		public async Task StreamTestAsync()
		{
			MemoryStorageClient store = new MemoryStorageClient();

			const int length = 4096;

			byte[] chunk = new byte[length];
			new Random(0).NextBytes(chunk);

			// Generate a tree
			NodeRef<ChunkedDataNode> nodeRef;
			{
				await using IStorageWriter writer = store.CreateWriter(options: new BundleOptions { MaxBlobSize = 1024 });

				ChunkingOptions options = new ChunkingOptions();
				options.LeafOptions = new LeafChunkedDataNodeOptions(128, 256, 64 * 1024);

				ChunkedDataWriter fileWriter = new ChunkedDataWriter(writer, options);
				for (int idx = 0; idx < chunk.Length / 16; idx++)
				{
					await fileWriter.AppendAsync(chunk.AsMemory(idx * 16, 16), CancellationToken.None);
				}

				nodeRef = await fileWriter.FlushAsync(CancellationToken.None);
			}

			// Check we can read it back in
			{
				ChunkedDataNode newRoot = await nodeRef.ExpandAsync();

				using MemoryStream stream = new MemoryStream();
				await newRoot.CopyToStreamAsync(stream, CancellationToken.None);

				byte[] output = stream.ToArray();
				Assert.IsTrue(chunk.SequenceEqual(output));
			}
		}

		[TestMethod]
		public async Task LargeFileTestAsync()
		{
			using IMemoryCache cache = new MemoryCache(new MemoryCacheOptions());
			MemoryStorageClient store = new MemoryStorageClient();

			const int length = 1024;
			const int copies = 4096;

			byte[] chunk = new byte[length];
			new Random(0).NextBytes(chunk);

			byte[] data = new byte[chunk.Length * copies];
			for (int idx = 0; idx < copies; idx++)
			{
				chunk.CopyTo(data.AsSpan(idx * chunk.Length));
			}

			// Generate a tree
			DirectoryNode root;
			{
				await using IStorageWriter writer = store.CreateWriter(options: new BundleOptions { MaxBlobSize = 1024 });

				ChunkingOptions options = new ChunkingOptions();
				options.LeafOptions = new LeafChunkedDataNodeOptions(128, 256, 64 * 1024);

				ChunkedDataWriter fileWriter = new ChunkedDataWriter(writer, options);
				NodeRef<ChunkedDataNode> nodeRef = await fileWriter.CreateAsync(data, CancellationToken.None);

				root = new DirectoryNode(DirectoryFlags.None);
				root.AddFile("test", FileEntryFlags.None, fileWriter.Length, nodeRef);

				NodeRef<DirectoryNode> rootRef = await writer.WriteNodeAsync(root);
				await store.WriteRefTargetAsync(new RefName("test"), rootRef);

				await CheckLargeFileTreeAsync(root, data);
			}

			// Check we can read it back in
			{
				DirectoryNode newRoot = await store.ReadNodeAsync<DirectoryNode>(new RefName("test"));
				await CompareTrees(root, newRoot);
				await CheckLargeFileTreeAsync(root, data);

				NodeRef<ChunkedDataNode> file = root.GetFileEntry("test");

				long uniqueSize = store.Blobs.Values.SelectMany(x => x.Header.Packets).Sum(x => x.DecodedLength);
				Assert.IsTrue(uniqueSize < data.Length / 3); // random fraction meaning "lots of dedupe happened"
			}
		}

		static async Task CompareTrees(DirectoryNode oldNode, DirectoryNode newNode)
		{
			Assert.AreEqual(oldNode.Length, newNode.Length);
			Assert.AreEqual(oldNode.Files.Count, newNode.Files.Count);
			Assert.AreEqual(oldNode.Directories.Count, newNode.Directories.Count);

			foreach ((FileEntry oldFileEntry, FileEntry newFileEntry) in oldNode.Files.Zip(newNode.Files))
			{
				ChunkedDataNode oldFile = await oldFileEntry.ExpandAsync();
				ChunkedDataNode newFile = await newFileEntry.ExpandAsync();
				await CompareTrees(oldFile, newFile);
			}
		}

		static async Task CompareTrees(ChunkedDataNode oldNode, ChunkedDataNode newNode)
		{
			if (oldNode is InteriorChunkedDataNode oldInteriorNode)
			{
				InteriorChunkedDataNode newInteriorNode = (InteriorChunkedDataNode)newNode;
				Assert.AreEqual(oldInteriorNode.Children.Count, newInteriorNode.Children.Count);

				int index = 0;
				foreach ((NodeRef<ChunkedDataNode> oldFileRef, NodeRef<ChunkedDataNode> newFileRef) in oldInteriorNode.Children.Zip(newInteriorNode.Children))
				{
					ChunkedDataNode oldFile = await oldFileRef.ExpandAsync();
					ChunkedDataNode newFile = await newFileRef.ExpandAsync();
					await CompareTrees(oldFile, newFile);
					index++;
				}
			}
			else if (oldNode is LeafChunkedDataNode oldLeafNode)
			{
				LeafChunkedDataNode newLeafNode = (LeafChunkedDataNode)newNode;
				Assert.IsTrue(oldLeafNode.Data.Span.SequenceEqual(newLeafNode.Data.Span));
			}
			else
			{
				throw new NotImplementedException();
			}
		}

		static async Task CheckLargeFileTreeAsync(DirectoryNode root, byte[] data)
		{
			Assert.AreEqual(0, root.Directories.Count);
			Assert.AreEqual(1, root.Files.Count);

			ChunkedDataNode world = await root.Files.First().ExpandAsync(CancellationToken.None);

			int length = await CheckFileDataAsync(world, data);
			Assert.AreEqual(data.Length, length);
		}

		static async Task<int> CheckFileDataAsync(ChunkedDataNode fileNode, ReadOnlyMemory<byte> data)
		{
			int offset = 0;
			if (fileNode is LeafChunkedDataNode leafNode)
			{
				Assert.IsTrue(leafNode.Data.Span.SequenceEqual(data.Span.Slice(offset, leafNode.Data.Length)));
				offset += leafNode.Data.Length;
			}
			else if (fileNode is InteriorChunkedDataNode interiorFileNode)
			{
				foreach (NodeRef<ChunkedDataNode> childRef in interiorFileNode.Children)
				{
					ChunkedDataNode child = await childRef.ExpandAsync();
					offset += await CheckFileDataAsync(child, data.Slice(offset));
				}
			}
			else
			{
				throw new NotImplementedException();
			}
			return offset;
		}

		static async Task<byte[]> GetFileDataAsync(ChunkedDataNode fileNode)
		{
			using (MemoryStream stream = new MemoryStream())
			{
				await fileNode.CopyToStreamAsync(stream, CancellationToken.None);
				return stream.ToArray();
			}
		}
	}
}

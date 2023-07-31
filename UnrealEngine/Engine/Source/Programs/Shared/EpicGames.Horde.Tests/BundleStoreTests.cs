// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Buffers;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Threading;
using System.Text;
using System.IO;
using System.Security.Cryptography;
using EpicGames.Core;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class BundleStoreTests
	{
		[TestMethod]
		public async Task TestTreeAsync()
		{
			InMemoryBlobStore blobStore = new InMemoryBlobStore();
			await TestTreeAsync(blobStore, new BundleOptions { MaxBlobSize = 1024 * 1024 });

			Assert.AreEqual(1, blobStore.Blobs.Count);
			Assert.AreEqual(1, blobStore.Refs.Count);
		}

		[TestMethod]
		public async Task TestTreeSeparateBlobsAsync()
		{
			InMemoryBlobStore blobStore = new InMemoryBlobStore();
			await TestTreeAsync(blobStore, new BundleOptions { MaxBlobSize = 1 });

			Assert.AreEqual(5, blobStore.Blobs.Count);
			Assert.AreEqual(1, blobStore.Refs.Count);
		}

		static async Task TestTreeAsync(InMemoryBlobStore blobStore, BundleOptions options)
		{
			// Generate a tree
			using (BundleStore store = new BundleStore(blobStore, options))
			{
				ITreeWriter writer = store.CreateTreeWriter("test");

				ITreeBlobRef node1 = await writer.WriteNodeAsync(new ReadOnlySequence<byte>(new byte[] { 1 }), Array.Empty<ITreeBlobRef>(), CancellationToken.None);
				ITreeBlobRef node2 = await writer.WriteNodeAsync(new ReadOnlySequence<byte>(new byte[] { 2 }), new[] { node1 }, CancellationToken.None);
				ITreeBlobRef node3 = await writer.WriteNodeAsync(new ReadOnlySequence<byte>(new byte[] { 3 }), new[] { node2 }, CancellationToken.None);
				ITreeBlobRef node4 = await writer.WriteNodeAsync(new ReadOnlySequence<byte>(new byte[] { 4 }), Array.Empty<ITreeBlobRef>(), CancellationToken.None);

				ITreeBlob root = TreeBlob.Create(new ReadOnlySequence<byte>(new byte[] { 5 }), new[] { node4, node3 });
				await writer.WriteRefAsync(new RefName("test"), root.Data, root.Refs);

				await CheckTree(root);
			}

			// Check we can read it back in
			using (BundleStore collection = new BundleStore(blobStore, options))
			{
				ITreeBlob? root = await collection.TryReadTreeAsync(new RefName("test"));
				Assert.IsNotNull(root);
				await CheckTree(root!);
			}
		}

		static async Task CheckTree(ITreeBlob root)
		{
			ITreeBlob node5 = root;
			byte[] data5 = node5.Data.ToArray();
			Assert.IsTrue(data5.SequenceEqual(new byte[] { 5 }));
			IReadOnlyList<ITreeBlobRef> refs5 = node5.Refs;
			Assert.AreEqual(2, refs5.Count);

			ITreeBlob node4 = await refs5[0].GetTargetAsync();
			byte[] data4 = node4.Data.ToArray();
			Assert.IsTrue(data4.SequenceEqual(new byte[] { 4 }));
			IReadOnlyList<ITreeBlobRef> refs4 = node4.Refs;
			Assert.AreEqual(0, refs4.Count);

			ITreeBlob node3 = await refs5[1].GetTargetAsync();
			byte[] data3 = node3.Data.ToArray();
			Assert.IsTrue(data3.SequenceEqual(new byte[] { 3 }));
			IReadOnlyList<ITreeBlobRef> refs3 = node3.Refs;
			Assert.AreEqual(1, refs3.Count);

			ITreeBlob node2 = await refs3[0].GetTargetAsync();
			byte[] data2 = node2.Data.ToArray();
			Assert.IsTrue(data2.SequenceEqual(new byte[] { 2 }));
			IReadOnlyList<ITreeBlobRef> refs2 = node2.Refs;
			Assert.AreEqual(1, refs2.Count);

			ITreeBlob node1 = await refs2[0].GetTargetAsync();
			byte[] data1 = node1.Data.ToArray();
			Assert.IsTrue(data1.SequenceEqual(new byte[] { 1 }));
			IReadOnlyList<ITreeBlobRef> refs1 = node1.Refs;
			Assert.AreEqual(0, refs1.Count);
		}

		[TestMethod]
		public async Task DirectoryNodesAsync()
		{
			InMemoryBlobStore blobStore = new InMemoryBlobStore();

			// Generate a tree
			using (BundleStore store = new BundleStore(blobStore, new BundleOptions()))
			{
				DirectoryNode root = new DirectoryNode(DirectoryFlags.None);
				DirectoryNode hello = root.AddDirectory("hello");
				DirectoryNode world = hello.AddDirectory("world");
				await store.WriteTreeAsync(new RefName("test"), root, CancellationToken.None);

				await CheckDirectoryTreeAsync(root);
			}

			// Check we can read it back in
			using (BundleStore store = new BundleStore(blobStore, new BundleOptions()))
			{
				DirectoryNode root = await store.ReadTreeAsync<DirectoryNode>(new RefName("test"));
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
			InMemoryBlobStore blobStore = new InMemoryBlobStore();

			// Generate a tree
			using (BundleStore store = new BundleStore(blobStore, new BundleOptions()))
			{
				ITreeWriter writer = store.CreateTreeWriter();

				DirectoryNode root = new DirectoryNode(DirectoryFlags.None);
				DirectoryNode hello = root.AddDirectory("hello");

				FileEntry world = hello.AddFile("world", FileEntryFlags.None);
				await world.AppendAsync(Encoding.UTF8.GetBytes("world"), new ChunkingOptions(), writer, CancellationToken.None);

				await writer.WriteRefAsync(new RefName("test"), root);

				await CheckFileTreeAsync(root);
			}

			// Check we can read it back in
			using (BundleStore store = new BundleStore(blobStore, new BundleOptions()))
			{
				DirectoryNode root = await store.ReadTreeAsync<DirectoryNode>(new RefName("test"));
				await CheckFileTreeAsync(root);
			}
		}

		static async Task CheckFileTreeAsync(DirectoryNode root)
		{
			Assert.AreEqual(1, root.Directories.Count);
			Assert.AreEqual("hello", root.Directories.First().Name);

			DirectoryNode hello = await root.Directories.First().ExpandAsync(CancellationToken.None);
			Assert.AreEqual(0, hello.Directories.Count);
			Assert.AreEqual(1, hello.Files.Count);
			Assert.AreEqual("world", hello.Files.First().Name);

			FileNode world = await hello.Files.First().ExpandAsync(CancellationToken.None);

			byte[] worldData = await GetFileDataAsync(world);
			Assert.IsTrue(worldData.SequenceEqual(Encoding.UTF8.GetBytes("world")));
		}

		[TestMethod]
		public async Task ChunkingTests()
		{
			using BundleStore store = new BundleStore(new InMemoryBlobStore(), new BundleOptions());
			ITreeWriter writer = store.CreateTreeWriter();

			byte[] chunk = RandomNumberGenerator.GetBytes(256);

			byte[] data = new byte[chunk.Length * 1024];
			for (int idx = 0; idx * chunk.Length < data.Length; idx++)
			{
				chunk.CopyTo(data.AsSpan(idx * chunk.Length));
			}

			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions.MinSize = 1;
			options.LeafOptions.TargetSize = 64;
			options.LeafOptions.MaxSize = 1024;

			ReadOnlyMemory<byte> remaining = data;

			Dictionary<IoHash, long> uniqueChunks = new Dictionary<IoHash, long>();

			List<LeafFileNode> nodes = new List<LeafFileNode>();
			while (remaining.Length > 0)
			{
				LeafFileNode node = new LeafFileNode();
				remaining = await node.AppendDataAsync(remaining, options, writer, CancellationToken.None);
				nodes.Add(node);

				IoHash hash = IoHash.Compute(node.Data);
				uniqueChunks[hash] = node.Length;
			}

			long uniqueSize = uniqueChunks.Sum(x => x.Value);
			Assert.IsTrue(uniqueSize < data.Length / 3);
		}

		[TestMethod]
		public async Task ChildWriterAsync()
		{
			InMemoryBlobStore blobStore = new InMemoryBlobStore();

			using (BundleStore store = new BundleStore(blobStore, new BundleOptions()))
			{
				ITreeWriter writer = store.CreateTreeWriter();

				ITreeWriter childWriter1 = writer.CreateChildWriter();
				ITreeBlobRef childNode1 = await childWriter1.WriteNodeAsync(new ReadOnlySequence<byte>(new byte[] { 4, 5, 6 }), Array.Empty<ITreeBlobRef>());

				ITreeWriter childWriter2 = writer.CreateChildWriter();
				ITreeBlobRef childNode2 = await childWriter2.WriteNodeAsync(new ReadOnlySequence<byte>(new byte[] { 7, 8, 9 }), Array.Empty<ITreeBlobRef>());

				RefName refName = new RefName("test");
				await writer.WriteRefAsync(refName, new ReadOnlySequence<byte>(new byte[] { 1, 2, 3 }), new[] { childNode1, childNode2 });

				Assert.AreEqual(3, blobStore.Blobs.Count);
				Assert.AreEqual(1, blobStore.Refs.Count);

				ITreeBlob? rootBlob = await store.TryReadTreeAsync(refName);
				Assert.IsNotNull(rootBlob);
				Assert.IsTrue(rootBlob.Data.AsSingleSegment().Span.SequenceEqual(new byte[] { 1, 2, 3 }));
				Assert.AreEqual(2, rootBlob.Refs.Count);

				ITreeBlob? childBlob1 = await rootBlob.Refs[0].GetTargetAsync();
				Assert.IsTrue(childBlob1.Data.AsSingleSegment().Span.SequenceEqual(new byte[] { 4, 5, 6 }));
				Assert.AreEqual(0, childBlob1.Refs.Count);

				ITreeBlob? childBlob2 = await rootBlob.Refs[1].GetTargetAsync();
				Assert.IsTrue(childBlob2.Data.AsSingleSegment().Span.SequenceEqual(new byte[] { 7, 8, 9 }));
				Assert.AreEqual(0, childBlob2.Refs.Count);
			}
		}

		[TestMethod]
		public async Task LargeFileTestAsync()
		{
			InMemoryBlobStore blobStore = new InMemoryBlobStore();

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
			using (BundleStore store = new BundleStore(blobStore, new BundleOptions { MaxBlobSize = 1024 }))
			{
				ITreeWriter writer = store.CreateTreeWriter();

				DirectoryNode root = new DirectoryNode(DirectoryFlags.None);

				ChunkingOptions options = new ChunkingOptions();
				options.LeafOptions.MinSize = 1;
				options.LeafOptions.TargetSize = 128;
				options.LeafOptions.MaxSize = 64 * 1024;

				FileEntry file = root.AddFile("test", FileEntryFlags.None);
				await file.AppendAsync(data, options, writer, CancellationToken.None);

				await writer.WriteRefAsync(new RefName("test"), root);

				await CheckLargeFileTreeAsync(root, data);
			}

			// Check we can read it back in
			using (BundleStore store = new BundleStore(blobStore, new BundleOptions()))
			{
				DirectoryNode root = await store.ReadTreeAsync<DirectoryNode>(new RefName("test"));
				await CheckLargeFileTreeAsync(root, data);

				TreeNodeRef<FileNode> file = root.GetFileEntry("test");

				Dictionary<IoHash, long> uniqueBlobs = new Dictionary<IoHash, long>();
				await FindUniqueBlobs(file.Target!, uniqueBlobs);

				long uniqueSize = uniqueBlobs.Sum(x => x.Value);
				Assert.IsTrue(uniqueSize < data.Length / 3); // random fraction meaning "lots of dedupe happened"
			}
		}

		static async Task CheckLargeFileTreeAsync(DirectoryNode root, byte[] data)
		{
			Assert.AreEqual(0, root.Directories.Count);
			Assert.AreEqual(1, root.Files.Count);

			FileNode world = await root.Files.First().ExpandAsync(CancellationToken.None);

			byte[] worldData = await GetFileDataAsync(world);
			Assert.AreEqual(data.Length, worldData.Length);
			Assert.IsTrue(worldData.SequenceEqual(data));
		}

		static async Task FindUniqueBlobs(ITreeBlobRef blobRef, Dictionary<IoHash, long> uniqueBlobs)
		{
			ITreeBlob blob = await blobRef.GetTargetAsync();

			ReadOnlySequence<byte> data = blob.Data;
			uniqueBlobs[blobRef.Hash] = data.Length;

			IReadOnlyList<ITreeBlobRef> references = blob.Refs;
			foreach (ITreeBlobRef reference in references)
			{
				await FindUniqueBlobs(reference, uniqueBlobs);
			}
		}

		static async Task<byte[]> GetFileDataAsync(FileNode fileNode)
		{
			using (MemoryStream stream = new MemoryStream())
			{
				await fileNode.CopyToStreamAsync(stream, CancellationToken.None);
				return stream.ToArray();
			}
		}
	}
}

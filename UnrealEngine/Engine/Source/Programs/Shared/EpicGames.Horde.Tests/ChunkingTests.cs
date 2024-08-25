// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class ChunkingTests
	{
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
		public async Task EmptyNodeTestAsync()
		{
			using KeyValueStorageClient store = KeyValueStorageClient.CreateInMemory();

			const string RefName = "hello";
			await using (IBlobWriter writer = store.CreateBlobWriter(RefName))
			{
				ChunkingOptions options = new ChunkingOptions();
				options.LeafOptions = new LeafChunkedDataNodeOptions(64, 64, 64);
				options.InteriorOptions = new InteriorChunkedDataNodeOptions(4, 4, 4);

				using MemoryStream emptyStream = new MemoryStream();
				LeafChunkedData leafChunkedData = await LeafChunkedDataNode.CreateFromStreamAsync(writer, emptyStream, new LeafChunkedDataNodeOptions(64, 64, 64), CancellationToken.None);
				ChunkedData chunkedData = await InteriorChunkedDataNode.CreateTreeAsync(leafChunkedData, new InteriorChunkedDataNodeOptions(4, 4, 4), writer, CancellationToken.None);

				DirectoryNode directory = new DirectoryNode();
				directory.AddFile("test.foo", FileEntryFlags.None, 0, chunkedData);

				IBlobRef handle = await writer.WriteBlobAsync(directory);
				await store.WriteRefAsync(RefName, handle);
			}
		}

		[TestMethod]
		public async Task FixedSizeChunkingTestsAsync()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new LeafChunkedDataNodeOptions(64, 64, 64);
			options.InteriorOptions = new InteriorChunkedDataNodeOptions(4, 4, 4);

			await TestChunkingAsync(options);
		}

		[TestMethod]
		public async Task VariableSizeChunkingTestsAsync()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new LeafChunkedDataNodeOptions(32, 64, 96);
			options.InteriorOptions = new InteriorChunkedDataNodeOptions(1, 4, 12);

			await TestChunkingAsync(options);
		}

		static async Task TestChunkingAsync(ChunkingOptions options)
		{
			using MemoryCache cache = new MemoryCache(new MemoryCacheOptions());

			using KeyValueStorageClient store = KeyValueStorageClient.CreateInMemory();

			await using IBlobWriter writer = store.CreateBlobWriter();

			byte[] data = new byte[4096];
			new Random(0).NextBytes(data);

			for (int idx = 0; idx < data.Length; idx++)
			{
				data[idx] = (byte)idx;
			}

			ChunkedDataNodeRef handle;

			const int NumIterations = 100;
			{
				using ChunkedDataWriter fileWriter = new ChunkedDataWriter(writer, options);

				for (int idx = 0; idx < NumIterations; idx++)
				{
					await fileWriter.AppendAsync(data, CancellationToken.None);
				}

				handle = (await fileWriter.FlushAsync(CancellationToken.None)).Root;
			}

			ChunkedDataNode root = await handle.ReadBlobAsync();

			byte[] result;
			using (MemoryStream stream = new MemoryStream())
			{
				await root.CopyToStreamAsync(stream);
				result = stream.ToArray();
			}

			Assert.AreEqual(NumIterations * data.Length, result.Length);

			for (int idx = 0; idx < NumIterations; idx++)
			{
				ReadOnlyMemory<byte> spanData = result.AsMemory(idx * data.Length, data.Length);
				Assert.IsTrue(spanData.Span.SequenceEqual(data));
			}

			await CheckSizesAsync(root, options, true);
		}

		static async Task CheckSizesAsync(ChunkedDataNode node, ChunkingOptions options, bool rightmost)
		{
			if (node is LeafChunkedDataNode leafNode)
			{
				Assert.IsTrue(rightmost || leafNode.Data.Length >= options.LeafOptions.MinSize);
				Assert.IsTrue(leafNode.Data.Length <= options.LeafOptions.MaxSize);
			}
			else
			{
				InteriorChunkedDataNode interiorNode = (InteriorChunkedDataNode)node;

				Assert.IsTrue(rightmost || interiorNode.Children.Count >= options.InteriorOptions.MinChildCount);
				Assert.IsTrue(interiorNode.Children.Count <= options.InteriorOptions.MaxChildCount);

				int childCount = interiorNode.Children.Count;
				for (int idx = 0; idx < childCount; idx++)
				{
					ChunkedDataNode childNode = await interiorNode.Children[idx].ReadBlobAsync();
					await CheckSizesAsync(childNode, options, idx == childCount - 1);
				}
			}
		}

		[TestMethod]
		public async Task ChunkingCompatV2Async()
		{
			ChunkingOptions chunkingOptions = new ChunkingOptions();
			chunkingOptions.LeafOptions = new LeafChunkedDataNodeOptions(2, 2, 2);
			chunkingOptions.InteriorOptions = new InteriorChunkedDataNodeOptions(2, 2, 2);

			BlobSerializerOptions serializerOptions = new BlobSerializerOptions();
			serializerOptions.Converters.Add(new InteriorChunkedDataNodeConverter(2)); // Does not include length fields in interior nodes

			using KeyValueStorageClient store = KeyValueStorageClient.CreateInMemory();

			byte[] data = Encoding.UTF8.GetBytes("hello world");

			IBlobRef<DirectoryNode> handle;
			await using (IBlobWriter writer = store.CreateBlobWriter(options: serializerOptions))
			{
				using ChunkedDataWriter chunkedWriter = new ChunkedDataWriter(writer, chunkingOptions);
				await chunkedWriter.AppendAsync(data, CancellationToken.None);
				ChunkedData chunkedData = await chunkedWriter.FlushAsync(CancellationToken.None);

				DirectoryNode directoryNode = new DirectoryNode();
				directoryNode.AddFile("test", FileEntryFlags.None, data.Length, chunkedData);
				handle = await writer.WriteBlobAsync(directoryNode);
			}

			DirectoryInfo tempDir = new DirectoryInfo(Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString()));
			try
			{
				DirectoryNode expandedNode = await handle.ReadBlobAsync();
				await expandedNode.CopyToDirectoryAsync(tempDir, null, NullLogger.Instance, CancellationToken.None);

				byte[] outputData = await File.ReadAllBytesAsync(Path.Combine(tempDir.FullName, "test"));
				Assert.IsTrue(outputData.SequenceEqual(data));
			}
			finally
			{
				tempDir.Delete(true);
			}
		}

		[TestMethod]
		public async Task ChunkOrderAsync()
		{
			InteriorChunkedDataNodeOptions interiorOptions = new InteriorChunkedDataNodeOptions(2, 2, 2);

			BlobSerializerOptions serializerOptions = new BlobSerializerOptions();
			serializerOptions.Converters.Add(new InteriorChunkedDataNodeConverter());

			await using MemoryBlobWriter blobWriter = new MemoryBlobWriter(new BlobSerializerOptions());

			IBlobRef<LeafChunkedDataNode> leafRef = await blobWriter.WriteBlobAsync(new LeafChunkedDataNode(new byte[] { 1, 2, 3 }));
			int leafRefIndex = MemoryBlobWriter.GetIndex((IBlobRef)leafRef);
			ChunkedDataNodeRef leafChunkedRef = new ChunkedDataNodeRef(3, leafRef);

			List<ChunkedDataNodeRef> leafNodeRefs = Enumerable.Repeat(leafChunkedRef, 10000).ToList();
			ChunkedDataNodeRef root = await InteriorChunkedDataNode.CreateTreeAsync(leafNodeRefs, interiorOptions, blobWriter, CancellationToken.None);

			List<IBlobHandle> list = new List<IBlobHandle>();
			await GetReadOrderAsync(root.Handle, list);

			int prevIndex = Int32.MaxValue;
			for (int idx = 0; idx < list.Count; idx++)
			{
				int index = MemoryBlobWriter.GetIndex((IBlobRef)list[idx].Innermost);
				if (index != leafRefIndex)
				{
					Console.WriteLine("{0}", index);
					Assert.IsTrue(index <= prevIndex);
					prevIndex = index;
				}
			}
		}

		static async Task GetReadOrderAsync(IBlobHandle handle, List<IBlobHandle> list)
		{
			list.Add(handle);

			using BlobData blobData = await handle.ReadBlobDataAsync(CancellationToken.None);
			foreach (IBlobHandle childHandle in blobData.Imports)
			{
				await GetReadOrderAsync(childHandle, list);
			}
		}
	}
}

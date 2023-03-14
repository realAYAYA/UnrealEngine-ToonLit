// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Linq;
using System.Threading.Tasks;
using System.IO;
using System.Threading;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;

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
		public async Task BasicChunkingTests()
		{
			using BundleStore store = new BundleStore(new InMemoryBlobStore(), new BundleOptions());
			ITreeWriter writer = store.CreateTreeWriter("test");

			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new ChunkingOptionsForNodeType(8, 8, 8);

			FileNode node = new LeafFileNode();
			await node.AppendAsync(new byte[7], options, writer, CancellationToken.None);
			Assert.AreEqual(7, node.Length);
			Assert.AreEqual(0, node.GetReferences().Count);
			Assert.AreEqual(7, (await node.ToByteArrayAsync(CancellationToken.None)).Length);

			node = new LeafFileNode();
			await node.AppendAsync(new byte[8], options, writer, CancellationToken.None);
			Assert.AreEqual(8, node.Length);
			Assert.AreEqual(0, node.GetReferences().Count);
			Assert.AreEqual(8, (await node.ToByteArrayAsync(CancellationToken.None)).Length);

			node = new LeafFileNode();
			node = await node.AppendAsync(new byte[9], options, writer, CancellationToken.None);
			Assert.AreEqual(9, node.Length);
			Assert.AreEqual(2, node.GetReferences().Count);

			FileNode? childNode1 = await ((TreeNodeRef<FileNode>)node.GetReferences()[0]).ExpandAsync();
			Assert.IsNotNull(childNode1);
			Assert.AreEqual(0, childNode1!.GetReferences().Count);
			Assert.AreEqual(8, (await childNode1!.ToByteArrayAsync(CancellationToken.None)).Length);

			FileNode? childNode2 = await ((TreeNodeRef<FileNode>)node.GetReferences()[1]).ExpandAsync();
			Assert.IsNotNull(childNode2);
			Assert.AreEqual(0, childNode2!.GetReferences().Count);
			Assert.AreEqual(1, (await childNode2!.ToByteArrayAsync(CancellationToken.None)).Length);
		}

		[TestMethod]
		public async Task FixedSizeChunkingTests()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new ChunkingOptionsForNodeType(64, 64, 64);
			options.InteriorOptions = new ChunkingOptionsForNodeType(IoHash.NumBytes * 4, IoHash.NumBytes * 4, IoHash.NumBytes * 4);

			await TestChunkingAsync(options);
		}

		[TestMethod]
		public async Task VariableSizeChunkingTests()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new ChunkingOptionsForNodeType(32, 64, 96);
			options.InteriorOptions = new ChunkingOptionsForNodeType(IoHash.NumBytes * 1, IoHash.NumBytes * 4, IoHash.NumBytes * 12);

			await TestChunkingAsync(options);
		}

		static async Task TestChunkingAsync(ChunkingOptions options)
		{
			using BundleStore store = new BundleStore(new InMemoryBlobStore(), new BundleOptions());
			ITreeWriter writer = store.CreateTreeWriter();

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

			byte[] result;
			using (MemoryStream stream = new MemoryStream())
			{
				await root.CopyToStreamAsync(stream, CancellationToken.None);
				result = stream.ToArray();
			}

			Assert.AreEqual(NumIterations * data.Length, result.Length);

			for (int idx = 0; idx < NumIterations; idx++)
			{
				ReadOnlyMemory<byte> spanData = result.AsMemory(idx * data.Length, data.Length);
				Assert.IsTrue(spanData.Span.SequenceEqual(data));
			}
		}
	}
}

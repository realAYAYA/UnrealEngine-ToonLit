// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Storage.Impl;

namespace EpicGames.Horde.Tests
{
	/// <summary>
	/// Base class for storage client tests.
	/// Allows for contract testing the implementations.
	/// </summary>
	[TestClass]
	public abstract class BaseStorageClientTest
	{
		private readonly NamespaceId _ns = new("my-ns");
		private readonly ReadOnlyMemory<byte> _uncompressedData = new byte[] { 0x41, 0x42, 0x43 }; // "ABC"
		private readonly ReadOnlyMemory<byte> _compressedData;
		private readonly IoHash _uncompressedHash;
		private readonly IoHash _compressedHash;
		protected FakeCompressor Compressor { get; } = new FakeCompressor();

		protected BaseStorageClientTest()
		{
			_compressedData = Compressor.CompressToMemoryAsync(_uncompressedData.ToArray()).Result;
			_uncompressedHash = IoHash.Compute(_uncompressedData.ToArray());
			_compressedHash = IoHash.Compute(_compressedData.ToArray());
		}

		protected abstract IStorageClient GetStorageClient();

		[TestMethod]
		public async Task WriteBlob()
		{
			IStorageClient client = GetStorageClient();
			IoHash writtenHash = await client.WriteBlobFromMemoryAsync(_ns, _uncompressedData);
			byte[] actual = await client.ReadBlobToMemoryAsync(_ns, writtenHash);
			CollectionAssert.AreEqual(_uncompressedData.ToArray(), actual);
		}
		
		[TestMethod]
		public async Task WriteBlobWithPreCalculatedHash()
		{
			IStorageClient client = GetStorageClient();
			await client.WriteBlobFromMemoryAsync(_ns, _uncompressedHash, _uncompressedData);
			byte[] actual = await client.ReadBlobToMemoryAsync(_ns, _uncompressedHash);
			CollectionAssert.AreEqual(_uncompressedData.ToArray(), actual);
		}
		
		[TestMethod]
		public async Task WriteAndReadCompressedBlob()
		{
			IStorageClient client = GetStorageClient();
			using MemoryStream compressedStream = new(_compressedData.ToArray());
			IoHash returnedHash = await client.WriteCompressedBlobAsync(_ns, compressedStream);
			Assert.AreEqual(_uncompressedHash, returnedHash);
			byte[] returnedData = await client.ReadCompressedBlobToMemoryAsync(_ns, returnedHash);
			CollectionAssert.AreNotEqual(_uncompressedData.ToArray(), returnedData);
		}
		
		[TestMethod]
		public async Task WriteAndReadCompressedBlobWithPreCalculatedHash()
		{
			IStorageClient client = GetStorageClient();
			using MemoryStream compressedStream = new(_compressedData.ToArray());
			await client.WriteCompressedBlobAsync(_ns, _uncompressedHash, compressedStream);
			byte[] returnedData = await client.ReadCompressedBlobToMemoryAsync(_ns, _uncompressedHash);
			CollectionAssert.AreNotEqual(_uncompressedData.ToArray(), returnedData);
		}
		
		[TestMethod]
		public async Task WriteWithBadPreCalculatedHash()
		{
			IStorageClient client = GetStorageClient();
			IoHash badHash = IoHash.Compute(new byte[] {0xBA, 0xDB, 0xAD});
			using MemoryStream compressedStream = new(_compressedData.ToArray());
			await Assert.ThrowsExceptionAsync<ArgumentException>(() => client.WriteCompressedBlobAsync(_ns, badHash, compressedStream));
		}
		
		[TestMethod]
		public async Task WriteWithInvalidCompression()
		{
			IStorageClient client = GetStorageClient();
			using MemoryStream compressedStream = new(new byte[] {0xBA, 0xDB, 0xAD});
			await Assert.ThrowsExceptionAsync<ArgumentException>(() => client.WriteCompressedBlobAsync(_ns, compressedStream));
		}
	}
	
	[TestClass]
	public class MemoryStorageClientTest : BaseStorageClientTest
	{
		protected override IStorageClient GetStorageClient()
		{
			return new MemoryStorageClient();
		}
	}
	
	[TestClass]
	public class FakeCompressorTest
	{
		[TestMethod]
		public async Task CompressAndDecompress()
		{
			FakeCompressor compressor = new ();
			byte[] uncompressedData = { 0x41, 0x42, 0x43 };
			using MemoryStream input = new(uncompressedData);
			using MemoryStream output = new();
			await compressor.CompressAsync(input, output);
			byte[] compressedData = output.ToArray();
			Assert.IsFalse(compressedData.SequenceEqual(uncompressedData));
			
			using MemoryStream input2 = new(compressedData);
			using MemoryStream output2 = new();
			await compressor.DecompressAsync(input2, output2);
			byte[] decompressedData = output2.ToArray();
			Assert.IsTrue(decompressedData.SequenceEqual(uncompressedData));
		}
	}
}

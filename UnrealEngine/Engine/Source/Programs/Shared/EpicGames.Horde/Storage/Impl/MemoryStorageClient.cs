// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage.Impl
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which writes directly to memory for testing. Not intended for production use.
	/// </summary>
	public class MemoryStorageClient : IStorageClient
	{
		class Ref : IRef
		{
			public NamespaceId NamespaceId { get; set; }
			public BucketId BucketId { get; set; }
			public RefId RefId { get; set; }
			public CbObject Value { get; set; }

			public Ref(NamespaceId namespaceId, BucketId bucketId, RefId refId, CbObject value)
			{
				NamespaceId = namespaceId;
				BucketId = bucketId;
				RefId = refId;
				Value = value;
			}
		}

		/// <inheritdoc/>
		public Dictionary<(NamespaceId, IoHash), ReadOnlyMemory<byte>> Blobs { get; } = new Dictionary<(NamespaceId, IoHash), ReadOnlyMemory<byte>>();
		
		/// <summary>
		/// Mapping from uncompressed hash to compressed hash.
		/// </summary>
		public Dictionary<(NamespaceId, IoHash), IoHash> UncompressedToCompressedHash { get; } = new Dictionary<(NamespaceId, IoHash), IoHash>();

		/// <inheritdoc/>
		public Dictionary<(NamespaceId, BucketId, RefId), IRef> Refs { get; } = new Dictionary<(NamespaceId, BucketId, RefId), IRef>();

		private readonly FakeCompressor _compressor = new FakeCompressor();

		/// <inheritdoc/>
		public Task<Stream> ReadBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default)
		{
			ReadOnlyMemory<byte> data;
			if(!Blobs.TryGetValue((namespaceId, hash), out data))
			{
				throw new BlobNotFoundException(namespaceId, hash);
			}
			return Task.FromResult<Stream>(new ReadOnlyMemoryStream(data));
		}
		
		/// <inheritdoc/>
		public Task<Stream> ReadCompressedBlobAsync(NamespaceId namespaceId, IoHash uncompressedHash, CancellationToken cancellationToken = default)
		{
			IoHash compressedHash = UncompressedToCompressedHash[(namespaceId, uncompressedHash)];
			ReadOnlyMemory<byte> data = Blobs[(namespaceId, compressedHash)];
			return Task.FromResult<Stream>(new ReadOnlyMemoryStream(data));
		}

		/// <inheritdoc/>
		public async Task WriteBlobAsync(NamespaceId namespaceId, IoHash hash, Stream stream, CancellationToken cancellationToken = default)
		{
			using (MemoryStream memoryStream = new MemoryStream())
			{
				await stream.CopyToAsync(memoryStream, cancellationToken);
				Blobs[(namespaceId, hash)] = memoryStream.ToArray();
			}
		}

		/// <inheritdoc/>
		public async Task<IoHash> WriteBlobAsync(NamespaceId namespaceId, Stream stream, CancellationToken cancellationToken = default)
		{
			using (MemoryStream memoryStream = new MemoryStream())
			{
				await stream.CopyToAsync(memoryStream, cancellationToken);

				byte[] data = memoryStream.ToArray();
				IoHash hash = IoHash.Compute(data);

				Blobs[(namespaceId, hash)] = data;
				return hash;
			}
		}

		/// <inheritdoc/>
		public async Task WriteCompressedBlobAsync(NamespaceId namespaceId, IoHash uncompressedHash, Stream compressedStream, CancellationToken cancellationToken = default)
		{
			using MemoryStream compressedStreamMs = new ();
			await compressedStream.CopyToAsync(compressedStreamMs, cancellationToken);
			byte[] compressedData = compressedStreamMs.ToArray();
			IoHash compressedHash = IoHash.Compute(compressedData);

			byte[] decompressedData = await _compressor.DecompressToMemoryAsync(compressedData, cancellationToken);
			IoHash computedDecompressedHash = IoHash.Compute(decompressedData);
			if (computedDecompressedHash != uncompressedHash)
			{
				throw new ArgumentException("Supplied uncompressed hash does not match hash of uncompressed data");
			}

			Blobs[(namespaceId, compressedHash)] = compressedData;
			UncompressedToCompressedHash[(namespaceId, uncompressedHash)] = compressedHash;
		}

		/// <inheritdoc/>
		public async Task<IoHash> WriteCompressedBlobAsync(NamespaceId namespaceId, Stream compressedStream, CancellationToken cancellationToken = default)
		{
			using MemoryStream compressedStreamMs = new();
			await compressedStream.CopyToAsync(compressedStreamMs, cancellationToken);
			byte[] decompressedData = await _compressor.DecompressToMemoryAsync(compressedStreamMs.ToArray(), cancellationToken);
			IoHash decompressedHash = IoHash.Compute(decompressedData);
			compressedStreamMs.Seek(0, SeekOrigin.Begin);

			await WriteCompressedBlobAsync(namespaceId, decompressedHash, compressedStreamMs, cancellationToken);
			return decompressedHash;
		}

		/// <inheritdoc/>
		public Task<bool> HasBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default)
		{
			return Task.FromResult(Blobs.ContainsKey((namespaceId, hash)));
		}

		/// <inheritdoc/>
		public Task<HashSet<IoHash>> FindMissingBlobsAsync(NamespaceId namespaceId, HashSet<IoHash> hashes, CancellationToken cancellationToken)
		{
			return Task.FromResult(hashes.Where(x => !Blobs.ContainsKey((namespaceId, x))).ToHashSet());
		}

		/// <inheritdoc/>
		public Task<bool> DeleteRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default)
		{
			return Task.FromResult(Refs.Remove((namespaceId, bucketId, refId)));
		}

		/// <inheritdoc/>
		public Task<List<RefId>> FindMissingRefsAsync(NamespaceId namespaceId, BucketId bucketId, List<RefId> refIds, CancellationToken cancellationToken = default)
		{
			return Task.FromResult(refIds.Where(x => !Refs.ContainsKey((namespaceId, bucketId, x))).ToList());
		}

		/// <inheritdoc/>
		public Task<IRef> GetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default)
		{
			IRef? result;
			if(!Refs.TryGetValue((namespaceId, bucketId, refId), out result))
			{
				throw new RefNotFoundException(namespaceId, bucketId, refId);
			}
			return Task.FromResult(result);
		}

		/// <inheritdoc/>
		public Task<bool> HasRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default)
		{
			return Task.FromResult(Refs.ContainsKey((namespaceId, bucketId, refId)));
		}

		/// <inheritdoc/>
		public Task<List<IoHash>> TryFinalizeRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, IoHash hash, CancellationToken cancellationToken = default)
		{
			throw new NotImplementedException();
		}

		/// <inheritdoc/>
		public Task<List<IoHash>> TrySetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CbObject value, CancellationToken cancellationToken = default)
		{
			Refs[(namespaceId, bucketId, refId)] = new Ref(namespaceId, bucketId, refId, value);
			return Task.FromResult(new List<IoHash>());
		}
	}
	
	/// <summary>
	/// Fake compressor used for testing. Simply prepends the data with const prefix.
	/// Allows for easy debugging and is enough to cause a changed hash.
	/// </summary>
	public class FakeCompressor
	{
		private readonly byte[] _prefix = Encoding.UTF8.GetBytes("FAKECOMP-");
		
		/// <summary>
		/// Compress data
		/// </summary>
		/// <param name="input">Data to be compressed</param>
		/// <param name="output">Compressed data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task CompressAsync(Stream input, Stream output, CancellationToken cancellationToken = default)
		{
			await output.WriteAsync(_prefix, cancellationToken);
			await input.CopyToAsync(output, cancellationToken);
		}

		/// <summary>
		/// Decompress data
		/// </summary>
		/// <param name="input">Data to be decompressed</param>
		/// <param name="output">Decompressed data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task DecompressAsync(Stream input, Stream output, CancellationToken cancellationToken = default)
		{
			using MemoryStream ms = new();
			using BinaryReader reader = new(input);
			byte[] prefixRead = reader.ReadBytes(_prefix.Length);
			if (!_prefix.SequenceEqual(prefixRead))
			{
				throw new ArgumentException($"Input stream is not compressed. Missing fake prefix!");
			}
			await input.CopyToAsync(output, cancellationToken);
		}
		
		/// <summary>
		/// Compress data to memory
		/// </summary>
		/// <param name="input">Data to be compressed</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Compressed data</returns>
		public async Task<byte[]> CompressToMemoryAsync(byte[] input, CancellationToken cancellationToken = default)
		{
			using MemoryStream inputMs = new (input);
			using MemoryStream outputMs = new ();
			await CompressAsync(inputMs, outputMs, cancellationToken);
			return outputMs.ToArray();
		}
		
		/// <summary>
		/// Decompress data to memory
		/// </summary>
		/// <param name="input">Data to be decompressed</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Decompressed data</returns>
		public async Task<byte[]> DecompressToMemoryAsync(byte[] input, CancellationToken cancellationToken = default)
		{
			using MemoryStream inputMs = new (input);
			using MemoryStream outputMs = new ();
			await DecompressAsync(inputMs, outputMs, cancellationToken);
			return outputMs.ToArray();
		}
		
		/// <summary>
		/// Compress data to memory
		/// </summary>
		/// <param name="input">Data to be compressed</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Compressed data</returns>
		public async Task<string> CompressToMemoryAsync(string input, CancellationToken cancellationToken = default)
		{
			byte[] output = await CompressToMemoryAsync(Encoding.UTF8.GetBytes(input), cancellationToken);
			return Encoding.UTF8.GetString(output);
		}
		
		/// <summary>
		/// Decompress data to memory
		/// </summary>
		/// <param name="input">Data to be decompressed</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Decompressed data</returns>
		public async Task<string> DecompressToMemoryAsync(string input, CancellationToken cancellationToken = default)
		{
			byte[] output = await DecompressToMemoryAsync(Encoding.UTF8.GetBytes(input), cancellationToken);
			return Encoding.UTF8.GetString(output);
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Backends;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Base class for storage clients that wrap a diirect key/value type store without any merging/splitting.
	/// </summary>
	public sealed class KeyValueStorageClient : IStorageClient
	{
		class Handle : IBlobHandle
		{
			readonly KeyValueStorageClient _keyValueStorageClient;
			readonly BlobLocator _locator;

			/// <inheritdoc/>
			public IBlobHandle Innermost => this;

			/// <summary>
			/// Constructor
			/// </summary>
			public Handle(KeyValueStorageClient keyValueStorageClient, BlobLocator locator)
			{
				_keyValueStorageClient = keyValueStorageClient;
				_locator = locator;
			}

			/// <inheritdoc/>
			public ValueTask FlushAsync(CancellationToken cancellationToken = default) => default;

			/// <inheritdoc/>
			public ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default) => _keyValueStorageClient.ReadBlobAsync(_locator, cancellationToken);

			/// <inheritdoc/>
			public bool TryGetLocator(out BlobLocator locator)
			{
				locator = _locator;
				return true;
			}

			/// <inheritdoc/>
			public override bool Equals(object? obj) => obj is Handle other && _locator == other._locator;

			/// <inheritdoc/>
			public override int GetHashCode() => _locator.GetHashCode();
		}

		class Writer : BlobWriter
		{
			readonly KeyValueStorageClient _outer;
			readonly string _basePath;
			byte[] _data = Array.Empty<byte>();
			int _offset;

			/// <summary>
			/// Constructor
			/// </summary>
			public Writer(KeyValueStorageClient outer, string? basePath, BlobSerializerOptions? options)
				: base(options)
			{
				_outer = outer;
				_basePath = basePath ?? String.Empty;

				if (!_basePath.EndsWith("/", StringComparison.Ordinal))
				{
					_basePath += "/";
				}
			}

			/// <inheritdoc/>
			public override ValueTask DisposeAsync() => new ValueTask();

			/// <inheritdoc/>
			public override Task FlushAsync(CancellationToken cancellationToken = default) => Task.CompletedTask;

			/// <inheritdoc/>
			public override IBlobWriter Fork() => new Writer(_outer, _basePath, Options);

			/// <inheritdoc/>
			public override Memory<byte> GetOutputBuffer(int usedSize, int desiredSize)
			{
				if (_offset + desiredSize > _data.Length)
				{
					byte[] newData = new byte[(desiredSize + 4095) & ~4095];
					_data.AsSpan(_offset, usedSize).CopyTo(newData);
					_data = newData;
					_offset = 0;
				}
				return _data.AsMemory(_offset);
			}

			/// <inheritdoc/>
			public override async ValueTask<IBlobRef> WriteBlobAsync(BlobType type, int size, IReadOnlyList<IBlobHandle> imports, IReadOnlyList<AliasInfo> aliases, CancellationToken cancellationToken = default)
			{
				ReadOnlyMemory<byte> data = _data.AsMemory(_offset, size);
				_offset += size;

				using ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(data);

				IBlobRef handle = await _outer.WriteBlobAsync(type, stream, imports, _basePath, cancellationToken);
				foreach (AliasInfo aliasInfo in aliases)
				{
					await _outer.AddAliasAsync(aliasInfo.Name, handle, aliasInfo.Rank, aliasInfo.Data, cancellationToken);
				}
				return handle;
			}
		}

		/// <inheritdoc/>
		public bool SupportsRedirects => _backend.SupportsRedirects;

		readonly IStorageBackend _backend;

		/// <summary>
		/// Constructor
		/// </summary>
		public KeyValueStorageClient(IStorageBackend backend)
		{
			_backend = backend;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <summary>
		/// Create an in-memory storage client
		/// </summary>
		public static KeyValueStorageClient CreateInMemory() => new KeyValueStorageClient(new MemoryStorageBackend());

		#region Blobs

		/// <inheritdoc/>
		public IBlobHandle CreateBlobHandle(BlobLocator locator)
		{
			return new Handle(this, locator);
		}

		/// <inheritdoc/>
		public IBlobRef CreateBlobRef(IoHash hash, BlobLocator locator)
		{
			return BlobRef.Create(hash, CreateBlobHandle(locator));
		}

		/// <inheritdoc/>
		public IBlobRef<T> CreateBlobRef<T>(IoHash hash, BlobLocator locator, BlobSerializerOptions? options)
		{
			return BlobRef.Create<T>(hash, CreateBlobHandle(locator), options);
		}

		/// <inheritdoc/>
		public IBlobWriter CreateBlobWriter(string? basePath = null, BlobSerializerOptions? options = null) => new Writer(this, basePath, options);

		/// <inheritdoc/>
		public async ValueTask<BlobData> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			IReadOnlyMemoryOwner<byte> owner = await _backend.ReadBlobAsync(locator, cancellationToken);
			EncodedBlobData encodedData = new EncodedBlobData(owner.Memory.ToArray());

			return new BlobDataWithOwner(encodedData.Type, encodedData.Payload, encodedData.Imports.Select(x => CreateBlobHandle(x)).ToArray(), owner);
		}

		/// <inheritdoc/>
		public async ValueTask<IBlobRef> WriteBlobAsync(BlobType type, Stream stream, IReadOnlyList<IBlobHandle> imports, string? basePath = null, CancellationToken cancellationToken = default)
		{
			BlobLocator[] locators = imports.ConvertAll(x => x.GetLocator()).ToArray();
			byte[] payload = await stream.ReadAllBytesAsync(cancellationToken);
			byte[] encodedData = EncodedBlobData.Create(type, locators, payload);

			using ReadOnlyMemoryStream encodedStream = new ReadOnlyMemoryStream(encodedData);
			BlobLocator locator = await _backend.WriteBlobAsync(encodedStream, basePath, cancellationToken);

			IoHash hash = IoHash.Compute(payload);
			return BlobRef.Create(hash, CreateBlobHandle(locator));
		}

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public async Task AddAliasAsync(string name, IBlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			await handle.FlushAsync(cancellationToken);
			await _backend.AddAliasAsync(name, handle.GetLocator(), rank, data, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task RemoveAliasAsync(string name, IBlobHandle handle, CancellationToken cancellationToken = default)
		{
			await handle.FlushAsync(cancellationToken);
			await _backend.RemoveAliasAsync(name, handle.GetLocator(), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<BlobAlias[]> FindAliasesAsync(string name, int? maxResults = null, CancellationToken cancellationToken = default)
		{
			BlobAliasLocator[] locators = await _backend.FindAliasesAsync(name, maxResults, cancellationToken);
			return locators.Select(x => new BlobAlias(CreateBlobHandle(x.Target), x.Rank, x.Data)).ToArray();
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public async Task<IBlobRef?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobRefValue? value = await _backend.TryReadRefAsync(name, cacheTime, cancellationToken);
			if (value == null)
			{
				return null;
			}
			return CreateBlobRef(value.Hash, value.Locator);
		}

		/// <inheritdoc/>
		public async Task WriteRefAsync(RefName name, IBlobRef value, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			await value.FlushAsync(cancellationToken);
			await _backend.WriteRefAsync(name, new BlobRefValue(value.Hash, value.GetLocator()), options, cancellationToken);
		}

		/// <inheritdoc/>
		public Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
			=> _backend.DeleteRefAsync(name, cancellationToken);

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
			=> _backend.GetStats(stats);
	}
}

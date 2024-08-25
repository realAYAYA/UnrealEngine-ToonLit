// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Bundles.V2
{
	/// <summary>
	/// Implements the primary storage writer interface for V2 bundles. Writes exports into packets, and flushes them to storage in bundles.
	/// </summary>
	public sealed class BundleWriter : BlobWriter
	{
		// Packet that is still being built, but may be redirected to a flushed packet
		internal sealed class PendingPacketHandle : PacketHandle
		{
			readonly PendingBundleHandle _bundle;
			FlushedPacketHandle? _flushedHandle;

			public override BundleHandle Bundle => _flushedHandle?.Bundle ?? _bundle;
			public FlushedPacketHandle? FlushedHandle => _flushedHandle;

			public PendingPacketHandle(PendingBundleHandle bundle) => _bundle = bundle;

			public override ValueTask FlushAsync(CancellationToken cancellationToken = default)
				=> _bundle.FlushAsync(cancellationToken);

			public void CompletePacket(FlushedPacketHandle flushedHandle)
				=> _flushedHandle = flushedHandle;

			public override async ValueTask<BlobData> ReadExportAsync(int exportIdx, CancellationToken cancellationToken = default)
			{
				lock (_bundle.LockObject)
				{
					if (_flushedHandle == null)
					{
						return _bundle.GetPendingExport(exportIdx);
					}
				}
				return await _flushedHandle!.ReadExportAsync(exportIdx, cancellationToken);
			}

			public override bool TryAppendIdentifier(Utf8StringBuilder builder)
				=> _flushedHandle?.TryAppendIdentifier(builder) ?? false;
		}

		// Fragment of a bundle that needs to be written to storage.
		internal sealed class PendingBundleHandle : BundleHandle, IDisposable
		{
			readonly object _lockObject = new object();

			readonly BundleStorageClient _storageClient;
			readonly string? _basePath;
			readonly BundleCache _cache;
			readonly BundleOptions _bundleOptions;

			FlushedBundleHandle? _flushedHandle;

			PacketWriter? _packetWriter;
			PendingPacketHandle? _packetHandle;
			List<(ExportHandle, AliasInfo)>? _pendingExportAliases;
			RefCountedMemoryWriter? _encodedPacketWriter;

			/// <summary>
			/// Object used for locking access to this bundle's state
			/// </summary>
			public object LockObject => _lockObject;

			public FlushedBundleHandle? FlushedHandle => _flushedHandle;

			/// <summary>
			/// Compressed length of this bundle
			/// </summary>
			public int Length => _encodedPacketWriter?.Length ?? throw new InvalidOperationException("Bundle has been flushed");

			public PendingBundleHandle(BundleStorageClient storageClient, string? basePath, BundleCache cache, BundleOptions bundleOptions)
			{
				_storageClient = storageClient;
				_basePath = basePath;
				_cache = cache;
				_bundleOptions = bundleOptions;

				_encodedPacketWriter = new RefCountedMemoryWriter(_cache.Allocator, 65536, nameof(PendingBundleHandle));

				StartPacket();
			}

			public void Dispose() => ReleaseResources();

			void ReleaseResources()
			{
				if (_packetWriter != null)
				{
					_packetWriter.Dispose();
					_packetWriter = null;
				}
				if (_encodedPacketWriter != null)
				{
					_encodedPacketWriter.Dispose();
					_encodedPacketWriter = null;
				}

				// Also clear out any arrays that can be GC'd
				_packetHandle = null;
				_pendingExportAliases = null;
			}

			public Memory<byte> GetOutputBuffer(int usedSize, int desiredSize)
			{
				RuntimeAssert(_packetWriter != null);
				return _packetWriter!.GetOutputBuffer(usedSize, desiredSize);
			}

			public BlobData GetPendingExport(int exportIdx)
			{
				RuntimeAssert(_packetWriter != null);
				return _packetWriter.GetExport(exportIdx);
			}

			public ExportHandle CompleteExport(BlobType type, int size, IReadOnlyList<IBlobHandle> imports, IReadOnlyList<AliasInfo> aliases)
			{
				RuntimeAssert(_packetWriter != null);
				RuntimeAssert(_packetHandle != null);

				IoHash hash = IoHash.Compute(_packetWriter.GetOutputBuffer(size, size).Span);

				int exportIdx = _packetWriter.CompleteExport(size, type, imports);
				ExportHandle exportHandle = new ExportHandle(hash, _packetHandle, exportIdx);

				if (aliases.Count > 0)
				{
					_pendingExportAliases ??= new List<(ExportHandle, AliasInfo)>();
					_pendingExportAliases.AddRange(aliases.Select(x => (exportHandle, x)));
				}

				if (_packetWriter.Length > Math.Min(_bundleOptions.MinCompressionPacketSize, _bundleOptions.MaxBlobSize))
				{
					FinishPacket();
					StartPacket();
				}

				return exportHandle;
			}

			void StartPacket()
			{
				RuntimeAssert(_packetHandle == null);
				RuntimeAssert(_packetWriter == null);

				_packetHandle = new PendingPacketHandle(this);
				_packetWriter = new PacketWriter(this, _packetHandle, _cache.Allocator, _lockObject);
			}

			void FinishPacket()
			{
				RuntimeAssert(_packetHandle != null);
				RuntimeAssert(_packetWriter != null);
				RuntimeAssert(_encodedPacketWriter != null);

				if (_packetWriter.GetExportCount() > 0)
				{
					int packetOffset = _encodedPacketWriter.Length;
					Packet packet = _packetWriter.CompletePacket();
					packet.Encode(_bundleOptions.CompressionFormat, _encodedPacketWriter);
					int packetLength = _encodedPacketWriter.Length - packetOffset;

					// Point the packet handle to the encoded data
					lock (_lockObject)
					{
						FlushedPacketHandle flushedPacketHandle = new FlushedPacketHandle(_storageClient, this, packetOffset, packetLength, _cache);
						_packetHandle.CompletePacket(flushedPacketHandle);
					}
				}

				_packetWriter.Dispose();
				_packetWriter = null;

				_packetHandle = null;
			}

			// Write this bundle to storage
			public override async ValueTask FlushAsync(CancellationToken cancellationToken = default)
			{
				// Check we haven't already flushed this bundle
				if (_encodedPacketWriter == null)
				{
					return;
				}

				if (_packetWriter != null)
				{
					FinishPacket();
				}

				if (_encodedPacketWriter.Length == 0)
				{
					return;
				}

				// Write the bundle data
				FlushedBundleHandle flushedHandle;
				using (ReadOnlySequenceStream stream = new ReadOnlySequenceStream(_encodedPacketWriter.AsSequence()))
				{
					BlobLocator locator = await _storageClient.Backend.WriteBlobAsync(stream, _basePath, cancellationToken);
					flushedHandle = new FlushedBundleHandle(_storageClient, locator);
				}

				// Release all the intermediate data
				List<(ExportHandle, AliasInfo)>? pendingExportAliases = _pendingExportAliases;
				lock (_lockObject)
				{
					_flushedHandle = flushedHandle;
					ReleaseResources();
				}

				// TODO: put all the encoded packets into the cache using the final handles

				// Add all the aliases
				if (pendingExportAliases != null)
				{
					foreach ((ExportHandle exportHandle, AliasInfo aliasInfo) in pendingExportAliases)
					{
						await _storageClient.AddAliasAsync(aliasInfo.Name, exportHandle, aliasInfo.Rank, aliasInfo.Data, cancellationToken);
					}
				}
			}

			/// <inheritdoc/>
			public override async Task<Stream> OpenAsync(int offset, int? length, CancellationToken cancellationToken = default)
			{
				if (_flushedHandle == null)
				{
					IReadOnlyMemoryOwner<byte> owner = await ReadAsync(offset, length, cancellationToken);
					return owner.AsStream();
				}

				return await _flushedHandle.OpenAsync(offset, length, cancellationToken);
			}

			/// <inheritdoc/>
			public override async ValueTask<IReadOnlyMemoryOwner<byte>> ReadAsync(int offset, int? length, CancellationToken cancellationToken = default)
			{
				if (_flushedHandle == null)
				{
					lock (_lockObject)
					{
						if (_flushedHandle == null)
						{
							Debug.Assert(_encodedPacketWriter != null);
							int fetchLength = length ?? (_encodedPacketWriter.Length - offset);

							IRefCountedHandle<ReadOnlyMemory<byte>> handle = _encodedPacketWriter.AsRefCountedMemory(offset, fetchLength);
							return ReadOnlyMemoryOwner.Create(handle.Target, handle);
						}
					}
				}

				return await _flushedHandle.ReadAsync(offset, length, cancellationToken);
			}

			/// <inheritdoc/>
			public override bool TryGetLocator(out BlobLocator locator)
			{
				if (_flushedHandle == null)
				{
					locator = default;
					return false;
				}
				return _flushedHandle.TryGetLocator(out locator);
			}
		}

		readonly BundleStorageClient _storageClient;
		readonly string? _basePath;
		readonly BundleOptions _bundleOptions;
		readonly BundleCache _bundleCache;

		PendingBundleHandle _currentBundle;

		/// <summary>
		/// 
		/// </summary>
		public BundleWriter(BundleStorageClient storageClient, string? basePath, BundleCache bundleCache, BundleOptions bundleOptions, BlobSerializerOptions? blobOptions)
			: base(blobOptions)
		{
			_storageClient = storageClient;
			_basePath = basePath;
			_bundleOptions = bundleOptions;
			_bundleCache = bundleCache;

			_currentBundle = new PendingBundleHandle(storageClient, basePath, bundleCache, _bundleOptions);
		}

		/// <inheritdoc/>
		public override async ValueTask DisposeAsync()
		{
			await FlushAsync();
			_currentBundle.Dispose();
		}

		/// <inheritdoc/>
		public override async Task FlushAsync(CancellationToken cancellationToken = default)
		{
			await _currentBundle.FlushAsync(cancellationToken);
			_currentBundle.Dispose();
			_currentBundle = new PendingBundleHandle(_storageClient, _basePath, _bundleCache, _bundleOptions);
		}

		/// <inheritdoc/>
		public override IBlobWriter Fork()
			=> new BundleWriter(_storageClient, _basePath, _bundleCache, _bundleOptions, Options);

		/// <inheritdoc/>
		public override Memory<byte> GetOutputBuffer(int usedSize, int desiredSize)
			=> _currentBundle.GetOutputBuffer(usedSize, desiredSize);

		/// <inheritdoc/>
		public override async ValueTask<IBlobRef> WriteBlobAsync(BlobType type, int size, IReadOnlyList<IBlobHandle> references, IReadOnlyList<AliasInfo> aliases, CancellationToken cancellationToken = default)
		{
			ExportHandle exportHandle = _currentBundle.CompleteExport(type, size, references, aliases);
			if (_currentBundle.Length > _bundleOptions.MaxBlobSize)
			{
				await FlushAsync(cancellationToken);
			}
			return exportHandle;
		}

		/// <summary>
		/// Helper method to check a precondition is valid at runtime, regardless of build configuration.
		/// </summary>
		static void RuntimeAssert([DoesNotReturnIf(false)] bool condition, [CallerArgumentExpression("condition")] string? message = null)
		{
			if (!condition)
			{
				throw new InvalidOperationException($"Condition failed: {message}");
			}
		}
	}
}

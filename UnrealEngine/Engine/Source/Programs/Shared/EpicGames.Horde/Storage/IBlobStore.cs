// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.CodeAnalysis.Rename;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Exception for a ref not existing
	/// </summary>
	public sealed class RefNameNotFoundException : Exception
	{
		/// <summary>
		/// Name of the missing ref
		/// </summary>
		public RefName Name { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name"></param>
		public RefNameNotFoundException(RefName name)
			: base($"Ref name '{name}' not found")
		{
			Name = name;
		}
	}

	/// <summary>
	/// Interface for accessing an object's data
	/// </summary>
	public interface IBlob
	{
		/// <summary>
		/// Identifier for the blob
		/// </summary>
		BlobId Id { get; }

		/// <summary>
		/// Gets the data for this blob
		/// </summary>
		/// <returns></returns>
		ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Find the outward references for a node
		/// </summary>
		/// <returns></returns>
		IReadOnlyList<BlobId> References { get; }
	}

	/// <summary>
	/// Base interface for a low-level storage backend. Blobs added to this store are not content addressed, but referenced by <see cref="BlobId"/>.
	/// </summary>
	public interface IBlobStore
	{
		#region Blobs

		/// <summary>
		/// Reads data for a blob from the store
		/// </summary>
		/// <param name="id">The blob identifier</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<IBlob> ReadBlobAsync(BlobId id, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a new object to the store
		/// </summary>
		/// <param name="data">Payload for the object</param>
		/// <param name="references">Object references</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <param name="prefix">Prefix for blob names. While the returned BlobId is guaranteed to be unique, this name can be used as a prefix to aid debugging.</param>
		/// <returns>Unique identifier for the blob</returns>
		Task<BlobId> WriteBlobAsync(ReadOnlySequence<byte> data, IReadOnlyList<BlobId> references, Utf8String prefix = default, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <summary>
		/// Reads data for a ref from the store, along with the blob contents.
		/// </summary>
		/// <param name="name">The ref name</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<IBlob?> TryReadRefAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads data for a ref from the store
		/// </summary>
		/// <param name="name">The ref name</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<BlobId> TryReadRefTargetAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a new ref to the store which points to a new blob
		/// </summary>
		/// <param name="name">Ref to write</param>
		/// <param name="data">Payload for the object</param>
		/// <param name="references">Object references</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Unique identifier for the blob</returns>
		Task<BlobId> WriteRefAsync(RefName name, ReadOnlySequence<byte> data, IReadOnlyList<BlobId> references, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a new ref to the store
		/// </summary>
		/// <param name="name">Ref to write</param>
		/// <param name="blobId">The target for the ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Unique identifier for the blob</returns>
		Task WriteRefTargetAsync(RefName name, BlobId blobId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads data for a ref from the store
		/// </summary>
		/// <param name="name">The ref identifier</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default);

		#endregion
	}

	/// <summary>
	/// Typed implementation of <see cref="IBlobStore"/> for use with dependency injection
	/// </summary>
	public interface IBlobStore<T> : IBlobStore
	{
	}

	/// <summary>
	/// Extension methods for <see cref="IBlobStore"/>
	/// </summary>
	public static class BlobStoreExtensions
	{
		class TypedBlobStore<T> : IBlobStore<T>
		{
			readonly IBlobStore _inner;

			public TypedBlobStore(IBlobStore inner) => _inner = inner;

			#region Blobs

			/// <inheritdoc/>
			public Task<IBlob> ReadBlobAsync(BlobId id, CancellationToken cancellationToken = default) => _inner.ReadBlobAsync(id, cancellationToken);

			/// <inheritdoc/>
			public Task<BlobId> WriteBlobAsync(ReadOnlySequence<byte> data, IReadOnlyList<BlobId> references, Utf8String prefix = default, CancellationToken cancellationToken = default) => _inner.WriteBlobAsync(data, references, prefix, cancellationToken);

			#endregion

			#region Refs

			/// <inheritdoc/>
			public Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default) => _inner.DeleteRefAsync(name, cancellationToken);

			/// <inheritdoc/>
			public Task<IBlob?> TryReadRefAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default) => _inner.TryReadRefAsync(name, cacheTime, cancellationToken);

			/// <inheritdoc/>
			public Task<BlobId> TryReadRefTargetAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default) => _inner.TryReadRefTargetAsync(name, cacheTime, cancellationToken);

			/// <inheritdoc/>
			public Task<BlobId> WriteRefAsync(RefName name, ReadOnlySequence<byte> data, IReadOnlyList<BlobId> references, CancellationToken cancellationToken = default) => _inner.WriteRefAsync(name, data, references, cancellationToken);

			/// <inheritdoc/>
			public Task WriteRefTargetAsync(RefName name, BlobId blobId, CancellationToken cancellationToken = default) => _inner.WriteRefTargetAsync(name, blobId, cancellationToken);

			#endregion
		}

		/// <summary>
		/// Wraps a <see cref="IBlobStore"/> interface with a type argument
		/// </summary>
		/// <param name="blobStore">Regular blob store instance</param>
		/// <returns></returns>
		public static IBlobStore<T> ForType<T>(this IBlobStore blobStore) => new TypedBlobStore<T>(blobStore);

		/// <summary>
		/// Checks if the given ref exists
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Name of the reference to look for</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the ref exists, false if it did not exist</returns>
		public static async Task<bool> HasRefAsync(this IBlobStore store, RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobId blobId = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			return blobId.IsValid();
		}

		/// <summary>
		/// Checks if the given ref exists
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Name of the reference to look for</param>
		/// <param name="maxAge">Maximum age of any cached ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the ref exists, false if it did not exist</returns>
		public static Task<bool> HasRefAsync(this IBlobStore store, RefName name, TimeSpan maxAge, CancellationToken cancellationToken = default)
		{
			return HasRefAsync(store, name, DateTime.UtcNow - maxAge, cancellationToken);
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="maxAge">Maximum age of any cached ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The blob instance</returns>
		public static Task<IBlob?> TryReadRefAsync(this IBlobStore store, RefName name, TimeSpan maxAge, CancellationToken cancellationToken = default)
		{
			return store.TryReadRefAsync(name, DateTime.UtcNow - maxAge, cancellationToken);
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="maxAge">Maximum age of any cached ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The blob instance</returns>
		public static Task<BlobId> TryReadRefIdAsync(this IBlobStore store, RefName name, TimeSpan maxAge, CancellationToken cancellationToken = default)
		{
			return store.TryReadRefTargetAsync(name, DateTime.UtcNow - maxAge, cancellationToken);
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="cacheTime">Minimum coherency of any cached result</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The blob instance</returns>
		public static async Task<IBlob> ReadRefAsync(this IBlobStore store, RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			IBlob? blob = await store.TryReadRefAsync(name, cacheTime, cancellationToken);
			if (blob == null)
			{
				throw new RefNameNotFoundException(name);
			}
			return blob;
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="maxAge">Maximum age for any cached result</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The blob instance</returns>
		public static Task<IBlob> ReadRefAsync(this IBlobStore store, RefName name, TimeSpan maxAge, CancellationToken cancellationToken = default)
		{
			return ReadRefAsync(store, name, DateTime.UtcNow - maxAge, cancellationToken);
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="cacheTime">Minimum coherency of any cached result</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The blob instance</returns>
		public static async Task<BlobId> ReadRefIdAsync(this IBlobStore store, RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobId blobId = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			if (!blobId.IsValid())
			{
				throw new RefNameNotFoundException(name);
			}
			return blobId;
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="maxAge">Maximum age for any cached result</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The blob instance</returns>
		public static Task<BlobId> ReadRefIdAsync(this IBlobStore store, RefName name, TimeSpan maxAge, CancellationToken cancellationToken = default)
		{
			return ReadRefIdAsync(store, name, DateTime.UtcNow - maxAge, cancellationToken);
		}
	}

	/// <summary>
	/// Utility methods for blobs
	/// </summary>
	public static class Blob
	{
		class InMemoryBlob : IBlob
		{
			/// <inheritdoc/>
			public BlobId Id { get; }

			/// <inheritdoc/>
			public ReadOnlyMemory<byte> Data { get; }

			/// <inheritdoc/>
			public IReadOnlyList<BlobId> References { get; }

			public InMemoryBlob(BlobId id, ReadOnlyMemory<byte> data, IReadOnlyList<BlobId> references)
			{
				Id = id;
				Data = data;
				References = references;
			}
		}

		/// <summary>
		/// Create a blob from memory
		/// </summary>
		/// <param name="id">Id for the blob</param>
		/// <param name="data">Payload for the blob</param>
		/// <param name="references">References to other blobs</param>
		/// <returns>Blob instance</returns>
		public static IBlob FromMemory(BlobId id, ReadOnlyMemory<byte> data, IReadOnlyList<BlobId> references) => new InMemoryBlob(id, data, references);

		/// <summary>
		/// Serialize a blob into a flat memory buffer
		/// </summary>
		/// <param name="blob">The blob to serialize</param>
		/// <returns>Serialized data</returns>
		public static ReadOnlySequence<byte> Serialize(IBlob blob)
		{
			return Serialize(blob.Data, blob.References);
		}

		/// <summary>
		/// Serialize a blob into a flat memory buffer
		/// </summary>
		/// <param name="data">Data for the blob</param>
		/// <param name="references">List of references to other blobs</param>
		/// <returns>Serialized data</returns>
		public static ReadOnlySequence<byte> Serialize(ReadOnlyMemory<byte> data, IReadOnlyList<BlobId> references)
		{
			return Serialize(new ReadOnlySequence<byte>(data), references);
		}

		/// <summary>
		/// Serialize a blob into a flat memory buffer
		/// </summary>
		/// <param name="data">Data for the blob</param>
		/// <param name="references">List of references to other blobs</param>
		/// <returns>Serialized data</returns>
		public static ReadOnlySequence<byte> Serialize(ReadOnlySequence<byte> data, IReadOnlyList<BlobId> references)
		{
			ByteArrayBuilder writer = new ByteArrayBuilder();
			writer.WriteVariableLengthArray(references, x => writer.WriteBlobId(x));
			writer.WriteUnsignedVarInt((ulong)data.Length);

			ReadOnlySequenceBuilder<byte> builder = new ReadOnlySequenceBuilder<byte>();
			builder.Append(writer.ToByteArray());
			builder.Append(data);

			return builder.Construct();
		}

		/// <summary>
		/// Deserialize a blob from a block of memory
		/// </summary>
		/// <param name="id">Id for the blob</param>
		/// <param name="memory">Memory to deserialize from</param>
		/// <returns>Deserialized blob data. May reference the supplied memory.</returns>
		public static IBlob Deserialize(BlobId id, ReadOnlyMemory<byte> memory) => Deserialize(id, new ReadOnlySequence<byte>(memory));

		/// <summary>
		/// Deserialize a blob from a block of memory
		/// </summary>
		/// <param name="id">Id for the blob</param>
		/// <param name="sequence">Sequence to deserialize from</param>
		/// <returns>Deserialized blob data. May reference the supplied memory.</returns>
		public static IBlob Deserialize(BlobId id, ReadOnlySequence<byte> sequence)
		{
			MemoryReader reader = new MemoryReader(sequence.First);
			IReadOnlyList<BlobId> references = reader.ReadVariableLengthArray(() => reader.ReadBlobId());
			long length = (long)reader.ReadUnsignedVarInt();

			ReadOnlyMemory<byte> data = sequence.Slice(sequence.First.Length - reader.Memory.Length).AsSingleSegment();
			return new InMemoryBlob(id, data, references);
		}
	}
}

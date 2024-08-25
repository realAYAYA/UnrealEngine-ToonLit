// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics.CodeAnalysis;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Reference to another node in storage. This type is similar to <see cref="IBlobRef"/>, but without a hash.
	/// </summary>
	public interface IBlobHandle
	{
		/// <summary>
		/// Accessor for the innermost import
		/// </summary>
		IBlobHandle Innermost { get; }

		/// <summary>
		/// Flush the referenced data to underlying storage
		/// </summary>
		ValueTask FlushAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads the blob's data
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempt to get a path for this blob.
		/// </summary>
		/// <param name="locator">Receives the blob path on success.</param>
		/// <returns>True if a path was available, false if the blob has not yet been flushed to storage.</returns>
		bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator);
	}

	/// <summary>
	/// Handle to a node. Can be used to reference nodes that have not been flushed yet.
	/// </summary>
	public interface IBlobRef : IBlobHandle
	{
		/// <summary>
		/// Hash of the target node
		/// </summary>
		IoHash Hash { get; }
	}

	/// <summary>
	/// Typed interface to a particular blob handle
	/// </summary>
	/// <typeparam name="T">Type of the deserialized blob</typeparam>
	public interface IBlobRef<out T> : IBlobRef
	{
		/// <summary>
		/// Options for deserializing the blob
		/// </summary>
		BlobSerializerOptions SerializerOptions { get; }
	}

	/// <summary>
	/// Helper methods for creating blob handles
	/// </summary>
	public static class BlobRef
	{
		class BlobRefImpl : IBlobRef
		{
			readonly IoHash _hash;
			readonly IBlobHandle _handle;

			public IBlobHandle Innermost => _handle.Innermost;
			public IoHash Hash => _hash;

			public BlobRefImpl(IoHash hash, IBlobHandle handle)
			{
				_hash = hash;
				_handle = handle;
			}

			public ValueTask FlushAsync(CancellationToken cancellationToken = default)
				=> _handle.FlushAsync(cancellationToken);

			public ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default)
				=> _handle.ReadBlobDataAsync(cancellationToken);

			public bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator)
				=> _handle.TryGetLocator(out locator);
		}

		class BlobRefImpl<T> : BlobRefImpl, IBlobRef<T>
		{
			readonly BlobSerializerOptions _options;

			public BlobSerializerOptions SerializerOptions => _options;

			public BlobRefImpl(IoHash hash, IBlobHandle handle, BlobSerializerOptions options)
				: base(hash, handle)
			{
				_options = options;
			}
		}

		/// <summary>
		/// Create an untyped blob handle
		/// </summary>
		/// <param name="handle">Imported blob interface</param>
		/// <param name="hash">Hash of the blob</param>
		/// <returns>Handle to the blob</returns>
		public static IBlobRef Create(IoHash hash, IBlobHandle handle)
			=> new BlobRefImpl(hash, handle);

		/// <summary>
		/// Create a typed blob handle
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="blobRef">Existing blob reference</param>
		/// <param name="options">Options for deserializing the target blob</param>
		/// <returns>Handle to the blob</returns>
		public static IBlobRef<T> Create<T>(IBlobRef blobRef, BlobSerializerOptions? options = null)
			=> new BlobRefImpl<T>(blobRef.Hash, blobRef, options ?? BlobSerializerOptions.Default);

		/// <summary>
		/// Create a typed blob handle
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="hash">Hash of the blob</param>
		/// <param name="handle">Imported blob interface</param>
		/// <param name="options">Options for deserializing the target blob</param>
		/// <returns>Handle to the blob</returns>
		public static IBlobRef<T> Create<T>(IoHash hash, IBlobHandle handle, BlobSerializerOptions? options = null)
			=> new BlobRefImpl<T>(hash, handle, options ?? BlobSerializerOptions.Default);
	}

	/// <summary>
	/// Contains the value for a blob ref
	/// </summary>
	public record class BlobRefValue(IoHash Hash, BlobLocator Locator);

	/// <summary>
	/// Extension methods for <see cref="IBlobHandle"/>
	/// </summary>
	public static class BlobHandleExtensions
	{
		/// <summary>
		/// Gets a path to this blob that can be used to describe blob references over the wire.
		/// </summary>
		/// <param name="import">Handle to query</param>
		public static BlobLocator GetLocator(this IBlobHandle import)
		{
			BlobLocator locator;
			if (!import.TryGetLocator(out locator))
			{
				throw new InvalidOperationException("Blob has not yet been written to storage");
			}
			return locator;
		}

		/// <summary>
		/// Gets a BlobRefValue from an IBlobRef
		/// </summary>
		public static BlobRefValue GetRefValue(this IBlobRef blobRef)
		{
			return new BlobRefValue(blobRef.Hash, blobRef.GetLocator());
		}
	}
}

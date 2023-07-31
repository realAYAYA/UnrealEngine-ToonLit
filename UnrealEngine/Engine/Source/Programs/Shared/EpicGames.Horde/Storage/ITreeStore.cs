// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Interface for a node within a tree, represented by an opaque blob of data
	/// </summary>
	public interface ITreeBlob
	{
		/// <summary>
		/// Data for the node
		/// </summary>
		ReadOnlySequence<byte> Data { get; }

		/// <summary>
		/// References to other tree blobs
		/// </summary>
		IReadOnlyList<ITreeBlobRef> Refs { get; }
	}

	/// <summary>
	/// Reference to a <see cref="ITreeBlob"/>
	/// </summary>
	public interface ITreeBlobRef
	{
		/// <summary>
		/// Hash of the target
		/// </summary>
		IoHash Hash { get; }

		/// <summary>
		/// Gets the target of a reference
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The referenced node</returns>
		ValueTask<ITreeBlob> GetTargetAsync(CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Base interface for a tree store
	/// </summary>
	public interface ITreeStore : IDisposable
	{
		/// <summary>
		/// Creates a new context for reading trees.
		/// </summary>
		/// <param name="prefix">Prefix for blobs. See <see cref="IBlobStore.WriteBlobAsync(ReadOnlySequence{Byte}, IReadOnlyList{BlobId}, Utf8String, CancellationToken)"/></param>
		/// <returns>New context instance</returns>
		ITreeWriter CreateTreeWriter(Utf8String prefix = default);

		/// <summary>
		/// Deletes a tree with the given name
		/// </summary>
		/// <param name="name">Name of the ref used to store the tree</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task DeleteTreeAsync(RefName name, CancellationToken cancellationToken = default);

		/// <summary>
		/// Tests whether a tree exists with the given name
		/// </summary>
		/// <param name="name">Name of the ref used to store the tree</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Tree reader instance</returns>
		Task<bool> HasTreeAsync(RefName name, CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempts to read a tree with the given name
		/// </summary>
		/// <param name="name">Name of the ref used to store the tree</param>
		/// <param name="maxAge"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Reference to the root of the tree. Null if the ref does not exist.</returns>
		Task<ITreeBlob?> TryReadTreeAsync(RefName name, TimeSpan maxAge = default, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Interface for a typed tree store
	/// </summary>
	public interface ITreeStore<T> : ITreeStore
	{
	}

	/// <summary>
	/// Allows incrementally writing new trees
	/// </summary>
	public interface ITreeWriter
	{
		/// <summary>
		/// Creates a new tree writer parented to this. Any nodes added to this writer can reference nodes written to any child writers, and bundle writes will 
		/// be sequenced to allow references between them.
		/// </summary>
		/// <returns>New writer instance</returns>
		ITreeWriter CreateChildWriter();

		/// <summary>
		/// Adds a new node to this tree
		/// </summary>
		/// <param name="data">Data for the node</param>
		/// <param name="refs">References to other nodes</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Reference to the node that was added</returns>
		Task<ITreeBlobRef> WriteNodeAsync(ReadOnlySequence<byte> data, IReadOnlyList<ITreeBlobRef> refs, CancellationToken cancellationToken = default);

		/// <summary>
		/// Flushes the current state using a new ref name.
		/// </summary>
		/// <param name="name">Name of the ref</param>
		/// <param name="root">Root reference</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task WriteRefAsync(RefName name, ITreeBlobRef root, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Utility class
	/// </summary>
	public static class TreeBlob
	{
		class TreeBlobImpl : ITreeBlob
		{
			public ReadOnlySequence<byte> Data { get; }
			public IReadOnlyList<ITreeBlobRef> Refs { get; }

			public TreeBlobImpl(ReadOnlySequence<byte> data, IReadOnlyList<ITreeBlobRef> refs)
			{
				Data = data;
				Refs = refs;
			}
		}

		/// <summary>
		/// Create a blob from the given parameters
		/// </summary>
		public static ITreeBlob Create(ReadOnlySequence<byte> Data, IReadOnlyList<ITreeBlobRef> Refs)
		{
			return new TreeBlobImpl(Data, Refs);
		}
	}

	/// <summary>
	/// Extension methods for tree store instances
	/// </summary>
	public static class TreeStoreExtensions
	{
		sealed class TypedTreeStore<T> : ITreeStore<T>
		{
			readonly ITreeStore _inner;

			public TypedTreeStore(ITreeStore inner)
			{
				_inner = inner;
			}

			/// <inheritdoc/>
			public ITreeWriter CreateTreeWriter(Utf8String prefix) => _inner.CreateTreeWriter(prefix);

			/// <inheritdoc/>
			public Task DeleteTreeAsync(RefName name, CancellationToken cancellationToken = default) => _inner.DeleteTreeAsync(name, cancellationToken);

			/// <inheritdoc/>
			public void Dispose() => _inner.Dispose();

			/// <inheritdoc/>
			public Task<bool> HasTreeAsync(RefName name, CancellationToken cancellationToken = default) => _inner.HasTreeAsync(name, cancellationToken);

			/// <inheritdoc/>
			public Task<ITreeBlob?> TryReadTreeAsync(RefName name, TimeSpan maxAge = default, CancellationToken cancellationToken = default) => _inner.TryReadTreeAsync(name, maxAge, cancellationToken);
		}

		/// <summary>
		/// Wraps an <see cref="ITreeStore"/> interface with a type for dependency injection
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="store">The instance to wrap</param>
		/// <returns>Wrapped instance of the tree store</returns>
		public static ITreeStore<T> ForType<T>(this ITreeStore store)
		{
			return new TypedTreeStore<T>(store);
		}

		/// <summary>
		/// Writes a ref using the given root data
		/// </summary>
		/// <param name="writer">Writer to modify</param>
		/// <param name="name">Name of the ref</param>
		/// <param name="data">Data for the node</param>
		/// <param name="refs">References to other nodes</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Reference to the node that was added</returns>
		public static async Task WriteRefAsync(this ITreeWriter writer, RefName name, ReadOnlySequence<byte> data, IReadOnlyList<ITreeBlobRef> refs, CancellationToken cancellationToken = default)
		{
			ITreeBlobRef root = await writer.WriteNodeAsync(data, refs, cancellationToken);
			await writer.WriteRefAsync(name, root, cancellationToken);
		}
	}
}

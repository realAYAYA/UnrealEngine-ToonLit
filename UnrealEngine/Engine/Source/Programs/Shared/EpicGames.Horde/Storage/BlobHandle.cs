// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Handle to a node. Can be used to reference nodes that have not been flushed yet.
	/// </summary>
	public abstract class BlobHandle
	{
		/// <summary>
		/// Hash of the target node
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="hash">Hash of the target node</param>
		protected BlobHandle(IoHash hash) => Hash = hash;

		/// <summary>
		/// Determines if the node has been written to storage
		/// </summary>
		public abstract bool HasLocator();

		/// <summary>
		/// Gets the node locator. May throw if the node has not been written to storage yet.
		/// </summary>
		/// <returns>Locator for the node</returns>
		public abstract NodeLocator GetLocator();

		/// <summary>
		/// Adds a callback to be executed once the node has been written. Triggers immediately if the node has already been written.
		/// </summary>
		/// <param name="callback">Action to be executed after the write</param>
		public abstract void AddWriteCallback(BlobWriteCallback callback);

		/// <summary>
		/// Gets the type of this blob
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public virtual async ValueTask<BlobType> GetTypeAsync(CancellationToken cancellationToken = default)
		{
			BlobData data = await ReadAsync(cancellationToken);
			return data.Type;
		}

		/// <summary>
		/// Gets the outward references from this blob
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public virtual async ValueTask<IReadOnlyList<BlobHandle>> GetRefsAsync(CancellationToken cancellationToken = default)
		{
			BlobData data = await ReadAsync(cancellationToken);
			return data.Refs;
		}

		/// <summary>
		/// Creates a reader for this node's data
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public abstract ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Creates a reader for this node's data
		/// </summary>
		/// <param name="offset">Offset within the payload stream to start reading</param>
		/// <param name="buffer">Buffer to receive the data that was read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Number of bytes that were read</returns>
		public virtual async ValueTask<int> ReadPartialAsync(int offset, Memory<byte> buffer, CancellationToken cancellationToken = default)
		{
			BlobData data = await ReadAsync(cancellationToken);

			int length = data.Data.Length - offset;
			if (length < 0)
			{
				return 0;
			}
			if (length > buffer.Length)
			{
				length = buffer.Length;
			}

			data.Data.Slice(offset, length).CopyTo(buffer);
			return length;
		}

		/// <summary>
		/// Flush the node to storage and retrieve its locator
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public abstract ValueTask<NodeLocator> FlushAsync(CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public override string ToString() => HasLocator()? GetLocator().ToString() : Hash.ToString();
	}

	/// <summary>
	/// Object to receive notifications on a node being written
	/// </summary>
	public abstract class BlobWriteCallback
	{
		internal BlobWriteCallback? _next;

		/// <summary>
		/// Callback for the node being written
		/// </summary>
		public abstract void OnWrite();
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using System.Reflection.Metadata;
using System.Threading;
using System.Threading.Tasks;
using System.Xml.Linq;
using EpicGames.Serialization;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Base class for user-defined types that are stored in a tree
	/// </summary>
	public abstract class TreeNode
	{
		/// <summary>
		/// Cached incoming reference to the owner of this node.
		/// </summary>
		internal TreeNodeRef? IncomingRef { get; set; }

		/// <summary>
		/// Queries if the node in its current state is read-only. Once we know that nodes are no longer going to be modified, they are favored for spilling to persistent storage.
		/// </summary>
		/// <returns>True if the node is read-only.</returns>
		public virtual bool IsReadOnly() => false;

		/// <summary>
		/// Mark this node as dirty
		/// </summary>
		protected void MarkAsDirty() => IncomingRef?.MarkAsDirty();

		/// <summary>
		/// Enumerates all the child references from this node
		/// </summary>
		/// <returns>Children of this node</returns>
		public abstract IReadOnlyList<TreeNodeRef> GetReferences();

		/// <summary>
		/// Static instance of the serializer for a particular <see cref="TreeNode"/> type.
		/// </summary>
		static class SerializerInstance<T> where T : TreeNode
		{
			static readonly TreeSerializerAttribute _attribute = typeof(T).GetCustomAttribute<TreeSerializerAttribute>()!;
			public static TreeNodeSerializer<T> Serializer { get; } = (TreeNodeSerializer<T>)Activator.CreateInstance(_attribute.Type)!;
		}

		/// <summary>
		/// Serialize a node to a block of memory
		/// </summary>
		/// <returns>New data to be stored into a blob</returns>
		public abstract Task<ITreeBlob> SerializeAsync(ITreeWriter writer, CancellationToken cancellationToken);

		/// <summary>
		/// Deserialize a node from data
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="blob">Blob to deserialize</param>
		/// <returns></returns>
		public static T Deserialize<T>(ITreeBlob blob) where T : TreeNode
		{
			return SerializerInstance<T>.Serializer.Deserialize(blob);
		}
	}

	/// <summary>
	/// Data to be stored in a tree blob
	/// </summary>
	public struct NewTreeBlob : ITreeBlob
	{
		/// <summary>
		/// The opaque data payload
		/// </summary>
		public ReadOnlySequence<byte> Data { get; }

		/// <summary>
		/// References to other tree nodes
		/// </summary>
		public IReadOnlyList<ITreeBlobRef> Refs { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="data">Data payload</param>
		/// <param name="refs">References to other nodes</param>
		public NewTreeBlob(ReadOnlyMemory<byte> data, IReadOnlyList<ITreeBlobRef> refs)
		{
			Data = new ReadOnlySequence<byte>(data);
			Refs = refs;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="data">Data payload</param>
		/// <param name="refs">References to other nodes</param>
		public NewTreeBlob(ReadOnlySequence<byte> data, IReadOnlyList<ITreeBlobRef> refs)
		{
			Data = data;
			Refs = refs;
		}
	}

	/// <summary>
	/// Factory class for deserializing node types
	/// </summary>
	/// <typeparam name="T">The type of node returned</typeparam>
	public abstract class TreeNodeSerializer<T> where T : TreeNode
	{
		/// <summary>
		/// Deserializes data from the given data
		/// </summary>
		/// <param name="blob">The typed blob</param>
		/// <returns>New node parsed from the data</returns>
		public abstract T Deserialize(ITreeBlob blob);
	}

	/// <summary>
	/// Attribute used to define a factory for a particular node type
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class TreeSerializerAttribute : Attribute
	{
		/// <summary>
		/// The factory type. Should be derived from <see cref="TreeNodeSerializer{T}"/>
		/// </summary>
		public Type Type { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public TreeSerializerAttribute(Type type) => Type = type;
	}

	/// <summary>
	/// Extension methods for serializing <see cref="TreeNode"/> objects
	/// </summary>
	public static class TreeNodeExtensions
	{
		/// <summary>
		/// Writes a node to storage
		/// </summary>
		/// <param name="writer">Writer to output the nodes to</param>
		/// <param name="node">Root node to serialize</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task<ITreeBlobRef> WriteNodeAsync(this ITreeWriter writer, TreeNode node, CancellationToken cancellationToken = default)
		{
			ITreeBlob blob = await node.SerializeAsync(writer, cancellationToken);
			return await writer.WriteNodeAsync(blob.Data, blob.Refs, cancellationToken);
		}

		/// <summary>
		/// Flushes a tree to storage using the given root node
		/// </summary>
		/// <param name="writer">Writer to output the nodes to</param>
		/// <param name="name">Name of the ref to write</param>
		/// <param name="node">Root node to serialize</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task WriteRefAsync(this ITreeWriter writer, RefName name, TreeNode node, CancellationToken cancellationToken = default)
		{
			ITreeBlobRef root = await writer.WriteNodeAsync(node, cancellationToken);
			await writer.WriteRefAsync(name, root, cancellationToken);
		}

		/// <inheritdoc/>
		public static async Task<T?> TryReadTreeAsync<T>(this ITreeStore store, RefName name, TimeSpan maxAge = default, CancellationToken cancellationToken = default) where T : TreeNode
		{
			ITreeBlob? root = await store.TryReadTreeAsync(name, maxAge, cancellationToken);
			if (root == null)
			{
				return null;
			}
			return TreeNode.Deserialize<T>(root);
		}

		/// <inheritdoc/>
		public static async Task<T> ReadTreeAsync<T>(this ITreeStore store, RefName name, TimeSpan maxAge = default, CancellationToken cancellationToken = default) where T : TreeNode
		{
			T? result = await store.TryReadTreeAsync<T>(name, maxAge, cancellationToken);
			if (result == null)
			{
				throw new RefNameNotFoundException(name);
			}
			return result;
		}

		/// <inheritdoc/>
		public static async Task WriteTreeAsync(this ITreeStore store, RefName name, TreeNode root, CancellationToken cancellationToken = default)
		{
			ITreeWriter writer = store.CreateTreeWriter(name.Text);
			await writer.WriteRefAsync(name, root, cancellationToken);
		}
	}
}

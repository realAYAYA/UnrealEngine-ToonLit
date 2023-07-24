// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Base class for user-defined types that are stored in a tree
	/// </summary>
	public abstract class TreeNode
	{
		/// <summary>
		/// Revision number of the node. Incremented whenever the node is modified, and used to track whether nodes are modified between 
		/// writes starting and completing.
		/// </summary>
		public uint Revision { get; private set; }

		/// <summary>
		/// Hash when deserialized
		/// </summary>
		public IoHash Hash { get; internal set; }

		/// <summary>
		/// Default constructor
		/// </summary>
		protected TreeNode()
		{
		}

		/// <summary>
		/// Serialization constructor. Leaves the revision number zeroed by default.
		/// </summary>
		/// <param name="reader"></param>
		protected TreeNode(ITreeNodeReader reader)
		{
			Hash = reader.Hash;
		}

		/// <summary>
		/// Mark this node as dirty
		/// </summary>
		protected void MarkAsDirty()
		{
			Hash = IoHash.Zero;
			Revision++;
		}

		/// <summary>
		/// Serialize the contents of this node
		/// </summary>
		/// <returns>Data for the node</returns>
		public abstract void Serialize(ITreeNodeWriter writer);

		/// <summary>
		/// Enumerate all outward references from this node
		/// </summary>
		/// <returns>References to other nodes</returns>
		public abstract IEnumerable<TreeNodeRef> EnumerateRefs();
	}

	/// <summary>
	/// Writer for tree nodes
	/// </summary>
	public interface ITreeNodeWriter : IMemoryWriter
	{
		/// <summary>
		/// Writes a reference to another node
		/// </summary>
		/// <param name="handle">Handle to the target node</param>
		void WriteNodeHandle(NodeHandle handle);
	}

	/// <summary>
	/// Attribute used to define a factory for a particular node type
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class TreeNodeAttribute : Attribute
	{
		/// <summary>
		/// Name of the type to store in the bundle header
		/// </summary>
		public string Guid { get; }

		/// <summary>
		/// Version number of the serializer
		/// </summary>
		public int Version { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public TreeNodeAttribute(string guid, int version = 1)
		{
			Guid = guid;
			Version = version;
		}
	}

	/// <summary>
	/// Extension methods for serializing <see cref="TreeNode"/> objects
	/// </summary>
	public static class TreeNodeExtensions
	{
		/// <summary>
		/// Read an untyped ref from the reader
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New untyped ref</returns>
		public static TreeNodeRef ReadRef(this ITreeNodeReader reader)
		{
			return new TreeNodeRef(reader);
		}

		/// <summary>
		/// Read a strongly typed ref from the reader
		/// </summary>
		/// <typeparam name="T">Type of the referenced node</typeparam>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New strongly typed ref</returns>
		public static TreeNodeRef<T> ReadRef<T>(this ITreeNodeReader reader) where T : TreeNode
		{
			return new TreeNodeRef<T>(reader);
		}

		/// <summary>
		/// Read an optional untyped ref from the reader
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New untyped ref</returns>
		public static TreeNodeRef? ReadOptionalRef(this ITreeNodeReader reader)
		{
			if (reader.ReadBoolean())
			{
				return reader.ReadRef();
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Read an optional strongly typed ref from the reader
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New strongly typed ref</returns>
		public static TreeNodeRef<T>? ReadOptionalRef<T>(this ITreeNodeReader reader) where T : TreeNode
		{
			if (reader.ReadBoolean())
			{
				return reader.ReadRef<T>();
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Writes a ref to storage
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteRef(this ITreeNodeWriter writer, TreeNodeRef value)
		{
			value.Serialize(writer);
		}

		/// <summary>
		/// Writes an optional ref value to storage
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteOptionalRef(this ITreeNodeWriter writer, TreeNodeRef? value)
		{
			if (value == null)
			{
				writer.WriteBoolean(false);
			}
			else
			{
				writer.WriteBoolean(true);
				writer.WriteRef(value);
			}
		}

		/// <summary>
		/// Writes a node to storage
		/// </summary>
		/// <param name="store">Store instance to write to</param>
		/// <param name="name">Name of the ref containing this node</param>
		/// <param name="node">Node to be written</param>
		/// <param name="options">Options for the node writer</param>
		/// <param name="prefix">Prefix for uploaded blobs</param>
		/// <param name="refOptions">Options for the ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Location of node targetted by the ref</returns>
		public static async Task<NodeHandle> WriteNodeAsync(this IStorageClient store, RefName name, TreeNode node, TreeOptions? options = null, Utf8String prefix = default, RefOptions? refOptions = null, CancellationToken cancellationToken = default)
		{
			using TreeWriter writer = new TreeWriter(store, options, prefix.IsEmpty ? name.Text : prefix);
			return await writer.WriteAsync(name, node, refOptions, cancellationToken);
		}

		/// <summary>
		/// Cache of constructed <see cref="BundleType"/> instances.
		/// </summary>
		static readonly ConcurrentDictionary<Type, BundleType> s_typeToBundleType = new ConcurrentDictionary<Type, BundleType>();

		/// <summary>
		/// Gets the bundle type object for a particular node
		/// </summary>
		/// <param name="node"></param>
		/// <returns></returns>
		public static BundleType GetBundleType(this TreeNode node) => GetBundleType(node.GetType());

		/// <summary>
		/// Gets the <see cref="BundleType"/> instance for a particular node type
		/// </summary>
		/// <param name="type"></param>
		/// <returns>Bundle</returns>
		public static BundleType GetBundleType(Type type)
		{
			BundleType? bundleType;
			if (!s_typeToBundleType.TryGetValue(type, out bundleType))
			{
				TreeNodeAttribute? attribute = type.GetCustomAttribute<TreeNodeAttribute>();
				if (attribute == null)
				{
					throw new InvalidOperationException($"Missing {nameof(TreeNodeAttribute)} from type {type.Name}");
				}
				bundleType = s_typeToBundleType.GetOrAdd(type, new BundleType(Guid.Parse(attribute.Guid), attribute.Version));
			}
			return bundleType;
		}
	}
}

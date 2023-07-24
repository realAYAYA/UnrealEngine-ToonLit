// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Stores a reference to a node, which may be in memory or in the storage system.
	/// 
	/// Refs may be in the following states:
	///  * In storage 
	///      * Locator is valid, Target is null, IsDirty() returns false.
	///      * Writing or calling Collapse() is a no-op.
	///  * In memory and target was expanded, has not been modified 
	///      * Locator is valid, Target is valid, IsDirty() returns false.
	///      * Writing or calling Collapse() on a ref transitions it to the "in storage" state.
	///  * In memory and target is new
	///      * Locator is invalid, Target is valid, IsDirty() returns true.
	///      * Writing a ref transitions it to the "in storage" state. Calling Collapse is a no-op.
	///  * In memory but target has been modified
	///      * Locator is set but may not reflect the current node state, Target is valid, IsDirty() returns true. 
	///      * Writing a ref transitions it to the "in storage" state. Calling Collapse is a no-op.
	///
	/// The <see cref="OnCollapse"/> and <see cref="OnExpand"/> methods allow overriden implementations to cache information about the target.
	///
	/// Each ref must have EXACTLY one owner; sharing of refs between objects is not permitted and will break change tracking.
	/// Multiple refs MAY point to the same target object.
	/// 
	/// To read an untracked object that can be added to a new ref, call <see cref="TreeReader.ReadNodeAsync(NodeLocator, CancellationToken)"/> 
	/// directly, or use <see cref="TreeNodeRef{T}.ExpandCopyAsync(TreeReader, CancellationToken)"/>.
	/// </summary>
	public class TreeNodeRef
	{
		/// <summary>
		/// The target node, or null if the node is not resident in memory.
		/// </summary>
		public TreeNode? Target { get; private set; }

		/// <summary>
		/// Handle to the node if in storage (or pending write to storage)
		/// </summary>
		public NodeHandle? Handle { get; private set; }

		/// <summary>
		/// Creates a reference to a node in memory.
		/// </summary>
		/// <param name="target">Node to reference</param>
		public TreeNodeRef(TreeNode target)
		{
			Target = target;
		}

		/// <summary>
		/// Creates a reference to a node in storage.
		/// </summary>
		/// <param name="handle">Handle to the referenced node</param>
		public TreeNodeRef(NodeHandle handle)
		{
			Handle = handle;
		}

		/// <summary>
		/// Deserialization constructor
		/// </summary>
		/// <param name="reader"></param>
		public TreeNodeRef(ITreeNodeReader reader) : this(reader.ReadNodeHandle())
		{
		}

		/// <summary>
		/// Determines whether the the referenced node has been modified from the last version written to storage
		/// </summary>
		/// <returns></returns>
		public bool IsDirty() => Target != null && (Handle == null || Target.Hash != Handle.Hash);

		/// <summary>
		/// Update the reference to refer to a node in memory.
		/// </summary>
		public void MarkAsDirty()
		{
			if (Target == null)
			{
				throw new InvalidOperationException("Cannot mark a ref as dirty without having expanded it.");
			}

			Handle = null;
		}

		/// <summary>
		/// Updates the hash and revision number for the ref.
		/// </summary>
		/// <returns></returns>
		internal void MarkAsPendingWrite(NodeHandle handle)
		{
			OnCollapse();
			Handle = handle;
		}

		/// <summary>
		/// Update the reference to refer to a location in storage.
		/// </summary>
		internal void MarkAsWritten()
		{
			if (Target != null && Handle != null && Target.Hash == Handle.Hash)
			{
				Target = null;
			}
		}

		/// <summary>
		/// Serialize the node to the given writer
		/// </summary>
		/// <param name="writer"></param>
		public virtual void Serialize(ITreeNodeWriter writer)
		{
			Debug.Assert(Handle != null);
			writer.WriteNodeHandle(Handle);
		}

		/// <summary>
		/// Resolve this reference to a concrete node
		/// </summary>
		/// <param name="reader">Reader to use for expanding this ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async ValueTask<TreeNode> ExpandAsync(TreeReader reader, CancellationToken cancellationToken = default)
		{
			if (Target == null)
			{
				Target = await reader.ReadNodeAsync(Handle!.Locator, cancellationToken);
				OnExpand();
			}
			return Target;
		}

		/// <summary>
		/// Collapse the current node
		/// </summary>
		public void Collapse()
		{
			if (Target != null && !IsDirty())
			{
				OnCollapse();
				Target = null;
			}
		}

		/// <summary>
		/// Callback after a node is expanded, allowing overridden implementations to cache any information about the target
		/// </summary>
		protected virtual void OnExpand()
		{
		}

		/// <summary>
		/// Callback before a node is collapsed, allowing overridden implementations to cache any information about the target
		/// </summary>
		protected virtual void OnCollapse()
		{
		}
	}

	/// <summary>
	/// Strongly typed reference to a <see cref="TreeNode"/>
	/// </summary>
	/// <typeparam name="T">Type of the node</typeparam>
	public class TreeNodeRef<T> : TreeNodeRef where T : TreeNode
	{
		/// <summary>
		/// Accessor for the target node
		/// </summary>
		public new T? Target => (T?)base.Target;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="target">The referenced node</param>
		public TreeNodeRef(T target) : base(target)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="handle">Handle to the referenced node</param>
		public TreeNodeRef(NodeHandle handle) : base(handle)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="reader">The reader to deserialize from</param>
		public TreeNodeRef(ITreeNodeReader reader) : base(reader)
		{
		}

		/// <summary>
		/// Resolve this reference to a concrete node
		/// </summary>
		/// <param name="reader">Reader to use for expanding this ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public new async ValueTask<T> ExpandAsync(TreeReader reader, CancellationToken cancellationToken = default)
		{
			return (T)await base.ExpandAsync(reader, cancellationToken);
		}

		/// <summary>
		/// Resolve this reference to a concrete node
		/// </summary>
		/// <param name="reader">Reader to use for expanding this ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async ValueTask<T> ExpandCopyAsync(TreeReader reader, CancellationToken cancellationToken = default)
		{
			if (Handle == null)
			{
				throw new InvalidOperationException("TreeNodeRef has not been serialized to storage");
			}
			return await reader.ReadNodeAsync<T>(Handle.Locator, cancellationToken);
		}
	}

	/// <summary>
	/// Extension methods for writing node
	/// </summary>
	public static class TreeNodeRefExtensions
	{
		// Implementation of INodeWriter that tracks refs
		class NodeWriter : ITreeNodeWriter
		{
			readonly TreeWriter _treeWriter;

			Memory<byte> _memory;
			int _length;
			readonly IReadOnlyList<NodeHandle> _refs;
			int _refIdx;

			public int Length => _length;

			public NodeWriter(TreeWriter treeWriter, IReadOnlyList<NodeHandle> refs)
			{
				_treeWriter = treeWriter;
				_memory = treeWriter.GetOutputBuffer(0, 256 * 1024);
				_refs = refs;
			}

			public void WriteNodeHandle(NodeHandle target)
			{
				if (_refs[_refIdx] != target)
				{
					throw new InvalidOperationException("Referenced node does not match the handle returned by owner's EnumerateRefs method.");
				}

				this.WriteIoHash(target.Hash);
				_refIdx++;
			}

			public Span<byte> GetSpan(int sizeHint = 0) => GetMemory(sizeHint).Span;

			public Memory<byte> GetMemory(int sizeHint = 0)
			{
				int newLength = _length + Math.Max(sizeHint, 1);
				if (newLength > _memory.Length)
				{
					newLength = _length + Math.Max(sizeHint, 1024);
					_memory = _treeWriter.GetOutputBuffer(_length, Math.Max(_memory.Length * 2, newLength));
				}
				return _memory.Slice(_length);
			}

			public void Advance(int length) => _length += length;
		}

		/// <summary>
		/// Class used to track nodes which are pending write (and the state of the object when the write was started)
		/// </summary>
		internal class NodeWriteCallback : TreeWriter.WriteCallback
		{
			readonly TreeNodeRef _nodeRef;
			readonly NodeHandle _handle;

			public NodeWriteCallback(TreeNodeRef nodeRef, NodeHandle handle)
			{
				_nodeRef = nodeRef;
				_handle = handle;
			}

			public override void OnWrite()
			{
				if (_nodeRef.Handle == _handle)
				{
					_nodeRef.MarkAsWritten();
				}
			}
		}

		/// <summary>
		/// Writes an individual node to storage
		/// </summary>
		/// <param name="writer">Writer to serialize nodes to</param>
		/// <param name="nodeRef">Reference to the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>A flag indicating whether the node is dirty, and if it is, an optional bundle that contains it</returns>
		public static async ValueTask<NodeHandle> WriteAsync(this TreeWriter writer, TreeNodeRef nodeRef, CancellationToken cancellationToken)
		{
			// Check we actually have a target node. If we don't, we don't need to write anything.
			TreeNode? target = nodeRef.Target;
			if (target == null)
			{
				Debug.Assert(nodeRef.Handle != null);
				return nodeRef.Handle;
			}

			// Write all the nodes it references, and mark the ref as dirty if any of them change.
			List<TreeNodeRef> nextRefs = target.EnumerateRefs().ToList();
			List<NodeHandle> nextRefHandles = new List<NodeHandle>(nextRefs.Count);
			foreach (TreeNodeRef nextRef in nextRefs)
			{
				NodeHandle nextRefHandle = await WriteAsync(writer, nextRef, cancellationToken);
				if (!nextRefHandle.Locator.IsValid())
				{
					nodeRef.MarkAsDirty();
				}
				nextRefHandles.Add(nextRefHandle);
			}

			// If the target node hasn't been modified, use the existing serialized state.
			if (!nodeRef.IsDirty())
			{
				// Make sure the locator is valid. The node may be queued for writing but not flushed to disk yet.
				Debug.Assert(nodeRef.Handle != null);
				if (nodeRef.Handle.Locator.IsValid())
				{
					nodeRef.Collapse();
				}
				return nodeRef.Handle;
			}

			// Serialize the node
			NodeWriter nodeWriter = new NodeWriter(writer, nextRefHandles);
			target.Serialize(nodeWriter);

			// Write the final data
			NodeHandle handle = await writer.WriteNodeAsync(nodeWriter.Length, nextRefHandles, target.GetBundleType(), cancellationToken);
			target.Hash = handle.Hash;

			NodeWriteCallback writeState = new NodeWriteCallback(nodeRef, handle);
			handle.AddWriteCallback(writeState);

			nodeRef.MarkAsPendingWrite(handle);
			return handle;
		}

		/// <summary>
		/// Writes a node to the given ref
		/// </summary>
		/// <param name="writer"></param>
		/// <param name="name">Name of the ref to write</param>
		/// <param name="node"></param>
		/// <param name="options"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async Task<NodeHandle> WriteAsync(this TreeWriter writer, RefName name, TreeNode node, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			TreeNodeRef nodeRef = new TreeNodeRef(node);
			await writer.WriteAsync(nodeRef, cancellationToken);
			await writer.FlushAsync(cancellationToken);

			Debug.Assert(nodeRef.Handle != null);
			await writer.Store.WriteRefTargetAsync(name, nodeRef.Handle, options, cancellationToken);
			return nodeRef.Handle;
		}

		/// <summary>
		/// Flushes all the current nodes to storage
		/// </summary>
		/// <param name="writer"></param>
		/// <param name="root">Root for the tree</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public static async Task<NodeHandle> FlushAsync(this TreeWriter writer, TreeNode root, CancellationToken cancellationToken)
		{
			TreeNodeRef rootRef = new TreeNodeRef(root);
			await writer.WriteAsync(rootRef, cancellationToken);
			await writer.FlushAsync(cancellationToken);
			return rootRef.Handle!;
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Serialization;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Stores a reference from a parent to child node, which can be resurrected after the child node is flushed to storage if subsequently modified.
	/// </summary>
	public class TreeNodeRef
	{
		/// <summary>
		/// Cached reference to the parent node
		/// </summary>
		internal TreeNode? _owner;

		/// <summary>
		/// Strong reference to the current node
		/// </summary>
		internal TreeNode? _strongRef;

		/// <summary>
		/// Weak reference to the child node. Maintained after the object has been flushed.
		/// </summary>
		internal WeakReference<TreeNode>? _weakRef;

		/// <summary>
		/// Last time that the node was modified
		/// </summary>
		internal long _lastModifiedTime;

		/// <summary>
		/// Handle to the node in storage. May be set to null when the node is in memory.
		/// </summary>
		internal ITreeBlobRef? _target;

		/// <summary>
		/// Reference to the node in storage, if unmodified.
		/// </summary>
		public ITreeBlobRef? Target => ReferenceEquals(_strongRef, null) ? null : _target;

		/// <summary>
		/// Reference to the node.
		/// </summary>
		public TreeNode? Node
		{
			get => _strongRef;
			set
			{
				_strongRef = value;
				_weakRef = null;
			}
		}

		/// <summary>
		/// Creates a reference to a node with the given hash
		/// </summary>
		/// <param name="persistedNode">Hash of the referenced node</param>
		public TreeNodeRef(ITreeBlobRef persistedNode)
		{
			_target = persistedNode;
		}

		/// <summary>
		/// Creates a reference to a node with the given hash
		/// </summary>
		/// <param name="owner">The node which owns the reference</param>
		/// <param name="persistedNode">Hash of the referenced node</param>
		public TreeNodeRef(TreeNode owner, ITreeBlobRef persistedNode)
		{
			_owner = owner;
			_target = persistedNode;
		}

		/// <summary>
		/// Creates a reference to the given node
		/// </summary>
		/// <param name="owner">The node which owns the reference</param>
		/// <param name="node">The referenced node</param>
		public TreeNodeRef(TreeNode owner, TreeNode node)
		{
			_owner = owner;
			_strongRef = node;
			_lastModifiedTime = Stopwatch.GetTimestamp();

			node.IncomingRef = this;
		}

		/// <summary>
		/// Reparent this reference to a new owner.
		/// </summary>
		public void Reparent(TreeNode newOwner)
		{
			MarkAsDirty();
			_owner = newOwner;
			MarkAsDirty();
		}

		/// <summary>
		/// Figure out whether this ref is dirty
		/// </summary>
		/// <returns></returns>
		public bool IsDirty() => _lastModifiedTime != 0;

		/// <summary>
		/// Converts the node in this reference from a weak to strong reference.
		/// </summary>
		/// <returns>True if the ref contains a valid node on return</returns>
		protected TreeNode? ConvertToStrongRef()
		{
			// Check if it's already a strong ref
			if (_strongRef != null)
			{
				return _strongRef;
			}

			// Otherwise check if it's a weak ref that hasn't been GC'd yet
			if (_weakRef != null && _weakRef.TryGetTarget(out _strongRef))
			{
				_weakRef = null;
				return _strongRef;
			}

			return null;
		}

		/// <summary>
		/// Marks this reference as dirty
		/// </summary>
		internal void MarkAsDirty()
		{
			if (_lastModifiedTime == 0)
			{
				_target = null;

				_lastModifiedTime = Stopwatch.GetTimestamp();
				if (_strongRef == null)
				{
					if (_weakRef == null || !_weakRef.TryGetTarget(out _strongRef))
					{
						throw new InvalidOperationException("Unable to resolve weak reference to node");
					}
					_weakRef = null;
				}
				_owner?.IncomingRef?.MarkAsDirty();
			}
		}

		/// <summary>
		/// Marks this reference as clean
		/// </summary>
		internal void MarkAsClean(ITreeBlobRef persistedNode)
		{
			_target = persistedNode;

			if (_strongRef != null)
			{
				OnCollapse();

				_weakRef = new WeakReference<TreeNode>(_strongRef);
				_strongRef = null;
			}

			_lastModifiedTime = 0;
		}

		/// <summary>
		/// Collapse this reference and write its node to storage
		/// </summary>
		/// <param name="writer">Writer to output the node to</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async ValueTask<ITreeBlobRef> CollapseAsync(ITreeWriter writer, CancellationToken cancellationToken)
		{
			ITreeBlobRef? target = _target;
			if (target == null)
			{
				ITreeBlob blob = await Node!.SerializeAsync(writer, cancellationToken);
				target = await writer.WriteNodeAsync(blob.Data, blob.Refs, cancellationToken);
				MarkAsClean(target);
			}
			return target;
		}

		/// <summary>
		/// Callback for when a reference is collapsed to a hash value. At the time of calling, both the hash and node data will be valid.
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
		static readonly TreeNodeSerializer<T> Serializer = CreateSerializer();

		/// <inheritdoc cref="TreeNodeRef.Node"/>
		public new T? Node
		{
			get => (T?)base.Node;
			set => base.Node = value;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="owner">The node which owns the reference</param>
		/// <param name="target">Hash of the referenced node</param>
		public TreeNodeRef(TreeNode owner, ITreeBlobRef target) : base(owner, target)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="owner">The node which owns the reference</param>
		/// <param name="node">The referenced node</param>
		public TreeNodeRef(TreeNode owner, T node) : base(owner, node)
		{
		}

		/// <summary>
		/// Create an instance of the serializer for this node type
		/// </summary>
		/// <returns>New serializer instance</returns>
		static TreeNodeSerializer<T> CreateSerializer()
		{
			TreeSerializerAttribute? attribute = typeof(T).GetCustomAttribute<TreeSerializerAttribute>();
			if (attribute == null)
			{
				throw new InvalidOperationException($"No serializer is defined for {typeof(T).Name}");
			}
			return (TreeNodeSerializer<T>)Activator.CreateInstance(attribute.Type)!;
		}

		/// <summary>
		/// Resolve this reference to a concrete node
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async ValueTask<T> ExpandAsync(CancellationToken cancellationToken = default)
		{
			T? result = (T?)ConvertToStrongRef();
			if (result == null)
			{
				Debug.Assert(_target != null);
				ITreeBlob node = await _target.GetTargetAsync(cancellationToken);

				_strongRef = Serializer.Deserialize(node);
				_strongRef.IncomingRef = this;

				result = (T)_strongRef;
			}
			return result;
		}
	}
}


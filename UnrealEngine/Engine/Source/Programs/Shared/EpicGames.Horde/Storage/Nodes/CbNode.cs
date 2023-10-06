// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Metadata;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// A node containing arbitrary compact binary data
	/// </summary>
	[NodeType("{34A0793F-8364-42F4-8632-98A71C843229}", 1)]
	public class CbNode : Node
	{
		class HandleMapper
		{
			readonly NodeReader _reader;
			readonly List<NodeRef> _refs;

			public HandleMapper(NodeReader reader, List<NodeRef> refs)
			{
				_reader = reader;
				_refs = refs;
			}

			public void IterateField(CbField field)
			{
				if (field.IsAttachment())
				{
					BlobHandle handle = _reader.GetNodeHandle(_refs.Count, field.AsAttachment());
					_refs.Add(new NodeRef(handle));
				}
				else if (field.IsArray())
				{
					CbArray array = field.AsArray();
					array.IterateAttachments(IterateField);
				}
				else if (field.IsObject())
				{
					CbObject obj = field.AsObject();
					obj.IterateAttachments(IterateField);
				}
			}
		}

		/// <summary>
		/// The compact binary object
		/// </summary>
		public CbObject Object { get; set; }

		/// <summary>
		/// Imported nodes
		/// </summary>
		public IReadOnlyList<NodeRef> References { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="obj">The compact binary object</param>
		/// <param name="references">List of references to attachments</param>
		public CbNode(CbObject obj, IReadOnlyList<NodeRef> references)
		{
			Object = obj;
			References = references;
		}

		/// <summary>
		/// Deserialization constructor
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		public CbNode(NodeReader reader)
		{
			Object = new CbObject(reader.ReadFixedLengthBytes(reader.Length));

			List<NodeRef> references = new List<NodeRef>();
			Object.IterateAttachments(new HandleMapper(reader, references).IterateField);

			References = references;
		}

		/// <inheritdoc/>
		public override void Serialize(NodeWriter writer)
		{
			writer.WriteFixedLengthBytes(Object.GetView().Span);
		}
	}
}

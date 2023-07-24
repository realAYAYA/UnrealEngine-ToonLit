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
	[TreeNode("{34A0793F-8364-42F4-8632-98A71C843229}", 1)]
	public class CbNode : TreeNode
	{
		class HandleMapper
		{
			public IReadOnlyList<NodeLocator> Locators { get; }
			public List<NodeHandle> Handles { get; }

			public HandleMapper(IReadOnlyList<NodeLocator> locators)
			{
				Locators = locators;
				Handles = new List<NodeHandle>(locators.Count);
			}

			public void IterateField(CbField field)
			{
				if (field.IsAttachment())
				{
					NodeLocator locator = Locators[Handles.Count];
					Handles.Add(new NodeHandle(field.AsAttachment(), locator));
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
		public IReadOnlyList<TreeNodeRef> References { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="obj">The compact binary object</param>
		/// <param name="references">List of references to attachments</param>
		public CbNode(CbObject obj, IReadOnlyList<TreeNodeRef> references)
		{
			Object = obj;
			References = references;
		}

		/// <summary>
		/// Deserialization constructor
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		public CbNode(ITreeNodeReader reader)
		{
			Object = new CbObject(reader.ReadFixedLengthBytes(reader.Length));

			HandleMapper mapper = new HandleMapper(reader.References);
			Object.IterateAttachments(mapper.IterateField);

			References = mapper.Handles.ConvertAll(x => new TreeNodeRef(x));
		}

		/// <inheritdoc/>
		public override void Serialize(ITreeNodeWriter writer)
		{
			writer.WriteFixedLengthBytes(Object.GetView().Span);
		}

		/// <inheritdoc/>
		public override IEnumerable<TreeNodeRef> EnumerateRefs() => References;
	}
}

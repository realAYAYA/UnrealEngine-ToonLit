// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using EpicGames.Core;
using EpicGames.Serialization;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// A node containing arbitrary compact binary data
	/// </summary>
	[BlobConverter(typeof(CbNodeConverter))]
	public class CbNode
	{
		/// <summary>
		/// Static accessor for the blob type guid
		/// </summary>
		public static Guid BlobTypeGuid { get; } = new Guid("{34A0793F-42F4-8364-A798-32862932841C}");

		/// <summary>
		/// The compact binary object
		/// </summary>
		public CbObject Object { get; set; }

		/// <summary>
		/// Imported nodes
		/// </summary>
		public IReadOnlyList<IBlobHandle> Imports { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="obj">The compact binary object</param>
		/// <param name="imports">List of imports for attachments</param>
		public CbNode(CbObject obj, IReadOnlyList<IBlobHandle> imports)
		{
			Object = obj;
			Imports = imports;
		}
	}

	class CbNodeConverter : BlobConverter<CbNode>
	{
		public static BlobType BlobType { get; } = new BlobType(CbNode.BlobTypeGuid, 1);

		/// <inheritdoc/>
		public override CbNode Read(IBlobReader reader, BlobSerializerOptions options)
		{
			CbObject obj = new CbObject(reader.GetMemory().ToArray());
			return new CbNode(obj, reader.Imports.ToArray());
		}

		/// <inheritdoc/>
		public override BlobType Write(IBlobWriter writer, CbNode value, BlobSerializerOptions options)
		{
			writer.WriteFixedLengthBytes(value.Object.GetView().Span);
			return BlobType;
		}
	}
}

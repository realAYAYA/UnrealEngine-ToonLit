// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Identifier for a workspace layer
	/// </summary>
	/// <param name="Id">Identifier for the layer</param>
	[LogValueType]
	[TypeConverter(typeof(StringIdTypeConverter<WorkspaceLayerId, WorkspaceLayerIdConverter>))]
	[BinaryIdConverter(typeof(WorkspaceLayerIdConverter))]
	public record struct WorkspaceLayerId(StringId Id)
	{
		/// <summary>
		/// Name of the default layer
		/// </summary>
		public static WorkspaceLayerId Default { get; } = new WorkspaceLayerId(new StringId("default"));

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter class to and from ObjectId values
	/// </summary>
	class WorkspaceLayerIdConverter : StringIdConverter<WorkspaceLayerId>
	{
		/// <inheritdoc/>
		public override WorkspaceLayerId FromStringId(StringId id) => new WorkspaceLayerId(id);

		/// <inheritdoc/>
		public override StringId ToStringId(WorkspaceLayerId value) => value.Id;
	}
}

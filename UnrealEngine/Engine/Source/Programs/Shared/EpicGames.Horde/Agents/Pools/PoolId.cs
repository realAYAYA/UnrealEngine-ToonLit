// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using EpicGames.Serialization;

namespace EpicGames.Horde.Agents.Pools
{
	/// <summary>
	/// Identifier for a pool
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[LogValueType]
	[JsonSchemaString]
	[TypeConverter(typeof(StringIdTypeConverter<PoolId, PoolIdConverter>))]
	[StringIdConverter(typeof(PoolIdConverter))]
	[CbConverter(typeof(StringIdCbConverter<PoolId, PoolIdConverter>))]
	public record struct PoolId(StringId Id)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public PoolId(string id) : this(new StringId(id))
		{
		}

		/// <inheritdoc cref="StringId.IsEmpty"/>
		public bool IsEmpty => Id.IsEmpty;

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="StringId"/> instances.
	/// </summary>
	class PoolIdConverter : StringIdConverter<PoolId>
	{
		/// <inheritdoc/>
		public override PoolId FromStringId(StringId id) => new PoolId(id);

		/// <inheritdoc/>
		public override StringId ToStringId(PoolId value) => value.Id;
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using EpicGames.Serialization;

namespace EpicGames.Horde.Devices
{
	/// <summary>
	/// Identifier for a pool
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[LogValueType]
	[JsonSchemaString]
	[TypeConverter(typeof(StringIdTypeConverter<DeviceId, DeviceIdConverter>))]
	[StringIdConverter(typeof(DeviceIdConverter))]
	[CbConverter(typeof(StringIdCbConverter<DeviceId, DeviceIdConverter>))]
	public record struct DeviceId(StringId Id)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public DeviceId(string id) : this(new StringId(id))
		{
		}

		/// <inheritdoc cref="StringId.IsEmpty"/>
		public bool IsEmpty => Id.IsEmpty;

		/// <inheritdoc cref="StringId.Sanitize(System.String)"/>
		public static DeviceId Sanitize(string text) => new DeviceId(StringId.Sanitize(text));

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="StringId"/> instances.
	/// </summary>
	class DeviceIdConverter : StringIdConverter<DeviceId>
	{
		/// <inheritdoc/>
		public override DeviceId FromStringId(StringId id) => new DeviceId(id);

		/// <inheritdoc/>
		public override StringId ToStringId(DeviceId value) => value.Id;
	}
}

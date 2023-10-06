// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Identifies the type of a blob
	/// </summary>
	/// <param name="Guid">Nominal identifier for the type</param>
	/// <param name="Version">Version number for the serializer</param>
	public record struct BlobType(Guid Guid, int Version)
	{
		/// <inheritdoc/>
		public override string ToString() => $"{Guid}#{Version}";
	}
}

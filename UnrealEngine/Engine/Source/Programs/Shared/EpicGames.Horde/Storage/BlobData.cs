// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Data for an individual node
	/// </summary>
	public record struct BlobData(BlobType Type, IoHash Hash, ReadOnlyMemory<byte> Data, IReadOnlyList<BlobHandle> Refs);
}

// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde;
using MongoDB.Bson;

namespace Horde.Server.Utilities
{
	/// <summary>
	/// Utility methods for <see cref="BinaryId"/>
	/// </summary>
	static class BinaryIdUtils
	{
		/// <summary>
		/// Creates a new BinaryId
		/// </summary>
		public static BinaryId CreateNew()
		{
			ObjectId objectId = ObjectId.GenerateNewId();
			return new BinaryId(objectId.ToByteArray());
		}

		/// <summary>
		/// Creates a BinaryId from an ObjectId
		/// </summary>
		public static BinaryId FromObjectId(ObjectId objectId)
		{
			return new BinaryId(objectId.ToByteArray());
		}

		/// <summary>
		/// Creates an ObjectId from a BinaryId
		/// </summary>
		public static ObjectId ToObjectId(BinaryId binaryId)
		{
			return new ObjectId(binaryId.ToByteArray());
		}
	}
}

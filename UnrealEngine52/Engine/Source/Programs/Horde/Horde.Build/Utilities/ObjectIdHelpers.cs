// Copyright Epic Games, Inc. All Rights Reserved.

using MongoDB.Bson;

namespace Horde.Build.Utilities
{
	static class ObjectIdHelpers
	{
		public static ObjectId ToObjectId(this string text)
		{
			if (text.Length == 0)
			{
				return ObjectId.Empty;
			}
			else
			{
				return ObjectId.Parse(text);
			}
		}

		public static ObjectId<T> ToObjectId<T>(this string text)
		{
			return new ObjectId<T>(text.ToObjectId());
		}
	}
}

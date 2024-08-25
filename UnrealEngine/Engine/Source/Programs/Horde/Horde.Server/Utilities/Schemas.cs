// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using EpicGames.Core;

namespace Horde.Server.Utilities
{
	/// <summary>
	/// Utility functions for generating schemas
	/// </summary>
	static class Schemas
	{
		static readonly XmlDocReader s_cachedReader = new XmlDocReader();
		static readonly ConcurrentDictionary<Type, JsonSchema> s_cachedSchemas = new ConcurrentDictionary<Type, JsonSchema>();

		/// <summary>
		/// Create a Json schema (or retrieve a cached schema)
		/// </summary>
		/// <param name="type"></param>
		/// <returns></returns>
		public static JsonSchema CreateSchema(Type type)
		{
			JsonSchema? schema;
			if (!s_cachedSchemas.TryGetValue(type, out schema))
			{
				schema = JsonSchema.FromType(type, s_cachedReader);
				s_cachedSchemas.TryAdd(type, schema);
			}
			return schema;
		}
	}
}

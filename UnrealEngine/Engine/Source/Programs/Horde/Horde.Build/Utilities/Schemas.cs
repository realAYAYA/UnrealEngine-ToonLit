// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Reflection;
using System.Xml;
using EpicGames.Core;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Utility functions for generating schemas
	/// </summary>
	static class Schemas
	{
		static readonly ConcurrentDictionary<Assembly, XmlDocument?> s_cachedDocumentation = new ConcurrentDictionary<Assembly, XmlDocument?>();
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
				XmlDocument? documentation;
				if (!s_cachedDocumentation.TryGetValue(type.Assembly, out documentation))
				{
					FileReference inputDocumentationFile = new FileReference(type.Assembly.Location).ChangeExtension(".xml");
					if (FileReference.Exists(inputDocumentationFile))
					{
						documentation = new XmlDocument();
						documentation.Load(inputDocumentationFile.FullName);
					}
					s_cachedDocumentation.TryAdd(type.Assembly, documentation);
				}

				schema = JsonSchema.FromType(type, documentation);
				s_cachedSchemas.TryAdd(type, schema);
			}
			return schema;
		}
	}
}

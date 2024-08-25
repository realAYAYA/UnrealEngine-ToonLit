// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace EpicGames.Core
{
	/// <summary>
	/// Interface used to write JSON schema types. This is abstracted to allow multiple passes over the document structure, in order to optimize multiple references to the same type definition.
	/// </summary>
	public interface IJsonSchemaWriter
	{
		/// <inheritdoc cref="Utf8JsonWriter.WriteStartObject()"/>
		void WriteStartObject();

		/// <inheritdoc cref="Utf8JsonWriter.WriteStartObject(String)"/>
		void WriteStartObject(string name);

		/// <inheritdoc cref="Utf8JsonWriter.WriteEndObject()"/>
		void WriteEndObject();

		/// <inheritdoc cref="Utf8JsonWriter.WriteStartArray()"/>
		void WriteStartArray();

		/// <inheritdoc cref="Utf8JsonWriter.WriteStartArray(String)"/>
		void WriteStartArray(string name);

		/// <inheritdoc cref="Utf8JsonWriter.WriteEndObject()"/>
		void WriteEndArray();

		/// <inheritdoc cref="Utf8JsonWriter.WriteBoolean(String, Boolean)"/>
		void WriteBoolean(string name, bool value);

		/// <inheritdoc cref="Utf8JsonWriter.WriteString(String, String)"/>
		void WriteString(string key, string value);

		/// <inheritdoc cref="Utf8JsonWriter.WriteStringValue(String)"/>
		void WriteStringValue(string name);

		/// <summary>
		/// Serialize a type to JSON
		/// </summary>
		/// <param name="type"></param>
		void WriteType(JsonSchemaType type);
	}

	/// <summary>
	/// Implementation of a JSON schema. Implements draft 04 (latest supported by Visual Studio 2019).
	/// </summary>
	public class JsonSchema
	{
		class JsonTypeRefCollector : IJsonSchemaWriter
		{
			/// <summary>
			/// Reference counts for each type (max of 2)
			/// </summary>
			public Dictionary<JsonSchemaType, int> TypeRefCount { get; } = new Dictionary<JsonSchemaType, int>();

			/// <inheritdoc/>
			public void WriteBoolean(string name, bool value) { }

			/// <inheritdoc/>
			public void WriteString(string key, string value) { }

			/// <inheritdoc/>
			public void WriteStringValue(string name) { }

			/// <inheritdoc/>
			public void WriteStartObject() { }

			/// <inheritdoc/>
			public void WriteStartObject(string name) { }

			/// <inheritdoc/>
			public void WriteEndObject() { }

			/// <inheritdoc/>
			public void WriteStartArray() { }

			/// <inheritdoc/>
			public void WriteStartArray(string name) { }

			/// <inheritdoc/>
			public void WriteEndArray() { }

			/// <inheritdoc/>
			public void WriteType(JsonSchemaType type)
			{
				if (!(type is JsonSchemaPrimitiveType))
				{
					TypeRefCount.TryGetValue(type, out int refCount);
					if (refCount < 2)
					{
						TypeRefCount[type] = ++refCount;
					}
					if (refCount < 2)
					{
						type.Write(this);
					}
				}
			}
		}

		/// <summary>
		/// Implementation of <see cref="IJsonSchemaWriter"/>
		/// </summary>
		class JsonSchemaWriter : IJsonSchemaWriter
		{
			/// <summary>
			/// Raw Json output
			/// </summary>
			readonly Utf8JsonWriter _jsonWriter;

			/// <summary>
			/// Mapping of type to definition name
			/// </summary>
			readonly Dictionary<JsonSchemaType, string> _typeToDefinition;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="writer"></param>
			/// <param name="typeToDefinition"></param>
			public JsonSchemaWriter(Utf8JsonWriter writer, Dictionary<JsonSchemaType, string> typeToDefinition)
			{
				_jsonWriter = writer;
				_typeToDefinition = typeToDefinition;
			}

			/// <inheritdoc/>
			public void WriteBoolean(string name, bool value) => _jsonWriter.WriteBoolean(name, value);

			/// <inheritdoc/>
			public void WriteString(string key, string value) => _jsonWriter.WriteString(key, value);

			/// <inheritdoc/>
			public void WriteStringValue(string name) => _jsonWriter.WriteStringValue(name);

			/// <inheritdoc/>
			public void WriteStartObject() => _jsonWriter.WriteStartObject();

			/// <inheritdoc/>
			public void WriteStartObject(string name) => _jsonWriter.WriteStartObject(name);

			/// <inheritdoc/>
			public void WriteEndObject() => _jsonWriter.WriteEndObject();

			/// <inheritdoc/>
			public void WriteStartArray() => _jsonWriter.WriteStartArray();

			/// <inheritdoc/>
			public void WriteStartArray(string name) => _jsonWriter.WriteStartArray(name);

			/// <inheritdoc/>
			public void WriteEndArray() => _jsonWriter.WriteEndArray();

			/// <summary>
			/// Writes a type, either inline or as a reference to a definition elsewhere
			/// </summary>
			/// <param name="type"></param>
			public void WriteType(JsonSchemaType type)
			{
				if (_typeToDefinition.TryGetValue(type, out string? definition))
				{
					_jsonWriter.WriteString("$ref", $"#/definitions/{definition}");
				}
				else
				{
					type.Write(this);
				}
			}
		}

		/// <summary>
		/// Identifier for the schema
		/// </summary>
		public string? Id { get; set; }

		/// <summary>
		/// The root schema type
		/// </summary>
		public JsonSchemaType RootType { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Id for the schema</param>
		/// <param name="rootType"></param>
		public JsonSchema(string? id, JsonSchemaType rootType)
		{
			Id = id;
			RootType = rootType;
		}

		/// <summary>
		/// Write this schema to a byte array
		/// </summary>
		/// <param name="writer"></param>
		public void Write(Utf8JsonWriter writer)
		{
			// Determine reference counts for each type. Any type referenced at least twice will be split off into a separate definition.
			JsonTypeRefCollector refCollector = new JsonTypeRefCollector();
			refCollector.WriteType(RootType);

			// Assign names to each type definition
			HashSet<string> definitionNames = new HashSet<string>();
			Dictionary<JsonSchemaType, string> typeToDefinition = new Dictionary<JsonSchemaType, string>();
			foreach ((JsonSchemaType type, int refCount) in refCollector.TypeRefCount)
			{
				if (refCount > 1)
				{
					string baseName = type.Name ?? "unnamed";

					string name = baseName;
					for (int idx = 1; !definitionNames.Add(name); idx++)
					{
						name = $"{baseName}{idx}";
					}

					typeToDefinition[type] = name;
				}
			}

			// Write the schema
			writer.WriteStartObject();
			writer.WriteString("$schema", "http://json-schema.org/draft-04/schema#");
			if (Id != null)
			{
				writer.WriteString("$id", Id);
			}

			JsonSchemaWriter schemaWriter = new JsonSchemaWriter(writer, typeToDefinition);
			RootType.Write(schemaWriter);

			if (typeToDefinition.Count > 0)
			{
				writer.WriteStartObject("definitions");
				foreach ((JsonSchemaType type, string refName) in typeToDefinition)
				{
					writer.WriteStartObject(refName);
					type.Write(schemaWriter);
					writer.WriteEndObject();
				}
				writer.WriteEndObject();
			}

			writer.WriteEndObject();
		}

		/// <summary>
		/// Write this schema to a stream
		/// </summary>
		/// <param name="stream">The output stream</param>
		public void Write(Stream stream)
		{
			using (Utf8JsonWriter writer = new Utf8JsonWriter(stream, new JsonWriterOptions { Indented = true }))
			{
				Write(writer);
			}
		}

		/// <summary>
		/// Writes this schema to a file
		/// </summary>
		/// <param name="file">The output file</param>
		public void Write(FileReference file)
		{
			using (FileStream stream = FileReference.Open(file, FileMode.Create))
			{
				Write(stream);
			}
		}

		/// <summary>
		/// Constructs a Json schema from a type
		/// </summary>
		/// <param name="type">The type to construct from</param>
		/// <param name="xmlDocReader">Reads documentation for requested types</param>
		/// <returns>New schema object</returns>
		public static JsonSchema FromType(Type type, XmlDocReader? xmlDocReader)
		{
			JsonSchemaAttribute? schemaAttribute = type.GetCustomAttribute<JsonSchemaAttribute>();
			return new JsonSchema(schemaAttribute?.Id, CreateSchemaType(type, new Dictionary<Type, JsonSchemaType>(), xmlDocReader));
		}

		/// <summary>
		/// Create a Json schema (or retrieve a cached schema)
		/// </summary>
		public static JsonSchema CreateSchema(Type type, XmlDocReader? xmlDocReader)
		{
			JsonSchemaAttribute? schemaAttribute = type.GetCustomAttribute<JsonSchemaAttribute>();
			return new JsonSchema(schemaAttribute?.Id, CreateSchemaType(type, new Dictionary<Type, JsonSchemaType>(), xmlDocReader));
		}

		/// <summary>
		/// Constructs a schema type for the given property
		/// </summary>
		static JsonSchemaType CreateSchemaType(PropertyInfo property, Dictionary<Type, JsonSchemaType> typeCache, XmlDocReader? xmlDocReader)
		{
			if (property.GetCustomAttribute<JsonSchemaStringAttribute>() != null)
			{
				return new JsonSchemaString();
			}

			return CreateSchemaType(property.PropertyType, typeCache, xmlDocReader);
		}

		/// <summary>
		/// Constructs a schema type from the given type object
		/// </summary>
		static JsonSchemaType CreateSchemaType(Type type, Dictionary<Type, JsonSchemaType> typeCache, XmlDocReader? xmlDocReader)
		{
			switch (Type.GetTypeCode(type))
			{
				case TypeCode.Boolean:
					return new JsonSchemaBoolean();
				case TypeCode.Byte:
				case TypeCode.SByte:
				case TypeCode.Int16:
				case TypeCode.UInt16:
				case TypeCode.Int32:
				case TypeCode.UInt32:
				case TypeCode.Int64:
				case TypeCode.UInt64:
					if (type.IsEnum)
					{
						return CreateEnumSchemaType(type, xmlDocReader);
					}
					else
					{
						return new JsonSchemaInteger();
					}
				case TypeCode.Single:
				case TypeCode.Double:
					return new JsonSchemaNumber();
				case TypeCode.String:
					return new JsonSchemaString();
			}

			JsonSchemaTypeAttribute? attribute = type.GetCustomAttribute<JsonSchemaTypeAttribute>();
			switch (attribute)
			{
				case JsonSchemaStringAttribute str:
					return new JsonSchemaString(str.Format);
			}

			if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Nullable<>))
			{
				return CreateSchemaType(type.GetGenericArguments()[0], typeCache, xmlDocReader);
			}
			if (type == typeof(DateTime) || type == typeof(DateTimeOffset))
			{
				return new JsonSchemaString(JsonSchemaStringFormat.DateTime);
			}
			if (type == typeof(TimeSpan))
			{
				return new JsonSchemaString();
			}
			if (type == typeof(Uri))
			{
				return new JsonSchemaString(JsonSchemaStringFormat.Uri);
			}
			if (type == typeof(System.Text.Json.Nodes.JsonObject))
			{
				return new JsonSchemaObject();
			}

			Type[] interfaceTypes = type.GetInterfaces();
			foreach (Type interfaceType in interfaceTypes)
			{
				if (interfaceType.IsGenericType)
				{
					Type[] arguments = interfaceType.GetGenericArguments();
					if (interfaceType.GetGenericTypeDefinition() == typeof(IList<>))
					{
						return new JsonSchemaArray(CreateSchemaType(arguments[0], typeCache, xmlDocReader));
					}
					if (interfaceType.GetGenericTypeDefinition() == typeof(IDictionary<,>))
					{
						JsonSchemaObject obj = new JsonSchemaObject();
						obj.AdditionalProperties = CreateSchemaType(arguments[1], typeCache, xmlDocReader);
						return obj;
					}
				}
			}

			if (type.IsClass)
			{
				if (typeCache.TryGetValue(type, out JsonSchemaType? schemaType))
				{
					return schemaType;
				}

				JsonKnownTypesAttribute? knownTypes = type.GetCustomAttribute<JsonKnownTypesAttribute>(false);
				if (knownTypes != null)
				{
					JsonSchemaOneOf obj = new JsonSchemaOneOf();
					typeCache[type] = obj;
					SetOneOfProperties(obj, type, knownTypes.Types, typeCache, xmlDocReader);
					return obj;
				}
				else
				{
					JsonSchemaObject obj = new JsonSchemaObject();
					typeCache[type] = obj;
					SetObjectProperties(obj, type, typeCache, xmlDocReader);
					return obj;
				}
			}

			throw new Exception($"Unknown type for schema generation: {type}");
		}

		static JsonSchemaEnum CreateEnumSchemaType(Type type, XmlDocReader? xmlDocReader)
		{
			string[] names = Enum.GetNames(type);
			string[] descriptions = new string[names.Length];

			for (int idx = 0; idx < names.Length; idx++)
			{
				descriptions[idx] = xmlDocReader?.GetDescription(type, names[idx]) ?? String.Empty;
			}

			string? enumDescription = xmlDocReader?.GetDescription(type);
			return new JsonSchemaEnum(names, descriptions) { Name = type.Name, Description = enumDescription };
		}

		static void SetOneOfProperties(JsonSchemaOneOf obj, Type type, Type[] knownTypes, Dictionary<Type, JsonSchemaType> typeCache, XmlDocReader? xmlDocReader)
		{
			obj.Name = type.Name;

			foreach (Type knownType in knownTypes)
			{
				JsonDiscriminatorAttribute? attribute = knownType.GetCustomAttribute<JsonDiscriminatorAttribute>();
				if (attribute != null)
				{
					JsonSchemaObject knownObject = new JsonSchemaObject();
					knownObject.Properties.Add(new JsonSchemaProperty("type", "Type discriminator", new JsonSchemaEnum(new[] { attribute.Name }, new[] { "Identifier for the derived type" })));
					SetObjectProperties(knownObject, knownType, typeCache, xmlDocReader);
					obj.Types.Add(knownObject);
				}
			}
		}

		static void SetObjectProperties(JsonSchemaObject obj, Type type, Dictionary<Type, JsonSchemaType> typeCache, XmlDocReader? xmlDocReader)
		{
			obj.Name = type.Name;
			obj.Description = xmlDocReader?.GetDescription(type);

			PropertyInfo[] properties = type.GetProperties(BindingFlags.Instance | BindingFlags.Public);
			foreach (PropertyInfo property in properties)
			{
				if (property.GetCustomAttribute<JsonIgnoreAttribute>() == null)
				{
					string? description = xmlDocReader?.GetDescription(property);
					JsonSchemaType propertyType = CreateSchemaType(property, typeCache, xmlDocReader);
					obj.Properties.Add(new JsonSchemaProperty(property.Name, description, propertyType));
				}
			}
		}
	}
}

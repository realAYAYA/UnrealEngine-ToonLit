// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

using JsonObject = System.Text.Json.Nodes.JsonObject;

namespace Horde.Server.Configuration
{
	/// <summary>
	/// Attribute used to mark <see cref="Uri"/> properties that include other config files
	/// </summary>
	[AttributeUsage(AttributeTargets.Property)]
	public sealed class ConfigIncludeAttribute : Attribute
	{
	}

	/// <summary>
	/// Specifies that a class is the root for including other files
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class ConfigIncludeRootAttribute : Attribute
	{
	}

	/// <summary>
	/// Captures the current scope of outer objects in the current variable
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class ConfigIncludeContextAttribute : Attribute
	{
	}

	/// <summary>
	/// Attribute used to mark <see cref="Uri"/> properties that are relative to their containing file
	/// </summary>
	[AttributeUsage(AttributeTargets.Property)]
	public sealed class ConfigRelativePathAttribute : Attribute
	{
	}

	/// <summary>
	/// Exception thrown when reading config files
	/// </summary>
	public sealed class ConfigException : Exception
	{
		readonly ConfigContext _context;

		/// <summary>
		/// Stack of properties
		/// </summary>
		public IEnumerable<string> ScopeStack => _context.ScopeStack;

		/// <summary>
		/// Stack of objects
		/// </summary>
		public IEnumerable<object> IncludeContextStack => _context.IncludeContextStack;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="context">Current parse context for the error</param>
		/// <param name="message">Description of the error</param>
		/// <param name="innerException">Inner exception details</param>
		internal ConfigException(ConfigContext context, string message, Exception? innerException = null)
			: base(message, innerException)
		{
			_context = context;
		}

		/// <summary>
		/// Gets the parser context when this exception was thrown. This is not exposed as a public property to avoid serializing the whole thing to Serilog.
		/// </summary>
		internal ConfigContext GetContext() => _context;
	}

	/// <summary>
	/// Base class for types that can be read from config files
	/// </summary>
	abstract class ConfigType
	{
		public abstract ValueTask<object?> ReadAsync(JsonNode? node, ConfigContext context, CancellationToken cancellationToken);

		static readonly ConcurrentDictionary<Type, ConfigType> s_typeToValueType = new ConcurrentDictionary<Type, ConfigType>();

		public static ConfigType FindOrAddValueType(Type type)
		{
			ConfigType? value;
			if (!s_typeToValueType.TryGetValue(type, out value))
			{
				if (!type.IsClass || type == typeof(string))
				{
					value = new ScalarConfigType(type);
				}
				else
				{
					value = ClassConfigType.FindOrAdd(type);
				}

				lock (s_typeToValueType)
				{
					value = s_typeToValueType.GetOrAdd(type, value);
				}
			}
			return value;
		}

		/// <summary>
		/// Reads an object from a particular URL
		/// </summary>
		/// <typeparam name="T">Type of object to read</typeparam>
		/// <param name="uri">Location of the file to read</param>
		/// <param name="context">Context for reading</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public static async Task<T> ReadAsync<T>(Uri uri, ConfigContext context, CancellationToken cancellationToken) where T : class, new()
		{
			ClassConfigType type = ClassConfigType.FindOrAdd(typeof(T));

			T target = new T();
			await type.MergeObjectAsync(target, uri, context, cancellationToken);
			return target;
		}

		/// <summary>
		/// Combine a relative path with a base URI to produce a new URI
		/// </summary>
		/// <param name="baseUri">Base uri to rebase relative to</param>
		/// <param name="path">Relative path</param>
		/// <returns>Absolute URI</returns>
		public static Uri CombinePaths(Uri baseUri, string path)
		{
			if (path.StartsWith("//", StringComparison.Ordinal))
			{
				if (baseUri.Scheme == PerforceConfigSource.Scheme)
				{
					return new Uri($"{PerforceConfigSource.Scheme}://{baseUri.Host}{path}");
				}
				else
				{
					return new Uri($"{PerforceConfigSource.Scheme}://{PerforceConnectionSettings.Default}{path}");
				}
			}
			return new Uri(baseUri, path);
		}
	}

	/// <summary>
	/// Implementation of <see cref="ConfigType"/> for scalar types
	/// </summary>
	class ScalarConfigType : ConfigType
	{
		readonly Type _type;

		public ScalarConfigType(Type type)
		{
			if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Nullable<>))
			{
				type = type.GetGenericArguments()[0];
			}
			_type = type;
		}

		public override ValueTask<object?> ReadAsync(JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
		{
			if (node == null)
			{
				return new ValueTask<object?>((object?)null);
			}
			else
			{
				return new ValueTask<object?>(node.Deserialize(_type, context.JsonOptions));
			}
		}
	}

	/// <summary>
	/// Implementation of <see cref="ConfigType"/> to handle class types
	/// </summary>
	class ClassConfigType : ConfigType
	{
		abstract class Property
		{
			public string Name { get; }
			public PropertyInfo PropertyInfo { get; }

			public Property(string name, PropertyInfo propertyInfo)
			{
				Name = name;
				PropertyInfo = propertyInfo;
			}

			public abstract bool HasIncludes();

			public abstract Task MergeAsync(object target, JsonNode? node, ConfigContext context, CancellationToken cancellationToken);

			public abstract Task ParseIncludesAsync(JsonNode jsonNode, object targetObject, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken);
		}

		class ScalarProperty : Property
		{
			public ScalarProperty(string name, PropertyInfo propertyInfo)
				: base(name, propertyInfo)
			{
			}

			public override bool HasIncludes() => PropertyInfo.GetCustomAttribute<ConfigIncludeAttribute>() != null;

			public override Task MergeAsync(object target, JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
			{
				context.AddProperty(Name);

				object? value;
				if (PropertyInfo.GetCustomAttribute<ConfigRelativePathAttribute>() != null)
				{
					value = CombinePaths(context.CurrentFile, JsonSerializer.Deserialize<string>(node, context.JsonOptions) ?? String.Empty).AbsoluteUri;
				}
				else
				{
					value = JsonSerializer.Deserialize(node, PropertyInfo.PropertyType, context.JsonOptions);
				}

				if (!PropertyInfo.CanWrite)
				{
					throw new ConfigException(context, $"Property {context.CurrentScope}.{Name} does not have a setter.");
				}

				PropertyInfo.SetValue(target, value);

				return Task.CompletedTask;
			}

			public override async Task ParseIncludesAsync(JsonNode jsonNode, object targetObject, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
			{
				string? path = (string?)jsonNode;

				Uri uri = ConfigType.CombinePaths(context.CurrentFile, path!);
				IConfigFile file = await ReadFileAsync(uri, context, cancellationToken);

				context.IncludeStack.Push(file);

				JsonObject includedJsonObject = await ParseFileAsync(file, context, cancellationToken);
				await targetType.MergeObjectAsync(targetObject, includedJsonObject, context, cancellationToken);

				context.IncludeStack.Pop();
			}
		}

		class ResourceProperty : ScalarProperty
		{
			public ResourceProperty(string name, PropertyInfo propertyInfo)
				: base(name, propertyInfo)
			{
			}

			public override async Task MergeAsync(object target, JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
			{
				context.AddProperty(Name);

				if (!PropertyInfo.CanWrite)
				{
					throw new ConfigException(context, $"Property {context.CurrentScope}{Name} does not have a setter.");
				}

				Uri uri = CombinePaths(context.CurrentFile, JsonSerializer.Deserialize<string>(node, context.JsonOptions) ?? String.Empty);
				IConfigFile file = await ReadFileAsync(uri, context, cancellationToken);

				ConfigResource resource = new ConfigResource();
				resource.Path = uri.AbsoluteUri;
				resource.Data = await file.ReadAsync(cancellationToken);
				PropertyInfo.SetValue(target, resource);
			}
		}

		class ListProperty : Property
		{
			readonly ConfigType _elementType;

			public ListProperty(string name, PropertyInfo propertyInfo, ConfigType elementType)
				: base(name, propertyInfo)
			{
				_elementType = elementType;
			}

			public override bool HasIncludes() => _elementType is ClassConfigType elementType && elementType.HasIncludes();

			public override async Task MergeAsync(object target, JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
			{
				IList? list = (IList?)PropertyInfo.GetValue(target);
				if(list == null)
				{
					object value = Activator.CreateInstance(PropertyInfo.PropertyType)!;
					PropertyInfo.SetValue(target, value);
					list = (IList)value;
				}

				JsonArray array = (JsonArray)node!;
				foreach (JsonNode? element in array)
				{
					context.EnterScope($"{Name}[{list.Count}]");

					object? elementValue = await _elementType.ReadAsync(element, context, cancellationToken);
					list.Add(elementValue);

					context.LeaveScope();
				}
			}

			public override async Task ParseIncludesAsync(JsonNode jsonNode, object targetObject, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
			{
				if (jsonNode is JsonArray jsonArrayValue)
				{
					ClassConfigType classElementType = (ClassConfigType)_elementType;
					foreach (JsonObject jsonObjectElement in jsonArrayValue.OfType<JsonObject>())
					{
						await classElementType.ParseIncludesAsync(jsonObjectElement, targetObject, targetType, context, cancellationToken);
					}
				}
			}
		}

		class DictionaryProperty : Property
		{
			readonly ConfigType _elementType;

			public DictionaryProperty(string name, PropertyInfo propertyInfo, ConfigType elementType)
				: base(name, propertyInfo)
			{
				_elementType = elementType;
			}

			public override bool HasIncludes() => _elementType is ClassConfigType elementType && elementType.HasIncludes();

			public override async Task MergeAsync(object target, JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
			{
				IDictionary? dictionary = (IDictionary?)PropertyInfo.GetValue(target);
				if (dictionary == null)
				{
					object value = Activator.CreateInstance(PropertyInfo.PropertyType)!;
					PropertyInfo.SetValue(target, value);
					dictionary = (IDictionary)value;
				}

				JsonObject obj = (JsonObject)node!;
				foreach ((string key, JsonNode? element) in obj)
				{
					context.EnterScope($"{Name}[{key}]");

					object? elementValue = await _elementType.ReadAsync(element, context, cancellationToken);
					dictionary.Add(key, elementValue);

					context.LeaveScope();
				}
			}

			public override async Task ParseIncludesAsync(JsonNode jsonNode, object targetObject, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
			{
				if (jsonNode is JsonObject jsonObjectValue)
				{
					ClassConfigType classElementType = (ClassConfigType)_elementType;
					foreach (JsonObject jsonObjectElement in jsonObjectValue.Select(x => x.Value).OfType<JsonObject>())
					{
						await classElementType.ParseIncludesAsync(jsonObjectElement, targetObject, targetType, context, cancellationToken);
					}
				}
			}
		}

		class ObjectProperty : Property
		{
			readonly ClassConfigType _classConfigType;

			public ObjectProperty(string name, PropertyInfo propertyInfo, ClassConfigType classConfigType)
				: base(name, propertyInfo)
			{
				_classConfigType = classConfigType;
			}

			public override bool HasIncludes() => _classConfigType.HasIncludes();

			public override async Task MergeAsync(object target, JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
			{
				if (node is JsonObject obj)
				{
					Uri? otherFile;
					context.TryAddProperty(Name, out otherFile);

					context.EnterScope(Name);

					object? childTarget = PropertyInfo.GetValue(target);
					if (childTarget == null)
					{
						if (otherFile != null)
						{
							throw new ConfigException(context, $"Property {context.CurrentScope}.{Name} conflicts with value in {otherFile}.");
						}

						childTarget = await _classConfigType.ReadAsync(node, context, cancellationToken);
						PropertyInfo.SetValue(target, childTarget);
					}
					else
					{
						await _classConfigType.MergeObjectAsync(childTarget, obj, context, cancellationToken);
					}

					context.LeaveScope();
				}
				else
				{
					context.AddProperty(Name);
					PropertyInfo.SetValue(target, JsonSerializer.Deserialize(node, PropertyInfo.PropertyType, context.JsonOptions));
				}
			}

			public override async Task ParseIncludesAsync(JsonNode jsonNode, object targetObject, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
			{
				if (jsonNode is JsonObject jsonObjectValue)
				{
					await _classConfigType.ParseIncludesAsync(jsonObjectValue, targetObject, targetType, context, cancellationToken);
				}
			}
		}

		readonly Type _type;
		readonly bool _isIncludeRoot;
		readonly bool _isIncludeContext;
		readonly Dictionary<string, Property> _nameToProperty = new Dictionary<string, Property>(StringComparer.OrdinalIgnoreCase);
		readonly Dictionary<string, Property> _nameToIncludeProperty = new Dictionary<string, Property>(StringComparer.OrdinalIgnoreCase);
		readonly Dictionary<string, ClassConfigType>? _knownTypes;

		static readonly ConcurrentDictionary<Type, ClassConfigType> s_typeToObjectValueType = new ConcurrentDictionary<Type, ClassConfigType>();

		public ClassConfigType(Type type)
		{
			s_typeToObjectValueType.TryAdd(type, this);

			_type = type;
			_isIncludeRoot = type.GetCustomAttribute<ConfigIncludeRootAttribute>() != null;
			_isIncludeContext = type.GetCustomAttribute<ConfigIncludeContextAttribute>() != null;

			// Find all the direct include properties
			PropertyInfo[] propertyInfos = type.GetProperties(BindingFlags.Instance | BindingFlags.Public | BindingFlags.GetProperty);
			foreach (PropertyInfo propertyInfo in propertyInfos)
			{
				if (propertyInfo.GetCustomAttribute<JsonIgnoreAttribute>() == null)
				{
					string name = propertyInfo.GetCustomAttribute<JsonPropertyNameAttribute>()?.Name ?? propertyInfo.Name;
					_nameToProperty.Add(name, CreateProperty(name, propertyInfo));
				}
			}

			// Build a map of all the properties which can include other files
			foreach (Property property in _nameToProperty.Values)
			{
				if (property.HasIncludes())
				{
					_nameToIncludeProperty.Add(property.Name, property);
				}
			}

			// Build up a list of possible types for this object
			JsonKnownTypesAttribute? knownTypes = _type.GetCustomAttribute<JsonKnownTypesAttribute>();
			if (knownTypes != null)
			{
				_knownTypes = new Dictionary<string, ClassConfigType>(StringComparer.Ordinal);
				foreach (Type knownType in knownTypes.Types)
				{
					ClassConfigType knownConfigType = FindOrAdd(knownType);
					foreach (JsonDiscriminatorAttribute discriminatorAttribute in knownType.GetCustomAttributes(typeof(JsonDiscriminatorAttribute), true))
					{
						_knownTypes.Add(discriminatorAttribute.Name, knownConfigType);
					}
				}
			}
		}

		bool HasIncludes() => !_isIncludeRoot && _nameToIncludeProperty.Count > 0;

		public static ClassConfigType FindOrAdd(Type type)
		{
			ClassConfigType? value;
			if (!s_typeToObjectValueType.TryGetValue(type, out value))
			{
				lock (s_typeToObjectValueType)
				{
					if (!s_typeToObjectValueType.TryGetValue(type, out value))
					{
						value = new ClassConfigType(type);
					}
				}
			}
			return value;
		}

		static Property CreateProperty(string name, PropertyInfo propertyInfo)
		{
			Type propertyType = propertyInfo.PropertyType;
			if (!propertyType.IsClass || propertyType == typeof(string))
			{
				return new ScalarProperty(name, propertyInfo);
			}
			else if (propertyType.IsAssignableTo(typeof(ConfigResource)))
			{
				return new ResourceProperty(name, propertyInfo);
			}
			else if (propertyType.IsGenericType && propertyType.GetGenericTypeDefinition() == typeof(List<>))
			{
				Type elementType = propertyType.GetGenericArguments()[0];
				return new ListProperty(name, propertyInfo, FindOrAddValueType(elementType));
			}
			else if (propertyType.IsGenericType && propertyType.GetGenericTypeDefinition() == typeof(Dictionary<,>))
			{
				Type elementType = propertyType.GetGenericArguments()[1];
				return new DictionaryProperty(name, propertyInfo, FindOrAddValueType(elementType));
			}
			else
			{
				return new ObjectProperty(name, propertyInfo, FindOrAdd(propertyType));
			}
		}

		public override async ValueTask<object?> ReadAsync(JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
		{
			if (node == null)
			{
				throw new ConfigException(context, "Unable to deserialize object from null value");
			}
			else if (node is JsonObject obj)
			{
				return await ReadAsync(obj, context, cancellationToken);
			}
			else
			{
				return JsonSerializer.Deserialize(node, _type, context.JsonOptions);
			}
		}

		public ValueTask<object> ReadAsync(JsonObject obj, ConfigContext context, CancellationToken cancellationToken)
		{
			ClassConfigType targetType = this;
			if (_knownTypes != null && obj.TryGetPropertyValue("Type", out JsonNode? knownTypeNode) && knownTypeNode != null)
			{
				targetType = _knownTypes[knownTypeNode.ToString()];
			}
			return targetType.ReadConcreteTypeAsync(obj, context, cancellationToken);
		}

		async ValueTask<object> ReadConcreteTypeAsync(JsonObject obj, ConfigContext context, CancellationToken cancellationToken)
		{
			object result = Activator.CreateInstance(_type)!;
			await MergeObjectAsync(result, obj, context, cancellationToken);
			return result;
		}

		static async ValueTask<IConfigFile> ReadFileAsync(Uri uri, ConfigContext context, CancellationToken cancellationToken)
		{
			IConfigSource? source = context.Sources[uri.Scheme];
			if (source == null)
			{
				throw new ConfigException(context, $"Invalid/unknown scheme for config file {uri}");
			}

			IConfigFile? file;
			if (!context.Files.TryGetValue(uri, out file))
			{
				file = await source.GetAsync(uri, cancellationToken);
				context.Files.Add(uri, file);
			}

			return file;
		}

		static async ValueTask<JsonObject> ParseFileAsync(IConfigFile file, ConfigContext context, CancellationToken cancellationToken)
		{
			ReadOnlyMemory<byte> data = await file.ReadAsync(cancellationToken);

			JsonObject? obj = JsonSerializer.Deserialize<JsonObject>(data.Span, context.JsonOptions);
			if (obj == null)
			{
				throw new ConfigException(context, $"Config file {file.Uri} contains a null object.");
			}

			return obj;
		}

		public async Task MergeObjectAsync(object target, Uri uri, ConfigContext context, CancellationToken cancellationToken)
		{
			if (context.IncludeStack.Any(x => x.Uri == uri))
			{
				throw new ConfigException(context, $"Recursive include of file {uri}");
			}

			IConfigFile file = await ReadFileAsync(uri, context, cancellationToken);

			context.IncludeStack.Push(file);

			JsonObject obj = await ParseFileAsync(file, context, cancellationToken);
			await MergeObjectAsync(target, obj, context, cancellationToken);

			context.IncludeStack.Pop();
		}

		async Task MergeObjectAsync(object target, JsonObject obj, ConfigContext context, CancellationToken cancellationToken)
		{
			// Before parsing properties into this object, read all the includes recursively
			if (_isIncludeRoot)
			{
				await ParseIncludesAsync(obj, target, this, context, cancellationToken);
			}

			// Parse all the properties into this object
			foreach ((string name, JsonNode? node) in obj)
			{
				if (_nameToProperty.TryGetValue(name, out Property? property))
				{
					await property.MergeAsync(target, node, context, cancellationToken);
				}
			}
		}

		async Task ParseIncludesAsync(JsonObject jsonObject, object targetObject, ClassConfigType targetType, ConfigContext context, CancellationToken cancellationToken)
		{
			if (_isIncludeContext)
			{
				context.IncludeContextStack.Push(JsonSerializer.Deserialize(jsonObject, _type, context.JsonOptions)!);
			}

			foreach ((string name, JsonNode? node) in jsonObject)
			{
				if (_nameToIncludeProperty.TryGetValue(name, out Property? property) && node != null)
				{
					await property.ParseIncludesAsync(node, targetObject, targetType, context, cancellationToken);
				}
			}

			if (_isIncludeContext)
			{
				context.IncludeContextStack.Pop();
			}
		}
	}
}

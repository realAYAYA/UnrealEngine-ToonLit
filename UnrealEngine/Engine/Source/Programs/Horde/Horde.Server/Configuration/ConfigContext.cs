// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Text.Json;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Configuration
{
	/// <summary>
	/// Context for reading a tree of config files
	/// </summary>
	[DebuggerDisplay("{CurrentFile}")]
	class ConfigContext
	{
		/// <summary>
		/// Options for serializing config files
		/// </summary>
		public JsonSerializerOptions JsonOptions { get; }

		/// <summary>
		/// Stack of included files
		/// </summary>
		public Stack<IConfigFile> IncludeStack { get; } = new Stack<IConfigFile>();

		/// <summary>
		/// Stack of properties
		/// </summary>
		public Stack<string> ScopeStack { get; } = new Stack<string>();

		/// <summary>
		/// Stack of objects
		/// </summary>
		public Stack<object> IncludeContextStack { get; } = new Stack<object>();

		/// <summary>
		/// Map of property path to the file declaring a value for it
		/// </summary>
		public Dictionary<string, Uri> PropertyPathToFile { get; } = new Dictionary<string, Uri>(StringComparer.OrdinalIgnoreCase);

		/// <summary>
		/// Sources to read config files from
		/// </summary>
		public IReadOnlyDictionary<string, IConfigSource> Sources { get; }

		/// <summary>
		/// Map of macro name to value 
		/// </summary>
		public List<Dictionary<string, string>> MacroScopes { get; } = new List<Dictionary<string, string>>();

		/// <summary>
		/// Tracks files read as part of the configuration
		/// </summary>
		public Dictionary<Uri, IConfigFile> Files { get; } = new Dictionary<Uri, IConfigFile>();

		/// <summary>
		/// Logger for config messages
		/// </summary>
		public ILogger Logger { get; }

		/// <summary>
		/// Uri of the current file
		/// </summary>
		public Uri CurrentFile => (IncludeStack.Count > 0) ? IncludeStack.Peek().Uri : null!;

		/// <summary>
		/// Current property scope
		/// </summary>
		public string CurrentScope => ScopeStack.Peek();

		readonly Func<string, string?> _getMacroValue;

		/// <summary>
		/// Constructor
		/// </summary>
		public ConfigContext(JsonSerializerOptions jsonOptions, IReadOnlyDictionary<string, IConfigSource> sources, ILogger logger)
		{
			JsonOptions = jsonOptions;
			Sources = sources;
			ScopeStack.Push("$");
			Logger = logger;
			MacroScopes.Add(new Dictionary<string, string> { ["HordeDir"] = ServerApp.AppDir.FullName });

			_getMacroValue = GetMacroValue;
		}

		/// <summary>
		/// Marks a property as defined in the current file
		/// </summary>
		/// <param name="name"></param>
		public void AddProperty(string name)
		{
			if (!TryAddProperty(name, out Uri? otherFile))
			{
				throw new ConfigException(this, $"Property {CurrentScope}.{name} was already defined in {otherFile}.");
			}
		}

		/// <summary>
		/// Marks a property as defined in the current file
		/// </summary>
		/// <param name="name">Name of the property within the current scope</param>
		/// <param name="otherFile">If the property is not added, the file that previously defined it</param>
		public bool TryAddProperty(string name, [NotNullWhen(false)] out Uri? otherFile)
		{
			string propertyPath = $"{CurrentScope}.{name}";

			Uri currentFile = CurrentFile;
			if (PropertyPathToFile.TryAdd(propertyPath, currentFile))
			{
				otherFile = null;
				return true;
			}
			else
			{
				otherFile = PropertyPathToFile[propertyPath];
				return false;
			}
		}

		/// <summary>
		/// Pushes a scope to the property stack
		/// </summary>
		/// <param name="name"></param>
		public void EnterScope(string name)
		{
			ScopeStack.Push($"{CurrentScope}.{name}");
		}

		/// <summary>
		/// Pops a scope from the property stack
		/// </summary>
		public void LeaveScope()
		{
			ScopeStack.Pop();
		}

		/// <summary>
		/// Expand macros in a string property
		/// </summary>
		public string ExpandMacros(string text) => StringUtils.ExpandProperties(text, _getMacroValue);

		/// <summary>
		/// Gets the value of a named macro
		/// </summary>
		string? GetMacroValue(string name)
		{
			for (int idx = MacroScopes.Count - 1; idx >= 0; idx--)
			{
				if (MacroScopes[idx].TryGetValue(name, out string? value))
				{
					return value;
				}
			}
			return null;
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Text.Json.Serialization;

namespace EpicGames.Core
{
	/// <summary>
	/// Different types of log values (stored in the $type field of log properties)
	/// </summary>
	public static class LogValueType
	{
#pragma warning disable CS1591
		public static readonly Utf8String Asset = "Asset";
		public static readonly Utf8String SourceFile = "SourceFile";
		public static readonly Utf8String Object = "Object"; // Arbitrary structured object
		public static readonly Utf8String Channel = "Channel";
		public static readonly Utf8String Severity = "Severity";
		public static readonly Utf8String Message = "Message";
		public static readonly Utf8String LineNumber = "Line";
		public static readonly Utf8String ColumnNumber = "Column";
		public static readonly Utf8String Symbol = "Symbol";
		public static readonly Utf8String ErrorCode = "ErrorCode";
		public static readonly Utf8String ToolName = "ToolName";
		public static readonly Utf8String ScreenshotTest = "ScreenshotTest";
		public static readonly Utf8String DepotPath = "DepotPath";
#pragma warning restore CS1591
	}

	/// <summary>
	/// Information for a structured value for use in log events
	/// </summary>
	public sealed class LogValue
	{
		/// <summary>
		/// Type of the event
		/// </summary>
		public Utf8String Type { get; set; }

		/// <summary>
		/// Rendering of the value
		/// </summary>
		public string Text { get; set; }

		/// <summary>
		/// Properties associated with the value
		/// </summary>
		public Dictionary<Utf8String, object>? Properties { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type">Type of the value</param>
		/// <param name="text">Rendering of the value as text</param>
		/// <param name="properties">Additional properties for this value</param>
		public LogValue(Utf8String type, string text, Dictionary<Utf8String, object>? properties = null)
		{
			Type = type;
			Text = text;
			Properties = properties;
		}

		/// <summary>
		/// Creates a LogValue from an object, overriding the type and display text for it
		/// </summary>
		/// <param name="obj">The object to construct from</param>
		/// <returns></returns>
		public static LogValue FromObject(object obj) => FromObject(LogValueType.Object, obj.ToString() ?? String.Empty, obj);

		/// <summary>
		/// Creates a LogValue from an object, overriding the type and display text for it
		/// </summary>
		/// <param name="type">Type of the object</param>
		/// <param name="text">Rendered representation of the object in the output string</param>
		/// <param name="obj">The object to construct from</param>
		/// <returns></returns>
		public static LogValue FromObject(Utf8String type, string text, object obj)
		{
			Type objType = obj.GetType();

			Dictionary<Utf8String, object>? properties = null;
			foreach (PropertyInfo propertyInfo in objType.GetProperties())
			{
				if (propertyInfo.GetCustomAttribute<JsonIgnoreAttribute>() == null)
				{
					string name = propertyInfo.GetCustomAttribute<JsonPropertyNameAttribute>()?.Name ?? propertyInfo.Name;
					properties ??= new Dictionary<Utf8String, object>();
					properties[name] = propertyInfo.GetValue(obj)!;
				}
			}

			return new LogValue(type, text, properties);
		}

		/// <inheritdoc/>
		public override string ToString()
		{
			return Text;
		}
	}
}

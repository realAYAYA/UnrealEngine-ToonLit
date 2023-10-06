// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Text.Json;

namespace EpicGames.Core
{
	/// <summary>
	/// Exception thrown for errors parsing JSON files
	/// </summary>
	public class JsonParseException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="format">Format string</param>
		/// <param name="args">Optional arguments</param>
		public JsonParseException(string format, params object[] args)
			: base(String.Format(format, args))
		{
		}
	}

	/// <summary>
	/// Stores a JSON object in memory
	/// </summary>
	public class JsonObject
	{
		readonly Dictionary<string, object?> _rawObject;

		/// <summary>
		/// Construct a JSON object from the raw string -> object dictionary
		/// </summary>
		/// <param name="inRawObject">Raw object parsed from disk</param>
		public JsonObject(Dictionary<string, object?> inRawObject)
		{
			_rawObject = new Dictionary<string, object?>(inRawObject, StringComparer.InvariantCultureIgnoreCase);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="element"></param>
		public JsonObject(JsonElement element)
		{
			_rawObject = new Dictionary<string, object?>(StringComparer.InvariantCultureIgnoreCase);
			foreach (JsonProperty property in element.EnumerateObject())
			{
				_rawObject[property.Name] = ParseElement(property.Value);
			}
		}

		/// <summary>
		/// Parse an individual element
		/// </summary>
		/// <param name="element"></param>
		/// <returns></returns>
		public static object? ParseElement(JsonElement element)
		{
			switch(element.ValueKind)
			{
				case JsonValueKind.Array:
					return element.EnumerateArray().Select(x => ParseElement(x)).ToArray();
				case JsonValueKind.Number:
					return element.GetDouble();
				case JsonValueKind.Object:
					return element.EnumerateObject().ToDictionary(x => x.Name, x => ParseElement(x.Value));
				case JsonValueKind.String:
					return element.GetString();
				case JsonValueKind.False:
					return false;
				case JsonValueKind.True:
					return true;
				case JsonValueKind.Null:
					return null;
				default:
					throw new NotImplementedException();
			}
		}

		/// <summary>
		/// Read a JSON file from disk and construct a JsonObject from it
		/// </summary>
		/// <param name="file">File to read from</param>
		/// <returns>New JsonObject instance</returns>
		public static JsonObject Read(FileReference file)
		{
			string text = FileReference.ReadAllText(file);
			try
			{
				return Parse(text);
			}
			catch(Exception ex)
			{
				throw new JsonParseException("Unable to parse {0}: {1}", file, ex.Message);
			}
		}

		/// <summary>
		/// Tries to read a JSON file from disk
		/// </summary>
		/// <param name="fileName">File to read from</param>
		/// <param name="result">On success, receives the parsed object</param>
		/// <returns>True if the file was read, false otherwise</returns>
		public static bool TryRead(FileReference fileName, [NotNullWhen(true)] out JsonObject? result)
		{
			if (!FileReference.Exists(fileName))
			{
				result = null;
				return false;
			}

			string text = FileReference.ReadAllText(fileName);
			return TryParse(text, out result);
		}

		/// <summary>
		/// Parse a JsonObject from the given raw text string
		/// </summary>
		/// <param name="text">The text to parse</param>
		/// <returns>New JsonObject instance</returns>
		public static JsonObject Parse(string text)
		{
			JsonDocument document = JsonDocument.Parse(text, new JsonDocumentOptions { AllowTrailingCommas = true });
			return new JsonObject(document.RootElement);
		}

		/// <summary>
		/// Try to parse a JsonObject from the given raw text string
		/// </summary>
		/// <param name="text">The text to parse</param>
		/// <param name="result">On success, receives the new JsonObject</param>
		/// <returns>True if the object was parsed</returns>
		public static bool TryParse(string text, [NotNullWhen(true)] out JsonObject? result)
		{
			try
			{
				result = Parse(text);
				return true;
			}
			catch (Exception)
			{
				result = null;
				return false;
			}
		}

		/// <summary>
		/// List of key names in this object
		/// </summary>
		public IEnumerable<string> KeyNames => _rawObject.Keys;

		/// <summary>
		/// Gets a string field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public string GetStringField(string fieldName)
		{
			string? stringValue;
			if (!TryGetStringField(fieldName, out stringValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", fieldName);
			}
			return stringValue;
		}

		/// <summary>
		/// Tries to read a string field by the given name from the object
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <param name="result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetStringField(string fieldName, [NotNullWhen(true)] out string? result)
		{
			object? rawValue;
			if (_rawObject.TryGetValue(fieldName, out rawValue) && (rawValue is string strValue))
			{
				result = strValue;
				return true;
			}
			else
			{
				result = null;
				return false;
			}
		}

		/// <summary>
		/// Gets a string array field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public string[] GetStringArrayField(string fieldName)
		{
			string[]? stringValues;
			if (!TryGetStringArrayField(fieldName, out stringValues))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", fieldName);
			}
			return stringValues;
		}

		/// <summary>
		/// Tries to read a string array field by the given name from the object
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <param name="result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetStringArrayField(string fieldName, [NotNullWhen(true)] out string[]? result)
		{
			object? rawValue;
			if (_rawObject.TryGetValue(fieldName, out rawValue) && (rawValue is IEnumerable<object> enumValue) && enumValue.All(x => x is string))
			{
				result = enumValue.Select(x => (string)x).ToArray();
				return true;
			}
			else
			{
				result = null;
				return false;
			}
		}

		/// <summary>
		/// Gets a boolean field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public bool GetBoolField(string fieldName)
		{
			bool boolValue;
			if (!TryGetBoolField(fieldName, out boolValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", fieldName);
			}
			return boolValue;
		}

		/// <summary>
		/// Tries to read a bool field by the given name from the object
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <param name="result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetBoolField(string fieldName, out bool result)
		{
			object? rawValue;
			if (_rawObject.TryGetValue(fieldName, out rawValue) && (rawValue is bool boolValue))
			{
				result = boolValue;
				return true;
			}
			else
			{
				result = false;
				return false;
			}
		}

		/// <summary>
		/// Gets an integer field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public int GetIntegerField(string fieldName)
		{
			int integerValue;
			if (!TryGetIntegerField(fieldName, out integerValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", fieldName);
			}
			return integerValue;
		}

		/// <summary>
		/// Tries to read an integer field by the given name from the object
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <param name="result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetIntegerField(string fieldName, out int result)
		{
			object? rawValue;
			if (!_rawObject.TryGetValue(fieldName, out rawValue) || !Int32.TryParse(rawValue?.ToString(), out result))
			{
				result = 0;
				return false;
			}
			return true;
		}

		/// <summary>
		/// Tries to read an unsigned integer field by the given name from the object
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <param name="result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetUnsignedIntegerField(string fieldName, out uint result)
		{
			object? rawValue;
			if (!_rawObject.TryGetValue(fieldName, out rawValue) || !UInt32.TryParse(rawValue?.ToString(), out result))
			{
				result = 0;
				return false;
			}
			return true;
		}

		/// <summary>
		/// Gets a double field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public double GetDoubleField(string fieldName)
		{
			double doubleValue;
			if (!TryGetDoubleField(fieldName, out doubleValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", fieldName);
			}
			return doubleValue;
		}

		/// <summary>
		/// Tries to read a double field by the given name from the object
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <param name="result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetDoubleField(string fieldName, out double result)
		{
			object? rawValue;
			if (!_rawObject.TryGetValue(fieldName, out rawValue) || !Double.TryParse(rawValue?.ToString(), out result))
			{
				result = 0.0;
				return false;
			}
			return true;
		}

		/// <summary>
		/// Gets an enum field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public T GetEnumField<T>(string fieldName) where T : struct
		{
			T enumValue;
			if (!TryGetEnumField(fieldName, out enumValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", fieldName);
			}
			return enumValue;
		}

		/// <summary>
		/// Tries to read an enum field by the given name from the object
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <param name="result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetEnumField<T>(string fieldName, out T result) where T : struct
		{
			string? stringValue;
			if (!TryGetStringField(fieldName, out stringValue) || !Enum.TryParse<T>(stringValue, true, out result))
			{
				result = default;
				return false;
			}
			return true;
		}

		/// <summary>
		/// Tries to read an enum array field by the given name from the object
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <param name="result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetEnumArrayField<T>(string fieldName, [NotNullWhen(true)] out T[]? result) where T : struct
		{
			string[]? stringValues;
			if (!TryGetStringArrayField(fieldName, out stringValues))
			{
				result = null;
				return false;
			}

			T[] enumValues = new T[stringValues.Length];
			for (int idx = 0; idx < stringValues.Length; idx++)
			{
				if (!Enum.TryParse<T>(stringValues[idx], true, out enumValues[idx]))
				{
					result = null;
					return false;
				}
			}

			result = enumValues;
			return true;
		}

		/// <summary>
		/// Gets an object field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public JsonObject GetObjectField(string fieldName)
		{
			JsonObject? result;
			if (!TryGetObjectField(fieldName, out result))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", fieldName);
			}
			return result;
		}

		/// <summary>
		/// Tries to read an object field by the given name from the object
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <param name="result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetObjectField(string fieldName, [NotNullWhen(true)] out JsonObject? result)
		{
			object? rawValue;
			if (_rawObject.TryGetValue(fieldName, out rawValue) && (rawValue is Dictionary<string, object?> dictValue))
			{
				result = new JsonObject(dictValue);
				return true;
			}
			else
			{
				result = null;
				return false;
			}
		}

		/// <summary>
		/// Gets an object array field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public JsonObject[] GetObjectArrayField(string fieldName)
		{
			JsonObject[]? result;
			if (!TryGetObjectArrayField(fieldName, out result))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", fieldName);
			}
			return result;
		}

		/// <summary>
		/// Tries to read an object array field by the given name from the object
		/// </summary>
		/// <param name="fieldName">Name of the field to get</param>
		/// <param name="result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetObjectArrayField(string fieldName, [NotNullWhen(true)] out JsonObject[]? result)
		{
			object? rawValue;
			if (_rawObject.TryGetValue(fieldName, out rawValue) && (rawValue is IEnumerable<object> enumValue) && enumValue.All(x => x is Dictionary<string, object>))
			{
				result = enumValue.Select(x => new JsonObject((Dictionary<string, object?>)x)).ToArray();
				return true;
			}
			else
			{
				result = null;
				return false;
			}
		}
	}
}

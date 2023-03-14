// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Text;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Text.RegularExpressions;
using EpicGames.Core;

namespace EpicGames.Perforce
{
	/// <summary>
	/// String constants for perforce values
	/// </summary>
	static class StringConstants
	{
		public static readonly Utf8String True = new Utf8String("true");
		public static readonly Utf8String New = new Utf8String("new");
		public static readonly Utf8String None = new Utf8String("none");
		public static readonly Utf8String Default = new Utf8String("default");
	}

	/// <summary>
	/// Stores cached information about a field with a P4Tag attribute
	/// </summary>
	class CachedTagInfo
	{
		/// <summary>
		/// Name of the tag. Specified in the attribute or inferred from the field name.
		/// </summary>
		public Utf8String _name;

		/// <summary>
		/// Whether this tag is optional or not.
		/// </summary>
		public bool _optional;

		/// <summary>
		/// The property containing the value of this data.
		/// </summary>
		public PropertyInfo _property;

		/// <summary>
		/// Parser for this field type
		/// </summary>
		public Action<object, int> _setFromInteger;

		/// <summary>
		/// Parser for this field type
		/// </summary>
		public Action<object, Utf8String> _setFromString;

		/// <summary>
		/// Index into the bitmask of required types
		/// </summary>
		public ulong _requiredTagBitMask;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name"></param>
		/// <param name="optional"></param>
		/// <param name="property"></param>
		/// <param name="requiredTagBitMask"></param>
		public CachedTagInfo(Utf8String name, bool optional, PropertyInfo property, ulong requiredTagBitMask)
		{
			_name = name;
			_optional = optional;
			_property = property;
			_requiredTagBitMask = requiredTagBitMask;
			_setFromInteger = (obj, value) => throw new PerforceException($"Field {name} was not expecting an integer value.");
			_setFromString = (obj, @string) => throw new PerforceException($"Field {name} was not expecting a string value.");
		}
	}

	/// <summary>
	/// Stores cached information about a record
	/// </summary>
	class CachedRecordInfo
	{
		/// <summary>
		/// Delegate type for creating a record instance
		/// </summary>
		/// <returns>New instance</returns>
		public delegate object CreateRecordDelegate();

		/// <summary>
		/// Type of the record
		/// </summary>
		public Type _type;

		/// <summary>
		/// Method to construct this record
		/// </summary>
		public CreateRecordDelegate _createInstance;

		/// <summary>
		/// List of fields in the record. These should be ordered to match P4 output for maximum efficiency.
		/// </summary>
		public List<CachedTagInfo> _fields = new List<CachedTagInfo>();

		/// <summary>
		/// Map of name to tag info
		/// </summary>
		public Dictionary<Utf8String, CachedTagInfo> _nameToInfo = new Dictionary<Utf8String, CachedTagInfo>();

		/// <summary>
		/// Bitmask of all the required tags. Formed by bitwise-or'ing the RequiredTagBitMask fields for each required CachedTagInfo.
		/// </summary>
		public ulong _requiredTagsBitMask;

		/// <summary>
		/// The type of records to create for subelements
		/// </summary>
		public Type? _subElementType;

		/// <summary>
		/// The cached record info for the subelement type
		/// </summary>
		public CachedRecordInfo? _subElementRecordInfo;

		/// <summary>
		/// Property containing subelements
		/// </summary>
		public PropertyInfo? _subElementProperty;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type">The record type</param>
		public CachedRecordInfo(Type type)
		{
			_type = type;

			ConstructorInfo? constructor = type.GetConstructor(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance, null, Type.EmptyTypes, null);
			if (constructor == null)
			{
				throw new PerforceException($"Unable to find default constructor for {type}");
			}

			DynamicMethod dynamicMethod = new DynamicMethod("_", type, null);
			ILGenerator generator = dynamicMethod.GetILGenerator();
			generator.Emit(OpCodes.Newobj, constructor);
			generator.Emit(OpCodes.Ret);
			_createInstance = (CreateRecordDelegate)dynamicMethod.CreateDelegate(typeof(CreateRecordDelegate));
		}
	}

	/// <summary>
	/// Information about an enum
	/// </summary>
	class CachedEnumInfo
	{
		/// <summary>
		/// The enum type
		/// </summary>
		public Type _enumType;

		/// <summary>
		/// Whether the enum has the [Flags] attribute
		/// </summary>
		public bool _bHasFlagsAttribute;

		/// <summary>
		/// Map of name to value
		/// </summary>
		public Dictionary<Utf8String, int> _nameToValue = new Dictionary<Utf8String, int>();

		/// <summary>
		/// List of name/value pairs
		/// </summary>
		public List<KeyValuePair<string, int>> _nameValuePairs = new List<KeyValuePair<string, int>>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="enumType">The type to construct from</param>
		public CachedEnumInfo(Type enumType)
		{
			_enumType = enumType;

			_bHasFlagsAttribute = enumType.GetCustomAttribute<FlagsAttribute>() != null;

			FieldInfo[] fields = enumType.GetFields(BindingFlags.Public | BindingFlags.Static);
			foreach (FieldInfo field in fields)
			{
				PerforceEnumAttribute? attribute = field.GetCustomAttribute<PerforceEnumAttribute>();
				if (attribute != null)
				{
					object? value = field.GetValue(null);
					if (value != null)
					{
						Utf8String name = new Utf8String(attribute.Name);
						_nameToValue[name] = (int)value;

						_nameValuePairs.Add(new KeyValuePair<string, int>(attribute.Name, (int)value));
					}
				}
			}
		}

		/// <summary>
		/// Parses the given integer as an enum
		/// </summary>
		/// <param name="value">The value to convert to an enum</param>
		/// <returns>The enum value corresponding to the given value</returns>
		public object ParseInteger(int value)
		{
			return Enum.ToObject(_enumType, value);
		}

		/// <summary>
		/// Parses the given text as an enum
		/// </summary>
		/// <param name="text">The text to parse</param>
		/// <returns>The enum value corresponding to the given text</returns>
		public object ParseString(Utf8String text)
		{
			return Enum.ToObject(_enumType, ParseToInteger(text));
		}

		/// <summary>
		/// Parses the given text as an enum
		/// </summary>
		/// <param name="name"></param>
		/// <returns></returns>
		public int ParseToInteger(Utf8String name)
		{
			if (_bHasFlagsAttribute)
			{
				int result = 0;
				for (int offset = 0; offset < name.Length;)
				{
					if (name.Span[offset] == (byte)' ')
					{
						offset++;
					}
					else
					{
						// Find the end of this name
						int startOffset = ++offset;
						while (offset < name.Length && name.Span[offset] != (byte)' ')
						{
							offset++;
						}

						// Take the subset
						Utf8String item = name.Slice(startOffset, offset - startOffset);

						// Get the value
						int itemValue;
						if (_nameToValue.TryGetValue(item, out itemValue))
						{
							result |= itemValue;
						}
					}
				}
				return result;
			}
			else
			{
				int result;
				_nameToValue.TryGetValue(name, out result);
				return result;
			}
		}

		/// <summary>
		/// Parses an enum value, using PerforceEnumAttribute markup for names.
		/// </summary>
		/// <param name="value">Value of the enum.</param>
		/// <returns>Text for the enum.</returns>
		public string GetEnumText(int value)
		{
			if (_bHasFlagsAttribute)
			{
				List<string> names = new List<string>();

				int combinedIntegerValue = 0;
				foreach (KeyValuePair<string, int> pair in _nameValuePairs)
				{
					if ((value & pair.Value) != 0)
					{
						names.Add(pair.Key);
						combinedIntegerValue |= pair.Value;
					}
				}

				if (combinedIntegerValue != value)
				{
					throw new ArgumentException($"Invalid enum value {value}");
				}

				return String.Join(" ", names);
			}
			else
			{
				string? name = null;
				foreach (KeyValuePair<string, int> pair in _nameValuePairs)
				{
					if (value == pair.Value)
					{
						name = pair.Key;
						break;
					}
				}

				if (name == null)
				{
					throw new ArgumentException($"Invalid enum value {value}");
				}
				return name;
			}
		}
	}

	/// <summary>
	/// Utility methods for converting to/from native types
	/// </summary>
	static class PerforceReflection
	{
		/// <summary>
		/// Unix epoch; used for converting times back into C# datetime objects
		/// </summary>
		public static readonly DateTime UnixEpoch = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);

		/// <summary>
		/// Constant for the default changelist, where valid.
		/// </summary>
		public const int DefaultChange = -2;

		/// <summary>
		/// Cached map of enum types to a lookup mapping from p4 strings to enum values.
		/// </summary>
		static readonly ConcurrentDictionary<Type, CachedEnumInfo> s_enumTypeToInfo = new ConcurrentDictionary<Type, CachedEnumInfo>();

		/// <summary>
		/// Cached set of record 
		/// </summary>
		static readonly ConcurrentDictionary<Type, CachedRecordInfo> s_recordTypeToInfo = new ConcurrentDictionary<Type, CachedRecordInfo>();

		/// <summary>
		/// Default type for info
		/// </summary>
		public static CachedRecordInfo InfoRecordInfo = GetCachedRecordInfo(typeof(PerforceInfo));

		/// <summary>
		/// Default type for errors
		/// </summary>
		public static CachedRecordInfo ErrorRecordInfo = GetCachedRecordInfo(typeof(PerforceError));

		/// <summary>
		/// Default type for errors
		/// </summary>
		public static CachedRecordInfo IoRecordInfo = GetCachedRecordInfo(typeof(PerforceIo));

		/// <summary>
		/// Gets a mapping of flags to enum values for the given type
		/// </summary>
		/// <param name="enumType">The enum type to retrieve flags for</param>
		/// <returns>Map of name to enum value</returns>
		static CachedEnumInfo GetCachedEnumInfo(Type enumType)
		{
			CachedEnumInfo? enumInfo;
			if (!s_enumTypeToInfo.TryGetValue(enumType, out enumInfo))
			{
				enumInfo = new CachedEnumInfo(enumType);
				if (!s_enumTypeToInfo.TryAdd(enumType, enumInfo))
				{
					enumInfo = s_enumTypeToInfo[enumType];
				}
			}
			return enumInfo;
		}

		/// <summary>
		/// Parses an enum value, using PerforceEnumAttribute markup for names.
		/// </summary>
		/// <param name="enumType">Type of the enum to parse.</param>
		/// <param name="value">Value of the enum.</param>
		/// <returns>Text for the enum.</returns>
		public static string GetEnumText(Type enumType, object value)
		{
			return GetCachedEnumInfo(enumType).GetEnumText((int)value);
		}

		/// <summary>
		/// Gets reflection data for the given record type
		/// </summary>
		/// <param name="recordType">The type to retrieve record info for</param>
		/// <returns>The cached reflection information for the given type</returns>
		public static CachedRecordInfo GetCachedRecordInfo(Type recordType)
		{
			CachedRecordInfo? record;
			if (!s_recordTypeToInfo.TryGetValue(recordType, out record))
			{
				record = new CachedRecordInfo(recordType);

				// Get all the fields for this type
				PropertyInfo[] properties = recordType.GetProperties(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);

				// Build the map of all tags for this record
				foreach (PropertyInfo property in properties)
				{
					PerforceTagAttribute? tagAttribute = property.GetCustomAttribute<PerforceTagAttribute>();
					if (tagAttribute != null)
					{
						string tagName = tagAttribute.Name ?? property.Name;

						ulong requiredTagBitMask = 0;
						if (!tagAttribute.Optional)
						{
							requiredTagBitMask = record._requiredTagsBitMask + 1;
							if (requiredTagBitMask == 0)
							{
								throw new PerforceException("Too many required tags in {0}; max is {1}", recordType.Name, sizeof(ulong) * 8);
							}
							record._requiredTagsBitMask |= requiredTagBitMask;
						}

						CachedTagInfo tagInfo = new CachedTagInfo(new Utf8String(tagName), tagAttribute.Optional, property, requiredTagBitMask);

						Type fieldType = property.PropertyType;

						PropertyInfo propertyCopy = property;
						if (fieldType == typeof(DateTime))
						{
							tagInfo._setFromString = (obj, @string) => propertyCopy.SetValue(obj, ParseStringAsDateTime(@string));
						}
						else if (fieldType == typeof(bool))
						{
							tagInfo._setFromString = (obj, @string) => propertyCopy.SetValue(obj, ParseStringAsBool(@string));
						}
						else if (fieldType == typeof(Nullable<bool>))
						{
							tagInfo._setFromString = (obj, @string) => propertyCopy.SetValue(obj, ParseStringAsNullableBool(@string));
						}
						else if (fieldType == typeof(int))
						{
							tagInfo._setFromInteger = (obj, @int) => propertyCopy.SetValue(obj, @int);
							tagInfo._setFromString = (obj, @string) => propertyCopy.SetValue(obj, ParseStringAsInt(@string));
						}
						else if (fieldType == typeof(long))
						{
							tagInfo._setFromString = (obj, @string) => propertyCopy.SetValue(obj, ParseStringAsLong(@string));
						}
						else if (fieldType == typeof(string))
						{
							tagInfo._setFromString = (obj, @string) => propertyCopy.SetValue(obj, ParseString(@string));
						}
						else if (fieldType == typeof(Utf8String))
						{
							tagInfo._setFromString = (obj, @string) => propertyCopy.SetValue(obj, @string.Clone());
						}
						else if (fieldType.IsEnum)
						{
							CachedEnumInfo enumInfo = GetCachedEnumInfo(fieldType);
							tagInfo._setFromInteger = (obj, @int) => propertyCopy.SetValue(obj, enumInfo.ParseInteger(@int));
							tagInfo._setFromString = (obj, @string) => propertyCopy.SetValue(obj, enumInfo.ParseString(@string));
						}
						else if (fieldType == typeof(DateTimeOffset?))
						{
							tagInfo._setFromString = (obj, @string) => propertyCopy.SetValue(obj, ParseStringAsNullableDateTimeOffset(@string));
						}
						else if (fieldType == typeof(List<string>))
						{
							tagInfo._setFromString = (obj, @string) => ((List<string>)propertyCopy.GetValue(obj)!).Add(@string.ToString());
						}
						else if (fieldType == typeof(ReadOnlyMemory<byte>))
						{
							tagInfo._setFromString = (obj, @string) => propertyCopy.SetValue(obj, @string.Memory);
						}
						else
						{
							throw new PerforceException("Unsupported type of {0}.{1} for tag '{2}'", recordType.Name, fieldType.Name, tagName);
						}

						record._fields.Add(tagInfo);
					}

					record._nameToInfo = record._fields.ToDictionary(x => x._name, x => x);

					PerforceRecordListAttribute? subElementAttribute = property.GetCustomAttribute<PerforceRecordListAttribute>();
					if (subElementAttribute != null)
					{
						record._subElementProperty = property;
						record._subElementType = property.PropertyType.GenericTypeArguments[0];
						record._subElementRecordInfo = GetCachedRecordInfo(record._subElementType);
					}
				}

				// Try to save the record info, or get the version that's already in the cache
				if (!s_recordTypeToInfo.TryAdd(recordType, record))
				{
					record = s_recordTypeToInfo[recordType];
				}
			}
			return record;
		}

		static object ParseString(Utf8String @string)
		{
			return @string.ToString();
		}

		static object ParseStringAsDateTime(Utf8String @string)
		{
			string text = @string.ToString();

			DateTime time;
			if (DateTime.TryParse(text, out time))
			{
				return time;
			}
			else
			{
				return PerforceReflection.UnixEpoch + TimeSpan.FromSeconds(long.Parse(text));
			}
		}

		static object ParseStringAsBool(Utf8String @string)
		{
			return @string.Length == 0 || @string == StringConstants.True;
		}

		static object ParseStringAsNullableBool(Utf8String @string)
		{
			return @string == StringConstants.True;
		}

		static object ParseStringAsInt(Utf8String @string)
		{
			int value;
			int bytesConsumed;
			if (Utf8Parser.TryParse(@string.Span, out value, out bytesConsumed) && bytesConsumed == @string.Length)
			{
				return value;
			}
			else if (@string == StringConstants.New || @string == StringConstants.None)
			{
				return -1;
			}
			else if (@string.Length > 0 && @string[0] == '#')
			{
				return ParseStringAsInt(@string.Slice(1));
			}
			else if (@string == StringConstants.Default)
			{
				return DefaultChange;
			}
			else
			{
				throw new PerforceException($"Unable to parse {@string} as an integer");
			}
		}

		static object ParseStringAsLong(Utf8String @string)
		{
			long value;
			int bytesConsumed;
			if (!Utf8Parser.TryParse(@string.Span, out value, out bytesConsumed) || bytesConsumed != @string.Length)
			{
				throw new PerforceException($"Unable to parse {@string} as a long value");
			}
			return value;
		}

		static object ParseStringAsNullableDateTimeOffset(Utf8String @string)
		{
			string text = @string.ToString();
			return DateTimeOffset.Parse(Regex.Replace(text, "[a-zA-Z ]*$", "")); // Strip timezone name (eg. "EST")
		}
	}
}

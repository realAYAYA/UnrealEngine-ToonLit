// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using Newtonsoft.Json;

namespace Jupiter.Implementation
{
    [JsonConverter(typeof(KeyIdJsonConverter))]
    [TypeConverter(typeof(KeyIdTypeConverter))]

    public readonly struct KeyId: IEquatable<KeyId>
    {
        public KeyId(string key)
        {
             _text = key;
 
            if (_text.Length == 0)
            {
				throw new ArgumentException("Keys must have at least one character.");
            }
 
            const int MaxLength = 250;
            if (_text.Length > MaxLength)
            {
                throw new ArgumentException($"Keys may not be longer than {MaxLength} characters");
            }
        }

        readonly string _text;

        public bool Equals(KeyId other)
        {
            return string.Equals(_text , other._text, StringComparison.Ordinal);
        }

        public override bool Equals(object? obj)
        {
            return obj is KeyId other && Equals(other);
        }

        public override int GetHashCode()
        {
            return _text.GetHashCode(StringComparison.Ordinal);
        }

        public override string ToString()
        {
            return _text;
        }

        public static bool operator ==(KeyId left, KeyId right)
        {
            return left.Equals(right);
        }
 
        public static bool operator !=(KeyId left, KeyId right)
        {
            return !left.Equals(right);
        }
    }

    public class KeyIdJsonConverter: JsonConverter<KeyId>
    {
        public override void WriteJson(JsonWriter writer, KeyId value, JsonSerializer serializer)
        {
            writer.WriteValue(value!.ToString());
        }

        public override KeyId ReadJson(JsonReader reader, Type objectType, KeyId existingValue, bool hasExistingValue, Newtonsoft.Json.JsonSerializer serializer)
        {
            string? s = (string?)reader.Value;

            return new KeyId(s!);
        }
    }

    public sealed class KeyIdTypeConverter : TypeConverter
    {
        /// <inheritdoc/>
        public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
        {
            if (sourceType == typeof(string))
            {
                return true;
            }

            return base.CanConvertFrom(context, sourceType);
        }
 
        public override object? ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object value)  
        {
            if (value is string s)
            {
                return new KeyId(s);
            }

            return base.ConvertFrom(context, culture, value);  
        }
    }
}


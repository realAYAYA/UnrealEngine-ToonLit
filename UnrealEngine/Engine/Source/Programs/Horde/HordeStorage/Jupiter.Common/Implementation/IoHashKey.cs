// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using System.Text;
using EpicGames.Core;
using EpicGames.Serialization;
using Newtonsoft.Json;
using JsonWriter = Newtonsoft.Json.JsonWriter;

namespace Jupiter.Implementation
{
    [JsonConverter(typeof(IoHashKeyJsonConverter))]
    [TypeConverter(typeof(IoHashKeyTypeConverter))]
    [CbConverter(typeof(IoHashKeyCbConverter))]

    public readonly struct IoHashKey: IEquatable<IoHashKey>
    {
        public IoHashKey(string key)
        {
             _text = key.ToLower();
 
            if (_text.Length != 40)
            {
                throw new ArgumentException("IoHashKeys must be exactly 40 bytes.");
            }
 
            for (int Idx = 0; Idx < _text.Length; Idx++)
            {
                if (!IsValidCharacter(_text[Idx]))
                {
                    throw new ArgumentException($"{_text} is not a valid namespace id");
                }
            }
        }

        static bool IsValidCharacter(char c)
        {
            if (c >= 'a' && c <= 'z')
            {
                return true;
            }
            if (c >= '0' && c <= '9')
            {
                return true;
            }
            return false;
        }
        readonly string _text;

        public bool Equals(IoHashKey other)
        {
            return string.Equals(_text , other._text, StringComparison.Ordinal);
        }

        public override bool Equals(object? obj)
        {
            return obj is IoHashKey other && Equals(other);
        }

        public override int GetHashCode()
        {
            return _text.GetHashCode(StringComparison.Ordinal);
        }

        public override string ToString()
        {
            return _text;
        }

        public static bool operator ==(IoHashKey left, IoHashKey right)
        {
            return left.Equals(right);
        }
 
        public static bool operator !=(IoHashKey left, IoHashKey right)
        {
            return !left.Equals(right);
        }

        public static IoHashKey FromName(string s)
        {
            return new IoHashKey(ContentHash.FromBlob(Encoding.UTF8.GetBytes(s)).ToString());
        }
    }

    public class IoHashKeyJsonConverter: JsonConverter<IoHashKey>
    {
        public override void WriteJson(JsonWriter writer, IoHashKey value, JsonSerializer serializer)
        {
            writer.WriteValue(value!.ToString());
        }

        public override IoHashKey ReadJson(JsonReader reader, Type objectType, IoHashKey existingValue, bool hasExistingValue, Newtonsoft.Json.JsonSerializer serializer)
        {
            string? s = (string?)reader.Value;

            return new IoHashKey(s!);
        }
    }

    public sealed class IoHashKeyTypeConverter : TypeConverter
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
                return new IoHashKey(s);
            }

            return base.ConvertFrom(context, culture, value);  
        }
    }

    sealed class IoHashKeyCbConverter : CbConverterBase<IoHashKey>
    {
        public override IoHashKey Read(CbField field) => new IoHashKey(field.AsString());

        /// <inheritdoc/>
        public override void Write(CbWriter writer, IoHashKey value) => writer.WriteStringValue(value.ToString());

        /// <inheritdoc/>
        public override void WriteNamed(CbWriter writer, Utf8String name, IoHashKey value) => writer.WriteString(name, value.ToString());
    }
}


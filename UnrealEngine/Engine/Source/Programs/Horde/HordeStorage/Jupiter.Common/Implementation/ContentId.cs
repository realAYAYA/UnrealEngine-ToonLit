// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using EpicGames.Core;
using Newtonsoft.Json;
using JsonWriter = Newtonsoft.Json.JsonWriter;

namespace Jupiter.Implementation
{
    [JsonConverter(typeof(ContentIdConverter))]
    [TypeConverter(typeof(ContentIdTypeConverter))]
    public class ContentId : ContentHash, IEquatable<ContentId>
    {
        public ContentId(byte[] identifier) : base(identifier)
        {
        }

        [JsonConstructor]
        public ContentId(string identifier) : base(identifier)
        {

        }

        public override int GetHashCode()
        {
            return Comparer.GetHashCode(Identifier);
        }

        public bool Equals(ContentId? other)
        {
            if (other == null)
            {
                return false;
            }

            return Comparer.Equals(Identifier, other.Identifier);
        }

        public override bool Equals(object? obj)
        {
            if (ReferenceEquals(null, obj))
            {
                return false;
            }

            if (ReferenceEquals(this, obj))
            {
                return true;
            }

            if (obj.GetType() != GetType())
            {
                return false;
            }

            return Equals((ContentId) obj);
        }

        public static ContentId FromContentHash(ContentHash contentHash)
        {
            return new ContentId(contentHash.HashData);
        }

        public static ContentId FromBlobIdentifier(BlobIdentifier blobIdentifier)
        {
            return new ContentId(blobIdentifier.HashData);
        }

        public BlobIdentifier AsBlobIdentifier()
        {
            return new BlobIdentifier(HashData);
        }

        public static ContentId FromIoHash(IoHash ioHash)
        {
            return new ContentId(ioHash.ToByteArray());
        }

        public IoHash AsIoHash()
        {
            return new IoHash(HashData);
        }
    }

    public class ContentIdTypeConverter : TypeConverter
    {
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
                return new ContentId(s);
            }

            return base.ConvertFrom(context, culture, value);  
        }  
    }

    public class ContentIdConverter : JsonConverter<ContentId?>
    {
        public override void WriteJson(JsonWriter writer, ContentId? value, JsonSerializer serializer)
        {
            writer.WriteValue(value!.ToString());
        }

        public override ContentId? ReadJson(JsonReader reader, Type objectType, ContentId? existingValue, bool hasExistingValue, JsonSerializer serializer)
        {
            if (reader.TokenType == JsonToken.Null)
            {
                return null;
            }

            string? s = (string?)reader.Value;

            if (s == null)
            {
                return null;
            }

            return new ContentId(s!);
        }
    }
}

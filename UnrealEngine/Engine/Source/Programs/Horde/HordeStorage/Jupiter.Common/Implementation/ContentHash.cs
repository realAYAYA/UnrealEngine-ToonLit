// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using Blake3;
using EpicGames.Core;
using EpicGames.Serialization;
using Newtonsoft.Json;
using JsonWriter = Newtonsoft.Json.JsonWriter;

namespace Jupiter.Implementation
{
    [JsonConverter(typeof(ContentHashConverter))]
    [CbConverter(typeof(ContentHashCbConverter))]
    public class ContentHash : IEquatable<ContentHash>, IEquatable<byte[]>
    {
	    protected ByteArrayComparer Comparer { get; } = new ByteArrayComparer();
        protected byte[] Identifier { get; init; }
        public const int HashLength = 20;
        public byte[] HashData => Identifier;
        public ContentHash(byte[] identifier)
        {
            Identifier = identifier;
            if (identifier.Length != HashLength)
            {
                throw new ArgumentException("Supplied identifier was not 20 bytes, this is not a valid identifier", nameof(identifier));
            }
        }

        [JsonConstructor]
        public ContentHash(string identifier)
        {
            if (identifier == null)
            {
                throw new ArgumentNullException(nameof(identifier));
            }

            byte[] byteIdentifier = StringUtils.ToHashFromHexString(identifier);
            Identifier = byteIdentifier;
            if (byteIdentifier.Length != HashLength)
            {
                throw new ArgumentException("Supplied identifier was not 20 bytes, this is not a valid identifier", nameof(identifier));
            }
        }

        public override int GetHashCode()
        {
            return Comparer.GetHashCode(Identifier);
        }

        public bool Equals(ContentHash? other)
        {
            if (other == null)
            {
                return false;
            }

            return Comparer.Equals(Identifier, other.Identifier);
        }

        public bool Equals(byte[]? other)
        {
            if (other == null)
            {
                return false;
            }

            return Comparer.Equals(Identifier, other);
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

            return Equals((ContentHash) obj);
        }

        public override string ToString()
        {
            return StringUtils.FormatAsHexString(Identifier);
        }

        public static ContentHash FromBlob(byte[] blobMemory)
        {
            using Hasher hasher = Hasher.New();
            hasher.UpdateWithJoin(blobMemory);
            Hash blake3Hash = hasher.Finalize();

            // we only keep the first 20 bytes of the Blake3 hash
            Span<byte> hash = blake3Hash.AsSpanUnsafe().Slice(0, HashLength);

            return new ContentHash(hash.ToArray());
        }
    }
    
    public class ContentHashConverter : JsonConverter<ContentHash>
    {
        public override void WriteJson(JsonWriter writer, ContentHash? value, JsonSerializer serializer)
        {
            writer.WriteValue(value!.ToString());
        }

        public override ContentHash ReadJson(JsonReader reader, Type objectType, ContentHash? existingValue, bool hasExistingValue, JsonSerializer serializer)
        {
            string? s = (string?)reader.Value;

            return new ContentHash(s!);
        }
    }

    public class ContentHashCbConverter : CbConverterBase<ContentHash>
    {
        public override ContentHash Read(CbField field) => new ContentHash(field.AsHash().ToByteArray());

        /// <inheritdoc/>
        public override void Write(CbWriter writer, ContentHash value) => writer.WriteHashValue(new IoHash(value.HashData));

        /// <inheritdoc/>
        public override void WriteNamed(CbWriter writer, Utf8String name, ContentHash value) => writer.WriteHash(name, new IoHash(value.HashData));
    }
}

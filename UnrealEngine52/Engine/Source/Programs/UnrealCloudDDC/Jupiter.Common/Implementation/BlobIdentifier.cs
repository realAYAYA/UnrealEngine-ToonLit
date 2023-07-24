// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Threading.Tasks;
using Blake3;
using EpicGames.Core;
using EpicGames.Serialization;
using Newtonsoft.Json;
using JsonWriter = Newtonsoft.Json.JsonWriter;

namespace Jupiter.Implementation
{
    [JsonConverter(typeof(BlobIdentifierConverter))]
    [TypeConverter(typeof(BlobIdentifierTypeConverter))]
    [CbConverter(typeof(BlobIdentifierCbConverter))]
    public class BlobIdentifier : ContentHash,  IEquatable<BlobIdentifier>
    {
        // multi thread the hashing for blobs larger then this size
        private const int MultiThreadedSize = 1_000_000;
        private string? _stringIdentifier;

        public BlobIdentifier(byte[] identifier) : base(identifier)
        {
        }

        [JsonConstructor]
        public BlobIdentifier(string identifier) : base(identifier)
        {

        }

        public override int GetHashCode()
        {
            return Comparer.GetHashCode(Identifier);
        }

        public bool Equals(BlobIdentifier? other)
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

            return Equals((BlobIdentifier) obj);
        }

        public override string ToString()
        {
            if (_stringIdentifier == null)
            {
                _stringIdentifier = StringUtils.FormatAsHexString(Identifier);
            }

            return _stringIdentifier;
        }

        public static new BlobIdentifier FromBlob(byte[] blobMemory)
        {
            Hash blake3Hash;
            if (blobMemory.Length < MultiThreadedSize)
            {
                using Hasher hasher = Hasher.New();
                hasher.Update(blobMemory);
                blake3Hash = hasher.Finalize();
            }
            else
            {
                using Hasher hasher = Hasher.New();
                hasher.UpdateWithJoin(blobMemory);
                blake3Hash = hasher.Finalize();
            }
            
            // we only keep the first 20 bytes of the Blake3 hash
            Span<byte> hash = blake3Hash.AsSpanUnsafe().Slice(0, 20);
            return new BlobIdentifier(hash.ToArray());
        }

        public static BlobIdentifier FromBlob(in Memory<byte> blobMemory)
        {
            Hash blake3Hash;
            if (blobMemory.Length < MultiThreadedSize)
            {
                using Hasher hasher = Hasher.New();
                hasher.Update(blobMemory.Span);
                blake3Hash = hasher.Finalize();
            }
            else
            {
                using Hasher hasher = Hasher.New();
                hasher.UpdateWithJoin(blobMemory.Span);
                blake3Hash = hasher.Finalize();
            }

            // we only keep the first 20 bytes of the Blake3 hash
            Span<byte> hash = blake3Hash.AsSpanUnsafe().Slice(0, 20);
            return new BlobIdentifier(hash.ToArray());
        }

        public static BlobIdentifier FromContentHash(ContentHash testObjectHash)
        {
            return new BlobIdentifier(testObjectHash.HashData);
        }

        public static async Task<BlobIdentifier> FromStream(Stream stream)
        {
            using Hasher hasher = Hasher.New();
            const int bufferSize = 1024 * 1024 * 5;
            byte[] buffer = new byte[bufferSize];
            int read = await stream.ReadAsync(buffer, 0, buffer.Length);
            while (read > 0)
            {
                hasher.UpdateWithJoin(new ReadOnlySpan<byte>(buffer, 0, read));
                read = await stream.ReadAsync(buffer, 0, buffer.Length);
            }
            Hash blake3Hash = hasher.Finalize();

            // we only keep the first 20 bytes of the Blake3 hash
            byte[] hash = blake3Hash.AsSpanUnsafe().Slice(0, 20).ToArray();
            return new BlobIdentifier(hash);
        }

        public static BlobIdentifier FromIoHash(IoHash blobIdentifier)
        {
            return new BlobIdentifier(blobIdentifier.ToByteArray());
        }

        public IoHash AsIoHash()
        {
            return new IoHash(HashData);
        }
    }

    public class BlobIdentifierTypeConverter : TypeConverter
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
                return new BlobIdentifier(s);
            }

            return base.ConvertFrom(context, culture, value);  
        }  
    }

    public class BlobIdentifierConverter : JsonConverter<BlobIdentifier?>
    {
        public override void WriteJson(JsonWriter writer, BlobIdentifier? value, JsonSerializer serializer)
        {
            writer.WriteValue(value!.ToString());
        }

        public override BlobIdentifier? ReadJson(JsonReader reader, Type objectType, BlobIdentifier? existingValue, bool hasExistingValue, JsonSerializer serializer)
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

            return new BlobIdentifier(s!);
        }
    }

    public class BlobIdentifierCbConverter : CbConverterBase<BlobIdentifier>
    {
        public override BlobIdentifier Read(CbField field) => new BlobIdentifier(field.AsHash().ToByteArray());

        /// <inheritdoc/>
        public override void Write(CbWriter writer, BlobIdentifier value) => writer.WriteBinaryAttachmentValue(new IoHash(value.HashData));

        /// <inheritdoc/>
        public override void WriteNamed(CbWriter writer, Utf8String name, BlobIdentifier value) => writer.WriteBinaryAttachment(name, new IoHash(value.HashData));
    }
}

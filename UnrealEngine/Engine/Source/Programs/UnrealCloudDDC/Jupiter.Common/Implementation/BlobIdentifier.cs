// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;
using Blake3;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

namespace Jupiter.Implementation
{
    [TypeConverter(typeof(BlobIdentifierTypeConverter))]
    [JsonConverter(typeof(BlobIdentifierJsonConverter))]
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

        public static BlobIdentifier FromBlobLocator(BlobLocator locator)
        {
            return new BlobIdentifier(Encoding.UTF8.GetBytes(locator.BlobId.ToString()));
        }

        public BlobLocator AsBlobLocator()
        {
            return new BlobLocator(HostId.Empty, new BlobId(Encoding.UTF8.GetString(HashData)));
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

        public override bool CanConvertTo(ITypeDescriptorContext? context, Type? destinationType)
        {
            if (destinationType == typeof(string))
            {
                return true;
            }
            return base.CanConvertTo(context, destinationType);
        }

        public override object? ConvertTo(ITypeDescriptorContext? context, CultureInfo? culture, object? value, Type destinationType)
        {
            if (destinationType == typeof(string))
            {
                BlobIdentifier? identifier = (BlobIdentifier?)value;
                return identifier?.ToString();
            }
            return base.ConvertTo(context, culture, value, destinationType);
        }
    }

    public class BlobIdentifierJsonConverter : JsonConverter<BlobIdentifier>
    {
        public override BlobIdentifier? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
        {
            string? str = reader.GetString();
            if (str == null)
            {
                throw new InvalidDataException("Unable to parse blob identifier");
            }

            return new BlobIdentifier(str);
        }

        public override void Write(Utf8JsonWriter writer, BlobIdentifier value, JsonSerializerOptions options)
        {
            writer.WriteStringValue(value.ToString());
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

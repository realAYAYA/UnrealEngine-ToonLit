// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using Blake3;
using Newtonsoft.Json;

namespace Jupiter.Implementation
{
    [JsonConverter(typeof(TreeHashConverter))]
    [TypeConverter(typeof(TreeHashTypeConverter))]
    public class TreeHash : ContentHash, IEquatable<TreeHash>
    {
        public TreeHash(byte[] identifier) : base(identifier)
        {
        }

        [JsonConstructor]
        public TreeHash(string identifier) : base(identifier)
        {
        }

        public override int GetHashCode()
        {
            return Comparer.GetHashCode(Identifier);
        }

        public bool Equals(TreeHash? other)
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

            return Equals((TreeHash) obj);
        }

         public static TreeHash FromTreeContents(TreeHash[] treeHashes, BlobIdentifier[] blobHashes)
        {
            int countOfHashes = treeHashes.Length + blobHashes.Length;
            byte[] bytes = new byte[countOfHashes * HashLength];
            int hashIndex = 0;
            foreach (TreeHash tree in treeHashes)
            {
                Array.Copy(tree.HashData, 0, bytes, hashIndex * HashLength, HashLength);
                ++hashIndex;
            }
            foreach (BlobIdentifier blob in blobHashes)
            {
                Array.Copy(blob.HashData, 0, bytes, hashIndex * HashLength, HashLength);
                ++hashIndex;
            }

            Hash blake3Hash = Hasher.Hash(bytes);

            // we only keep the first 20 bytes of the Blake3 hash
            TreeHash hash = new TreeHash(blake3Hash.AsSpanUnsafe().Slice(0, HashLength).ToArray());
            return hash;
        }
    }

    public class TreeHashConverter : JsonConverter<TreeHash>
    {
        public override void WriteJson(JsonWriter writer, TreeHash? value, JsonSerializer serializer)
        {
            writer.WriteValue(value!.ToString());
        }

        public override TreeHash ReadJson(JsonReader reader, Type objectType, TreeHash? existingValue, bool hasExistingValue, Newtonsoft.Json.JsonSerializer serializer)
        {
            string? s = (string?)reader.Value;

            return new TreeHash(s!);
        }
    }

    public sealed class TreeHashTypeConverter : TypeConverter
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
                return new TreeHash(s);
            }

            return base.ConvertFrom(context, culture, value);
        }
    }
}

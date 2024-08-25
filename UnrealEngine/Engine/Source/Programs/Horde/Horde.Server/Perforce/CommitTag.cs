// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Perforce;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Bson.Serialization.Serializers;

namespace Horde.Server.Perforce
{
	/// <summary>
	/// Constants for known commit tags
	/// </summary>
	[JsonSchemaString]
	[JsonConverter(typeof(CommitTagJsonConverter))]
	[BsonSerializer(typeof(CommitTagBsonSerializer))]
	public readonly struct CommitTag : IEquatable<CommitTag>
	{
		/// <summary>
		/// Predefined filter name for commits containing code
		/// </summary>
		public static CommitTag Code { get; } = new CommitTag("code");

		/// <summary>
		/// Predefined filter for code
		/// </summary>
		public static IReadOnlyList<string> CodeFilter { get; } = PerforceUtils.CodeExtensions.Select(x => $"*{x}").ToList();

		/// <summary>
		/// Predefined filter name for commits containing content
		/// </summary>
		public static CommitTag Content { get; } = new CommitTag("content");

		/// <summary>
		/// Predefined filter name for content
		/// </summary>
		public static IReadOnlyList<string> ContentFilter { get; } = Enumerable.Concat(new[] { "*" }, PerforceUtils.CodeExtensions.Select(x => $"-*{x}")).ToArray();

		/// <summary>
		/// The tag text
		/// </summary>
		public string Text { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="text"></param>
		public CommitTag(string text)
		{
			Text = text;
			StringId.ValidateArgument(new Utf8String(text), nameof(text));
		}

		/// <summary>
		/// Check if the current tag is empty
		/// </summary>
		public bool IsEmpty() => Text == null;

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is CommitTag id && Equals(id);

		/// <inheritdoc/>
		public override int GetHashCode() => String.GetHashCode(Text, StringComparison.Ordinal);

		/// <inheritdoc/>
		public bool Equals(CommitTag other) => String.Equals(Text, other.Text, StringComparison.Ordinal);

		/// <inheritdoc/>
		public override string ToString() => Text;

		/// <summary>
		/// Compares two string ids for equality
		/// </summary>
		/// <param name="left">The first string id</param>
		/// <param name="right">Second string id</param>
		/// <returns>True if the two string ids are equal</returns>
		public static bool operator ==(CommitTag left, CommitTag right) => left.Equals(right);

		/// <summary>
		/// Compares two string ids for inequality
		/// </summary>
		/// <param name="left">The first string id</param>
		/// <param name="right">Second string id</param>
		/// <returns>True if the two string ids are not equal</returns>
		public static bool operator !=(CommitTag left, CommitTag right) => !left.Equals(right);
	}

	/// <summary>
	/// Converts <see cref="CommitTag"/> values to and from JSON
	/// </summary>
	public class CommitTagJsonConverter : JsonConverter<CommitTag>
	{
		/// <inheritdoc/>
		public override CommitTag Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
		{
			return new CommitTag(reader.GetString() ?? String.Empty);
		}

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, CommitTag value, JsonSerializerOptions options)
		{
			writer.WriteStringValue(value.Text);
		}
	}

	/// <summary>
	/// Serializer for StringId objects
	/// </summary>
	public sealed class CommitTagBsonSerializer : SerializerBase<CommitTag>
	{
		/// <inheritdoc/>
		public override CommitTag Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
		{
			return new CommitTag(context.Reader.ReadString());
		}

		/// <inheritdoc/>
		public override void Serialize(BsonSerializationContext context, BsonSerializationArgs args, CommitTag value)
		{
			context.Writer.WriteString(value.ToString());
		}
	}
}

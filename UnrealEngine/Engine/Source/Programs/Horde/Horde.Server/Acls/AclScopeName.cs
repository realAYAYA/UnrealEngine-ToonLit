// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Serialization;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Server.Acls
{
	/// <summary>
	/// Name of an ACL scope
	/// </summary>
	[DebuggerDisplay("{Name}")]
	[JsonConverter(typeof(AclScopeNameJsonConverter))]
	public record struct AclScopeName(string Text)
	{
		/// <summary>
		/// The root scope name
		/// </summary>
		public static AclScopeName Root { get; } = new AclScopeName("horde");

		/// <summary>
		/// Append another name to this scope
		/// </summary>
		/// <param name="name">Name to append</param>
		/// <returns>New scope name</returns>
		public AclScopeName Append(string name) => new AclScopeName($"{Text}/{name}");

		/// <summary>
		/// Append another name to this scope
		/// </summary>
		/// <param name="type">Type of the scope</param>
		/// <param name="name">Name to append</param>
		/// <returns>New scope name</returns>
		public AclScopeName Append(string type, string name) => new AclScopeName($"{Text}/{type}:{name}");

		/// <inheritdoc/>
		public bool Equals(AclScopeName other) => Text.Equals(other.Text, StringComparison.Ordinal);

		/// <inheritdoc/>
		public override int GetHashCode() => Text.GetHashCode(StringComparison.Ordinal);

		/// <inheritdoc/>
		public override string ToString() => Text;
	}

	/// <summary>
	/// Serializes <see cref="AclScopeName"/> objects to JSON
	/// </summary>
	class AclScopeNameJsonConverter : JsonConverter<AclScopeName>
	{
		/// <inheritdoc/>
		public override AclScopeName Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new AclScopeName(reader.GetString() ?? String.Empty);

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, AclScopeName value, JsonSerializerOptions options) => writer.WriteStringValue(value.Text);
	}
}

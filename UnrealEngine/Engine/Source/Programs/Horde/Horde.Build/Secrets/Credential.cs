// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Horde.Build.Acls;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Build.Secrets
{
	/// <summary>
	/// Stores information about a credential
	/// </summary>
	public class Credential
	{
		/// <summary>
		/// Unique id for this credential
		/// </summary>
		[BsonId]
		public ObjectId Id { get; set; }

		/// <summary>
		/// Name of this credential
		/// </summary>
		[BsonRequired]
		public string Name { get; set; }

		/// <summary>
		/// The normalized name of this credential
		/// </summary>
		[BsonRequired]
		public string NormalizedName { get; set; }

		/// <summary>
		/// Properties for this credential
		/// </summary>
		public Dictionary<string, string> Properties { get; set; } = new Dictionary<string, string>();

		/// <summary>
		/// The ACL for this credential
		/// </summary>
		public Acl? Acl { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		[BsonConstructor]
		private Credential()
		{
			Name = null!;
			NormalizedName = null!;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name of this credential</param>
		public Credential(string name)
		{
			Id = ObjectId.GenerateNewId();
			Name = name;
			NormalizedName = GetNormalizedName(name);
		}

		/// <summary>
		/// Gets the normalized form of a name, used for case-insensitive comparisons
		/// </summary>
		/// <param name="name">The name to normalize</param>
		/// <returns>The normalized name</returns>
		public static string GetNormalizedName(string name)
		{
			return name.ToUpperInvariant();
		}
	}
}

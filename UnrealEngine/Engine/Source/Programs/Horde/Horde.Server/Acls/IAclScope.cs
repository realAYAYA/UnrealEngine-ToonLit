// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Security.Claims;
using System.Text.Json;
using System.Text.Json.Serialization;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Bson.Serialization.Serializers;

namespace Horde.Server.Acls
{
	/// <summary>
	/// Interface for parent classes that implement authorization
	/// </summary>
	public interface IAclScope
	{
		/// <summary>
		/// The parent scope object
		/// </summary>
		IAclScope? ParentScope { get; }

		/// <summary>
		/// Name of this scope
		/// </summary>
		AclScopeName ScopeName { get; }

		/// <summary>
		/// ACL for this scope
		/// </summary>
		AclConfig? Acl { get; }
	}

	/// <summary>
	/// Extension methods for ACL scopes
	/// </summary>
	public static class AclScopeExtensions
	{
		/// <summary>
		/// Authorize a particular operation against a scope
		/// </summary>
		/// <param name="scope">Scope to query</param>
		/// <param name="action">Action to check</param>
		/// <param name="principal">Principal to authorize</param>
		/// <returns>True if the given principal is allowed to perform a particular action</returns>
		public static bool Authorize(this IAclScope scope, AclAction action, ClaimsPrincipal principal)
		{
			if (principal.HasAdminClaim())
			{
				return true;
			}

			for (IAclScope? next = scope; next != null; next = next.ParentScope)
			{
				if (next.Acl != null)
				{
					bool? result = next.Acl.Authorize(action, principal);
					if (result.HasValue)
					{
						return result.Value;
					}
				}
			}
			return false;
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Security.Claims;

namespace Horde.Server.Acls
{
	/// <summary>
	/// Parameters to update an ACL
	/// </summary>
	public class AclConfig
	{
		/// <summary>
		/// Entries to replace the existing ACL
		/// </summary>
		public List<AclEntryConfig> Entries { get; set; } = new List<AclEntryConfig>();

		/// <summary>
		/// Whether to inherit permissions from the parent ACL
		/// </summary>
		public bool? Inherit { get; set; }

		/// <summary>
		/// List of exceptions to the inherited setting
		/// </summary>
		public List<AclAction>? Exceptions { get; set; }

		/// <summary>
		/// Tests whether a user is authorized to perform the given actions
		/// </summary>
		/// <param name="action">Action that is being performed. This should be a single flag.</param>
		/// <param name="user">The principal to authorize</param>
		/// <returns>True/false if the action is allowed or denied, null if there is no specific setting for this user</returns>
		public bool? Authorize(AclAction action, ClaimsPrincipal user)
		{
			// Check if there's a specific entry for this action
			foreach (AclEntryConfig entry in Entries)
			{
				if (entry.Actions.Contains(action) && user.HasClaim(entry.Claim.Type, entry.Claim.Value))
				{
					return true;
				}
			}

			// Otherwise check if we're prevented from inheriting permissions
			if (Inherit ?? true)
			{
				if (Exceptions != null && Exceptions.Contains(action))
				{
					return false;
				}
			}
			else
			{
				if (Exceptions == null || !Exceptions.Contains(action))
				{
					return false;
				}
			}

			// Otherwise allow to propagate up the hierarchy
			return null;
		}
	}

	/// <summary>
	/// Individual entry in an ACL
	/// </summary>
	public class AclEntryConfig
	{
		/// <summary>
		/// Name of the user or group
		/// </summary>
		[Required]
		public AclClaimConfig Claim { get; set; }

		/// <summary>
		/// Array of actions to allow
		/// </summary>
		[Required]
		public List<AclAction> Actions { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		public AclEntryConfig()
		{
			Claim = new AclClaimConfig();
			Actions = new List<AclAction>();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="claim">The claim this entry applies to</param>
		/// <param name="actions">List of allowed operations</param>
		public AclEntryConfig(AclClaimConfig claim, IEnumerable<AclAction> actions)
		{
			Claim = claim;
			Actions = new List<AclAction>(actions);
		}
	}

	/// <summary>
	/// New claim to create
	/// </summary>
	public class AclClaimConfig
	{
		/// <summary>
		/// The claim type
		/// </summary>
		[Required]
		public string Type { get; set; } = null!;

		/// <summary>
		/// The claim value
		/// </summary>
		[Required]
		public string Value { get; set; } = null!;

		/// <summary>
		/// Constructor
		/// </summary>
		public AclClaimConfig()
		{
			Type = String.Empty;
			Value = String.Empty;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="claim">The claim object</param>
		public AclClaimConfig(Claim claim)
			: this(claim.Type, claim.Value)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type">The claim type</param>
		/// <param name="value">The claim value</param>
		public AclClaimConfig(string type, string value)
		{
			Type = type;
			Value = value;
		}

		/// <summary>
		/// Converts this object to a regular <see cref="Claim"/> object.
		/// </summary>
		public Claim ToClaim() => new Claim(Type, Value);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;

namespace Horde.Build.Acls
{
	/// <summary>
	/// New claim to create
	/// </summary>
	public class CreateAclClaimRequest
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
	}

	/// <summary>
	/// Individual entry in an ACL
	/// </summary>
	public class CreateAclEntryRequest
	{
		/// <summary>
		/// Name of the user or group
		/// </summary>
		[Required]
		public CreateAclClaimRequest Claim { get; set; } = null!;

		/// <summary>
		/// Array of actions to allow
		/// </summary>
		[Required]
		public string[] Actions { get; set; } = null!;
	}

	/// <summary>
	/// Parameters to update an ACL
	/// </summary>
	public class UpdateAclRequest
	{
		/// <summary>
		/// Entries to replace the existing ACL
		/// </summary>
		public List<CreateAclEntryRequest>? Entries { get; set; }

		/// <summary>
		/// Whether to inherit permissions from the parent ACL
		/// </summary>
		public bool? Inherit { get; set; }

		/// <summary>
		/// List of exceptions to the inherited setting
		/// </summary>
		public List<string>? Exceptions { get; set; }
	}

	/// <summary>
	/// New claim to update
	/// </summary>
	public class GetAclClaimResponse
	{
		/// <summary>
		/// The claim type
		/// </summary>
		public string Type { get; set; }

		/// <summary>
		/// The claim value
		/// </summary>
		public string Value { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="claim">The claim to construct from</param>
		public GetAclClaimResponse(AclClaim claim)
			: this(claim.Type, claim.Value)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type">The claim type</param>
		/// <param name="value">The claim value</param>
		public GetAclClaimResponse(string type, string value)
		{
			Type = type;
			Value = value;
		}
	}

	/// <summary>
	/// Individual entry in an ACL
	/// </summary>
	public class GetAclEntryResponse
	{
		/// <summary>
		/// Names of the user or group
		/// </summary>
		public GetAclClaimResponse Claim { get; set; }

		/// <summary>
		/// Array of actions to allow
		/// </summary>
		public List<string> Actions { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="aclEntry">The acl entry to construct from</param>
		public GetAclEntryResponse(AclEntry aclEntry)
		{
			Claim = new GetAclClaimResponse(aclEntry.Claim.Type, aclEntry.Claim.Value);
			Actions = AclEntry.GetActionNames(aclEntry.Actions);
		}
	}

	/// <summary>
	/// Information about an ACL
	/// </summary>
	public class GetAclResponse
	{
		/// <summary>
		/// Entries to replace the existing ACL
		/// </summary>
		[Required]
		public List<GetAclEntryResponse> Entries { get; set; }

		/// <summary>
		/// Whether to inherit permissions from the parent entity
		/// </summary>
		public bool Inherit { get; set; }

		/// <summary>
		/// Exceptions from permission inheritance setting
		/// </summary>
		public List<string>? Exceptions { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="acl">The ACL to construct from</param>
		public GetAclResponse(Acl acl)
		{
			Entries = acl.Entries.ConvertAll(x => new GetAclEntryResponse(x));
			Inherit = acl.Inherit;
			Exceptions = acl.Exceptions?.ConvertAll(x => x.ToString());
		}
	}
}

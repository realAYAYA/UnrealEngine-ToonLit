// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using Horde.Build.Acls;
using Horde.Build.Secrets;

namespace Horde.Build.Credentials
{
	/// <summary>
	/// Parameters to create a new credential
	/// </summary>
	public class CreateCredentialRequest
	{
		/// <summary>
		/// Name for the new credential
		/// </summary>
		[Required]
		public string Name { get; set; } = null!;

		/// <summary>
		/// Properties for the new credential
		/// </summary>
		public Dictionary<string, string>? Properties { get; set; }
	}

	/// <summary>
	/// Response from creating a new credential
	/// </summary>
	public class CreateCredentialResponse
	{
		/// <summary>
		/// Unique id for the new credential
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Unique id for the new credential</param>
		public CreateCredentialResponse(string id)
		{
			Id = id;
		}
	}

	/// <summary>
	/// Parameters to update a credential
	/// </summary>
	public class UpdateCredentialRequest
	{
		/// <summary>
		/// Optional new name for the credential
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// Properties to update for the credential. Properties set to null will be removed.
		/// </summary>
		public Dictionary<string, string?>? Properties { get; set; }

		/// <summary>
		/// Custom permissions for this object
		/// </summary>
		public UpdateAclRequest? Acl { get; set; }
	}

	/// <summary>
	/// Response describing a credential
	/// </summary>
	public class GetCredentialResponse
	{
		/// <summary>
		/// Unique id of the credential
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Name of the credential
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Properties for the credential
		/// </summary>
		public Dictionary<string, string> Properties { get; set; }

		/// <summary>
		/// Custom permissions for this object
		/// </summary>
		public GetAclResponse? Acl { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="credential">The credential to construct from</param>
		/// <param name="bIncludeAcl">Whether to include the ACL in the response</param>
		public GetCredentialResponse(Credential credential, bool bIncludeAcl)
		{
			Id = credential.Id.ToString();
			Name = credential.Name;
			Properties = credential.Properties;
			Acl = (bIncludeAcl && credential.Acl != null)? new GetAclResponse(credential.Acl) : null;
		}
	}
}

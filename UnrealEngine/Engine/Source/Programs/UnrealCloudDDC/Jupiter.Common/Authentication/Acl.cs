// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Security.Claims;
using Microsoft.AspNetCore.Authorization;

namespace Jupiter;

public enum JupiterAclAction
{
	/// <summary>
	/// General read access to refs / blobs and so on
	/// </summary>
	ReadObject,
	/// <summary>
	/// General write access to upload refs / blobs etc
	/// </summary>
	WriteObject,
	/// <summary>
	/// Access to delete blobs / refs etc
	/// </summary>
	DeleteObject,

	/// <summary>
	/// Access to delete a particular bucket
	/// </summary>
	DeleteBucket,
	/// <summary>
	/// Access to delete a whole namespace
	/// </summary>
	DeleteNamespace,

	/// <summary>
	/// Access to read the transaction log
	/// </summary>
	ReadTransactionLog,

	/// <summary>
	/// Access to write the transaction log
	/// </summary>
	WriteTransactionLog,

	/// <summary>
	/// Access to perform administrative task
	/// </summary>
	AdminAction
}

public class AclEntry
{
	/// <summary>
	/// Claims required to be present to be allowed to do the actions - if multiple claims are present *all* of them are required
	/// </summary>
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by the configuration system")]
	public List<string> Claims { get; set; } = new List<string>();

	/// <summary>
	/// The actions granted if the claims match
	/// </summary>
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by the configuration system")]
	public List<JupiterAclAction> Actions { get; set; } = new List<JupiterAclAction>();

	public IEnumerable<JupiterAclAction> Resolve(ClaimsPrincipal user)
	{
		List<JupiterAclAction> allowedActions = new List<JupiterAclAction>();

		bool allClaimsFound = true;
		// These are ANDed, e.g. all claims needs to be present
		foreach (string expectedClaim in Claims)
		{
			bool claimFound = false;
			// if expected claim is * then everyone has the associated actions
			if (expectedClaim == "*")
			{
				claimFound = true;
			}
			else if (expectedClaim.Contains('=', StringComparison.InvariantCultureIgnoreCase))
			{
				int separatorIndex = expectedClaim.IndexOf('=', StringComparison.InvariantCultureIgnoreCase);
				string claimName = expectedClaim.Substring(0, separatorIndex);
				string claimValue = expectedClaim.Substring(separatorIndex + 1);
				if (user.HasClaim(claim => string.Equals(claim.Type, claimName, StringComparison.OrdinalIgnoreCase) && string.Equals(claim.Value, claimValue, StringComparison.OrdinalIgnoreCase)))
				{
					claimFound = true;
				}
			}
			else if (user.HasClaim(claim => string.Equals(claim.Type, expectedClaim, StringComparison.OrdinalIgnoreCase)))
			{
				claimFound = true;
			}

			if (!claimFound)
			{
				allClaimsFound = false;
			}
		}

		if (allClaimsFound)
		{
			allowedActions.AddRange(Actions);
		}
		return allowedActions;
	}

	public IEnumerable<JupiterAclAction> Resolve(AuthorizationHandlerContext context)
	{
		return Resolve(context.User);
	}
}
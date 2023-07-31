// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Credentials;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using MongoDB.Bson;

namespace Horde.Build.Secrets
{
	/// <summary>
	/// Controller for the /api/v1/credentials endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class CredentialsController : ControllerBase
	{
		/// <summary>
		/// Singleton instance of the ACL service
		/// </summary>
		private readonly AclService _aclService;

		/// <summary>
		/// Singleton instance of the credential service
		/// </summary>
		private readonly CredentialService _credentialService;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="aclService">The ACL service</param>
		/// <param name="credentialService">The credential service</param>
		public CredentialsController(AclService aclService, CredentialService credentialService)
		{
			_aclService = aclService;
			_credentialService = credentialService;
		}

		/// <summary>
		/// Creates a new credential
		/// </summary>
		/// <param name="create">Parameters for the new credential.</param>
		/// <returns>Http result code</returns>
		[HttpPost]
		[Route("/api/v1/credentials")]
		public async Task<ActionResult<CreateCredentialResponse>> CreateCredentialAsync([FromBody] CreateCredentialRequest create)
		{
			if(!await _aclService.AuthorizeAsync(AclAction.CreateCredential, User))
			{
				return Forbid();
			}

			Credential newCredential = await _credentialService.CreateCredentialAsync(create.Name, create.Properties);
			return new CreateCredentialResponse(newCredential.Id.ToString());
		}

		/// <summary>
		/// Query all the credentials
		/// </summary>
		/// <param name="name">Id of the credential to get information about</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about all the credentials</returns>
		[HttpGet]
		[Route("/api/v1/credentials")]
		[ProducesResponseType(typeof(List<GetCredentialResponse>), 200)]
		public async Task<ActionResult<object>> FindCredentialAsync([FromQuery] string? name = null, [FromQuery] PropertyFilter? filter = null)
		{
			if (!await _aclService.AuthorizeAsync(AclAction.ListCredentials, User))
			{
				return Forbid();
			}

			List<Credential> credentials = await _credentialService.FindCredentialsAsync(name);
			GlobalPermissionsCache cache = new GlobalPermissionsCache();

			List<object> responses = new List<object>();
			foreach (Credential credential in credentials)
			{
				if (await _credentialService.AuthorizeAsync(credential, AclAction.ViewCredential, User, cache))
				{
					bool bIncludeAcl = await _credentialService.AuthorizeAsync(credential, AclAction.ViewPermissions, User, cache);
					responses.Add(new GetCredentialResponse(credential, bIncludeAcl).ApplyFilter(filter));
				}
			}
			return responses;
		}

		/// <summary>
		/// Retrieve information about a specific credential
		/// </summary>
		/// <param name="credentialId">Id of the credential to get information about</param>
		/// <param name="filter">Filter for properties to return</param>
		/// <returns>Information about the requested credential</returns>
		[HttpGet]
		[Route("/api/v1/credentials/{credentialId}")]
		[ProducesResponseType(typeof(GetCredentialResponse), 200)]
		public async Task<ActionResult<object>> GetCredentialAsync(string credentialId, [FromQuery] PropertyFilter? filter = null)
		{
			ObjectId projectIdValue = credentialId.ToObjectId();

			Credential? credential = await _credentialService.GetCredentialAsync(projectIdValue);
			if (credential == null)
			{
				return NotFound();
			}

			GlobalPermissionsCache cache = new GlobalPermissionsCache();
			if (!await _credentialService.AuthorizeAsync(credential, AclAction.ViewCredential, User, cache))
			{
				return Forbid();
			}

			bool bIncludeAcl = await _credentialService.AuthorizeAsync(credential, AclAction.ViewPermissions, User, cache);
			return new GetCredentialResponse(credential, bIncludeAcl).ApplyFilter(filter);
		}

		/// <summary>
		/// Update a credential's properties.
		/// </summary>
		/// <param name="credentialId">Id of the credential to update</param>
		/// <param name="update">Items on the credential to update</param>
		/// <returns>Http result code</returns>
		[HttpPut]
		[Route("/api/v1/credentials/{credentialId}")]
		public async Task<ActionResult> UpdateCredentialAsync(string credentialId, [FromBody] UpdateCredentialRequest update)
		{
			ObjectId credentialIdValue = credentialId.ToObjectId();

			Credential? credential = await _credentialService.GetCredentialAsync(credentialIdValue);
			if(credential == null)
			{
				return NotFound();
			}

			GlobalPermissionsCache cache = new GlobalPermissionsCache();
			if (!await _credentialService.AuthorizeAsync(credential, AclAction.UpdateCredential, User, cache))
			{
				return Forbid();
			}
			if (update.Acl != null && !await _credentialService.AuthorizeAsync(credential, AclAction.ChangePermissions, User, cache))
			{
				return Forbid();
			}

			await _credentialService.UpdateCredentialAsync(credentialIdValue, update.Name, update.Properties, Acl.Merge(credential.Acl, update.Acl));
			return new OkResult();
		}

		/// <summary>
		/// Delete a credential
		/// </summary>
		/// <param name="credentialId">Id of the credential to delete</param>
		/// <returns>Http result code</returns>
		[HttpDelete]
		[Route("/api/v1/credentials/{credentialId}")]
		public async Task<ActionResult> DeleteCredentialAsync(string credentialId)
		{
			ObjectId credentialIdValue = credentialId.ToObjectId();

			Credential? credential = await _credentialService.GetCredentialAsync(credentialIdValue);
			if (credential == null)
			{
				return NotFound();
			}
			if (!await _credentialService.AuthorizeAsync(credential, AclAction.DeleteCredential, User, null))
			{
				return Forbid();
			}
			if (!await _credentialService.DeleteCredentialAsync(credentialIdValue))
			{
				return NotFound();
			}
			return new OkResult();
		}
	}
}

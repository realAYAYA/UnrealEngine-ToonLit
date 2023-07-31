// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Server;
using MongoDB.Bson;
using MongoDB.Driver;

namespace Horde.Build.Secrets
{
	/// <summary>
	/// Service for accessing and modifying credentials
	/// </summary>
	public class CredentialService
	{
		/// <summary>
		/// The ACL service instance
		/// </summary>
		readonly AclService _aclService;

		/// <summary>
		/// Collection of credential documents
		/// </summary>
		readonly IMongoCollection<Credential> _credentials; 

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="aclService">The ACL service instance</param>
		/// <param name="mongoService">The database service instance</param>
		public CredentialService(AclService aclService, MongoService mongoService)
		{
			_aclService = aclService;

			_credentials = mongoService.Credentials;

			if (!mongoService.ReadOnlyMode)
			{
				_credentials.Indexes.CreateOne(new CreateIndexModel<Credential>(Builders<Credential>.IndexKeys.Ascending(x => x.NormalizedName), new CreateIndexOptions { Unique = true }));
			}
		}

		/// <summary>
		/// Creates a new credential
		/// </summary>
		/// <param name="name">Name of the new credential</param>
		/// <param name="properties">Properties for the new credential</param>
		/// <returns>The new credential document</returns>
		public async Task<Credential> CreateCredentialAsync(string name, Dictionary<string, string>? properties)
		{
			Credential newCredential = new Credential(name);
			if (properties != null)
			{
				newCredential.Properties = new Dictionary<string, string>(properties, newCredential.Properties.Comparer);
			}

			await _credentials.InsertOneAsync(newCredential);
			return newCredential;
		}

		/// <summary>
		/// Updates an existing credential
		/// </summary>
		/// <param name="id">Unique id of the credential</param>
		/// <param name="newName">The new name for the credential</param>
		/// <param name="newProperties">Properties on the credential to update. Any properties with a value of null will be removed.</param>
		/// <param name="newAcl">The new ACL for the credential</param>
		/// <returns>Async task object</returns>
		public async Task UpdateCredentialAsync(ObjectId id, string? newName, Dictionary<string, string?>? newProperties, Acl? newAcl)
		{
			UpdateDefinitionBuilder<Credential> updateBuilder = Builders<Credential>.Update;

			List<UpdateDefinition<Credential>> updates = new List<UpdateDefinition<Credential>>();
			if (newName != null)
			{
				updates.Add(updateBuilder.Set(x => x.Name, newName));
				updates.Add(updateBuilder.Set(x => x.NormalizedName, Credential.GetNormalizedName(newName)));
			}
			if (newProperties != null)
			{
				foreach (KeyValuePair<string, string?> pair in newProperties)
				{
					if (pair.Value == null)
					{
						updates.Add(updateBuilder.Unset(x => x.Properties[pair.Key]));
					}
					else
					{
						updates.Add(updateBuilder.Set(x => x.Properties[pair.Key], pair.Value));
					}
				}
			}
			if (newAcl != null)
			{
				updates.Add(Acl.CreateUpdate<Credential>(x => x.Acl!, newAcl));
			}

			if (updates.Count > 0)
			{
				await _credentials.FindOneAndUpdateAsync<Credential>(x => x.Id == id, updateBuilder.Combine(updates));
			}
		}

		/// <summary>
		/// Gets all the available credentials
		/// </summary>
		/// <returns>List of project documents</returns>
		public Task<List<Credential>> FindCredentialsAsync(string? name)
		{
			FilterDefinitionBuilder<Credential> filterBuilder = Builders<Credential>.Filter;

			FilterDefinition<Credential> filter = FilterDefinition<Credential>.Empty;
			if (name != null)
			{
				filter &= filterBuilder.Eq(x => x.NormalizedName, Credential.GetNormalizedName(name));
			}

			return _credentials.Find(filter).ToListAsync();
		}

		/// <summary>
		/// Gets a credential by ID
		/// </summary>
		/// <param name="id">Unique id of the credential</param>
		/// <returns>The credential document</returns>
		public async Task<Credential?> GetCredentialAsync(ObjectId id)
		{
			return await _credentials.Find<Credential>(x => x.Id == id).FirstOrDefaultAsync();
		}

		/// <summary>
		/// Gets a credential by name
		/// </summary>
		/// <param name="name">Name of the credential</param>
		/// <returns>The credential document</returns>
		public async Task<Credential?> GetCredentialAsync(string name)
		{
			string normalizedName = Credential.GetNormalizedName(name);
			return await _credentials.Find<Credential>(x => x.NormalizedName == normalizedName).FirstOrDefaultAsync();
		}

		/// <summary>
		/// Deletes a credential by id
		/// </summary>
		/// <param name="id">Unique id of the credential</param>
		/// <returns>True if the credential was deleted</returns>
		public async Task<bool> DeleteCredentialAsync(ObjectId id)
		{
			DeleteResult result = await _credentials.DeleteOneAsync<Credential>(x => x.Id == id);
			return result.DeletedCount > 0;
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular credential
		/// </summary>
		/// <param name="credential">The credential to check</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <param name="cache">Permissions cache</param>
		/// <returns>True if the action is authorized</returns>
		public Task<bool> AuthorizeAsync(Credential credential, AclAction action, ClaimsPrincipal user, GlobalPermissionsCache? cache)
		{
			bool? result = credential.Acl?.Authorize(action, user);
			if (result == null)
			{
				return _aclService.AuthorizeAsync(action, user, cache);
			}
			else
			{
				return Task.FromResult(result.Value);
			}
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using MongoDB.Bson;

namespace Horde.Build.Users
{
	/// <summary>
	/// Interface for a collection of service account documents
	/// </summary>
	public interface IServiceAccountCollection
	{
		/// <summary>
		/// Adds a new service account to the collection
		/// </summary>
		/// <param name="secretToken">Secret token used for identifying this service account</param>
		/// <param name="claims">Dictionary of claims to assign</param>
		/// <param name="description">Description of the account</param>
		Task<IServiceAccount> AddAsync(string secretToken, List<string> claims, string description);

		/// <summary>
		/// Get service account via ID
		/// </summary>
		/// <param name="id">The unique service account id</param>
		/// <returns>The service account</returns>
		Task<IServiceAccount?> GetAsync(ObjectId id);
		
		/// <summary>
		/// Get service account via secret token
		/// </summary>
		/// <param name="secretToken">Secret token to use for searching</param>
		/// <returns>The service account</returns>
		Task<IServiceAccount?> GetBySecretTokenAsync(string secretToken);

		/// <summary>
		/// Update a service account from the collection
		/// </summary>
		/// <param name="id">The service account</param>
		/// <param name="secretToken">If set, secret token will be set</param>
		/// <param name="claims">If set, claims will be set</param>
		/// <param name="enabled">If set, enabled flag will be set</param>
		/// <param name="description">If set, description will be set</param>
		/// <returns>Async task</returns>
		Task UpdateAsync(ObjectId id, string? secretToken, List<string>? claims, bool? enabled, string? description);

		/// <summary>
		/// Delete a service account from the collection
		/// </summary>
		/// <param name="id">The service account</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(ObjectId id);
	}
}

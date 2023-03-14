// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using MongoDB.Bson;

namespace Horde.Build.Users
{
	/// <summary>
	/// A service account used for authenticating during server-to-server communication
	/// For example between Robomerge and Horde.
	/// </summary>
	public interface IServiceAccount
	{
		/// <summary>
		/// Unique id for this session
		/// </summary>
		public ObjectId Id { get; }
		
		/// <summary>
		/// Secret token used for identifying calls made as the service account
		/// </summary>
		public string SecretToken { get; }
		
		/// <summary>
		/// If the service account is active
		/// </summary>
		public bool Enabled { get; }
		
		/// <summary>
		/// Description of the service account (who is it for, is there an owner etc)
		/// </summary>
		public string Description { get; }

		/// <summary>
		/// Add a claim
		/// </summary>
		/// <param name="type">Type of claim</param>
		/// <param name="value">Value of claim</param>
		/// <returns></returns>
		public void AddClaim(string type, string value);
		
		/// <summary>
		/// Get list of claims
		/// </summary>
		/// <returns>List of claims</returns>
		public IReadOnlyList<(string Type, string Value)> GetClaims();
	}
}

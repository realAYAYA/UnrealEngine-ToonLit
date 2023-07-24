// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Server;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Users
{
	/// <summary>
	/// Collection of service account documents
	/// </summary>
	public class ServiceAccountCollection : IServiceAccountCollection
	{
		/// <summary>
		/// Concrete implementation of IServiceAccount
		/// </summary>
		class ServiceAccountDocument : IServiceAccount
		{
			public const string ClaimSeparator = "###";
			
			[BsonRequired, BsonId]
			public ObjectId Id { get; set; }

			[BsonRequired]
			public string SecretToken { get; set; } = "empty";

			[BsonRequired]
			public List<string> Claims { get; set; } = new List<string>();
			
			public bool Enabled { get; set; }
			public string Description { get; set; } = "empty";

			[BsonConstructor]
			private ServiceAccountDocument()
			{
			}

			public ServiceAccountDocument(ObjectId id, string secretToken, List<string> claims, bool enabled, string description)
			{
				Id = id;
				SecretToken = secretToken;
				Claims = claims;
				Enabled = enabled;
				Description = description;
			}
			
			/// <inheritdoc/>
			public void AddClaim(string type, string value)
			{
				Claims.Add(type + ClaimSeparator + value);
			}

			/// <inheritdoc/>
			public IReadOnlyList<(string Type, string Value)> GetClaims()
			{
				return Claims.Select(x =>
				{
					string[] split = x.Split(ClaimSeparator);
					return (split[0], split[1]);
				}).ToList();
			}

			protected bool Equals(ServiceAccountDocument other)
			{
				bool areClaimsEqual = !Claims.Except(other.Claims).Any();

                return Id.Equals(other.Id) && SecretToken == other.SecretToken && areClaimsEqual && Enabled == other.Enabled && Description == other.Description;
			}

			public override bool Equals(object? obj)
			{
				if (obj is null)
				{
					return false;
				}
				if (ReferenceEquals(this, obj))
				{
					return true;
				}
				if (obj.GetType() != GetType())
				{
					return false;
				}
				return Equals((ServiceAccountDocument) obj);
			}

			public override int GetHashCode()
			{
				return HashCode.Combine(Id, SecretToken, Claims, Enabled, Description);
			}
		}

		/// <summary>
		/// Collection of session documents
		/// </summary>
		private readonly IMongoCollection<ServiceAccountDocument> _serviceAccounts;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service</param>
		public ServiceAccountCollection(MongoService mongoService)
		{
			_serviceAccounts = mongoService.GetCollection<ServiceAccountDocument>("ServiceAccounts", keys => keys.Ascending(x => x.SecretToken));
		}

		/// <inheritdoc/>
		public async Task<IServiceAccount> AddAsync(string secretToken, List<string> claims, string description)
		{
			ServiceAccountDocument newSession = new ServiceAccountDocument(ObjectId.GenerateNewId(), secretToken, claims, true, description);
			await _serviceAccounts.InsertOneAsync(newSession);
			return newSession;
		}

		/// <inheritdoc/>
		public async Task<IServiceAccount?> GetAsync(ObjectId id)
		{
			return await _serviceAccounts.Find(x => x.Id == id).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<IServiceAccount?> GetBySecretTokenAsync(string secretToken)
		{
			return await _serviceAccounts.Find(x => x.SecretToken == secretToken).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public Task UpdateAsync(ObjectId id, string? secretToken, List<string>? claims, bool? enabled, string? description)
		{
			UpdateDefinitionBuilder<ServiceAccountDocument> update = Builders<ServiceAccountDocument>.Update;
			List<UpdateDefinition<ServiceAccountDocument>> updates = new List<UpdateDefinition<ServiceAccountDocument>>();

			if (secretToken != null)
			{
				updates.Add(update.Set(x => x.SecretToken, secretToken));
			}
			if (claims != null)
			{
				updates.Add(update.Set(x => x.Claims, claims));
			}
			if (enabled != null)
			{
				updates.Add(update.Set(x => x.Enabled, enabled));
			}
			if (description != null)
			{
				updates.Add(update.Set(x => x.Description, description));
			}

			return _serviceAccounts.FindOneAndUpdateAsync(x => x.Id == id, update.Combine(updates));
		}

		/// <inheritdoc/>
		public Task DeleteAsync(ObjectId sessionId)
		{
			return _serviceAccounts.DeleteOneAsync(x => x.Id == sessionId);
		}
	}
}

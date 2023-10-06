// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Common;
using EpicGames.Perforce;
using Horde.Server.Acls;
using Horde.Server.Configuration;
using Horde.Server.Utilities;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.Tokens;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Server.Server
{
	/// <summary>
	/// Accessor for the global <see cref="Globals"/> singleton
	/// </summary>
	public class GlobalsService
	{
		/// <summary>
		/// Global server settings
		/// </summary>
		[SingletonDocument("globals", "5e3981cb28b8ec59cd07184a")]
		class Globals : SingletonBase, IGlobals
		{
			[BsonIgnore]
			public GlobalsService _owner = null!;

			public ObjectId InstanceId { get; set; }
			public string ConfigRevision { get; set; } = String.Empty;
			public byte[]? JwtSigningKey { get; set; }
			public int? SchemaVersion { get; set; }

			[BsonIgnore]
			string IGlobals.JwtIssuer => _owner._jwtIssuer;

			[BsonIgnore]
			SymmetricSecurityKey IGlobals.JwtSigningKey => new SymmetricSecurityKey(_owner._fixedJwtSecret ?? JwtSigningKey);

			public Globals()
			{
				InstanceId = ObjectId.GenerateNewId();
			}

			public Globals Clone()
			{
				return (Globals)MemberwiseClone();
			}

			public void RotateSigningKey()
			{
				JwtSigningKey = RandomNumberGenerator.GetBytes(128);
			}
		}

		readonly MongoService _mongoService;
		readonly string _jwtIssuer;
		readonly byte[]? _fixedJwtSecret;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService"></param>
		/// <param name="settings">Global settings instance</param>
		public GlobalsService(MongoService mongoService, IOptions<ServerSettings> settings)
		{
			_mongoService = mongoService;

			if (String.IsNullOrEmpty(settings.Value.JwtIssuer))
			{
				_jwtIssuer = Dns.GetHostName();
			}
			else
			{
				_jwtIssuer = settings.Value.JwtIssuer;
			}

			if (!String.IsNullOrEmpty(settings.Value.JwtSecret))
			{
				_fixedJwtSecret = Convert.FromBase64String(settings.Value.JwtSecret);
			}
		}

		/// <summary>
		/// Gets the current globals instance
		/// </summary>
		/// <returns>Globals instance</returns>
		public async ValueTask<IGlobals> GetAsync()
		{
			Globals globals = await _mongoService.GetSingletonAsync<Globals>(() => CreateGlobals());
			globals._owner = this;
			return globals;
		}

		static Globals CreateGlobals()
		{
			Globals globals = new Globals();
			globals.RotateSigningKey();
			return globals;
		}

		/// <summary>
		/// Try to update the current globals object
		/// </summary>
		/// <param name="globals">The current options value</param>
		/// <param name="configRevision"></param>
		/// <returns></returns>
		public async ValueTask<IGlobals?> TryUpdateAsync(IGlobals globals, string? configRevision)
		{
			Globals concreteGlobals = ((Globals)globals).Clone();
			if (configRevision != null)
			{
				concreteGlobals.ConfigRevision = configRevision;
			}
			if (!await _mongoService.TryUpdateSingletonAsync(concreteGlobals))
			{
				return null;
			}
			return concreteGlobals;
		}
	}
}

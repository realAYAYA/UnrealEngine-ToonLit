// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.IdentityModel.Tokens.Jwt;
using System.Linq;
using System.Security.Claims;
using System.Security.Cryptography;
using EpicGames.Horde.Common;
using EpicGames.Perforce;
using Horde.Server.Acls;
using Horde.Server.Utilities;
using Microsoft.IdentityModel.Tokens;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Server.Server
{
	/// <summary>
	/// Global server settings
	/// </summary>
	public interface IGlobals
	{
		/// <summary>
		/// Unique instance id of this database
		/// </summary>
		ObjectId InstanceId { get; }

		/// <summary>
		/// Issuer for JWT keys. Can be overridden by config file.
		/// </summary>
		string JwtIssuer { get; }

		/// <summary>
		/// The signing key for this server cluster
		/// </summary>
		public SymmetricSecurityKey JwtSigningKey { get; }
	}
}

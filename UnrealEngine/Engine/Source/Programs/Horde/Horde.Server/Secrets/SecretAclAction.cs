// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Secrets
{
	/// <summary>
	/// ACL actions for secrets
	/// </summary>
	public static class SecretAclAction
	{
		/// <summary>
		/// View a credential
		/// </summary>
		public static AclAction ViewSecret { get; } = new AclAction("ViewSecret");
	}
}

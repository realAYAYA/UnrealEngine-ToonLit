// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents.Sessions;
using Horde.Server.Utilities;

namespace Horde.Server.Agents.Sessions
{
	/// <summary>
	/// Server helper methods for <see cref="SessionId"/>
	/// </summary>
	static class SessionIdUtils
	{
		/// <summary>
		/// Creates a new <see cref="SessionId"/>
		/// </summary>
		public static SessionId GenerateNewId() => new SessionId(BinaryIdUtils.CreateNew());
	}
}

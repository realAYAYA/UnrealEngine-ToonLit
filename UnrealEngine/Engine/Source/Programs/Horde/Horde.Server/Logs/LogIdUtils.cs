// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Logs;
using Horde.Server.Utilities;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Server helper methods for <see cref="LogId"/>
	/// </summary>
	static class LogIdUtils
	{
		/// <summary>
		/// Creates a new <see cref="LogId"/>
		/// </summary>
		public static LogId GenerateNewId() => new LogId(BinaryIdUtils.CreateNew());
	}
}

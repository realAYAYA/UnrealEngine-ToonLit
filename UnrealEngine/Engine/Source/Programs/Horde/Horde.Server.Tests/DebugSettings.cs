// Copyright Epic Games, Inc. All Rights Reserved.

namespace Horde.Server.Tests
{
	/// <summary>
	/// Settings used for debug tests
	///
	/// Put into a separate file to avoid accidental commit of credentials.
	/// </summary>
	public static class DebugSettings
	{
		public static readonly string DbUsername = "username-not-set";
		public static readonly string DbPassword = "password-not-set";
		public static readonly string DbHostname = "";
	}
}
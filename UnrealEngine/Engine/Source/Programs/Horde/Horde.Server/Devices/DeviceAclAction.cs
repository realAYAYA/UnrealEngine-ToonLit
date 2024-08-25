// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Devices
{
	/// <summary>
	/// ACL actions which apply to devices
	/// </summary>
	public static class DeviceAclAction
	{
		/// <summary>
		/// Ability to read devices
		/// </summary>
		public static AclAction DeviceRead { get; } = new AclAction("DeviceRead");

		/// <summary>
		/// Ability to write devices
		/// </summary>
		public static AclAction DeviceWrite { get; } = new AclAction("DeviceWrite");
	}
}

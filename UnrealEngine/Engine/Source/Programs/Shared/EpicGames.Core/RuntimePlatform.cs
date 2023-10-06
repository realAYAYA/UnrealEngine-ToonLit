// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;

namespace EpicGames.Core
{
	/// <summary>
	/// Allows testing different platforsm
	/// </summary>
	public static class RuntimePlatform
	{
		/// <summary>
		/// Whether we are currently running on Linux.
		/// </summary>
		public static readonly bool IsLinux = RuntimeInformation.IsOSPlatform(OSPlatform.Linux);

		/// <summary>
		/// Whether we are currently running on a MacOS platform.
		/// </summary>
		public static readonly bool IsMac = RuntimeInformation.IsOSPlatform(OSPlatform.OSX);

		/// <summary>
		/// Whether we are currently running a Windows platform.
		/// </summary>
		public static readonly bool IsWindows = RuntimeInformation.IsOSPlatform(OSPlatform.Windows);

		/// <summary>
		/// The platform type
		/// </summary>
		public enum Type
		{
			/// <summary>
			/// Windows
			/// </summary>
			Windows, 
			
			/// <summary>
			/// Linux
			/// </summary>
			Linux, 
			
			/// <summary>
			/// Mac
			/// </summary>
			Mac
		};

		/// <summary>
		/// The current runtime platform
		/// </summary>
		public static readonly Type Current = IsWindows ? Type.Windows : IsMac ? Type.Mac : Type.Linux;

		/// <summary>
		/// The extension executables have on the current platform
		/// </summary>
		public static readonly string ExeExtension = IsWindows ? ".exe" : "";
	}
}

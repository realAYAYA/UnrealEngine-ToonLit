// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Runtime.InteropServices;

namespace EpicGames.Core
{
	/// <summary>
	/// Additional functionality around <see cref="System.IO.Compression.ZipArchive"/> to support non-Windows filesystems
	/// </summary>
	public static class ZipArchiveExtensions
	{
		/// <summary>
		/// Create a zip archive entry, preserving platform mode bits
		/// </summary>
		/// <param name="destination"></param>
		/// <param name="sourceFileName"></param>
		/// <param name="entryName"></param>
		/// <param name="compressionLevel"></param>
		/// <returns></returns>
		public static ZipArchiveEntry CreateEntryFromFile_CrossPlatform(this ZipArchive destination, string sourceFileName, string entryName, CompressionLevel compressionLevel)
		{
			ZipArchiveEntry entry = ZipFileExtensions.CreateEntryFromFile(destination, sourceFileName, entryName, compressionLevel);
			int result = -1;

			if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				result = FileUtils.GetFileMode_Linux(sourceFileName);
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				result = FileUtils.GetFileMode_Mac(sourceFileName);
			}

			if(result >= 0)
			{
				entry.ExternalAttributes = (int)result << 16;
			}

			return entry;
		}

		/// <summary>
		/// Internal field storing information about the platform that created a ZipArchiveEntry. Cannot interpret how to treat the attribute bits without reading this.
		/// </summary>
		static readonly FieldInfo s_versionMadeByPlatformField = typeof(ZipArchiveEntry).GetField("_versionMadeByPlatform", BindingFlags.NonPublic | BindingFlags.Instance)!;

		/// <summary>
		/// Extract a zip archive entry, preserving platform mode bits
		/// </summary>
		/// <param name="entry"></param>
		/// <param name="targetFileName"></param>
		/// <param name="overwrite"></param>
		public static void ExtractToFile_CrossPlatform(this ZipArchiveEntry entry, string targetFileName, bool overwrite)
		{
			// This seems to be consistently at least 35% faster than ZipFileExtensions.ExtractToFile(Entry, TargetFileName, Overwrite) when the
			// archive contains many small files.
			using (FileStream OutputStream = new FileStream(targetFileName, overwrite ? FileMode.Create : FileMode.CreateNew, FileAccess.Write))
			{
				using (Stream InStream = entry.Open())
				{
					InStream.CopyTo(OutputStream);
				}
			}

			if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				int madeByPlatform = Convert.ToInt32(s_versionMadeByPlatformField.GetValue(entry)!);
				if (madeByPlatform == 3 || madeByPlatform == 19) // Unix or OSX
				{
					FileUtils.SetFileMode_Linux(targetFileName, (ushort)(entry.ExternalAttributes >> 16));
				}
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				int madeByPlatform = Convert.ToInt32(s_versionMadeByPlatformField.GetValue(entry)!);
				if (madeByPlatform == 3 || madeByPlatform == 19) // Unix or OSX
				{
					FileUtils.SetFileMode_Mac(targetFileName, (ushort)(entry.ExternalAttributes >> 16));
				}
			}
		}
	}
}

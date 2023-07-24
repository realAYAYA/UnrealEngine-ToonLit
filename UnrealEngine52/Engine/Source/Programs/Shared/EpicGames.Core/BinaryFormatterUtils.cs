// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Runtime.Serialization.Formatters.Binary;

#pragma warning disable SYSLIB0011
#pragma warning disable CA2300 // Do not use insecure deserializer BinaryFormatter
#pragma warning disable CA2301 // Do not use insecure deserializer BinaryFormatter

namespace EpicGames.Core
{
	/// <summary>
	/// Utility functions for serializing using the BinaryFormatter
	/// </summary>
	public static class BinaryFormatterUtils
	{
		/// <summary>
		/// Load an object from a file on disk, using the binary formatter
		/// </summary>
		/// <param name="location">File to read from</param>
		/// <returns>Instance of the object that was read from disk</returns>
		public static T Load<T>(FileReference location)
		{
			using FileStream stream = new FileStream(location.FullName, FileMode.Open, FileAccess.Read);
			BinaryFormatter formatter = new BinaryFormatter();
			return (T)formatter.Deserialize(stream);
		}

		/// <summary>
		/// Saves a file to disk, using the binary formatter
		/// </summary>
		/// <param name="location">File to write to</param>
		/// <param name="obj">Object to serialize</param>
		public static void Save(FileReference location, object obj)
		{
			DirectoryReference.CreateDirectory(location.Directory);
			using FileStream stream = new FileStream(location.FullName, FileMode.Create, FileAccess.Write);
			BinaryFormatter formatter = new BinaryFormatter();
			formatter.Serialize(stream, obj);
		}

		/// <summary>
		/// Saves a file to disk using the binary formatter, without updating the timestamp if it hasn't changed
		/// </summary>
		/// <param name="location">File to write to</param>
		/// <param name="obj">Object to serialize</param>
		public static void SaveIfDifferent(FileReference location, object obj)
		{
			byte[] contents;
			using (MemoryStream stream = new MemoryStream())
			{
				BinaryFormatter formatter = new BinaryFormatter();
				formatter.Serialize(stream, obj);
				contents = stream.ToArray();
			}

			DirectoryReference.CreateDirectory(location.Directory);
			FileReference.WriteAllBytesIfDifferent(location, contents);
		}
	}
}

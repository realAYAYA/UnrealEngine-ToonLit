// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;

namespace Horde.Build.Agents.Software
{
	/// <summary>
	/// Interface for a collection of template documents
	/// </summary>
	public interface IAgentSoftwareCollection
	{
		/// <summary>
		/// Adds a new software revision
		/// </summary>
		/// <param name="version">The version number</param>
		/// <param name="data">Zip file containing the new software</param>
		/// <returns>New software instance</returns>
		Task<bool> AddAsync(string version, byte[] data);

		/// <summary>
		/// Tests whether a given version exists
		/// </summary>
		/// <param name="version">Version of the software</param>
		/// <returns>True if it exists, false otherwise</returns>
		Task<bool> ExistsAsync(string version);

		/// <summary>
		/// Removes a software archive 
		/// </summary>
		/// <param name="version">Version of the software to delete</param>
		Task<bool> RemoveAsync(string version);

		/// <summary>
		/// Downloads software of a given revision
		/// </summary>
		/// <param name="version">Version of the software</param>
		/// <returns>Data for the given software</returns>
		Task<byte[]?> GetAsync(string version);
	}
}

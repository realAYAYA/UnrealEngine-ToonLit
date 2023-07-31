// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using EpicGames.Perforce;
using Horde.Build.Jobs.Templates;
using Horde.Build.Streams;
using Horde.Build.Users;

namespace Horde.Build.Perforce
{
	/// <summary>
	/// Result from checking a shelved change status
	/// </summary>
	public enum CheckShelfResult
	{
		/// <summary>
		/// The shelf is ok
		/// </summary>
		Ok,

		/// <summary>
		/// Changelist does not exist
		/// </summary>
		NoChange,

		/// <summary>
		/// The change does not contain any shelved files
		/// </summary>
		NoShelvedFiles,

		/// <summary>
		/// The shelf contains *some* files from a different stream
		/// </summary>
		MixedStream,

		/// <summary>
		/// The shelf contains only files from a different stream
		/// </summary>
		WrongStream,
	}

	/// <summary>
	/// Wrapper around Perforce functionality. Can use a local p4.exe client for development purposes, or a separate HordePerforceBridge instance over REST for deployments.
	/// </summary>
	public interface IPerforceService
	{
		/// <summary>
		/// Finds or adds a user from the given Perforce server, adding the user (and populating their profile with Perforce data) if they do not currently exist
		/// </summary>
		/// <param name="clusterName"></param>
		/// <param name="userName"></param>
		/// <returns></returns>
		public ValueTask<IUser> FindOrAddUserAsync(string clusterName, string userName);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="clusterName"></param>
		/// <returns></returns>
		public Task<IPerforceConnection?> GetServiceUserConnection(string? clusterName);

		/// <summary>
		/// Create a new changelist by submitting the given file
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="streamName">The stream to query</param>
		/// <param name="path">Path for the file to submit</param>
		/// <param name="description">Description for the changelist</param>
		/// <returns>New changelist number</returns>
		public Task<int> CreateNewChangeAsync(string clusterName, string streamName, string path, string description);

		/// <summary>
		/// Gets the code change corresponding to an actual change submitted to a stream
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="streamName"></param>
		/// <param name="change">The changelist number to query</param>
		/// <returns>Code change for the latest change</returns>
		public Task<int> GetCodeChangeAsync(string clusterName, string streamName, int change);

		/// <summary>
		/// Finds changes submitted to a depot Gets the latest change for a particular stream
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="minChange">The minimum changelist number</param>
		/// <param name="maxChange">The maximum changelist number</param>
		/// <param name="maxResults"></param>
		/// <returns>Changelist information</returns>
		public Task<List<ChangeSummary>> GetChangesAsync(string clusterName, int? minChange, int? maxChange, int maxResults);

		/// <summary>
		/// Gets the latest change for a particular stream
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="streamName">The stream to query</param>
		/// <param name="minChange">The minimum changelist number</param>
		/// <param name="maxChange">The maximum changelist number</param>
		/// <param name="results">Number of results to return</param>
		/// <param name="impersonateUser">Name of the user to impersonate</param>
		/// <returns>Latest changelist number</returns>
		public Task<List<ChangeSummary>> GetChangesAsync(string clusterName, string streamName, int? minChange, int? maxChange, int results, string? impersonateUser);

		/// <summary>
		/// Checks a shelf is valid for the given stream
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="streamName">The stream to query</param>
		/// <param name="changeNumber">Shelved changelist number</param>
		/// <param name="impersonateUser">Name of the user to impersonate</param>
		/// <returns>Whether the shelf is valid, plus the description for it</returns>
		public Task<(CheckShelfResult, string?)> CheckShelfAsync(string clusterName, string streamName, int changeNumber, string? impersonateUser);

		/// <summary>
		/// Gets the latest change for a particular stream
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="streamName">The stream to query</param>
		/// <param name="changeNumber">Change numbers to query</param>
		/// <returns>Commit details</returns>
		public Task<ChangeDetails> GetChangeDetailsAsync(string clusterName, string streamName, int changeNumber);

		/// <summary>
		/// Gets the latest change for a particular stream
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="streamName">The stream to query</param>
		/// <param name="changeNumbers">Change numbers to query</param>
		/// <param name="impersonateUser">Name of the user to impersonate</param>
		/// <returns>Commit details</returns>
		public Task<List<ChangeDetails>> GetChangeDetailsAsync(string clusterName, string streamName, IReadOnlyList<int> changeNumbers, string? impersonateUser);

		/// <summary>
		/// Gets the latest changes for a set of depot paths
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="paths"></param>
		/// <returns></returns>
		public Task<List<FileSummary>> FindFilesAsync(string clusterName, IEnumerable<string> paths);

		/// <summary>
		/// Gets the contents of a file in the depot
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="path">Path to read</param>
		/// <returns>Data for the file</returns>
		public Task<byte[]> PrintAsync(string clusterName, string path);

		/// <summary>
		/// Duplicates a shelved changelist
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="shelvedChange">The shelved changelist</param>
		/// <returns>The duplicated changelist</returns>
		public Task<int> DuplicateShelvedChangeAsync(string clusterName, int shelvedChange);

		/// <summary>
		/// Submit a shelved changelist
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="shelvedChange">The shelved changelist number, created by <see cref="DuplicateShelvedChangeAsync(String,Int32)"/></param>
		/// <param name="originalChange">The original changelist number</param>
		/// <returns>Tuple consisting of the submitted changelist number and message</returns>
		public Task<(int? Change, string Message)> SubmitShelvedChangeAsync(string clusterName, int shelvedChange, int originalChange);

		/// <summary>
		/// Deletes a changelist containing shelved files.
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="shelvedChange">The changelist containing shelved files</param>
		/// <returns>Async tasy</returns>
		public Task DeleteShelvedChangeAsync(string clusterName, int shelvedChange);

		/// <summary>
		/// Updates a changelist description
		/// </summary>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="change">The change to update</param>
		/// <param name="description">The new description</param>
		/// <returns>Async task</returns>
		public Task UpdateChangelistDescription(string clusterName, int change, string description);
	}

	/// <summary>
	/// Extension methods for IPerforceService implementations
	/// </summary>
	public static class PerforceServiceExtensions
	{
		/// <summary>
		/// Creates a new change for a template
		/// </summary>
		/// <param name="perforce">The Perforce service instance</param>
		/// <param name="stream">Stream containing the template</param>
		/// <param name="template">The template being built</param>
		/// <returns>New changelist number</returns>
		public static Task<int> CreateNewChangeForTemplateAsync(this IPerforceService perforce, IStream stream, ITemplate template)
		{
			string description = (template.SubmitDescription ?? "[Horde] New change for $(TemplateName)").Replace("$(TemplateName)", template.Name, StringComparison.OrdinalIgnoreCase);

			Match match = Regex.Match(template.SubmitNewChange!, @"^(//[^/]+/[^/]+)/(.+)$");
			if (match.Success)
			{
				return perforce.CreateNewChangeAsync(stream.ClusterName, match.Groups[1].Value, match.Groups[2].Value, description);
			}
			else
			{
				return perforce.CreateNewChangeAsync(stream.ClusterName, stream.Name, template.SubmitNewChange!, description);
			}
		}

		/// <summary>
		/// Gets the latest change for a particular stream
		/// </summary>
		/// <param name="perforce">The perforce implementation</param>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="streamName">The stream to query</param>
		/// <param name="changeNumber">Change number to query</param>
		/// <param name="impersonateUser">Name of the user to impersonate</param>
		/// <returns>Commit details</returns>
		public static async Task<ChangeDetails> GetChangeDetailsAsync(this IPerforceService perforce, string clusterName, string streamName, int changeNumber, string? impersonateUser)
		{
			List<ChangeDetails> results = await perforce.GetChangeDetailsAsync(clusterName, streamName, new[] { changeNumber }, impersonateUser);
			return results[0];
		}

		/// <summary>
		/// Gets the latest change for a particular stream
		/// </summary>
		/// <param name="perforce">The perforce implementation</param>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="streamName">The stream to query</param>
		/// <param name="minChange">The minimum changelist number</param>
		/// <param name="maxChange">The maximum changelist number</param>
		/// <param name="results">Number of results to return</param>
		/// <param name="impersonateUser">Name of the user to impersonate</param>
		/// <returns>Commit details</returns>
		public static async Task<List<ChangeDetails>> GetChangeDetailsAsync(this IPerforceService perforce, string clusterName, string streamName, int? minChange, int? maxChange, int results, string? impersonateUser)
		{
			List<ChangeSummary> changes = await perforce.GetChangesAsync(clusterName, streamName, minChange, maxChange, results, impersonateUser);
			return await perforce.GetChangeDetailsAsync(clusterName, streamName, changes.ConvertAll(x => x.Number), impersonateUser);
		}

		/// <summary>
		/// Get the latest submitted change to the stream
		/// </summary>
		/// <param name="perforce">The perforce implementation</param>
		/// <param name="clusterName">Name of the Perforce cluster</param>
		/// <param name="streamName">The stream to query</param>
		/// <param name="impersonateUser">Name of the user to impersonate</param>
		/// <returns>Latest changelist number</returns>
		public static async Task<int> GetLatestChangeAsync(this IPerforceService perforce, string clusterName, string streamName, string? impersonateUser)
		{
			List<ChangeSummary> changes = await perforce.GetChangesAsync(clusterName, streamName, null, null, 1, impersonateUser);
			if (changes.Count == 0)
			{
				throw new Exception($"No changes have been submitted to stream {streamName}");
			}
			return changes[0].Number;
		}
	}
}

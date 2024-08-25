// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Perforce;
using Horde.Server.Jobs.Templates;

namespace Horde.Server.Perforce
{
	/// <summary>
	/// VCS abstraction. Provides information about commits to a particular stream.
	/// </summary>
	public interface ICommitCollection
	{
		/// <summary>
		/// Creates a new change
		/// </summary>
		/// <param name="path">Path to modify in the change</param>
		/// <param name="description">Description of the change</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New commit information</returns>
		Task<int> CreateNewAsync(string path, string description, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets the latest change for a particular stream
		/// </summary>
		/// <param name="changeNumber">Change numbers to query</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Commit details</returns>
		Task<ICommit> GetAsync(int changeNumber, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds changes submitted to a stream, in reverse order.
		/// </summary>
		/// <param name="minChange">The minimum changelist number</param>
		/// <param name="maxChange">The maximum changelist number</param>
		/// <param name="maxResults">Maximum number of results to return</param>
		/// <param name="tags">Tags for the commits to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Changelist information</returns>
		IAsyncEnumerable<ICommit> FindAsync(int? minChange = null, int? maxChange = null, int? maxResults = null, IReadOnlyList<CommitTag>? tags = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Subscribes to changes from this commit source
		/// </summary>
		/// <param name="minChange">Minimum changelist number (exclusive)</param>
		/// <param name="tags">Tags for the commit to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New change information</returns>
		IAsyncEnumerable<ICommit> SubscribeAsync(int minChange, IReadOnlyList<CommitTag>? tags = null, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Extension methods for <see cref="ICommitCollection"/>
	/// </summary>
	public static class CommitCollectionExtensions
	{
		/// <summary>
		/// Creates a new change for a template
		/// </summary>
		/// <param name="perforce">The Perforce service instance</param>
		/// <param name="template">The template being built</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New changelist number</returns>
		public static Task<int> CreateNewAsync(this ICommitCollection perforce, ITemplate template, CancellationToken cancellationToken)
		{
			string description = (template.SubmitDescription ?? "[Horde] New change for $(TemplateName)").Replace("$(TemplateName)", template.Name, StringComparison.OrdinalIgnoreCase);
			return perforce.CreateNewAsync(template.SubmitNewChange!, description, cancellationToken);
		}

		/// <summary>
		/// Gets the last code code equal or before the given change number
		/// </summary>
		/// <param name="source">The commit source to query</param>
		/// <param name="maxChange">Maximum code change to query</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The last code change</returns>
		public static async ValueTask<ICommit?> GetLastCodeChangeAsync(this ICommitCollection source, int? maxChange, CancellationToken cancellationToken = default)
		{
			return await source.FindAsync(null, maxChange, 1, new[] { CommitTag.Code }, cancellationToken).FirstOrDefaultAsync(cancellationToken);
		}

		/// <summary>
		/// Finds the latest commit from a source
		/// </summary>
		/// <param name="source">The commit source to query</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The latest commit</returns>
		public static async Task<ICommit> GetLatestAsync(this ICommitCollection source, CancellationToken cancellationToken = default)
		{
			ICommit? commit = await source.FindAsync(null, null, 1, null, cancellationToken).FirstOrDefaultAsync(cancellationToken);
			if (commit == null)
			{
				throw new PerforceException("No changes found for stream.");
			}
			return commit;
		}

		/// <summary>
		/// Finds the latest commit from a source
		/// </summary>
		/// <param name="source">The commit source to query</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The latest commit</returns>
		public static async Task<int> GetLatestNumberAsync(this ICommitCollection source, CancellationToken cancellationToken = default)
		{
			ICommit commit = await GetLatestAsync(source, cancellationToken);
			return commit.Number;
		}
	}
}

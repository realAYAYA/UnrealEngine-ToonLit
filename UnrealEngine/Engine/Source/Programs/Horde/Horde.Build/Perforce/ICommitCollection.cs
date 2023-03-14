// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Streams;
using Horde.Build.Utilities;

namespace Horde.Build.Perforce
{
	using CommitId = ObjectId<ICommit>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Stores a collection of commits
	/// </summary>
	public interface ICommitCollection
	{
		/// <summary>
		/// Adds or replaces an existing commit
		/// </summary>
		/// <param name="newCommit">The new commit to add</param>
		/// <returns>The commit that was created</returns>
		Task<ICommit> AddOrReplaceAsync(NewCommit newCommit);

		/// <summary>
		/// Gets a single commit
		/// </summary>
		/// <param name="id">Identifier for the commit</param>
		Task<ICommit?> GetCommitAsync(CommitId id);

		/// <summary>
		/// Finds commits matching certain criteria
		/// </summary>
		/// <param name="streamId"></param>
		/// <param name="minChange"></param>
		/// <param name="maxChange"></param>
		/// <param name="index"></param>
		/// <param name="count"></param>
		/// <returns></returns>
		Task<List<ICommit>> FindCommitsAsync(StreamId streamId, int? minChange = null, int? maxChange = null, int? index = null, int? count = null);
	}

	/// <summary>
	/// Extension methods for <see cref="ICommitCollection"/>
	/// </summary>
	static class CommitCollectionExtensions
	{
		/// <summary>
		/// Gets a commit from a stream by changelist number
		/// </summary>
		/// <param name="commitCollection">The commit collection</param>
		/// <param name="streamId"></param>
		/// <param name="change"></param>
		/// <returns></returns>
		public static async Task<ICommit?> GetCommitAsync(this ICommitCollection commitCollection, StreamId streamId, int change)
		{
			List<ICommit> commits = await commitCollection.FindCommitsAsync(streamId, change, change);
			if (commits.Count == 0)
			{
				return null;
			}
			return commits[0];
		}
	}
}

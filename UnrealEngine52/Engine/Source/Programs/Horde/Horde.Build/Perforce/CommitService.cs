// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Build.Streams;

namespace Horde.Build.Perforce
{
	/// <summary>
	/// Provides commit information for streams
	/// </summary>
	class CommitService : ICommitService
	{
		readonly IPerforceService _perforceService;

		/// <summary>
		/// Constructor
		/// </summary>
		public CommitService(IPerforceService perforceService)
		{
			_perforceService = perforceService;
		}

		/// <inheritdoc/>
		public ICommitCollection GetCollection(StreamConfig streamConfig) => _perforceService.GetCommits(streamConfig);
	}
}

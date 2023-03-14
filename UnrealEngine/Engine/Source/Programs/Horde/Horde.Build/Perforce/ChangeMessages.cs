// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Horde.Build.Perforce;
using Horde.Build.Users;

namespace Horde.Build.Perforce
{
	/// <summary>
	/// Information about a submitted changelist
	/// </summary>
	public class GetChangeSummaryResponse
	{
		/// <summary>
		/// The source changelist number
		/// </summary>
		public int Number { get; set; }

		/// <summary>
		/// Name of the user that authored this change [DEPRECATED]
		/// </summary>
		public string Author { get; set; }

		/// <summary>
		/// Information about the change author
		/// </summary>
		public GetThinUserInfoResponse AuthorInfo { get; set; }

		/// <summary>
		/// The description text
		/// </summary>
		public string Description { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="changeSummary">The commit to construct from</param>
		public GetChangeSummaryResponse(ChangeSummary changeSummary)
		{
			Number = changeSummary.Number;
			Author = changeSummary.Author.Name;
			AuthorInfo = new GetThinUserInfoResponse(changeSummary.Author);
			Description = changeSummary.Description;
		}
	}

	/// <summary>
	/// Information about a submitted changelist
	/// </summary>
	public class GetChangeDetailsResponse
	{
		/// <summary>
		/// The source changelist number
		/// </summary>
		public int Number { get; set; }

		/// <summary>
		/// Name of the user that authored this change [DEPRECATED]
		/// </summary>
		public string Author { get; set; }

		/// <summary>
		/// Information about the user that authored this change
		/// </summary>
		public GetThinUserInfoResponse AuthorInfo { get; set; }

		/// <summary>
		/// The description text
		/// </summary>
		public string Description { get; set; }

		/// <summary>
		/// List of files that were modified, relative to the stream base
		/// </summary>
		public List<string> Files { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="changeDetails">The commit to construct from</param>
		public GetChangeDetailsResponse(ChangeDetails changeDetails)
		{
			Number = changeDetails.Number;
			Author = changeDetails.Author.Name;
			AuthorInfo = new GetThinUserInfoResponse(changeDetails.Author);
			Description = changeDetails.Description;
			Files = changeDetails.Files.ConvertAll(x => x.Path);
		}
	}
}

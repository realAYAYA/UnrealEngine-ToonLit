// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;

namespace P4VUtils.Perforce
{
	public static class P4ActionGroups
	{
		public static readonly IntegrateAction[] IntegrateFromActions =
		{
			IntegrateAction.BranchFrom,
			IntegrateAction.MergeFrom,
			IntegrateAction.MovedFrom,
			IntegrateAction.CopyFrom,
			IntegrateAction.DeleteFrom,
			IntegrateAction.EditFrom,
			IntegrateAction.AddFrom
		};

		public static readonly FileAction[] EditActions =
		{
			FileAction.Add,
			FileAction.Edit,
			FileAction.Delete
		};

		public static readonly FileAction[] MoveActions =
		{
			FileAction.MoveAdd,
			FileAction.MoveDelete
		};

		public static readonly FileAction[] IntegrateActions =
		{
			FileAction.Integrate,
			FileAction.Branch,
		};
	}
}

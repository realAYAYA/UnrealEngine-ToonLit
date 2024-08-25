// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace Jupiter.Implementation
{
	public class OnLeaderChangedEventArgs
	{
		public bool IsLeader { get; }
		public string LeaderName { get; }

		public OnLeaderChangedEventArgs(bool isLeader, string leaderName)
		{
			IsLeader = isLeader;
			LeaderName = leaderName;
		}
	}

	public interface ILeaderElection
	{
		bool IsThisInstanceLeader();

		event EventHandler<OnLeaderChangedEventArgs>? OnLeaderChanged;
	}
	
	public class LeaderElectionStub : ILeaderElection
	{
		private readonly bool _isLeader;

		public LeaderElectionStub(bool isLeader)
		{
			_isLeader = isLeader;

			OnLeaderChanged?.Invoke(this, new OnLeaderChangedEventArgs(isLeader, ""));
		}

		public bool IsThisInstanceLeader()
		{
			return _isLeader;
		}

		public event EventHandler<OnLeaderChangedEventArgs>? OnLeaderChanged;
	}
}

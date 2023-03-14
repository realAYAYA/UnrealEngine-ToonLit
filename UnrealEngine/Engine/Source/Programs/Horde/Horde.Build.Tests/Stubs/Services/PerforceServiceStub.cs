// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Perforce;
using Horde.Build.Perforce;
using Horde.Build.Users;
using Horde.Build.Utilities;

namespace Horde.Build.Tests.Stubs.Services
{
	using UserId = ObjectId<IUser>;

	static class PerforceExtensions
	{
		public static ChangeFile CreateChangeFile(string path)
		{
			return new ChangeFile(path, $"//UE5/Main/{path}", 0, 0, EpicGames.Core.Md5Hash.Zero, null!);
		}

		public static void Add(this List<ChangeFile> files, string path)
		{
			files.Add(CreateChangeFile(path));
		}
	}

	class PerforceServiceStub : IPerforceService
	{
		class User : IUser
		{
			public UserId Id { get; }
			public string Name { get; set; }
			public string Email { get; set; }
			public string Login { get; set; }

			public User(string login)
			{
				Id = UserId.GenerateNewId();
				Name = login.ToUpperInvariant();
				Email = $"{login}@server";
				Login = login;
			}
		}

		readonly User _testUser = new User("TestUser");

		class ChangeComparer : IComparer<int>
		{
			public int Compare(int x, int y) => y.CompareTo(x);
		}

		public Dictionary<string, SortedDictionary<int, ChangeDetails>> Changes { get; } = new Dictionary<string, SortedDictionary<int, ChangeDetails>>(StringComparer.OrdinalIgnoreCase);

		readonly IUserCollection _userCollection;

		public PerforceServiceStub(IUserCollection userCollection)
		{
			_userCollection = userCollection;
		}

		public Task<IPerforceConnection?> GetServiceUserConnection(string? clusterName)
		{
			throw new NotImplementedException();
		}

		public async ValueTask<IUser> FindOrAddUserAsync(string clusterName, string userName)
		{
			return await _userCollection.FindOrAddUserByLoginAsync(userName);
		}

		public void AddChange(string streamName, int number, IUser author, string description, IEnumerable<string> files)
		{
			SortedDictionary<int, ChangeDetails>? streamChanges;
			if (!Changes.TryGetValue(streamName, out streamChanges))
			{
				streamChanges = new SortedDictionary<int, ChangeDetails>(new ChangeComparer());
				Changes[streamName] = streamChanges;
			}
			streamChanges.Add(number, new ChangeDetails(number, author, null!, description, files.Select(x => PerforceExtensions.CreateChangeFile(x)).ToList(), DateTime.Now));
		}

		public Task<List<ChangeSummary>> GetChangesAsync(string clusterName, string streamName, int? minChange, int? maxChange, int numResults, string? impersonateUser)
		{
			List<ChangeSummary> results = new List<ChangeSummary>();

			SortedDictionary<int, ChangeDetails>? streamChanges;
			if (Changes.TryGetValue(streamName, out streamChanges) && streamChanges.Count > 0)
			{
				foreach (ChangeDetails details in streamChanges.Values)
				{
					if (minChange.HasValue && details.Number < minChange)
					{
						break;
					}
					if (!maxChange.HasValue || details.Number <= maxChange.Value)
					{
						results.Add(new ChangeSummary(details.Number, details.Author, "//...", details.Description));
					}
					if (numResults > 0 && results.Count >= numResults)
					{
						break;
					}
				}
			}

			return Task.FromResult(results);
		}

		public Task<(CheckShelfResult, string?)> CheckShelfAsync(string clusterName, string streamName, int changeNumber, string? impersonateUser)
		{
			throw new NotImplementedException();
		}

		public Task<List<ChangeDetails>> GetChangeDetailsAsync(string clusterName, string streamName, IReadOnlyList<int> changeNumbers, string? impersonateUser)
		{
			List<ChangeDetails> results = new List<ChangeDetails>();
			foreach (int changeNumber in changeNumbers)
			{
				results.Add(Changes[streamName][changeNumber]);
			}
			return Task.FromResult(results);
		}

		public static Task<string> CreateTicket()
		{
			return Task.FromResult("bogus-ticket");
		}

		public Task<int> GetCodeChangeAsync(string clusterName, string streamName, int change)
		{
			int codeChange = 0;

			SortedDictionary<int, ChangeDetails>? streamChanges;
			if (Changes.TryGetValue(streamName, out streamChanges))
			{
				foreach (ChangeDetails details in streamChanges.Values)
				{
					if (details.Number <= change && details.Files.Any(x => x.Path.EndsWith(".h", StringComparison.OrdinalIgnoreCase) || x.Path.EndsWith(".cpp", StringComparison.OrdinalIgnoreCase)))
					{
						codeChange = details.Number;
						break;
					}
				}
			}

			return Task.FromResult(codeChange);
		}

		public Task<int> CreateNewChangeAsync(string clusterName, string streamName, string path, string description)
		{
			ChangeDetails newChange = new ChangeDetails(Changes[streamName].First().Key + 1, _testUser, null!, description, new List<ChangeFile> { PerforceExtensions.CreateChangeFile(path) }, DateTime.Now);
			Changes[streamName].Add(newChange.Number, newChange);
			return Task.FromResult(newChange.Number);
		}

		public Task<List<FileSummary>> FindFilesAsync(string clusterName, IEnumerable<string> paths)
		{
			throw new NotImplementedException();
		}

		public Task<byte[]> PrintAsync(string clusterName, string path)
		{
			throw new NotImplementedException();
		}

		public Task<(int? Change, string Message)> SubmitShelvedChangeAsync(string clusterName, int shelvedChange, int originalChange)
		{
			throw new NotImplementedException();
		}

		public Task<int> DuplicateShelvedChangeAsync(string clusterName, int shelvedChange)
		{
			return Task.FromResult(shelvedChange);
		}

		public Task DeleteShelvedChangeAsync(string clusterName, int shelvedChange)
		{
			return Task.CompletedTask;
		}

		public Task UpdateChangelistDescription(string clusterName, int change, string description)
		{
			return Task.CompletedTask;
		}

		public Task<List<ChangeSummary>> GetChangesAsync(string clusterName, int? minChange, int? maxChange, int maxResults)
		{
			throw new NotImplementedException();
		}

		public Task<ChangeDetails> GetChangeDetailsAsync(string clusterName, int changeNumber)
		{
			throw new NotImplementedException();
		}

		public Task<List<ChangeFile>> GetStreamSnapshotAsync(string clusterName, string streamName, int change)
		{
			throw new NotImplementedException();
		}

		public Task<ChangeDetails> GetChangeDetailsAsync(string clusterName, string streamName, int changeNumber)
		{
			return Task.FromResult(Changes[streamName][changeNumber]);
		}
	}
}

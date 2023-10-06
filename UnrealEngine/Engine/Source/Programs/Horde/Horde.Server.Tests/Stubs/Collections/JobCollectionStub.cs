// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Build.Collections;
using Horde.Build.Models;
using Horde.Build.Utilities;
using MongoDB.Bson;
using MongoDB.Driver;
using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;

namespace Horde.Build.Tests.Stubs.Collections
{
	class JobCollectionStub : IJobCollection
	{
		public Task AddAsync(Job NewJob)
		{
			throw new NotImplementedException();
		}

		public Task AddIssueToJobAsync(JobId JobId, ObjectId IssueId)
		{
			throw new NotImplementedException();
		}

		public Task<List<Job>> FindAsync(StreamId? StreamId, string Name, TemplateId[] Templates, int? MinChange, int? MaxChange, DateTimeOffset? MinCreateTime, DateTimeOffset? MaxCreateTime, int Index, int Count)
		{
			throw new NotImplementedException();
		}

		public Task<Job> GetAsync(ObjectId JobId)
		{
			throw new NotImplementedException();
		}

		public Task<List<Job>> GetDispatchQueueAsync(int MaxQueueLength)
		{
			throw new NotImplementedException();
		}

		public Task<JobPermissions> GetPermissionsAsync(ObjectId JobId)
		{
			throw new NotImplementedException();
		}

		public Task<bool> RemoveAsync(ObjectId JobId)
		{
			throw new NotImplementedException();
		}

		public Task RemoveStreamAsync(StreamId StreamId)
		{
			throw new NotImplementedException();
		}

		public Task<bool> TryUpdateAsync(Job Job, UpdateDefinition<Job> Update)
		{
			throw new NotImplementedException();
		}
	}
}

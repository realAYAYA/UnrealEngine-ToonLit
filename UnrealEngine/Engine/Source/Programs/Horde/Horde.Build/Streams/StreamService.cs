// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Jobs;
using Horde.Build.Projects;
using Horde.Build.Utilities;
using Microsoft.Extensions.Caching.Memory;
using MongoDB.Driver;

namespace Horde.Build.Streams
{
	using JobId = ObjectId<IJob>;
	using ProjectId = StringId<IProject>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;

	/// <summary>
	/// Cache of information about stream ACLs
	/// </summary>
	public class StreamPermissionsCache : ProjectPermissionsCache
	{
		/// <summary>
		/// Map of stream id to permissions for that stream
		/// </summary>
		public Dictionary<StreamId, IStreamPermissions?> Streams { get; set; } = new Dictionary<StreamId, IStreamPermissions?>();
	}

	/// <summary>
	/// Wraps functionality for manipulating streams
	/// </summary>
	public sealed class StreamService : IDisposable
	{
		/// <summary>
		/// The project service instance
		/// </summary>
		readonly ProjectService _projectService;

		/// <summary>
		/// Collection of stream documents
		/// </summary>
		readonly IStreamCollection _streams;

		/// <summary>
		/// Cache of stream documents
		/// </summary>
		readonly MemoryCache _streamCache = new MemoryCache(new MemoryCacheOptions());

		/// <summary>
		/// Accessor for the stream collection
		/// </summary>
		public IStreamCollection StreamCollection => _streams;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="projectService">The project service instance</param>
		/// <param name="streams">Collection of stream documents</param>
		public StreamService(ProjectService projectService, IStreamCollection streams)
		{
			_projectService = projectService;
			_streams = streams;
		}

		/// <summary>
		/// Dispose of any managed resources
		/// </summary>
		public void Dispose()
		{
			_streamCache.Dispose();
		}

		/// <summary>
		/// Deletes an existing stream
		/// </summary>
		/// <param name="streamId">Unique id of the stream</param>
		/// <returns>Async task object</returns>
		public async Task DeleteStreamAsync(StreamId streamId)
		{
			await _streams.DeleteAsync(streamId);
		}

		/// <summary>
		/// Updates an existing stream
		/// </summary>
		/// <param name="stream">The stream to update</param>
		/// <param name="newPausedUntil">The new datetime for pausing builds</param>
		/// <param name="newPauseComment">The reason for pausing</param>
		/// <returns>Async task object</returns>
		public async Task<IStream?> UpdatePauseStateAsync(IStream? stream, DateTime? newPausedUntil = null, string? newPauseComment = null)
		{
			for (; stream != null; stream = await GetStreamAsync(stream.Id))
			{
				IStream? newStream = await _streams.TryUpdatePauseStateAsync(stream, newPausedUntil, newPauseComment);
				if (newStream != null)
				{
					return newStream;
				}
			}
			return null;
		}

		/// <summary>
		/// Attempts to update the last trigger time for a schedule
		/// </summary>
		/// <param name="stream">The stream to update</param>
		/// <param name="templateRefId">The template ref id</param>
		/// <param name="lastTriggerTimeUtc"></param>
		/// <param name="lastTriggerChange"></param>
		/// <param name="addJobs">Jobs to add</param>
		/// <param name="removeJobs">Jobs to remove</param>
		/// <returns>True if the stream was updated</returns>
		public async Task<IStream?> UpdateScheduleTriggerAsync(IStream stream, TemplateRefId templateRefId, DateTime? lastTriggerTimeUtc = null, int? lastTriggerChange = null, List<JobId>? addJobs = null, List<JobId>? removeJobs = null)
		{
			IStream? newStream = stream;
			while (newStream != null)
			{
				TemplateRef? templateRef;
				if (!newStream.Templates.TryGetValue(templateRefId, out templateRef))
				{
					break;
				}
				if (templateRef.Schedule == null)
				{
					break;
				}

				IEnumerable<JobId> newActiveJobs = templateRef.Schedule.ActiveJobs;
				if (removeJobs != null)
				{
					newActiveJobs = newActiveJobs.Except(removeJobs);
				}
				if (addJobs != null)
				{
					newActiveJobs = newActiveJobs.Union(addJobs);
				}

				newStream = await _streams.TryUpdateScheduleTriggerAsync(newStream, templateRefId, lastTriggerTimeUtc, lastTriggerChange, newActiveJobs.ToList());

				if (newStream != null)
				{
					return newStream;
				}

				newStream = await _streams.GetAsync(stream.Id);
			}
			return null;
		}

		/// <summary>
		/// Attempts to update a stream template ref
		/// </summary>
		/// <param name="streamInterface"></param>
		/// <param name="templateRefId"></param>
		/// <param name="stepStates"></param>
		/// <returns></returns>
		public Task<IStream?> TryUpdateTemplateRefAsync(IStream streamInterface, TemplateRefId templateRefId, List<UpdateStepStateRequest>? stepStates = null)
		{
			return _streams.TryUpdateTemplateRefAsync(streamInterface, templateRefId, stepStates );
		}

		/// <summary>
		/// Gets all the available streams for a project
		/// </summary>
		/// <returns>List of stream documents</returns>
		public Task<List<IStream>> GetStreamsAsync()
		{
			return _streams.FindAllAsync();
		}

		/// <summary>
		/// Gets all the available streams for a project
		/// </summary>
		/// <param name="projectId">Unique id of the project to query</param>
		/// <returns>List of stream documents</returns>
		public Task<List<IStream>> GetStreamsAsync(ProjectId projectId)
		{
			return _streams.FindForProjectsAsync(new[] { projectId });
		}

		/// <summary>
		/// Gets all the available streams for a project
		/// </summary>
		/// <param name="projectIds">Unique id of the project to query</param>
		/// <returns>List of stream documents</returns>
		public Task<List<IStream>> GetStreamsAsync(ProjectId[] projectIds)
		{
			if (projectIds.Length == 0)
			{
				return _streams.FindAllAsync();
			}
			else
			{
				return _streams.FindForProjectsAsync(projectIds);
			}
		}

		/// <summary>
		/// Gets a stream by ID
		/// </summary>
		/// <param name="streamId">Unique id of the stream</param>
		/// <returns>The stream document</returns>
		public Task<IStream?> GetStreamAsync(StreamId streamId)
		{
			return _streams.GetAsync(streamId);
		}

		/// <summary>
		/// Adds a cached stream interface
		/// </summary>
		/// <param name="stream">The stream to add</param>
		/// <returns>The new stream</returns>
		private void AddCachedStream(IStream stream)
		{
			MemoryCacheEntryOptions options = new MemoryCacheEntryOptions().SetAbsoluteExpiration(TimeSpan.FromMinutes(1.0));
			_streamCache.Set(stream.Id, stream, options);
		}

		/// <summary>
		/// Gets a cached stream interface
		/// </summary>
		/// <param name="streamId">Unique id for the stream</param>
		/// <returns>The new stream</returns>
		public async Task<IStream?> GetCachedStream(StreamId streamId)
		{
			object? stream;
			if (!_streamCache.TryGetValue(streamId, out stream))
			{
				stream = await GetStreamAsync(streamId);
				if (stream != null)
				{
					AddCachedStream((IStream)stream);
				}
			}
			return (IStream?)stream;
		}

		/// <summary>
		/// Gets a stream's permissions by ID
		/// </summary>
		/// <param name="streamId">Unique id of the stream</param>
		/// <returns>The stream document</returns>
		public Task<IStreamPermissions?> GetStreamPermissionsAsync(StreamId streamId)
		{
			return _streams.GetPermissionsAsync(streamId);
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular stream
		/// </summary>
		/// <param name="acl">ACL for the stream to check</param>
		/// <param name="projectId">The parent project id</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <param name="cache">Cache of project permissions</param>
		/// <returns>True if the action is authorized</returns>
		private Task<bool> AuthorizeAsync(Acl? acl, ProjectId projectId, AclAction action, ClaimsPrincipal user, ProjectPermissionsCache? cache)
		{
			bool? result = acl?.Authorize(action, user);
			if (result == null)
			{
				return _projectService.AuthorizeAsync(projectId, action, user, cache);
			}
			else
			{
				return Task.FromResult(result.Value);
			}
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular stream
		/// </summary>
		/// <param name="stream">The stream to check</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <param name="cache">Cache of project permissions</param>
		/// <returns>True if the action is authorized</returns>
		public Task<bool> AuthorizeAsync(IStream stream, AclAction action, ClaimsPrincipal user, ProjectPermissionsCache? cache)
		{
			return AuthorizeAsync(stream.Acl, stream.ProjectId, action, user, cache);
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular stream
		/// </summary>
		/// <param name="stream">The stream to check</param>
		/// <param name="template">Template within the stream to check</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <param name="cache">Cache of project permissions</param>
		/// <returns>True if the action is authorized</returns>
		public Task<bool> AuthorizeAsync(IStream stream, TemplateRef template, AclAction action, ClaimsPrincipal user, ProjectPermissionsCache? cache)
		{
			bool? result = template.Acl?.Authorize(action, user);
			if (result == null)
			{
				return AuthorizeAsync(stream, action, user, cache);
			}
			else
			{
				return Task.FromResult(result.Value);
			}
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular project
		/// </summary>
		/// <param name="streamId">The stream id to check</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <param name="cache">Cache of stream permissions</param>
		/// <returns>True if the action is authorized</returns>
		public async Task<bool> AuthorizeAsync(StreamId streamId, AclAction action, ClaimsPrincipal user, StreamPermissionsCache? cache)
		{
			IStreamPermissions? permissions;
			if (cache == null)
			{
				permissions = await GetStreamPermissionsAsync(streamId);
			}
			else if (!cache.Streams.TryGetValue(streamId, out permissions))
			{
				permissions = await GetStreamPermissionsAsync(streamId);
				cache.Streams.Add(streamId, permissions);
			}
			return permissions != null && await AuthorizeAsync(permissions.Acl, permissions.ProjectId, action, user, cache);
		}
	}
}

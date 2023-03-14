// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Configuration;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Schedules;
using Horde.Build.Jobs.Templates;
using Horde.Build.Projects;
using Horde.Build.Server;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Logging;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Streams
{
	using JobId = ObjectId<IJob>;
	using ProjectId = StringId<IProject>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Collection of stream documents
	/// </summary>
	public class StreamCollection : IStreamCollection
	{
		/// <summary>
		/// Information about a stream
		/// </summary>
		class StreamDocument : IStream
		{
			public const int DefaultOrder = 128;

			[BsonRequired, BsonId]
			public StreamId Id { get; set; }

			[BsonRequired]
			public ProjectId ProjectId { get; set; }

			[BsonRequired]
			public string Name { get; set; }

			public string? ClusterName { get; set; }
			public string ConfigRevision { get; set; } = String.Empty;

			[BsonIgnore]
			public StreamConfig? Config { get; set; }

			public int Order { get; set; } = DefaultOrder;
			public string? NotificationChannel { get; set; }
			public string? NotificationChannelFilter { get; set; }
			public string? TriageChannel { get; set; }
			public DefaultPreflight? DefaultPreflight { get; set; }
			public List<StreamTab> Tabs { get; set; } = new List<StreamTab>();
			public Dictionary<string, AgentType> AgentTypes { get; set; } = new Dictionary<string, AgentType>(StringComparer.Ordinal);
			public Dictionary<string, WorkspaceType> WorkspaceTypes { get; set; } = new Dictionary<string, WorkspaceType>(StringComparer.Ordinal);
			public Dictionary<TemplateRefId, TemplateRef> Templates { get; set; } = new Dictionary<TemplateRefId, TemplateRef>();
			public DateTime? PausedUntil { get; set; }
			public string? PauseComment { get; set; }

			[BsonIgnoreIfDefault]
			[BsonDefaultValue(ContentReplicationMode.None)]
			public ContentReplicationMode ReplicationMode { get; set; }

			[BsonIgnoreIfNull]
			public string? ReplicationFilter { get; set; }

			public Acl? Acl { get; set; }
			public int UpdateIndex { get; set; }
			public bool Deleted { get; set; }

			StreamConfig IStream.Config => Config!;
			string IStream.ClusterName => ClusterName ?? PerforceCluster.DefaultName;
			IReadOnlyList<StreamTab> IStream.Tabs => Tabs;
			IReadOnlyDictionary<string, AgentType> IStream.AgentTypes => AgentTypes;
			IReadOnlyDictionary<string, WorkspaceType> IStream.WorkspaceTypes => WorkspaceTypes;
			IReadOnlyDictionary<TemplateRefId, TemplateRef> IStream.Templates => Templates;

			[BsonConstructor]
			private StreamDocument()
			{
				Name = null!;
			}

			public StreamDocument(StreamId id, string name, ProjectId projectId)
			{
				Id = id;
				Name = name;
				ProjectId = projectId;
			}
		}

		/// <summary>
		/// Projection of a stream definition to just include permissions info
		/// </summary>
		[SuppressMessage("Design", "CA1812: Class is never instantiated")]
		private class StreamPermissions : IStreamPermissions
		{
			public Acl? Acl { get; set; }
			public ProjectId ProjectId { get; set; }

			public static readonly ProjectionDefinition<StreamDocument> Projection = Builders<StreamDocument>.Projection.Include(x => x.Acl).Include(x => x.ProjectId);
		}

		readonly IMongoCollection<StreamDocument> _streams;
		readonly ConfigCollection _configCollection;
		readonly IClock _clock;
		readonly ITemplateCollection _templateCollection;
		readonly ILogger<StreamCollection> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		/// <param name="configCollection"></param>
		/// <param name="clock"></param>
		/// <param name="templateCollection"></param>
		/// <param name="logger"></param>
		public StreamCollection(MongoService mongoService, ConfigCollection configCollection, IClock clock, ITemplateCollection templateCollection, ILogger<StreamCollection> logger)
		{
			_streams = mongoService.GetCollection<StreamDocument>("Streams");
			_configCollection = configCollection;
			_clock = clock;
			_templateCollection = templateCollection;
			_logger = logger;
		}

		async Task PostLoadAsync(StreamDocument stream)
		{
			try
			{
				if (stream.Deleted)
				{
					stream.Config = new StreamConfig();
				}
				else
				{
					stream.Config = await _configCollection.GetConfigAsync<StreamConfig>(stream.ConfigRevision);
				}
			}
			catch (Exception)
			{
				_logger.LogError("Unable to get stream config for {StreamId} at {Revision}", stream.Id, stream.ConfigRevision);
				throw;
			}
		}

		/// <inheritdoc/>
		public async Task<IStream?> TryCreateOrReplaceAsync(StreamId id, IStream? stream, string revision, ProjectId projectId)
		{
			StreamConfig config = await _configCollection.GetConfigAsync<StreamConfig>(revision);

			List<StreamTab> tabs = config.Tabs.ConvertAll(x => StreamTab.FromRequest(x));
			Dictionary<TemplateRefId, TemplateRef> templateRefs = await CreateTemplateRefsAsync(config.Templates, stream, _templateCollection);

			Dictionary<string, AgentType> agentTypes = new Dictionary<string, AgentType>();
			if (config.AgentTypes != null)
			{
				agentTypes = config.AgentTypes.Where(x => x.Value != null).ToDictionary(x => x.Key, x => new AgentType(x.Value!));
			}

			Dictionary<string, WorkspaceType> workspaceTypes = new Dictionary<string, WorkspaceType>();
			if (config.WorkspaceTypes != null)
			{
				workspaceTypes = config.WorkspaceTypes.Where(x => x.Value != null).ToDictionary(x => x.Key, x => new WorkspaceType(x.Value!));
			}

			DefaultPreflight? defaultPreflight = config.DefaultPreflight?.ToModel();
			if (defaultPreflight == null && config.DefaultPreflightTemplate != null)
			{
				defaultPreflight = new DefaultPreflight(new TemplateRefId(config.DefaultPreflightTemplate), null);
			}

			Validate(id, defaultPreflight, templateRefs, tabs, agentTypes, workspaceTypes);

			Acl? acl = Acl.Merge(new Acl(), config.Acl);
			if (stream == null)
			{
				return await TryCreateAsync(id, projectId, revision, config, defaultPreflight, tabs, agentTypes, workspaceTypes, templateRefs, acl);
			}
			else
			{
				return await TryReplaceAsync(stream, projectId, revision, config, defaultPreflight, tabs, agentTypes, workspaceTypes, templateRefs, acl);
			}
		}

		/// <summary>
		/// Creates a list of template refs from a set of request objects
		/// </summary>
		/// <param name="requests">Request objects</param>
		/// <param name="stream">The current stream state</param>
		/// <param name="templateCollection">The template service</param>
		/// <returns>List of new template references</returns>
		async Task<Dictionary<TemplateRefId, TemplateRef>> CreateTemplateRefsAsync(List<TemplateRefConfig> requests, IStream? stream, ITemplateCollection templateCollection)
		{
			Dictionary<TemplateRefId, TemplateRef> newTemplateRefs = new Dictionary<TemplateRefId, TemplateRef>();
			foreach (TemplateRefConfig request in requests)
			{
				// Create the template
				ITemplate newTemplate = await templateCollection.AddAsync(request.Name, request.Priority, request.AllowPreflights, request.UpdateIssues, request.PromoteIssuesByDefault, request.InitialAgentType, request.SubmitNewChange, request.SubmitDescription, request.Arguments, request.Parameters.ConvertAll(x => x.ToModel()));

				// Get an identifier for the new template ref
				TemplateRefId newTemplateRefId = request.Id;

				// Create the schedule object
				Schedule? schedule = null;
				if (request.Schedule != null)
				{
					schedule = request.Schedule.ToModel(_clock.UtcNow);
				}

				// Add it to the list
				TemplateRef newTemplateRef = new TemplateRef(newTemplate, request.ShowUgsBadges, request.ShowUgsAlerts, request.NotificationChannel, request.NotificationChannelFilter, request.TriageChannel, schedule, request.ChainedJobs?.ConvertAll(x => new ChainedJobTemplate(x)), null, Acl.Merge(null, request.Acl));
				if (stream != null && stream.Templates.TryGetValue(newTemplateRefId, out TemplateRef? oldTemplateRef))
				{
					if (oldTemplateRef.Schedule != null && newTemplateRef.Schedule != null)
					{
						newTemplateRef.Schedule.CopyState(oldTemplateRef.Schedule);
					}

					if (oldTemplateRef.StepStates != null)
					{
						newTemplateRef.StepStates = new List<TemplateStepState>(oldTemplateRef.StepStates);
					}

				}
				newTemplateRefs.Add(newTemplateRefId, newTemplateRef);
			}
			foreach (TemplateRef templateRef in newTemplateRefs.Values)
			{
				if (templateRef.ChainedJobs != null)
				{
					foreach (ChainedJobTemplate chainedJob in templateRef.ChainedJobs)
					{
						if (!newTemplateRefs.ContainsKey(chainedJob.TemplateRefId))
						{
							throw new InvalidDataException($"Invalid template ref id '{chainedJob.TemplateRefId}");
						}
					}
				}
			}
			return newTemplateRefs;
		}

		/// <inheritdoc/>
		public async Task<IStream?> TryUpdateTemplateRefAsync(IStream streamInterface, TemplateRefId templateRefId, List<UpdateStepStateRequest>? stepStates = null)
		{
			StreamDocument stream = (StreamDocument)streamInterface;

			TemplateRef? oldTemplateRef;
			TemplateRef? newTemplateRef = null;

			if (!stream.Templates.TryGetValue(templateRefId, out oldTemplateRef))
			{
				return null;
			}

			ITemplate? template = await _templateCollection.GetAsync(oldTemplateRef.Hash);
			if (template == null)
			{
				return null;
			}

			UpdateDefinitionBuilder<StreamDocument> updateBuilder = Builders<StreamDocument>.Update;
			List<UpdateDefinition<StreamDocument>> updates = new List<UpdateDefinition<StreamDocument>>();

			Dictionary<TemplateRefId, TemplateRef> newTemplates = new Dictionary<TemplateRefId, TemplateRef>(stream.Templates);

			// clear
			if (stepStates != null && stepStates.Count == 0)
			{
				bool hasUpdates = false;
				foreach (KeyValuePair<TemplateRefId, TemplateRef> entry in newTemplates)
				{
					if (entry.Value?.StepStates != null)
					{
						hasUpdates = true;
						entry.Value.StepStates = null;
					}
				}

				if (hasUpdates)
				{
					updates.Add(updateBuilder.Set(x => x.Templates, newTemplates));
				}
				
			}
			else if (stepStates != null)
			{
				newTemplateRef = new TemplateRef(template, oldTemplateRef.ShowUgsAlerts, oldTemplateRef.ShowUgsAlerts, oldTemplateRef.NotificationChannel, oldTemplateRef.NotificationChannelFilter, oldTemplateRef.TriageChannel, oldTemplateRef.Schedule, oldTemplateRef.ChainedJobs, null, oldTemplateRef.Acl);

				// get currently valid step states
				List<TemplateStepState> newStepStates = oldTemplateRef.StepStates?.Where(x => x.PausedByUserId != null).ToList() ?? new List<TemplateStepState>();

				// generate update list
				foreach (UpdateStepStateRequest updateState in stepStates)
				{
					TemplateStepState? newState = newStepStates.Where(x => x.Name == updateState.Name).FirstOrDefault();

					UserId? PausedByUserId = updateState.PausedByUserId != null ? new UserId(updateState.PausedByUserId) : null;

					if (newState == null)
					{
						// if this is a new state without anything set, ignore it
						if (PausedByUserId == null)
						{
							continue;
						}

						newStepStates.Add(new TemplateStepState(updateState.Name, PausedByUserId, PausedByUserId != null ? DateTime.UtcNow : null));
					}
					else
					{
						if (PausedByUserId == null)
						{
							newState.PauseTimeUtc = null;
						}
						else if (newState.PauseTimeUtc == null)
						{
							newState.PauseTimeUtc = DateTime.UtcNow;
						}

						newState.PausedByUserId = PausedByUserId;						
					}
				}

				newTemplateRef.StepStates = newStepStates.Where(x => x.PausedByUserId != null).ToList();
				if (newTemplateRef.StepStates.Count == 0)
				{
					newTemplateRef.StepStates = null;
				}

				newTemplates[templateRefId] = newTemplateRef;
				updates.Add(updateBuilder.Set(x => x.Templates, newTemplates));

			}

			if (updates.Count == 0)
			{
				return streamInterface;
			}

			return await TryUpdateStreamAsync(stream, updateBuilder.Combine(updates));

		}

		/// <inheritdoc/>
		async Task<IStream?> TryCreateAsync(StreamId id, ProjectId projectId, string configRevision, StreamConfig config, DefaultPreflight? defaultPreflight, List<StreamTab> tabs, Dictionary<string, AgentType> agentTypes, Dictionary<string, WorkspaceType> workspaceTypes, Dictionary<TemplateRefId, TemplateRef> templateRefs, Acl? acl)
		{
			StreamDocument newStream = new StreamDocument(id, config.Name, projectId);
			newStream.ClusterName = config.ClusterName;
			newStream.ConfigRevision = configRevision;
			newStream.Config = config;
			newStream.Order = config.Order ?? StreamDocument.DefaultOrder;
			newStream.NotificationChannel = config.NotificationChannel;
			newStream.NotificationChannelFilter = config.NotificationChannelFilter;
			newStream.TriageChannel = config.TriageChannel;
			newStream.DefaultPreflight = defaultPreflight;
			newStream.Tabs = tabs;
			newStream.AgentTypes = agentTypes;
			newStream.WorkspaceTypes = workspaceTypes;
			newStream.Templates = templateRefs;
			newStream.ReplicationMode = config.ReplicationMode;
			newStream.ReplicationFilter = config.ReplicationFilter;
			newStream.Acl = acl;

			try
			{
				await _streams.InsertOneAsync(newStream);
				return newStream;
			}
			catch (MongoWriteException ex)
			{
				if (ex.WriteError.Category == ServerErrorCategory.DuplicateKey)
				{
					return null;
				}
				else
				{
					throw;
				}
			}
		}

		/// <inheritdoc/>
		async Task<IStream?> TryReplaceAsync(IStream streamInterface, ProjectId projectId, string configRevision, StreamConfig config, DefaultPreflight? defaultPreflight, List<StreamTab> tabs, Dictionary<string, AgentType>? agentTypes, Dictionary<string, WorkspaceType>? workspaceTypes, Dictionary<TemplateRefId, TemplateRef>? templateRefs, Acl? acl)
		{
			int order = config.Order ?? StreamDocument.DefaultOrder;

			StreamDocument stream = (StreamDocument)streamInterface;

			UpdateDefinitionBuilder<StreamDocument> updateBuilder = Builders<StreamDocument>.Update;

			List<UpdateDefinition<StreamDocument>> updates = new List<UpdateDefinition<StreamDocument>>();
			updates.Add(updateBuilder.Set(x => x.Name, config.Name));
			updates.Add(updateBuilder.Set(x => x.ProjectId, projectId));
			updates.Add(updateBuilder.Set(x => x.ClusterName, config.ClusterName));
			updates.Add(updateBuilder.Set(x => x.ConfigRevision, configRevision));
			updates.Add(updateBuilder.Set(x => x.Order, order));
			updates.Add(updateBuilder.Set(x => x.NotificationChannel, config.NotificationChannel));
			updates.Add(updateBuilder.Set(x => x.NotificationChannelFilter, config.NotificationChannelFilter));
			updates.Add(updateBuilder.Set(x => x.TriageChannel, config.TriageChannel));
			updates.Add(updateBuilder.Set(x => x.DefaultPreflight, defaultPreflight));
			updates.Add(updateBuilder.Set(x => x.Tabs, tabs ?? new List<StreamTab>()));
			updates.Add(updateBuilder.Set(x => x.AgentTypes, agentTypes ?? new Dictionary<string, AgentType>()));
			updates.Add(updateBuilder.Set(x => x.WorkspaceTypes, workspaceTypes ?? new Dictionary<string, WorkspaceType>()));
			updates.Add(updateBuilder.Set(x => x.Templates, templateRefs ?? new Dictionary<TemplateRefId, TemplateRef>()));
			updates.Add(updateBuilder.Set(x => x.ReplicationMode, config.ReplicationMode));
			updates.Add(updateBuilder.SetOrUnsetNullRef(x => x.ReplicationFilter, config.ReplicationFilter));
			updates.Add(updateBuilder.SetOrUnsetNullRef(x => x.Acl, acl));
			updates.Add(updateBuilder.Unset(x => x.Deleted));

			return await TryUpdateStreamAsync(stream, updateBuilder.Combine(updates));
		}

		/// <inheritdoc/>
		public async Task<IStream?> GetAsync(StreamId streamId)
		{
			StreamDocument? stream = await _streams.Find<StreamDocument>(x => x.Id == streamId).FirstOrDefaultAsync();
			if (stream != null)
			{
				await PostLoadAsync(stream);
			}
			return stream;
		}

		/// <inheritdoc/>
		public async Task<IStreamPermissions?> GetPermissionsAsync(StreamId streamId)
		{
			return await _streams.Find<StreamDocument>(x => x.Id == streamId).Project<StreamPermissions>(StreamPermissions.Projection).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<List<IStream>> FindAllAsync()
		{
			List<StreamDocument> results = await _streams.Find(Builders<StreamDocument>.Filter.Ne(x => x.Deleted, true)).ToListAsync();
			foreach (StreamDocument result in results)
			{
				await PostLoadAsync(result);
			}
			return results.ConvertAll<IStream>(x => x);
		}

		/// <inheritdoc/>
		public async Task<List<IStream>> FindForProjectsAsync(ProjectId[] projectIds)
		{
			FilterDefinition<StreamDocument> filter = Builders<StreamDocument>.Filter.In(x => x.ProjectId, projectIds) & Builders<StreamDocument>.Filter.Ne(x => x.Deleted, true);

			List<StreamDocument> results = await _streams.Find(filter).ToListAsync();
			foreach (StreamDocument result in results)
			{
				await PostLoadAsync(result);
			}
			return results.ConvertAll<IStream>(x => x);
		}

		/// <inheritdoc/>
		public async Task<IStream?> TryUpdatePauseStateAsync(IStream streamInterface, DateTime? newPausedUntil, string? newPauseComment)
		{
			StreamDocument stream = (StreamDocument)streamInterface;

			UpdateDefinitionBuilder<StreamDocument> updateBuilder = Builders<StreamDocument>.Update;

			List<UpdateDefinition<StreamDocument>> updates = new List<UpdateDefinition<StreamDocument>>();
			stream.PausedUntil = newPausedUntil;
			stream.PauseComment = newPauseComment;
			updates.Add(updateBuilder.Set(x => x.PausedUntil, newPausedUntil));
			updates.Add(updateBuilder.Set(x => x.PauseComment, newPauseComment));

			return await TryUpdateStreamAsync(stream, updateBuilder.Combine(updates));
		}

		/// <inheritdoc/>
		public async Task<IStream?> TryUpdateScheduleTriggerAsync(IStream streamInterface, TemplateRefId templateId, DateTime? lastTriggerTimeUtc, int? lastTriggerChange, List<JobId> newActiveJobs)
		{
			StreamDocument stream = (StreamDocument)streamInterface;
			TemplateRef template = stream.Templates[templateId];
			Schedule schedule = template.Schedule!;

			// Build the updates. MongoDB driver cannot parse TemplateRefId in expression tree; need to specify field name explicitly
			List<UpdateDefinition<StreamDocument>> updates = new List<UpdateDefinition<StreamDocument>>();
			if (lastTriggerTimeUtc.HasValue && lastTriggerTimeUtc.Value != schedule.LastTriggerTime)
			{
				FieldDefinition<StreamDocument, DateTimeOffset> lastTriggerTimeField = $"{nameof(stream.Templates)}.{templateId}.{nameof(template.Schedule)}.{nameof(schedule.LastTriggerTime)}";
				updates.Add(Builders<StreamDocument>.Update.Set(lastTriggerTimeField, lastTriggerTimeUtc.Value));
				schedule.LastTriggerTimeUtc = lastTriggerTimeUtc.Value;
			}
			if (lastTriggerChange.HasValue && lastTriggerChange.Value > schedule.LastTriggerChange)
			{
				FieldDefinition<StreamDocument, int> lastTriggerChangeField = $"{nameof(stream.Templates)}.{templateId}.{nameof(template.Schedule)}.{nameof(schedule.LastTriggerChange)}";
				updates.Add(Builders<StreamDocument>.Update.Set(lastTriggerChangeField, lastTriggerChange.Value));
				schedule.LastTriggerChange = lastTriggerChange.Value;
			}
			if (newActiveJobs != null)
			{
				FieldDefinition<StreamDocument, List<JobId>> field = $"{nameof(stream.Templates)}.{templateId}.{nameof(template.Schedule)}.{nameof(schedule.ActiveJobs)}";
				updates.Add(Builders<StreamDocument>.Update.Set(field, newActiveJobs));
				schedule.ActiveJobs = newActiveJobs;
			}

			return (updates.Count == 0)? streamInterface : await TryUpdateStreamAsync(stream, Builders<StreamDocument>.Update.Combine(updates));
		}

		/// <summary>
		/// Update a stream
		/// </summary>
		/// <param name="stream">The stream to update</param>
		/// <param name="update">The update definition</param>
		/// <returns>The updated document, or null the update failed</returns>
		private async Task<StreamDocument?> TryUpdateStreamAsync(StreamDocument stream, UpdateDefinition<StreamDocument> update)
		{
			FilterDefinition<StreamDocument> filter = Builders<StreamDocument>.Filter.Expr(x => x.Id == stream.Id && x.UpdateIndex == stream.UpdateIndex);
			update = update.Set(x => x.UpdateIndex, stream.UpdateIndex + 1);

			FindOneAndUpdateOptions<StreamDocument> options = new FindOneAndUpdateOptions<StreamDocument> { ReturnDocument = ReturnDocument.After };

			StreamDocument? result = await _streams.FindOneAndUpdateAsync(filter, update, options);
			if(result != null)
			{
				await PostLoadAsync(result);
			}
			return result;
		}

		/// <inheritdoc/>
		public async Task DeleteAsync(StreamId streamId)
		{
			await _streams.UpdateOneAsync<StreamDocument>(x => x.Id == streamId, Builders<StreamDocument>.Update.Set(x => x.Deleted, true).Inc(x => x.UpdateIndex, 1));
		}

		/// <summary>
		/// Checks the stream definition for consistency
		/// </summary>
		public static void Validate(StreamId streamId, DefaultPreflight? defaultPreflight, IReadOnlyDictionary<TemplateRefId, TemplateRef> templates, IReadOnlyList<StreamTab> tabs, IReadOnlyDictionary<string, AgentType> agentTypes, IReadOnlyDictionary<string, WorkspaceType> workspaceTypes)
		{
			// Check the default preflight template is valid
			if (defaultPreflight != null)
			{
				if (defaultPreflight.TemplateRefId != null && !templates.ContainsKey(defaultPreflight.TemplateRefId.Value))
				{
					throw new InvalidStreamException($"Default preflight template was listed as '{defaultPreflight.TemplateRefId.Value}', but no template was found by that name");
				}
			}

			// Check that all the templates are referenced by a tab
			HashSet<TemplateRefId> remainingTemplates = new HashSet<TemplateRefId>(templates.Keys);
			foreach (JobsTab jobsTab in tabs.OfType<JobsTab>())
			{
				if (jobsTab.Templates != null)
				{
					remainingTemplates.ExceptWith(jobsTab.Templates);
				}
			}
			if (remainingTemplates.Count > 0)
			{
				throw new InvalidStreamException(String.Join("\n", remainingTemplates.Select(x => $"Template '{x}' is not listed on any tab for {streamId}")));
			}

			// Check that all the agent types reference valid workspace names
			foreach (KeyValuePair<string, AgentType> pair in agentTypes)
			{
				string? workspaceTypeName = pair.Value.Workspace;
				if (workspaceTypeName != null && !workspaceTypes.ContainsKey(workspaceTypeName))
				{
					throw new InvalidStreamException($"Agent type '{pair.Key}' references undefined workspace type '{pair.Value.Workspace}' in {streamId}");
				}
			}
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Streams
{
	/// <summary>
	/// Collection of stream documents
	/// </summary>
	public class StreamCollection : IStreamCollection
	{
		/// <summary>
		/// Information about a stream
		/// </summary>
		class StreamDoc : IStream
		{
			[BsonRequired, BsonId]
			public StreamId Id { get; set; }

			public Dictionary<TemplateId, TemplateRefDoc> Templates { get; set; } = new Dictionary<TemplateId, TemplateRefDoc>();
			public DateTime? PausedUntil { get; set; }
			public string? PauseComment { get; set; }

			public int UpdateIndex { get; set; }

			[BsonConstructor]
			private StreamDoc()
			{
			}

			public StreamDoc(StreamId id)
			{
				Id = id;
			}

			#region IStream Implementation

			[BsonIgnore]
			public StreamConfig Config { get; private set; } = null!;

			[BsonIgnore]
			IReadOnlyDictionary<TemplateId, ITemplateRef>? _cachedTemplates;

			IReadOnlyDictionary<TemplateId, ITemplateRef> IStream.Templates
			{
				get
				{
					_cachedTemplates ??= Templates.ToDictionary(x => x.Key, x => (ITemplateRef)x.Value);
					return _cachedTemplates;
				}
			}

			public bool PostLoad(StreamConfig config, DateTime utcNow)
			{
				Config = config;

				bool replaceDocument = false;

				Dictionary<TemplateId, TemplateRefDoc> templateRefDocs = new Dictionary<TemplateId, TemplateRefDoc>(config.Templates.Count);
				foreach (TemplateRefConfig templateRefConfig in config.Templates)
				{
					TemplateRefDoc? templateRefDoc;
					if (!Templates.TryGetValue(templateRefConfig.Id, out templateRefDoc))
					{
						templateRefDoc = new TemplateRefDoc();
						replaceDocument = true;
					}
					replaceDocument |= templateRefDoc.PostLoad(templateRefConfig, utcNow);

					templateRefDocs.Add(templateRefConfig.Id, templateRefDoc);
				}

				replaceDocument |= Templates.Count != templateRefDocs.Count;
				Templates = templateRefDocs;

				return replaceDocument;
			}

			#endregion
		}

		class TemplateRefDoc : ITemplateRef
		{
			[BsonIgnoreIfNull]
			public TemplateScheduleDoc? Schedule { get; set; }

			[BsonIgnoreIfNull]
			public List<TemplateStepDoc>? StepStates { get; set; }

			#region ITemplateRef implementation

			[BsonIgnore]
			public TemplateId Id { get; private set; }

			[BsonIgnore]
			public TemplateRefConfig Config { get; private set; } = null!;

			ITemplateSchedule? ITemplateRef.Schedule => Schedule;
			IReadOnlyList<ITemplateStep> ITemplateRef.StepStates => (IReadOnlyList<ITemplateStep>?)StepStates ?? Array.Empty<ITemplateStep>();

			public bool PostLoad(TemplateRefConfig config, DateTime utcNow)
			{
				Id = config.Id;
				Config = config;

				bool replaceDocument = false;

				if (Config.Schedule == null)
				{
					if (Schedule != null)
					{
						Schedule = null;
						replaceDocument = true;
					}
				}
				else
				{
					if (Schedule == null)
					{
						Schedule = new TemplateScheduleDoc();
						Schedule.LastTriggerTimeUtc = utcNow;
						replaceDocument = true;
					}
				}

				Schedule?.PostLoad(this);

				if (StepStates != null)
				{
					int count = StepStates.RemoveAll(x => x.PausedByUserId == null);
					replaceDocument |= count > 0;
				}

				return replaceDocument;
			}

			#endregion
		}

		class TemplateScheduleDoc : ITemplateSchedule
		{
			public int LastTriggerChange { get; set; }

			[BsonIgnoreIfNull, Obsolete("Use LastTriggerTimeUtc instead")]
			public DateTimeOffset? LastTriggerTime { get; set; }

			public DateTime LastTriggerTimeUtc { get; set; }
			public List<JobId> ActiveJobs { get; set; } = new List<JobId>();

			#region ITemplateSchedule implementation

			[BsonIgnore]
			TemplateRefDoc? _owner;

			[BsonIgnore]
			public ScheduleConfig Config => _owner?.Config.Schedule!;

			IReadOnlyList<JobId> ITemplateSchedule.ActiveJobs => ActiveJobs;

			public void PostLoad(TemplateRefDoc owner)
			{
				_owner = owner;

#pragma warning disable CS0618 // Type or member is obsolete
				if (LastTriggerTime.HasValue)
				{
					LastTriggerTimeUtc = LastTriggerTime.Value.UtcDateTime;
					LastTriggerTime = null;
				}
#pragma warning restore CS0618 // Type or member is obsolete
			}

			#endregion
		}

		class TemplateStepDoc : ITemplateStep
		{
			public string Name { get; set; } = String.Empty;
			public UserId? PausedByUserId { get; set; }
			public DateTime? PauseTimeUtc { get; set; }

			UserId ITemplateStep.PausedByUserId => PausedByUserId ?? UserId.Empty;
			DateTime ITemplateStep.PauseTimeUtc => PauseTimeUtc ?? DateTime.MinValue;

			public TemplateStepDoc()
			{
			}

			public TemplateStepDoc(string name, UserId pausedByUserId, DateTime pauseTimeUtc)
			{
				Name = name;
				PausedByUserId = pausedByUserId;
				PauseTimeUtc = pauseTimeUtc;
			}
		}

		readonly IMongoCollection<StreamDoc> _streams;
		readonly IClock _clock;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		/// <param name="clock"></param>
		public StreamCollection(MongoService mongoService, IClock clock)
		{
			_streams = mongoService.GetCollection<StreamDoc>("Streams");
			_clock = clock;
		}

		/// <inheritdoc/>
		public async Task<IStream> GetAsync(StreamConfig streamConfig, CancellationToken cancellationToken)
			=> await GetInternalAsync(streamConfig, cancellationToken);

		async Task<StreamDoc> GetInternalAsync(StreamConfig streamConfig, CancellationToken cancellationToken)
		{
			FindOneAndUpdateOptions<StreamDoc, StreamDoc> options = new FindOneAndUpdateOptions<StreamDoc, StreamDoc>();
			options.IsUpsert = true;
			options.ReturnDocument = ReturnDocument.After;

			for (; ; )
			{
				StreamDoc stream = await _streams.FindOneAndUpdateAsync<StreamDoc>(x => x.Id == streamConfig.Id, Builders<StreamDoc>.Update.SetOnInsert(x => x.UpdateIndex, 0), options, cancellationToken);
				if (await PostLoadAsync(stream, streamConfig, cancellationToken))
				{
					return stream;
				}
			}
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IStream>> GetAsync(IReadOnlyList<StreamConfig> streamConfigs, CancellationToken cancellationToken)
			=> await GetInternalAsync(streamConfigs, cancellationToken);

		async Task<List<StreamDoc>> GetInternalAsync(IReadOnlyList<StreamConfig> streamConfigs, CancellationToken cancellationToken)
		{
			FilterDefinition<StreamDoc> filter = Builders<StreamDoc>.Filter.In(x => x.Id, streamConfigs.Select(x => x.Id));
			List<StreamDoc> matches = await _streams.Find(filter).ToListAsync(cancellationToken);

			List<StreamDoc> results = new List<StreamDoc>(streamConfigs.Count);
			foreach (StreamConfig streamConfig in streamConfigs)
			{
				StreamDoc? stream = matches.FirstOrDefault(x => x.Id == streamConfig.Id);
				if (stream == null)
				{
					stream = await GetInternalAsync(streamConfig, cancellationToken);
				}
				else
				{
					stream.PostLoad(streamConfig, _clock.UtcNow);
				}
				results.Add(stream);
			}
			return results;
		}

		async ValueTask<bool> PostLoadAsync(StreamDoc streamDoc, StreamConfig streamConfig, CancellationToken cancellationToken)
		{
			if (streamDoc.PostLoad(streamConfig, _clock.UtcNow))
			{
				int updateIndex = streamDoc.UpdateIndex++;

				ReplaceOneResult result = await _streams.ReplaceOneAsync(x => x.Id == streamDoc.Id && x.UpdateIndex == updateIndex, streamDoc, cancellationToken: cancellationToken);
				if (result.MatchedCount != 1)
				{
					return false;
				}
			}
			return true;
		}

		/// <inheritdoc/>
		public async Task<IStream?> TryUpdateTemplateRefAsync(IStream streamInterface, TemplateId templateId, List<UpdateStepStateRequest>? stepStates = null, CancellationToken cancellationToken = default)
		{
			StreamDoc stream = (StreamDoc)streamInterface;

			TemplateRefDoc? templateRef;
			if (!stream.Templates.TryGetValue(templateId, out templateRef))
			{
				return null;
			}

			UpdateDefinitionBuilder<StreamDoc> updateBuilder = Builders<StreamDoc>.Update;
			List<UpdateDefinition<StreamDoc>> updates = new List<UpdateDefinition<StreamDoc>>();

			Dictionary<TemplateId, TemplateRefDoc> newTemplates = new Dictionary<TemplateId, TemplateRefDoc>(stream.Templates);

			// clear
			if (stepStates != null && stepStates.Count == 0)
			{
				bool hasUpdates = false;
				foreach (KeyValuePair<TemplateId, TemplateRefDoc> entry in newTemplates)
				{
					if (entry.Value.StepStates != null)
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
				// get currently valid step states
				List<TemplateStepDoc> newStepStates = templateRef.StepStates?.ToList() ?? new List<TemplateStepDoc>();

				// generate update list
				foreach (UpdateStepStateRequest updateState in stepStates)
				{
					int stateIndex = newStepStates.FindIndex(x => x.Name == updateState.Name);

					UserId? pausedByUserId = updateState.PausedByUserId != null ? UserId.Parse(updateState.PausedByUserId) : null;

					if (stateIndex == -1)
					{
						// if this is a new state without anything set, ignore it
						if (pausedByUserId != null)
						{
							newStepStates.Add(new TemplateStepDoc(updateState.Name, pausedByUserId.Value, _clock.UtcNow));
						}
					}
					else
					{
						if (pausedByUserId == null)
						{
							newStepStates.RemoveAt(stateIndex);
						}
						else
						{
							newStepStates[stateIndex].PausedByUserId = pausedByUserId.Value;
						}
					}
				}

				if (newStepStates.Count == 0)
				{
					templateRef.StepStates = null;
				}
				else
				{
					templateRef.StepStates = newStepStates;
				}

				updates.Add(updateBuilder.Set(x => x.Templates, newTemplates));
			}

			if (updates.Count == 0)
			{
				return streamInterface;
			}

			return await TryUpdateStreamAsync(stream, updateBuilder.Combine(updates), cancellationToken);

		}

		/// <inheritdoc/>
		public async Task<IStream?> TryUpdatePauseStateAsync(IStream streamInterface, DateTime? newPausedUntil, string? newPauseComment, CancellationToken cancellationToken)
		{
			StreamDoc stream = (StreamDoc)streamInterface;

			UpdateDefinitionBuilder<StreamDoc> updateBuilder = Builders<StreamDoc>.Update;

			List<UpdateDefinition<StreamDoc>> updates = new List<UpdateDefinition<StreamDoc>>();
			stream.PausedUntil = newPausedUntil;
			stream.PauseComment = newPauseComment;
			updates.Add(updateBuilder.Set(x => x.PausedUntil, newPausedUntil));
			updates.Add(updateBuilder.Set(x => x.PauseComment, newPauseComment));

			return await TryUpdateStreamAsync(stream, updateBuilder.Combine(updates), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IStream?> TryUpdateScheduleTriggerAsync(IStream streamInterface, TemplateId templateId, DateTime? lastTriggerTimeUtc, int? lastTriggerChange, List<JobId> newActiveJobs, CancellationToken cancellationToken)
		{
			StreamDoc stream = (StreamDoc)streamInterface;
			TemplateRefDoc template = stream.Templates[templateId];
			TemplateScheduleDoc schedule = template.Schedule!;

			// Build the updates. MongoDB driver cannot parse TemplateRefId in expression tree; need to specify field name explicitly
			List<UpdateDefinition<StreamDoc>> updates = new List<UpdateDefinition<StreamDoc>>();
			if (lastTriggerTimeUtc.HasValue && lastTriggerTimeUtc.Value != schedule.LastTriggerTimeUtc)
			{
				FieldDefinition<StreamDoc, DateTime> lastTriggerTimeField = $"{nameof(stream.Templates)}.{templateId}.{nameof(template.Schedule)}.{nameof(schedule.LastTriggerTimeUtc)}";
				updates.Add(Builders<StreamDoc>.Update.Set(lastTriggerTimeField, lastTriggerTimeUtc.Value));
				schedule.LastTriggerTimeUtc = lastTriggerTimeUtc.Value;
			}
			if (lastTriggerChange.HasValue && lastTriggerChange.Value > schedule.LastTriggerChange)
			{
				FieldDefinition<StreamDoc, int> lastTriggerChangeField = $"{nameof(stream.Templates)}.{templateId}.{nameof(template.Schedule)}.{nameof(schedule.LastTriggerChange)}";
				updates.Add(Builders<StreamDoc>.Update.Set(lastTriggerChangeField, lastTriggerChange.Value));
				schedule.LastTriggerChange = lastTriggerChange.Value;
			}
			if (newActiveJobs != null)
			{
				FieldDefinition<StreamDoc, List<JobId>> field = $"{nameof(stream.Templates)}.{templateId}.{nameof(template.Schedule)}.{nameof(schedule.ActiveJobs)}";
				updates.Add(Builders<StreamDoc>.Update.Set(field, newActiveJobs));
				schedule.ActiveJobs = newActiveJobs;
			}

			return (updates.Count == 0) ? streamInterface : await TryUpdateStreamAsync(stream, Builders<StreamDoc>.Update.Combine(updates), cancellationToken);
		}

		/// <summary>
		/// Update a stream
		/// </summary>
		/// <param name="stream">The stream to update</param>
		/// <param name="update">The update definition</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The updated document, or null the update failed</returns>
		private async Task<StreamDoc?> TryUpdateStreamAsync(StreamDoc stream, UpdateDefinition<StreamDoc> update, CancellationToken cancellationToken)
		{
			FilterDefinition<StreamDoc> filter = Builders<StreamDoc>.Filter.Expr(x => x.Id == stream.Id && x.UpdateIndex == stream.UpdateIndex);
			update = update.Set(x => x.UpdateIndex, stream.UpdateIndex + 1);

			FindOneAndUpdateOptions<StreamDoc> options = new FindOneAndUpdateOptions<StreamDoc> { ReturnDocument = ReturnDocument.After };

			StreamDoc? result = await _streams.FindOneAndUpdateAsync(filter, update, options, cancellationToken);
			if (result != null)
			{
				result.PostLoad(stream.Config, _clock.UtcNow);
			}
			return result;
		}

		/// <summary>
		/// Checks the stream definition for consistency
		/// </summary>
		public static void Validate(StreamId streamId, StreamConfig config)
		{
			HashSet<TemplateId> remainingTemplates = new HashSet<TemplateId>(config.Templates.Select(x => x.Id));

			// Check the default preflight template is valid
			if (config.DefaultPreflight != null)
			{
				if (config.DefaultPreflight.TemplateId != null && !remainingTemplates.Contains(config.DefaultPreflight.TemplateId.Value))
				{
					throw new InvalidStreamException($"Default preflight template was listed as '{config.DefaultPreflight.TemplateId.Value}', but no template was found by that name");
				}
			}

			// Check the chained jobs are valid
			foreach (TemplateRefConfig templateRef in config.Templates)
			{
				if (templateRef.ChainedJobs != null)
				{
					foreach (ChainedJobTemplateConfig chainedJob in templateRef.ChainedJobs)
					{
						if (!remainingTemplates.Contains(chainedJob.TemplateId))
						{
							throw new InvalidDataException($"Invalid template ref id '{chainedJob.TemplateId}");
						}
					}
				}
			}

			// Check that all the templates are referenced by a tab
			HashSet<TemplateId> undefinedTemplates = new();
			foreach (TabConfig tab in config.Tabs)
			{
				if (tab.Templates != null)
				{
					remainingTemplates.ExceptWith(tab.Templates);
					foreach (TemplateId templateId in tab.Templates)
					{
						if (config.Templates.Find(x => x.Id == templateId) == null)
						{
							undefinedTemplates.Add(templateId);
						}
					}
				}
			}
			if (remainingTemplates.Count > 0)
			{
				throw new InvalidStreamException(String.Join("\n", remainingTemplates.Select(x => $"Template '{x}' is not listed on any tab for {streamId}")));
			}

			if (undefinedTemplates.Count > 0)
			{
				throw new InvalidStreamException(String.Join("\n", undefinedTemplates.Select(x => $"Template '{x}' is not defined for {streamId}")));
			}

			// Check that all the agent types reference valid workspace names
			foreach (KeyValuePair<string, AgentConfig> pair in config.AgentTypes)
			{
				string? workspaceTypeName = pair.Value.Workspace;
				if (workspaceTypeName != null && !config.WorkspaceTypes.ContainsKey(workspaceTypeName))
				{
					throw new InvalidStreamException($"Agent type '{pair.Key}' references undefined workspace type '{pair.Value.Workspace}' in {streamId}");
				}
			}
		}
	}
}

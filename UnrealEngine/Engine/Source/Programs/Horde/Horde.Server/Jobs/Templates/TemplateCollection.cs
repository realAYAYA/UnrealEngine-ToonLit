// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Server;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Jobs.Templates
{
	/// <summary>
	/// Collection of template documents
	/// </summary>
	public sealed class TemplateCollection : ITemplateCollection, IDisposable
	{
		/// <summary>
		/// Document describing a job template. These objects are considered immutable once created and uniquely referenced by hash, in order to de-duplicate across all job runs.
		/// </summary>
		class TemplateDocument : ITemplate
		{
			[BsonRequired, BsonId]
			public ContentHash Id { get; private set; } = ContentHash.Empty;

			[BsonRequired]
			public string Name { get; private set; }

			[BsonIgnoreIfNull]
			public string? Description { get; set; }

			public Priority? Priority { get; private set; }
			public bool AllowPreflights { get; set; } = true;
			public bool UpdateIssues { get; set; } = false;
			public bool PromoteIssuesByDefault { get; set; } = false;
			public string? InitialAgentType { get; set; }

			[BsonIgnoreIfNull]
			public string? SubmitNewChange { get; set; }

			[BsonIgnoreIfNull]
			public string? SubmitDescription { get; set; }

			public List<string> Arguments { get; private set; } = new List<string>();
			public List<Parameter> Parameters { get; private set; } = new List<Parameter>();

			ContentHash ITemplate.Hash => Id;
			IReadOnlyList<string> ITemplate.Arguments => Arguments;
			IReadOnlyList<Parameter> ITemplate.Parameters => Parameters;

			[BsonConstructor]
			private TemplateDocument()
			{
				Name = null!;
			}

			public TemplateDocument(TemplateConfig config)
			{
				Name = config.Name;
				Description = config.Description;
				Priority = config.Priority;
				AllowPreflights = config.AllowPreflights;
				UpdateIssues = config.UpdateIssues;
				PromoteIssuesByDefault = config.PromoteIssuesByDefault;
				InitialAgentType = config.InitialAgentType;
				SubmitNewChange = config.SubmitNewChange;
				SubmitDescription = config.SubmitDescription;
				Arguments = config.Arguments ?? new List<string>();
				Parameters = config.Parameters.ConvertAll(x => x.ToModel());

				// Compute the hash once all other fields have been set
				Id = ContentHash.SHA1(BsonExtensionMethods.ToBson(this));
			}
		}

		/// <summary>
		/// Template documents
		/// </summary>
		readonly IMongoCollection<TemplateDocument> _templates;

		/// <summary>
		/// Cache of template documents
		/// </summary>
		readonly MemoryCache _templateCache;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service singleton</param>
		public TemplateCollection(MongoService mongoService)
		{
			_templates = mongoService.GetCollection<TemplateDocument>("Templates");

			MemoryCacheOptions options = new MemoryCacheOptions();
			_templateCache = new MemoryCache(options);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_templateCache.Dispose();
		}

		/// <inheritdoc/>
		public async Task<ITemplate?> GetAsync(ContentHash templateId)
		{
			object? result;
			if (_templateCache.TryGetValue(templateId, out result))
			{
				return (ITemplate?)result;
			}

			ITemplate? template = await _templates.Find<TemplateDocument>(x => x.Id == templateId).FirstOrDefaultAsync();
			if (template != null)
			{
				using (ICacheEntry entry = _templateCache.CreateEntry(templateId))
				{
					entry.SetSlidingExpiration(TimeSpan.FromMinutes(30.0));
					entry.SetValue(template);
				}
			}

			return template;
		}

		/// <inheritdoc/>
		public async Task<ITemplate> GetOrAddAsync(TemplateConfig config)
		{
			if (config.CachedHash != null)
			{
				ITemplate? existingTemplate = await GetAsync(config.CachedHash);
				if (existingTemplate != null)
				{
					return existingTemplate;
				}
			}

			TemplateDocument template = new TemplateDocument(config);
			if (await GetAsync(template.Id) == null)
			{
				await _templates.ReplaceOneAsync(x => x.Id == template.Id, template, new ReplaceOptions { IsUpsert = true });
			}
			return template;
		}
	}
}

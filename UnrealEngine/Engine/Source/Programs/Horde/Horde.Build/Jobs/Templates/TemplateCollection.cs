// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Jobs.Templates
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

			IReadOnlyList<string> ITemplate.Arguments => Arguments;
			IReadOnlyList<Parameter> ITemplate.Parameters => Parameters;

			[BsonConstructor]
			private TemplateDocument()
			{
				Name = null!;
			}

			public TemplateDocument(string name, Priority? priority, bool bAllowPreflights, bool updateIssues, bool promoteIssuesByDefault, string? initialAgentType, string? submitNewChange, string? submitDescription, List<string>? arguments, List<Parameter>? parameters)
			{
				Name = name;
				Priority = priority;
				AllowPreflights = bAllowPreflights;
				UpdateIssues = updateIssues;
				PromoteIssuesByDefault = promoteIssuesByDefault;
				InitialAgentType = initialAgentType;
				SubmitNewChange = submitNewChange;
				SubmitDescription = submitDescription;
				Arguments = arguments ?? new List<string>();
				Parameters = parameters ?? new List<Parameter>();

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
			// Ensure discriminator cannot be registered twice (throws exception). Can otherwise happen during unit tests.
			if (BsonSerializer.LookupDiscriminatorConvention(typeof(JobsTabColumn)) == null)
			{
				BsonSerializer.RegisterDiscriminatorConvention(typeof(JobsTabColumn), new DefaultDiscriminatorConvention(typeof(JobsTabColumn), typeof(JobsTabLabelColumn)));	
			}
			
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
		public async Task<ITemplate> AddAsync(string name, Priority? priority, bool bAllowPreflights, bool bUpdateIssues, bool bPromoteIssuesByDefault, string? initialAgentType, string? submitNewChange, string? submitDescription, List<string>? arguments, List<Parameter>? parameters)
		{
			TemplateDocument template = new TemplateDocument(name, priority, bAllowPreflights, bUpdateIssues, bPromoteIssuesByDefault, initialAgentType, submitNewChange, submitDescription, arguments, parameters);
			if (await GetAsync(template.Id) == null)
			{
				await _templates.ReplaceOneAsync(x => x.Id == template.Id, template, new ReplaceOptions { IsUpsert = true });
			}
			return template;
		}

		/// <inheritdoc/>
		public async Task<List<ITemplate>> FindAllAsync()
		{
			List<TemplateDocument> results = await _templates.Find(FilterDefinition<TemplateDocument>.Empty).ToListAsync();
			return results.ConvertAll<ITemplate>(x => x);
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
	}
}

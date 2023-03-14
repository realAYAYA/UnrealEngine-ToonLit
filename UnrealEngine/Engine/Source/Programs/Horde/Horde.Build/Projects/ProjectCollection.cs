// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Server;
using Horde.Build.Utilities;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Projects
{
	using ProjectId = StringId<IProject>;

	/// <summary>
	/// Wraps functionality for manipulating projects
	/// </summary>
	public class ProjectCollection : IProjectCollection
	{
		/// <summary>
		/// Represents a project
		/// </summary>
		class ProjectDocument : IProject
		{
			public const int DefaultOrder = 128;

			[BsonRequired, BsonId]
			public ProjectId Id { get; set; }

			[BsonRequired]
			public string Name { get; set; }

			public string? ConfigPath { get; set; }
			public string? ConfigRevision { get; set; }

			public int Order { get; set; } = DefaultOrder;
			public Acl? Acl { get; set; }
			public List<StreamCategory> Categories { get; set; } = new List<StreamCategory>();

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool Deleted { get; set; }

			IReadOnlyList<StreamCategory> IProject.Categories => Categories;

			[BsonConstructor]
			private ProjectDocument()
			{
				Name = null!;
			}

			public ProjectDocument(ProjectId id, string name)
			{
				Id = id;
				Name = name;
			}
		}

		/// <summary>
		/// Logo for a project
		/// </summary>
		class ProjectLogoDocument : IProjectLogo
		{
			public ProjectId Id { get; set; }

			public string Path { get; set; } = String.Empty;
			public string Revision { get; set; } = String.Empty;
			public string MimeType { get; set; } = String.Empty;

			public byte[] Data { get; set; } = Array.Empty<byte>();
		}

		/// <summary>
		/// Projection of a project definition to just include permissions info
		/// </summary>
		[SuppressMessage("Design", "CA1812: Class is never instantiated")]
		class ProjectPermissions : IProjectPermissions
		{
			/// <summary>
			/// ACL for the project
			/// </summary>
			public Acl? Acl { get; set; }

			/// <summary>
			/// Projection to extract the permissions info from the project
			/// </summary>
			public static readonly ProjectionDefinition<ProjectDocument> Projection = Builders<ProjectDocument>.Projection.Include(x => x.Acl);
		}

		/// <summary>
		/// Collection of project documents
		/// </summary>
		readonly IMongoCollection<ProjectDocument> _projects;

		/// <summary>
		/// Collection of project logo documents
		/// </summary>
		readonly IMongoCollection<ProjectLogoDocument> _projectLogos;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		public ProjectCollection(MongoService mongoService)
		{
			_projects = mongoService.GetCollection<ProjectDocument>("Projects");
			_projectLogos = mongoService.GetCollection<ProjectLogoDocument>("ProjectLogos");
		}

		/// <inheritdoc/>
		public async Task<IProject?> AddOrUpdateAsync(ProjectId id, string configPath, string revision, int order, ProjectConfig config)
		{
			ProjectDocument newProject = new ProjectDocument(id, config.Name);
			newProject.ConfigPath = configPath;
			newProject.ConfigRevision = revision;
			newProject.Order = order;
			newProject.Categories = config.Categories.ConvertAll(x => new StreamCategory(x));
			newProject.Acl = Acl.Merge(new Acl(), config.Acl);

			await _projects.FindOneAndReplaceAsync<ProjectDocument>(x => x.Id == id, newProject, new FindOneAndReplaceOptions<ProjectDocument> { IsUpsert = true });
			return newProject;
		}

		/// <inheritdoc/>
		public async Task<List<IProject>> FindAllAsync()
		{
			List<ProjectDocument> results = await _projects.Find(x => !x.Deleted).ToListAsync();
			return results.OrderBy(x => x.Order).ThenBy(x => x.Name).Select<ProjectDocument, IProject>(x => x).ToList();
		}

		/// <inheritdoc/>
		public async Task<IProject?> GetAsync(ProjectId projectId)
		{
			return await _projects.Find<ProjectDocument>(x => x.Id == projectId).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<IProjectLogo?> GetLogoAsync(ProjectId projectId)
		{
			return await _projectLogos.Find(x => x.Id == projectId).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task SetLogoAsync(ProjectId projectId, string logoPath, string logoRevision, string mimeType, byte[] data)
		{
			ProjectLogoDocument logo = new ProjectLogoDocument();
			logo.Id = projectId;
			logo.Path = logoPath;
			logo.Revision = logoRevision;
			logo.MimeType = mimeType;
			logo.Data = data;
			await _projectLogos.ReplaceOneAsync(x => x.Id == projectId, logo, new ReplaceOptions { IsUpsert = true });
		}

		/// <inheritdoc/>
		public async Task<IProjectPermissions?> GetPermissionsAsync(ProjectId projectId)
		{
			return await _projects.Find<ProjectDocument>(x => x.Id == projectId).Project<ProjectPermissions>(ProjectPermissions.Projection).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task DeleteAsync(ProjectId projectId)
		{
			await _projects.UpdateOneAsync(x => x.Id == projectId, Builders<ProjectDocument>.Update.Set(x => x.Deleted, true));
		}
	}
}

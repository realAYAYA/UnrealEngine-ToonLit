// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using HordeCommon;

namespace Horde.Server.Jobs.Templates
{
	/// <summary>
	/// Response describing a template
	/// </summary>
	public class GetTemplateResponseBase
	{
		/// <summary>
		/// Name of the template
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Description for the template
		/// </summary>
		public string? Description { get; set; }

		/// <summary>
		/// Default priority for this job
		/// </summary>
		public Priority? Priority { get; set; }

		/// <summary>
		/// Whether to allow preflights of this template
		/// </summary>
		public bool AllowPreflights { get; set; }

		/// <summary>
		/// Whether to always update issues on jobs using this template
		/// </summary>
		public bool UpdateIssues { get; set; }

		/// <summary>
		/// The initial agent type to parse the BuildGraph script on
		/// </summary>
		public string? InitialAgentType { get; set; }

		/// <summary>
		/// Path to a file within the stream to submit to generate a new changelist for jobs
		/// </summary>
		public string? SubmitNewChange { get; }

		/// <summary>
		/// Parameters for the job.
		/// </summary>
		public List<string> Arguments { get; set; }

		/// <summary>
		/// List of parameters for this template
		/// </summary>
		public List<ParameterData> Parameters { get; set; }

		/// <summary>
		/// Parameterless constructor for serialization
		/// </summary>
		protected GetTemplateResponseBase()
		{
			Name = null!;
			AllowPreflights = true;
			Arguments = new List<string>();
			Parameters = new List<ParameterData>();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="template">The template to construct from</param>
		public GetTemplateResponseBase(ITemplate template)
		{
			Name = template.Name;
			Description = template.Description;
			Priority = template.Priority;
			AllowPreflights = template.AllowPreflights;
			UpdateIssues = template.UpdateIssues;
			InitialAgentType = template.InitialAgentType;
			SubmitNewChange = template.SubmitNewChange;
			Arguments = new List<string>(template.Arguments);
			Parameters = template.Parameters.ConvertAll(x => x.ToData());
		}
	}

	/// <summary>
	/// Response describing a template
	/// </summary>
	public class GetTemplateResponse : GetTemplateResponseBase
	{
		/// <summary>
		/// Unique id of the template
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Parameterless constructor for serialization
		/// </summary>
		protected GetTemplateResponse()
			: base()
		{
			Id = null!;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="template">The template to construct from</param>
		public GetTemplateResponse(ITemplate template)
			: base(template)
		{
			Id = template.Hash.ToString();
		}
	}
}

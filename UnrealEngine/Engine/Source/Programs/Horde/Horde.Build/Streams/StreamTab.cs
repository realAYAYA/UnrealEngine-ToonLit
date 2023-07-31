// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Horde.Build.Utilities;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Build.Streams
{
	using TemplateRefId = StringId<TemplateRef>;

	/// <summary>
	/// Information about a page to display in the dashboard for a stream
	/// </summary>
	[BsonKnownTypes(typeof(JobsTab))]
	public abstract class StreamTab
	{
		/// <summary>
		/// Title of this page
		/// </summary>
		[BsonRequired]
		public string Title { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		private StreamTab()
		{
			Title = null!;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="title">Title of this page</param>
		protected StreamTab(string title)
		{
			Title = title;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="request">Public request object</param>
		protected StreamTab(CreateStreamTabRequest request)
		{
			Title = request.Title;
		}

		/// <summary>
		/// Creates an instance from a request
		/// </summary>
		/// <param name="request">The request object</param>
		/// <returns>New tab instance</returns>
		public static StreamTab FromRequest(CreateStreamTabRequest request)
		{
			CreateJobsTabRequest? jobsRequest = request as CreateJobsTabRequest;
			if (jobsRequest != null)
			{
				return new JobsTab(jobsRequest);
			}

			throw new Exception($"Unknown tab request type '{request.GetType()}'");
		}

		/// <summary>
		/// Creates a response object
		/// </summary>
		/// <returns>Response object</returns>
		public abstract GetStreamTabResponse ToResponse();
	}

	/// <summary>
	/// Describes a column to display on the jobs page
	/// </summary>
	[BsonKnownTypes(typeof(JobsTabLabelColumn), typeof(JobsTabParameterColumn))]
	public abstract class JobsTabColumn
	{
		/// <summary>
		/// Heading for this column
		/// </summary>
		public string Heading { get; set; }

		/// <summary>
		/// Relative width of this column.
		/// </summary>
		public int? RelativeWidth { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		protected JobsTabColumn()
		{
			Heading = null!;
			RelativeWidth = 1;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="heading">Heading for this column</param>
		/// <param name="relativeWidth">Relative width of this column.</param>
		protected JobsTabColumn(string heading, int? relativeWidth)
		{
			Heading = heading;
			RelativeWidth = relativeWidth;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="request">Public request object</param>
		protected JobsTabColumn(CreateJobsTabColumnRequest request)
		{
			Heading = request.Heading;
			RelativeWidth = request.RelativeWidth;
		}

		/// <summary>
		/// Converts this object to a JSON response object
		/// </summary>
		/// <returns>Response object</returns>
		public abstract GetJobsTabColumnResponse ToResponse();
	}

	/// <summary>
	/// Column that contains a set of labels
	/// </summary>
	public class JobsTabLabelColumn : JobsTabColumn
	{
		/// <summary>
		/// Category of labels to display in this column. If null, includes any label not matched by another column.
		/// </summary>
		public string? Category { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		protected JobsTabLabelColumn()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="heading">Heading for this column</param>
		/// <param name="category">Category of labels to display in this column</param>
		/// <param name="relativeWidth">Relative width of this column.</param>
		public JobsTabLabelColumn(string heading, string? category, int? relativeWidth)
			: base(heading, relativeWidth)
		{
			Category = category;
		}

		/// <summary>
		/// Converts this object to a JSON response object
		/// </summary>
		/// <returns>Response object</returns>
		public override GetJobsTabColumnResponse ToResponse()
		{
			return new GetJobsTabLabelColumnResponse(Heading, Category, RelativeWidth);
		}
	}

	/// <summary>
	/// Include the value for a parameter on the jobs tab
	/// </summary>
	public class JobsTabParameterColumn : JobsTabColumn
	{
		/// <summary>
		/// Name of a parameter to show in this column. Should be in the form of a prefix, eg. "-set:Foo="
		/// </summary>
		public string? Parameter { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		protected JobsTabParameterColumn()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="heading">Heading for this column</param>
		/// <param name="parameter">Category of labels to display in this column</param>
		/// <param name="relativeWidth">Relative width of this column.</param>
		public JobsTabParameterColumn(string heading, string? parameter, int? relativeWidth)
			: base(heading, relativeWidth)
		{
			Parameter = parameter;
		}

		/// <summary>
		/// Converts this object to a JSON response object
		/// </summary>
		/// <returns>Response object</returns>
		public override GetJobsTabColumnResponse ToResponse()
		{
			return new GetJobsTabParameterColumnResponse(Heading, Parameter, RelativeWidth);
		}
	}

	/// <summary>
	/// Describes a job page
	/// </summary>
	[BsonDiscriminator("Jobs")]
	public class JobsTab : StreamTab
	{
		/// <summary>
		/// Whether to show job names
		/// </summary>
		public bool ShowNames { get; set; }

		/// <summary>
		/// Names of jobs to include on this page. If there is only one name specified, the name column does not need to be displayed.
		/// </summary>
		public List<string>? JobNames { get; set; }

		/// <summary>
		/// List of templates to display on this tab.
		/// </summary>
		public List<TemplateRefId>? Templates { get; set; }

		/// <summary>
		/// Columns to display in the table
		/// </summary>
		public List<JobsTabColumn>? Columns { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="title">Title for this page</param>
		/// <param name="showNames">Show names of jobs on the page</param>
		/// <param name="templates">Template names to include on this page</param>
		/// <param name="jobNames">Names of jobs to include on this page</param>
		/// <param name="columns">Columns ot display in the table</param>
		public JobsTab(string title, bool showNames, List<TemplateRefId>? templates, List<string>? jobNames, List<JobsTabColumn>? columns)
			: base(title)
		{
			ShowNames = showNames;
			Templates = templates;
			JobNames = jobNames;
			Columns = columns;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="request">Public request object</param>
		public JobsTab(CreateJobsTabRequest request)
			: base(request)
		{
			ShowNames = request.ShowNames;
			Templates = request.Templates;
			JobNames = request.JobNames;
			Columns = request.Columns?.ConvertAll(x => x.ToModel());
		}

		/// <inheritdoc/>
		public override GetStreamTabResponse ToResponse()
		{
			return new GetJobsTabResponse(Title, ShowNames, Templates, JobNames, Columns?.ConvertAll(x => x.ToResponse()));
		}
	}
}

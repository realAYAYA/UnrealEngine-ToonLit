// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;
using HordeCommon;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Build.Jobs.Templates
{
	/// <summary>
	/// Base class for parameters used to configure templates via the new build dialog
	/// </summary>
	[BsonDiscriminator(RootClass = true)]
	[BsonKnownTypes(typeof(GroupParameter), typeof(TextParameter), typeof(ListParameter), typeof(BoolParameter))]
	public abstract class Parameter
	{
		/// <summary>
		/// Gets the default arguments for this parameter and its children
		/// </summary>
		/// <param name="defaultArguments">List of default arguments</param>
		public abstract void GetDefaultArguments(List<string> defaultArguments);

		/// <summary>
		/// Convert this parameter to data for serialization
		/// </summary>
		/// <returns>Serializable parameter data</returns>
		public abstract ParameterData ToData();
	}

	/// <summary>
	/// Used to group a number of other parameters
	/// </summary>
	public class GroupParameter : Parameter
	{
		/// <summary>
		/// Label to display next to this parameter
		/// </summary>
		public string Label { get; set; }

		/// <summary>
		/// How to display this group
		/// </summary>
		public GroupParameterStyle Style { get; set; }

		/// <summary>
		/// List of child parameters
		/// </summary>
		public List<Parameter> Children { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		public GroupParameter()
		{
			Label = null!;
			Children = null!;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="label">Name of the group</param>
		/// <param name="style">How to display this group</param>
		/// <param name="children">List of child parameters</param>
		public GroupParameter(string label, GroupParameterStyle style, List<Parameter> children)
		{
			Label = label;
			Style = style;
			Children = children;
		}

		/// <summary>
		/// Gets the default arguments for this parameter and its children
		/// </summary>
		/// <param name="defaultArguments">List of default arguments</param>
		public override void GetDefaultArguments(List<string> defaultArguments)
		{
			foreach (Parameter child in Children)
			{
				child.GetDefaultArguments(defaultArguments);
			}
		}

		/// <summary>
		/// Convert this parameter to data for serialization
		/// </summary>
		/// <returns>Serializable parameter data</returns>
		public override ParameterData ToData()
		{
			return new GroupParameterData(Label, Style, Children.ConvertAll(x => x.ToData()));
		}
	}

	/// <summary>
	/// Free-form text entry parameter
	/// </summary>
	public sealed class TextParameter : Parameter
	{
		/// <summary>
		/// Label to display next to this parameter. Should default to the parameter name.
		/// </summary>
		public string Label { get; set; }

		/// <summary>
		/// Argument to add (will have the value of this field appended)
		/// </summary>
		public string Argument { get; set; }

		/// <summary>
		/// Default value for this argument
		/// </summary>
		public string Default { get; set; }

		/// <summary>
		/// Hint text to display when the field is empty
		/// </summary>
		public string? Hint { get; set; }

		/// <summary>
		/// Regex used to validate values entered into this text field.
		/// </summary>
		[BsonIgnoreIfNull]
		public string? Validation { get; set; }

		/// <summary>
		/// Message displayed to explain valid values if validation fails.
		/// </summary>
		[BsonIgnoreIfNull]
		public string? Description { get; set; }

		/// <summary>
		/// Tool tip text to display
		/// </summary>
		public string? ToolTip { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		public TextParameter()
		{
			Label = null!;
			Argument = null!;
			Default = String.Empty;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="label">Parameter to pass this value to the BuildGraph script with</param>
		/// <param name="argument">Label to show next to the parameter</param>
		/// <param name="defaultValue">Default value for this argument</param>
		/// <param name="hint">Hint text to display</param>
		/// <param name="validation">Regex used to validate entries</param>
		/// <param name="description">Message displayed for invalid values</param>
		/// <param name="toolTip">Tool tip text to display</param>
		public TextParameter(string label, string argument, string defaultValue, string? hint, string? validation, string? description, string? toolTip)
		{
			Label = label;
			Argument = argument;
			Default = defaultValue;
			Hint = hint;
			Validation = validation;
			Description = description;
			ToolTip = toolTip;
		}

		/// <summary>
		/// Gets the default arguments for this parameter and its children
		/// </summary>
		/// <param name="defaultArguments">List of default arguments</param>
		public override void GetDefaultArguments(List<string> defaultArguments)
		{
			defaultArguments.Add(Argument + Default);
		}

		/// <summary>
		/// Convert this parameter to data for serialization
		/// </summary>
		/// <returns>Serializable parameter data</returns>
		public override ParameterData ToData()
		{
			return new TextParameterData(Label, Argument, Default, Hint, Validation, Description, ToolTip);
		}
	}

	/// <summary>
	/// Possible option for a list parameter
	/// </summary>
	public class ListParameterItem
	{
		/// <summary>
		/// Group to display this entry in
		/// </summary>
		[BsonIgnoreIfNull]
		public string? Group { get; set; }

		/// <summary>
		/// Text to display for this option.
		/// </summary>
		public string Text { get; set; }

		/// <summary>
		/// Argument to add with this parameter.
		/// </summary>
		[BsonIgnoreIfNull]
		public string? ArgumentIfEnabled { get; set; }

		/// <summary>
		/// Argument to add with this parameter.
		/// </summary>
		[BsonIgnoreIfNull]
		public string? ArgumentIfDisabled { get; set; }

		/// <summary>
		/// Whether this item is selected by default
		/// </summary>
		[BsonIgnoreIfDefault, BsonDefaultValue(false)]
		public bool Default { get; set; }

		/// <summary>
		/// Default constructor for JSON serializer
		/// </summary>
		public ListParameterItem()
		{
			Text = String.Empty;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="group">The group to put this parameter in</param>
		/// <param name="text">Text to display for this option</param>
		/// <param name="argumentIfEnabled">Argument to add if this option is enabled</param>
		/// <param name="argumentIfDisabled">Argument to add if this option is disabled</param>
		/// <param name="defaultValue">Whether this item is selected by default</param>
		public ListParameterItem(string? group, string text, string? argumentIfEnabled, string? argumentIfDisabled, bool defaultValue)
		{
			Group = group;
			Text = text;
			ArgumentIfEnabled = argumentIfEnabled;
			ArgumentIfDisabled = argumentIfDisabled;
			Default = defaultValue;
		}

		/// <summary>
		/// Convert this parameter to data for serialization
		/// </summary>
		/// <returns>Serializable parameter data</returns>
		public ListParameterItemData ToData()
		{
			return new ListParameterItemData(Group, Text, ArgumentIfEnabled, ArgumentIfDisabled, Default);
		}
	}

	/// <summary>
	/// Allows the user to select a value from a constrained list of choices
	/// </summary>
	public class ListParameter : Parameter
	{
		/// <summary>
		/// Label to display next to this parameter.
		/// </summary>
		public string Label { get; set; }

		/// <summary>
		/// Style of picker parameter to use
		/// </summary>
		public ListParameterStyle Style { get; set; }

		/// <summary>
		/// List of values to display in the list
		/// </summary>
		public List<ListParameterItem> Items { get; set; }

		/// <summary>
		/// Tool tip text to display
		/// </summary>
		public string? ToolTip { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		public ListParameter()
		{
			Label = null!;
			Items = null!;
		}

		/// <summary>
		/// List of possible values
		/// </summary>
		/// <param name="label">Label to show next to this parameter</param>
		/// <param name="style">Style of list parameter to use</param>
		/// <param name="entries">Entries for this list</param>
		/// <param name="toolTip">Tool tip text to display</param>
		public ListParameter(string label, ListParameterStyle style, List<ListParameterItem> entries, string? toolTip)
		{
			Label = label;
			Style = style;
			Items = entries;
			ToolTip = toolTip;
		}

		/// <summary>
		/// Gets the default arguments for this parameter and its children
		/// </summary>
		/// <param name="defaultArguments">List of default arguments</param>
		public override void GetDefaultArguments(List<string> defaultArguments)
		{
			foreach(ListParameterItem item in Items)
			{
				if (item.Default)
				{
					if (item.ArgumentIfEnabled != null)
					{
						defaultArguments.Add(item.ArgumentIfEnabled);
					}
				}
				else
				{
					if (item.ArgumentIfDisabled != null)
					{
						defaultArguments.Add(item.ArgumentIfDisabled);
					}
				}
			}
		}

		/// <summary>
		/// Convert this parameter to data for serialization
		/// </summary>
		/// <returns>Serializable parameter data</returns>
		public override ParameterData ToData()
		{
			return new ListParameterData(Label, Style, Items.ConvertAll(x => x.ToData()), ToolTip);
		}
	}

	/// <summary>
	/// Allows the user to toggle an option on or off
	/// </summary>
	public class BoolParameter : Parameter
	{
		/// <summary>
		/// Label to display next to this parameter.
		/// </summary>
		public string Label { get; set; }

		/// <summary>
		/// Value if enabled
		/// </summary>
		[BsonIgnoreIfNull]
		public string? ArgumentIfEnabled { get; set; }

		/// <summary>
		/// Value if disabled
		/// </summary>
		[BsonIgnoreIfNull]
		public string? ArgumentIfDisabled { get; set; }

		/// <summary>
		/// Whether this option should be enabled by default
		/// </summary>
		[BsonIgnoreIfDefault, BsonDefaultValue(false)]
		public bool Default { get; set; }

		/// <summary>
		/// Tool tip text to display
		/// </summary>
		public string? ToolTip { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		public BoolParameter()
		{
			Label = String.Empty;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="label">Label to display for this parameter</param>
		/// <param name="argumentIfEnabled">Value if enabled</param>
		/// <param name="argumentIfDisabled">Value if disabled</param>
		/// <param name="defaultValue">Default value for this argument</param>
		/// <param name="toolTip">Tool tip text to display</param>
		public BoolParameter(string label, string? argumentIfEnabled, string? argumentIfDisabled, bool defaultValue, string? toolTip)
		{
			Label = label;
			ArgumentIfEnabled = argumentIfEnabled;
			ArgumentIfDisabled = argumentIfDisabled;
			Default = defaultValue;
			ToolTip = toolTip;
		}

		/// <summary>
		/// Gets the default arguments for this parameter and its children
		/// </summary>
		/// <param name="defaultArguments">List of default arguments</param>
		public override void GetDefaultArguments(List<string> defaultArguments)
		{
			string? defaultArgument = Default ? ArgumentIfEnabled : ArgumentIfDisabled;
			if (!String.IsNullOrEmpty(defaultArgument))
			{
				defaultArguments.Add(defaultArgument);
			}
		}
		/// <summary>
		/// Convert this parameter to data for serialization
		/// </summary>
		/// <returns><see cref="BoolParameterData"/> instance</returns>
		public override ParameterData ToData()
		{
			return new BoolParameterData(Label, ArgumentIfEnabled, ArgumentIfDisabled, Default, ToolTip);
		}
	}

	/// <summary>
	/// Document describing a job template. These objects are considered immutable once created and uniquely referenced by hash, in order to de-duplicate across all job runs.
	/// </summary>
	public interface ITemplate
	{
		/// <summary>
		/// Hash of this template
		/// </summary>
		public ContentHash Hash { get; }

		/// <summary>
		/// Name of the template.
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Priority of this job
		/// </summary>
		public Priority? Priority { get; }

		/// <summary>
		/// Whether to allow preflights for this job type
		/// </summary>
		public bool AllowPreflights { get; }

		/// <summary>
		/// Whether to always issues for jobs using this template
		/// </summary>
		public bool UpdateIssues { get; }

		/// <summary>
		/// Whether to promote issues by default for jobs using this template
		/// </summary>
		public bool PromoteIssuesByDefault { get; }

		/// <summary>
		/// Agent type to use for parsing the job state
		/// </summary>
		public string? InitialAgentType { get; }

		/// <summary>
		/// Path to a file within the stream to submit to generate a new changelist for jobs
		/// </summary>
		public string? SubmitNewChange { get; }

		/// <summary>
		/// Description for new changelists
		/// </summary>
		public string? SubmitDescription { get; }

		/// <summary>
		/// Optional predefined user-defined properties for this job
		/// </summary>
		public IReadOnlyList<string> Arguments { get; }

		/// <summary>
		/// Parameters for this template
		/// </summary>
		public IReadOnlyList<Parameter> Parameters { get; }
	}

	/// <summary>
	/// Extension methods for templates
	/// </summary>
	public static class TemplateExtensions
	{
		/// <summary>
		/// Gets the arguments for default options in this template. Does not include the standard template arguments.
		/// </summary>
		/// <returns>List of default arguments</returns>
		public static List<string> GetDefaultArguments(this ITemplate template)
		{
			List<string> defaultArguments = new List<string>(template.Arguments);
			foreach (Parameter parameter in template.Parameters)
			{
				parameter.GetDefaultArguments(defaultArguments);
			}
			return defaultArguments;
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Build.Streams;
using Horde.Build.Utilities;

namespace Horde.Build.Notifications
{
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;

	/// <summary>
	/// Base interface for events that users can subscribe for notifications to
	/// </summary>
	public interface IEvent
	{
		/// <summary>
		/// Converts this event into an API event record
		/// </summary>
		/// <returns>The API event record</returns>
		public EventRecord ToRecord();
	}

	/// <summary>
	/// Event that occurs when a job completes
	/// </summary>
	public interface IJobCompleteEvent : IEvent
	{
		/// <summary>
		/// The stream id
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// The template id
		/// </summary>
		TemplateRefId TemplateId { get; }
	}

	/// <summary>
	/// Event that occurs when a label completes
	/// </summary>
	public interface ILabelCompleteEvent : IEvent
	{
		/// <summary>
		/// The stream id
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// The template id
		/// </summary>
		TemplateRefId TemplateId { get; }

		/// <summary>
		/// Name of the label to monitor
		/// </summary>
		string LabelName { get; }
	}

	/// <summary>
	/// Event that occurs when a job step completes
	/// </summary>
	public interface IStepCompleteEvent : IEvent
	{
		/// <summary>
		/// The stream id
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// The template id
		/// </summary>
		TemplateRefId TemplateId { get; }

		/// <summary>
		/// Name of the step to monitor
		/// </summary>
		string StepName { get; }
	}
}

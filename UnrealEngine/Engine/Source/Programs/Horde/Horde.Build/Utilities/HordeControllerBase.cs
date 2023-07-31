// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Horde.Build.Acls;
using Horde.Build.Agents;
using Horde.Build.Agents.Leases;
using Horde.Build.Jobs;
using Horde.Build.Projects;
using Horde.Build.Streams;
using Horde.Build.Tools;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Utilities
{
	using JobId = ObjectId<IJob>;
	using LeaseId = ObjectId<ILease>;
	using ProjectId = StringId<IProject>;
	using StreamId = StringId<IStream>;
	using ToolId = StringId<Tool>;
	using ToolDeploymentId = ObjectId<ToolDeployment>;
	using TemplateRefId = StringId<TemplateRef>;

	/// <summary>
	/// Base class for Horde controllers
	/// </summary>
	public abstract class HordeControllerBase : ControllerBase
	{
		/// <summary>
		/// Create a response indicating a problem with the request
		/// </summary>
		/// <param name="message">Standard structured-logging format string</param>
		/// <param name="args">Arguments for the format string</param>
		[NonAction]
		protected ActionResult BadRequest(string message, params object[] args)
		{
			return BadRequest(LogEvent.Create(LogLevel.Error, message, args));
		}

		/// <summary>
		/// Create a response indicating a problem with the request
		/// </summary>
		/// <param name="eventId">Id identifying the error</param>
		/// <param name="message">Standard structured-logging format string</param>
		/// <param name="args">Arguments for the format string</param>
		[NonAction]
		protected ActionResult BadRequest(EventId eventId, string message, params object[] args)
		{
			return BadRequest(LogEvent.Create(LogLevel.Error, eventId, message, args));
		}

		/// <summary>
		/// Returns a 403 (forbidden) response with the given action
		/// </summary>
		[NonAction]
		protected ActionResult Forbid(AclAction action)
		{
			return Forbid("User does not have {Action} permission", action);
		}

		/// <summary>
		/// Returns a 403 (forbidden) response with the given action and object
		/// </summary>
		[NonAction]
		protected ActionResult Forbid(AclAction action, AgentId agentId)
		{
			return Forbid(action, "agent {AgentId}", agentId);
		}

		/// <summary>
		/// Returns a 403 (forbidden) response with the given action and object
		/// </summary>
		[NonAction]
		protected ActionResult Forbid(AclAction action, JobId jobId)
		{
			return Forbid(action, "job {JobId}", jobId);
		}

		/// <summary>
		/// Returns a 403 (forbidden) response with the given action and object
		/// </summary>
		[NonAction]
		protected ActionResult Forbid(AclAction action, ProjectId projectId)
		{
			return Forbid(action, "project {ProjectId}", projectId);
		}

		/// <summary>
		/// Returns a 403 (forbidden) response with the given action and object
		/// </summary>
		[NonAction]
		protected ActionResult Forbid(AclAction action, StreamId streamId)
		{
			return Forbid(action, "stream {StreamId}", streamId);
		}

		/// <summary>
		/// Returns a 403 (forbidden) response with the given action and object
		/// </summary>
		[NonAction]
		protected ActionResult Forbid(AclAction action, ToolId toolId)
		{
			return Forbid(action, "tool {ToolId}", toolId);
		}

		/// <summary>
		/// Returns a 403 (forbidden) response with the given action and object
		/// </summary>
		[NonAction]
		protected ActionResult Forbid(AclAction action, TemplateRefId templateId)
		{
			return Forbid(action, "template {TemplateId}", templateId);
		}

		/// <summary>
		/// Returns a 403 (forbidden) response with the given action and object
		/// </summary>
		[NonAction]
		private ActionResult Forbid(AclAction action, string objectMessage, object obj)
		{
			return Forbid($"User does not have {{Action}} permission for {objectMessage}", action, obj);
		}

		/// <summary>
		/// Returns a 403 response with the given log event
		/// </summary>
		[NonAction]
		protected ActionResult Forbid(string message, params object[] args)
		{
			return StatusCode(StatusCodes.Status403Forbidden, LogEvent.Create(LogLevel.Error, message, args));
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(AgentId agentId)
		{
			return NotFound("Agent {AgentId} not found", agentId);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(AgentId agentId, LeaseId leaseId)
		{
			return NotFound("Lease {LeaseId} not found for agent {AgentId}", leaseId, agentId);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(JobId jobId)
		{
			return NotFound("Job {JobId} not found", jobId);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(JobId jobId, int groupIdx)
		{
			return NotFound("Group {GroupIdx} on job {JobId} not found", groupIdx, jobId);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(JobId jobId, int groupIdx, int nodeIdx)
		{
			return NotFound("Node {NodeIdx} not found on job {JobId} group {GroupIdx}", nodeIdx, jobId, groupIdx);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(JobId jobId, SubResourceId batchId)
		{
			return NotFound("Batch {BatchId} not found on job {JobId}", batchId, jobId);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(JobId jobId, SubResourceId batchId, SubResourceId stepId)
		{
			return NotFound("Step {StepId} not found on job {JobId} batch {BatchId}", stepId, jobId, batchId);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(StreamId streamId)
		{
			return NotFound("Stream {StreamId} not found", streamId);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(StreamId streamId, TemplateRefId templateId)
		{
			return NotFound("Template {TemplateId} not found on stream {StreamId}", templateId, streamId);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(ToolId toolId)
		{
			return NotFound("Tool {ToolId} not found", toolId);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(ToolId toolId, ToolDeploymentId deploymentId)
		{
			return NotFound("Deployment {DeploymentId} not found on tool {ToolId}", deploymentId, toolId);
		}

		/// <summary>
		/// Returns a 404 response for the given object
		/// </summary>
		[NonAction]
		protected ActionResult NotFound(string message, params object[] args)
		{
			return NotFound(LogEvent.Create(LogLevel.Error, message, args));
		}
	}
}

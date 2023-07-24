// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Security.Claims;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Acls
{
	/// <summary>
	/// Set of actions that can be performed by a user. NOTE: This enum is sensitive to ordering. Do not change values.
	/// </summary>
	public enum AclAction
	{
		//// PROJECTS ////

        #region Projects

        /// <summary>
        /// Allows the creation of new projects
        /// </summary>
        CreateProject,

		/// <summary>
		/// Allows deletion of projects.
		/// </summary>
		DeleteProject,
		
		/// <summary>
		/// Modify attributes of a project (name, categories, etc...)
		/// </summary>
		UpdateProject,

		/// <summary>
		/// View information about a project
		/// </summary>
		ViewProject,

        #endregion

		//// STREAMS ////

        #region Streams

        /// <summary>
        /// Allows the creation of new streams within a project
        /// </summary>
        CreateStream,

		/// <summary>
		/// Allows updating a stream (agent types, templates, schedules)
		/// </summary>
		UpdateStream,

		/// <summary>
		/// Allows deleting a stream
		/// </summary>
		DeleteStream,

		/// <summary>
		/// Ability to view a stream
		/// </summary>
		ViewStream,

		/// <summary>
		/// View changes submitted to a stream. NOTE: this returns responses from the server's Perforce account, which may be a priviledged user.
		/// </summary>
		ViewChanges,

		/// <summary>
		/// Update records in commit queues for a stream
		/// </summary>
		UpdateCommitQueues,

		/// <summary>
		/// View commit queues for a stream
		/// </summary>
		ViewCommitQueues,

        #endregion

		//// JOBS ////

        #region Jobs

        /// <summary>
        /// Ability to start new jobs
        /// </summary>
        CreateJob,

		/// <summary>
		/// Rename a job, modify its priority, etc...
		/// </summary>
		UpdateJob,

		/// <summary>
		/// Delete a job properties
		/// </summary>
		DeleteJob,

		/// <summary>
		/// Allows updating a job metadata (name, changelist number, step properties, new groups, job states, etc...). Typically granted to agents. Not user facing.
		/// </summary>
		ExecuteJob,

		/// <summary>
		/// Ability to retry a failed job step
		/// </summary>
		RetryJobStep,

		/// <summary>
		/// Ability to view a job
		/// </summary>
		ViewJob,

        #endregion

		//// EVENTS ////

        #region Events

        /// <summary>
        /// Ability to create events
        /// </summary>
        CreateEvent,

		/// <summary>
		/// Ability to view events
		/// </summary>
		ViewEvent,

        #endregion

		//// AGENTS ////

        #region Agents

        /// <summary>
        /// Ability to create an agent. This may be done explicitly, or granted to agents to allow them to self-register.
        /// </summary>
        CreateAgent,

		/// <summary>
		/// Update an agent's name, pools, etc...
		/// </summary>
		UpdateAgent,

		/// <summary>
		/// Soft-delete an agent
		/// </summary>
		DeleteAgent,

		/// <summary>
		/// View an agent
		/// </summary>
		ViewAgent,

		/// <summary>
		/// List the available agents
		/// </summary>
		ListAgents,

        #endregion

		//// POOLS ////

        #region Pools

        /// <summary>
        /// Create a global pool of agents
        /// </summary>
        CreatePool,

		/// <summary>
		/// Modify an agent pool
		/// </summary>
		UpdatePool,

		/// <summary>
		/// Delete an agent pool
		/// </summary>
		DeletePool,

		/// <summary>
		/// Ability to view a pool
		/// </summary>
		ViewPool,

		/// <summary>
		/// View all the available agent pools
		/// </summary>
		ListPools,

        #endregion

		//// SESSIONS ////

        #region Sessions

        /// <summary>
        /// Granted to agents to call CreateSession, which returns a bearer token identifying themselves valid to call UpdateSesssion via gRPC.
        /// </summary>
        CreateSession,

		/// <summary>
		/// Allows viewing information about an agent session
		/// </summary>
		ViewSession,

        #endregion

        //// CREDENTIALS ////

        #region Credentials

        /// <summary>
        /// Create a new credential
        /// </summary>
        CreateCredential,

		/// <summary>
		/// Delete a credential
		/// </summary>
		DeleteCredential,

		/// <summary>
		/// Modify an existing credential
		/// </summary>
		UpdateCredential,

		/// <summary>
		/// Enumerates all the available credentials
		/// </summary>
		ListCredentials,

		/// <summary>
		/// View a credential
		/// </summary>
		ViewCredential,

        #endregion

        //// LEASES ////

        #region Leases

        /// <summary>
        /// View all the leases that an agent has worked on
        /// </summary>
        ViewLeases,

        #endregion

        //// COMMITS ////

        #region Commits

        /// <summary>
        /// Add a new commit
        /// </summary>
        AddCommit,

		/// <summary>
		/// List all the commits that have been added
		/// </summary>
		FindCommits,

		/// <summary>
		/// View the commits for a particular stream
		/// </summary>
		ViewCommits,

        #endregion

        //// ISSUES ////

        #region Issues

        /// <summary>
        /// View a build health issue
        /// </summary>
        ViewIssue,

		/// <summary>
		/// Mark an issue as fixed via the p4fix endpoint
		/// </summary>
		IssueFixViaPerforce,

        #endregion

        //// TEMPLATES ////

        #region Templates

        /// <summary>
        /// View template associated with a stream
        /// </summary>
        ViewTemplate,

        #endregion

        //// LOGS ////

        #region Logs

        /// <summary>
        /// Ability to create a log. Implicitly granted to agents.
        /// </summary>
        CreateLog,

		/// <summary>
		/// Ability to update log metadata
		/// </summary>
		UpdateLog,

		/// <summary>
		/// Ability to view a log contents
		/// </summary>
		ViewLog,

		/// <summary>
		/// Ability to write log data
		/// </summary>
		WriteLogData,

        #endregion

        //// ARTIFACTS ////

        #region Artifacts

        /// <summary>
        /// Ability to create an artifact. Typically just for debugging; agents have this access for a particular session.
        /// </summary>
        UploadArtifact,

		/// <summary>
		/// Ability to download an artifact
		/// </summary>
		DownloadArtifact,

        #endregion

        //// SOFTWARE ////

        #region Software

        /// <summary>
        /// Ability to upload new versions of the agent software
        /// </summary>
        UploadSoftware,

		/// <summary>
		/// Ability to download the agent software
		/// </summary>
		DownloadSoftware,

		/// <summary>
		/// Ability to delete agent software
		/// </summary>
		DeleteSoftware,

		#endregion

		//// TOOLS ////

		#region Tools

		/// <summary>
		/// Ability to download a tool
		/// </summary>
		DownloadTool,

		/// <summary>
		/// Ability to upload new tool versions
		/// </summary>
		UploadTool,

		#endregion

		//// ADMIN ////

		#region Admin

		/// <summary>
		/// Ability to read any data from the server. Always inherited.
		/// </summary>
		AdminRead,

		/// <summary>
		/// Ability to write any data to the server.
		/// </summary>
		AdminWrite,

		/// <summary>
		/// Ability to impersonate another user
		/// </summary>
		Impersonate,

		/// <summary>
		/// View estimated costs for particular operations
		/// </summary>
		ViewCosts,

        #endregion

        //// PERMISSIONS ////

        #region Permissions

        /// <summary>
        /// Ability to view permissions on an object
        /// </summary>
        ViewPermissions,

		/// <summary>
		/// Ability to change permissions on an object
		/// </summary>
		ChangePermissions,

		/// <summary>
		/// Issue bearer token for the current user
		/// </summary>
		IssueBearerToken,

        #endregion

        //// NOTIFICATIONS ////

        #region Notifications

        /// <summary>
        /// Ability to subscribe to notifications
        /// </summary>
        CreateSubscription,

        #endregion

        //// DEVICES ////

        #region Devices

        /// <summary>
        /// Ability to read devices
        /// </summary>
        DeviceRead,

		/// <summary>
		/// Ability to write devices
		/// </summary>
		DeviceWrite,

        #endregion

        //// COMPUTE ////

        #region Compute

        /// <summary>
        /// User can add tasks to the compute cluster
        /// </summary>
        AddComputeTasks,

		/// <summary>
		/// User can poll for compute results
		/// </summary>
		ViewComputeTasks,

        #endregion

        //// STORAGE ////

        #region Storage

        /// <summary>
        /// Ability to read blobs from the storage service
        /// </summary>
        ReadBlobs,

		/// <summary>
		/// Ability to write blobs to the storage service
		/// </summary>
		WriteBlobs,

		/// <summary>
		/// Ability to read refs from the storage service
		/// </summary>
		ReadRefs,

		/// <summary>
		/// Ability to write refs to the storage service
		/// </summary>
		WriteRefs, 

		/// <summary>
		/// Ability to delete refs
		/// </summary>
		DeleteRefs,

        #endregion
    }
}

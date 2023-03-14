// Copyright Epic Games, Inc. All Rights Reserved.	

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Polly;
using Polly.Extensions.Http;
using Horde.Build.Utilities;
using Horde.Build.Configuration;
using Microsoft.Extensions.Hosting;
using HordeCommon;
using System.Threading;
using System.Linq;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Users;

namespace Horde.Build.Issues.External
{
	/// <summary>
	/// Jira issue information
	/// </summary>
	internal class JiraIssue : IExternalIssue
	{
		/// <summary>
		/// The Jira issue key
		/// </summary>
		public string Key { get; set; } = String.Empty;

		/// <summary>
		/// The Jira issue link
		/// </summary>
		public string? Link { get; set; }

		/// <summary>
		/// The issue status name, "To Do", "In Progress", etc
		/// </summary>
		public string? StatusName { get; set; }

		/// <summary>
		/// The issue resolution name, "Fixed", "Closed", etc
		/// </summary>
		public string? ResolutionName { get; set; }

		/// <summary>
		/// The issue priority name, "1 - Critical", "2 - Major", etc
		/// </summary>
		public string? PriorityName { get; set; }

		/// <summary>
		/// The current assignee's user name
		/// </summary>
		public string? AssigneeName { get; set; }

		/// <summary>
		/// The current assignee's display name
		/// </summary>
		public string? AssigneeDisplayName { get; set; }

		/// <summary>
		/// The current assignee's email address
		/// </summary>
		public string? AssigneeEmailAddress { get; set; }
	}

	internal class JiraCreateResponse
	{
		/// <summary>
		/// Key of external issue
		/// </summary>
		public string Key { get; set; } = String.Empty;

	}

	internal class JiraProject : IExternalIssueProject
	{
		public string Key { get; set; } = String.Empty;

		public string Name { get; set; } = String.Empty;

		public string Id { get; set; } = String.Empty;

		// component id => name
		public Dictionary<string, string> Components { get; set; } = new Dictionary<string, string>();

		// IssueType id => name
		public Dictionary<string, string> IssueTypes { get; set; } = new Dictionary<string, string>();

	}

	/// <summary>
	/// Jira service functionality
	/// </summary>
	internal sealed class JiraService : IExternalIssueService, IHostedService, IDisposable
	{
		readonly ILogger _logger;

		/// <summary>
		/// The server settings
		/// </summary>
		readonly ServerSettings _settings;
		readonly HttpClient _client;
		readonly AsyncPolicy<HttpResponseMessage> _retryPolicy;
		readonly ConcurrentDictionary<string, JiraCacheValue> _issueCache = new ConcurrentDictionary<string, JiraCacheValue>();

		/// <summary>
		/// Singleton instance of the stream service
		/// </summary>
		readonly StreamService _streamService;

		/// <summary>
		/// Singleton instance of the project service
		/// </summary>
		readonly IssueService _issueService;

		/// <summary>
		/// Config collection
		/// </summary>
		readonly ConfigCollection _configCollection;

		readonly IClock _clock;
		readonly ITicker _ticker;

		readonly Uri _jiraUrl;

		Dictionary<string, JiraProject> _cachedJiraProjects = new Dictionary<string, JiraProject>();


		/// <summary>
		/// Jira service constructor
		/// </summary>
		/// <param name="settings"></param>
		/// <param name="streamService"></param>
		/// <param name="issueService"></param>
		/// <param name="configCollection"></param>
		/// <param name="clock"></param>
		/// <param name="logger"></param>
		public JiraService(IOptions<ServerSettings> settings, StreamService streamService, IssueService issueService, ConfigCollection configCollection, IClock clock, ILogger<JiraService> logger)
		{

			_settings = settings.Value;
			_logger = logger;
			_streamService = streamService;
			_issueService = issueService;
			_configCollection = configCollection;
			_clock = clock;
			_ticker = clock.AddTicker<JiraService>(TimeSpan.FromMinutes(2.0), TickAsync, logger);
			_jiraUrl = _settings.JiraUrl!;

			// setup http client for Jira rest api queries
			_client = new HttpClient();
			byte[] authBytes = Encoding.ASCII.GetBytes($"{_settings.JiraUsername}:{_settings.JiraApiToken}");
			_client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Basic", Convert.ToBase64String(authBytes));
			_client.Timeout = TimeSpan.FromSeconds(15.0);
			_retryPolicy = HttpPolicyExtensions.HandleTransientHttpError().WaitAndRetryAsync(3, attempt => TimeSpan.FromSeconds(Math.Pow(2.0, attempt)));
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public void Dispose()
		{
			_ticker.Dispose();
			_client.Dispose();
		}

		/// <inheritdoc/>
		public string? GetIssueUrl(string key) => $"{_jiraUrl}browse/{key}";

		public async Task<List<IExternalIssueProject>> GetProjects(IStream stream)
		{
			HashSet<string> projectKeys = new HashSet<string>();
			List<IExternalIssueProject> result = new List<IExternalIssueProject>();

			StreamConfig? config = null;

			try
			{
				config = await _configCollection.GetConfigAsync<StreamConfig>(stream.ConfigRevision);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Unable to get stream config for {StreamId}", stream.Id);

			}

			if (config == null || config.Workflows == null)
			{
				return result;
			}

			config.Workflows.ForEach(workflow =>
			{
				if (workflow.ExternalIssues != null)
				{
					projectKeys.Add(workflow.ExternalIssues.ProjectKey);
				}				

			});

			foreach (string key in projectKeys)
			{
				if (_cachedJiraProjects.TryGetValue(key, out JiraProject? project))
				{
					result.Add(project);
				}
			}

			return result;
		}

		async Task UpdateJiraProjects(string[] jiraProjectKeys)
		{
			if (jiraProjectKeys.Length == 0)
			{
				return;
			}

			for (int i = 0; i < jiraProjectKeys.Length; i++)
			{
				string projectKey = jiraProjectKeys[i];

				Uri uri = new Uri(_jiraUrl, $"/rest/api/2/project/{projectKey}");

				HttpResponseMessage response;

				try
				{
					response = await _retryPolicy.ExecuteAsync(() => _client.GetAsync(uri));
					response.EnsureSuccessStatusCode();
				}
				catch (Exception)
				{
					_logger.LogError("Unable to get project info for {ProjectKey}", projectKey);
					continue;
				}

				string responseBody = await response.Content.ReadAsStringAsync();

				JsonElement jsonProject = JsonSerializer.Deserialize<JsonElement>(responseBody);

				JiraProject jiraProject = new JiraProject() { Key = projectKey, Id = jsonProject.GetProperty("id")!.GetString()!, Name = jsonProject.GetProperty("name")!.GetString()! };

				// Issue Types
				if (jsonProject.TryGetProperty("issueTypes", out JsonElement issueTypes))
				{
					foreach (JsonElement issueType in issueTypes.EnumerateArray())
					{
						// filter out subtasks issue types
						if (issueType.TryGetProperty("subtask", out JsonElement subtask))
						{
							if (subtask.GetBoolean())
							{
								continue;
							}
						}

						// filter out certain issue types
						string name = issueType.GetProperty("name").GetString()!;
						if (name == "Epic")
						{
							continue;
						}

						jiraProject.IssueTypes[issueType.GetProperty("id").GetString()!] = name;

					}

				}

				// Components
				// We do get components from the projects endpoint, though these don't contain the archived property :/
				try
				{
					response = await _retryPolicy.ExecuteAsync(() => _client.GetAsync(new Uri(_jiraUrl, $"/rest/api/2/project/{projectKey}/components")));
					response.EnsureSuccessStatusCode();
				}
				catch (Exception)
				{
					_logger.LogError("Unable to get components info for {ProjectKey}", projectKey);
					continue;
				}

				responseBody = await response.Content.ReadAsStringAsync();

				JsonElement jiraComponents = JsonSerializer.Deserialize<JsonElement>(responseBody);

				for (int j = 0; j < jiraComponents.GetArrayLength(); j++)
				{
					JsonElement component = jiraComponents[j];

					// skip archived components
					if (component.TryGetProperty("archived", out JsonElement archived))
					{
						if (archived.GetBoolean())
						{
							continue;
						}

						if (component.TryGetStringProperty("id", out string? id))
						{
							if (component.TryGetStringProperty("name", out string? name))
							{
								jiraProject.Components[id] = name;
							}
						}
					}
				}

				_cachedJiraProjects[projectKey] = jiraProject;
			}
		}

		async ValueTask TickAsync(CancellationToken stoppingToken)
		{

			HashSet<string> jiraProjectKeys = new HashSet<string>();

			List<IStream> cachedStreams = await _streamService.StreamCollection.FindAllAsync();
			StreamConfig? config = null;
			foreach (IStream cachedStream in cachedStreams)
			{
				config = null;
				try
				{
					config = await _configCollection.GetConfigAsync<StreamConfig>(cachedStream.ConfigRevision);
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Unable to get stream config for {StreamId}", cachedStream.Id);
					continue;
				}

				foreach (WorkflowConfig workflow in config.Workflows)
				{
					if (workflow.ExternalIssues == null || String.IsNullOrEmpty(workflow.ExternalIssues.ProjectKey))
					{
						continue;
					}

					jiraProjectKeys.Add(workflow.ExternalIssues.ProjectKey);
				}
			}

			// update projects
			await UpdateJiraProjects(jiraProjectKeys.ToArray());

			HashSet<string> jiraKeys = new HashSet<string>();

			// Refresh issues
			List<IIssue> openIssues = await _issueService.Collection.FindIssuesAsync(resolved: false);
			for (int idx = 0; idx < openIssues.Count; idx++)
			{
				IIssue openIssue = openIssues[idx];
				if (!String.IsNullOrEmpty(openIssue.ExternalIssueKey))
				{
					jiraKeys.Add(openIssue.ExternalIssueKey);
				}
			}

			await GetIssuesAsync(jiraKeys.ToArray());

		}

		public async Task<(string? key, string? url)> CreateIssueAsync(IUser user, string? externalIssueUser, int issueId, string summary, string projectId, string componentId, string issueType, string? description, string? hordeIssueLink)
		{

			IIssue? issue = await _issueService.Collection.GetIssueAsync(issueId);
			if (issue == null)
			{
				throw new Exception($"Issue not found: {issueId}");
			}

			string issueDesc = description ?? "";

			if (hordeIssueLink != null)
			{
				issueDesc = $"\n{issueDesc}\n\nJira issue created by {user.Name} for *[Horde Issue {issueId}|{hordeIssueLink}]*\n\n";
			}
			else
			{
				issueDesc = $"\n{issueDesc}\n\nJira Issue created by {user.Name} from Horde\n\n";
			}


			Uri uri = new Uri(_jiraUrl, $"/rest/api/2/issue");

			string bodyJson = JsonSerializer.Serialize(new
			{
				fields = new
				{
					project = new
					{
						id = projectId
					},
					issuetype = new
					{
						id = issueType
					},
					summary = summary,
					description = issueDesc,
					components = new object[]
					{
						new
						{
							id = componentId
						}
					}
				}
			});

			HttpResponseMessage response = await _retryPolicy.ExecuteAsync(() => _client.PostAsync(uri, new StringContent(bodyJson, Encoding.UTF8, "application/json")));

			response.EnsureSuccessStatusCode();

			string responseBody = await response.Content.ReadAsStringAsync();

			JiraCreateResponse? jiraResponse = JsonSerializer.Deserialize<JiraCreateResponse>(responseBody, new JsonSerializerOptions { PropertyNameCaseInsensitive = true });

			if (jiraResponse == null || String.IsNullOrEmpty(jiraResponse.Key))
			{
				throw new Exception($"Unable to parse returned jira json: {responseBody}");
			}

			await _issueService.UpdateIssueAsync(issueId, externalIssueKey: jiraResponse.Key);

			// add the user as a watcher to the newly created issue
			if (externalIssueUser != null)
			{
				uri = new Uri(_jiraUrl, $"/rest/api/2/issue/{jiraResponse.Key}/watchers");

				try
				{
					response = await _retryPolicy.ExecuteAsync(() => _client.PostAsync(uri, new StringContent(JsonSerializer.Serialize(externalIssueUser), Encoding.UTF8, "application/json")));
					response.EnsureSuccessStatusCode();
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Unable to add {ExternalUser} as watcher to issue {IssueKey}", externalIssueUser, jiraResponse.Key);
				}
				
			}

			return (jiraResponse.Key, $"{_jiraUrl}browse/{jiraResponse.Key}");
		}

		/// <inheritdoc/>
		public async Task<List<IExternalIssue>> GetIssuesAsync(string[] jiraKeys)
		{
			List<IExternalIssue> result = new List<IExternalIssue>();

			if (jiraKeys.Length == 0)
			{
				return result;
			}

			List<string> queryJiras = new List<string>();
			for (int i = 0; i < jiraKeys.Length; i++)
			{
				JiraCacheValue value;
				if (_issueCache.TryGetValue(jiraKeys[i], out value))
				{
					if (DateTime.UtcNow.Subtract(value._cacheTime).TotalMinutes >= 2)
					{
						queryJiras.Add(jiraKeys[i]);
					}
				}
				else
				{
					queryJiras.Add(jiraKeys[i]);
				}
			}

			if (queryJiras.Count > 0)
			{
				Uri uri = new Uri(_jiraUrl, $"/rest/api/2/search?jql=issueKey%20in%20({String.Join(",", queryJiras)})&fields=assignee,status,resolution,priority&maxResults={queryJiras.Count}");

				HttpResponseMessage response = await _retryPolicy.ExecuteAsync(() => _client.GetAsync(uri));
				if (!response.IsSuccessStatusCode)
				{
					_logger.LogError("GET to {Uri} returned {Code} ({Response})", uri, response.StatusCode, await response.Content.ReadAsStringAsync());
					return result;
				}

				byte[] data = await response.Content.ReadAsByteArrayAsync();
				IssueQueryResponse? jiras = JsonSerializer.Deserialize<IssueQueryResponse>(data.AsSpan(), new JsonSerializerOptions() { PropertyNameCaseInsensitive = true });

				if (jiras != null)
				{
					for (int i = 0; i < jiras.Issues.Count; i++)
					{
						IssueResponse issue = jiras.Issues[i];

						JiraIssue jiraIssue = new JiraIssue();
						jiraIssue.Key = issue.Key;
						jiraIssue.Link = $"{_jiraUrl}browse/{issue.Key}";
						jiraIssue.AssigneeName = issue.Fields?.Assignee?.Name;
						jiraIssue.AssigneeDisplayName = issue.Fields?.Assignee?.DisplayName;
						jiraIssue.AssigneeEmailAddress = issue.Fields?.Assignee?.EmailAddress;
						jiraIssue.PriorityName = issue.Fields?.Priority?.Name;
						jiraIssue.ResolutionName = issue.Fields?.Resolution?.Name;
						jiraIssue.StatusName = issue.Fields?.Status?.Name;

						_issueCache[issue.Key] = new JiraCacheValue() { _issue = jiraIssue, _cacheTime = DateTime.UtcNow };
					}
				}
			}

			for (int i = 0; i < jiraKeys.Length; i++)
			{
				JiraCacheValue value;
				if (_issueCache.TryGetValue(jiraKeys[i], out value))
				{
					result.Add(value._issue);
				}
			}

			return result;

		}

		// Jira rest api mapping and caching

		struct JiraCacheValue
		{
			public JiraIssue _issue;
			public DateTime _cacheTime;
		};

		class IssueQueryResponse
		{
			public int Total { get; set; } = 0;

			public List<IssueResponse> Issues { get; set; } = new List<IssueResponse>();
		}

		class IssueResponse
		{
			public string Key { get; set; } = String.Empty;

			public IssueFields? Fields { get; set; }
		}

		class IssueFields
		{
			public AssigneeField? Assignee { get; set; }

			public StatusField? Status { get; set; }

			public ResolutionField? Resolution { get; set; }

			public PriorityField? Priority { get; set; }
		}

		class AssigneeField
		{
			public string? Name { get; set; }

			public string? DisplayName { get; set; }

			public string? EmailAddress { get; set; }
		}

		class StatusField
		{
			public string? Name { get; set; }
		}

		class PriorityField
		{
			public string? Name { get; set; }
		}

		class ResolutionField
		{
			public string? Name { get; set; }
		}
	}
}

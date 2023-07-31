// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;

namespace EpicGames.Slack
{
	/// <summary>
	/// Exception thrown due to an error response from Slack
	/// </summary>
	public class SlackException : Exception
	{
		/// <summary>
		/// Slack error code
		/// </summary>
		public string Code { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public SlackException(string code) : base(code)
		{
			Code = code;
		}
	}

	/// <summary>
	/// Wrapper around Slack client functionality
	/// </summary>
	public class SlackClient
	{
		class SlackResponse
		{
			[JsonPropertyName("ok")]
			public bool Ok { get; set; }

			[JsonPropertyName("error")]
			public string? Error { get; set; }
		}

		readonly HttpClient _httpClient;
		readonly JsonSerializerOptions _serializerOptions;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="httpClient">Http client for connecting to slack. Should have the necessary authorization headers.</param>
		/// <param name="logger">Logger interface</param>
		public SlackClient(HttpClient httpClient, ILogger logger)
		{
			_httpClient = httpClient;

			_serializerOptions = new JsonSerializerOptions();
			_serializerOptions.DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull;

			_logger = logger;
		}

		static bool ShouldLogError<TResponse>(TResponse response) where TResponse : SlackResponse => !response.Ok;

		private Task<TResponse> SendRequestAsync<TResponse>(string requestUrl, object request) where TResponse : SlackResponse
		{
			return SendRequestAsync<TResponse>(requestUrl, request, ShouldLogError);
		}

		private async Task<TResponse> SendRequestAsync<TResponse>(string requestUrl, object request, Func<TResponse, bool> shouldLogError) where TResponse : SlackResponse
		{
			using (HttpRequestMessage sendMessageRequest = new HttpRequestMessage(HttpMethod.Post, requestUrl))
			{
				string requestJson = JsonSerializer.Serialize(request, _serializerOptions);
				using (StringContent messageContent = new StringContent(requestJson, Encoding.UTF8, "application/json"))
				{
					sendMessageRequest.Content = messageContent;
					return await SendRequestAsync<TResponse>(sendMessageRequest, requestJson, shouldLogError);
				}
			}
		}

		private Task<TResponse> SendRequestAsync<TResponse>(HttpRequestMessage request, string requestJson) where TResponse : SlackResponse
		{
			return SendRequestAsync<TResponse>(request, requestJson, ShouldLogError);
		}

		private async Task<TResponse> SendRequestAsync<TResponse>(HttpRequestMessage request, string requestJson, Func<TResponse, bool> shouldLogError) where TResponse : SlackResponse
		{
			HttpResponseMessage response = await _httpClient.SendAsync(request);
			byte[] responseBytes = await response.Content.ReadAsByteArrayAsync();

			TResponse responseObject = JsonSerializer.Deserialize<TResponse>(responseBytes)!;
			if (shouldLogError(responseObject))
			{
				try
				{
					throw new SlackException(responseObject.Error ?? "unknown");
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Failed to send Slack message to {Url} ({Error}). Request: {Request}. Response: {Response}", request.RequestUri, responseObject.Error, requestJson, Encoding.UTF8.GetString(responseBytes));
					throw;
				}
			}
			return responseObject;
		}

		#region Chat

		const string PostMessageUrl = "https://slack.com/api/chat.postMessage";
		const string UpdateMessageUrl = "https://slack.com/api/chat.update";
		const string GetPermalinkUrl = "https://slack.com/api/chat.getPermalink";

		class PostMessageRequest
		{
			[JsonPropertyName("channel")]
			public string? Channel { get; set; }

			[JsonPropertyName("ts")]
			public string? Ts { get; set; }

			[JsonPropertyName("thread_ts")]
			public string? ThreadTs { get; set; }

			[JsonPropertyName("text")]
			public string? Text { get; set; }

			[JsonPropertyName("mrkdwn")]
			public bool? Markdown { get; set; }

			[JsonPropertyName("blocks")]
			public List<Block>? Blocks { get; set; }

			[JsonPropertyName("attachments")]
			public List<SlackAttachment>? Attachments { get; set; }
			
			[JsonPropertyName("reply_broadcast")]
			public bool? ReplyBroadcast { get; set; }

			[JsonPropertyName("unfurl_links")]
			public bool? UnfurlLinks { get; set; }

			[JsonPropertyName("unfurl_media")]
			public bool? UnfurlMedia { get; set; }
		}

		class PostMessageResponse : SlackResponse
		{
			[JsonPropertyName("channel")]
			public string? Channel { get; set; }

			[JsonPropertyName("ts")]
			public string? Ts { get; set; }
		}

		/// <summary>
		/// Posts a message to a recipient
		/// </summary>
		/// <param name="recipient">Recipient of the message. May be a channel or Slack user id.</param>
		/// <param name="message">New message to post</param>
		public async Task<string> PostMessageAsync(string recipient, SlackMessage message)
		{
			return await PostOrUpdateMessageAsync(recipient, null, null, message, false);
		}

		/// <summary>
		/// Posts a message to a recipient
		/// </summary>
		/// <param name="recipient">Recipient of the message. May be a channel or Slack user id.</param>
		/// <param name="threadTs">Timestamp of the thread to post the message to</param>
		/// <param name="message">New message to post</param>
		/// <param name="replyBroadcast">Whether to broadcast the message to the channel</param>
		public async Task<string> PostMessageAsync(string recipient, string threadTs, SlackMessage message, bool replyBroadcast = false)
		{
			return await PostOrUpdateMessageAsync(recipient, null, threadTs, message, replyBroadcast);
		}

		/// <summary>
		/// Updates an existing message
		/// </summary>
		/// <param name="recipient">Recipient of the message. May be a channel or Slack user id.</param>
		/// <param name="ts">The message timestamp</param>
		/// <param name="message">New message to post</param>
		public async Task UpdateMessageAsync(string recipient, string ts, SlackMessage message)
		{
			await PostOrUpdateMessageAsync(recipient, ts, null, message, false);
		}

		/// <summary>
		/// Updates an existing message
		/// </summary>
		/// <param name="recipient">Recipient of the message. May be a channel or Slack user id.</param>
		/// <param name="ts">The message timestamp</param>
		/// <param name="threadTs">Timestamp of the thread to post the message to</param>
		/// <param name="message">New message to post</param>
		/// <param name="replyBroadcast">Whether to broadcast the message to the channel</param>
		public async Task UpdateMessageAsync(string recipient, string ts, string threadTs, SlackMessage message, bool replyBroadcast = false)
		{
			await PostOrUpdateMessageAsync(recipient, ts, threadTs, message, replyBroadcast);
		}

		async Task<string> PostOrUpdateMessageAsync(string recipient, string? ts, string? threadTs, SlackMessage message, bool replyBroadcast)
		{
			PostMessageRequest request = new PostMessageRequest();
			request.Channel = recipient;
			request.Ts = ts;
			request.ThreadTs = threadTs;
			request.Text = message.Text;
			request.Blocks = message.Blocks;
			request.Markdown = message.Markdown;

			if (replyBroadcast)
			{
				request.ReplyBroadcast = replyBroadcast;
			}
			if (message.Attachments.Count > 0)
			{
				request.Attachments = message.Attachments;
			}
			if (!message.UnfurlLinks)
			{
				request.UnfurlLinks = false;
			}
			if (!message.UnfurlMedia)
			{
				request.UnfurlMedia = false;
			}

			PostMessageResponse response = await SendRequestAsync<PostMessageResponse>(ts == null? PostMessageUrl : UpdateMessageUrl, request);
			if (!response.Ok || response.Ts == null)
			{
				throw new SlackException(response.Error ?? "unknown");
			}

			return response.Ts;
		}

		class GetPermalinkRequest
		{
			[JsonPropertyName("channel")]
			public string? Channel { get; set; }

			[JsonPropertyName("message_ts")]
			public string? MessageTs { get; set; }
		}

		class GetPermalinkResponse : SlackResponse
		{
			[JsonPropertyName("channel")]
			public string? Channel { get; set; }

			[JsonPropertyName("permalink")]
			public string? Permalink { get; set; }
		}

		/// <summary>
		/// Gets a permalink for a message
		/// </summary>
		/// <param name="channel">Channel containing the message</param>
		/// <param name="ts">Message timestamp</param>
		/// <returns>Link to the message</returns>
		public async Task<string> GetPermalinkAsync(string channel, string ts)
		{
			string requestUrl = $"{GetPermalinkUrl}?channel={channel}&message_ts={ts}";
			using (HttpRequestMessage sendMessageRequest = new HttpRequestMessage(HttpMethod.Get, requestUrl))
			{
				GetPermalinkResponse response = await SendRequestAsync<GetPermalinkResponse>(sendMessageRequest, "");
				if (!response.Ok || response.Permalink == null)
				{
					throw new SlackException(response.Error ?? "unknown");
				}
				return response.Permalink;
			}
		}

		#endregion

		#region Reactions

		const string AddReactionUrl = "https://slack.com/api/reactions.add";
		const string RemoveReactionUrl = "https://slack.com/api/reactions.remove";

		class ReactionMessage
		{
			[JsonPropertyName("channel")]
			public string? Channel { get; set; }

			[JsonPropertyName("timestamp")]
			public string? Ts { get; set; }

			[JsonPropertyName("name")]
			public string? Name { get; set; }
		}

		/// <summary>
		/// Adds a reaction to a posted message
		/// </summary>
		/// <param name="channel">Channel containing the message</param>
		/// <param name="ts">Message timestamp</param>
		/// <param name="name">Name of the reaction to post</param>
		public async Task AddReactionAsync(string channel, string ts, string name)
		{
			ReactionMessage message = new ReactionMessage();
			message.Channel = channel;
			message.Ts = ts;
			message.Name = name;

			static bool ShouldLogError(SlackResponse response) => !response.Ok && !String.Equals(response.Error, "already_reacted", StringComparison.Ordinal);

			await SendRequestAsync<SlackResponse>(AddReactionUrl, message, ShouldLogError);
		}

		/// <summary>
		/// Removes a reaction from a posted message
		/// </summary>
		/// <param name="channel">Channel containing the message</param>
		/// <param name="ts">Message timestamp</param>
		/// <param name="name">Name of the reaction to post</param>
		public async Task RemoveReactionAsync(string channel, string ts, string name)
		{
			ReactionMessage message = new ReactionMessage();
			message.Channel = channel;
			message.Ts = ts;
			message.Name = name;

			static bool ShouldLogError(SlackResponse response) => !response.Ok && !String.Equals(response.Error, "no_reaction", StringComparison.Ordinal);

			await SendRequestAsync<SlackResponse>(RemoveReactionUrl, message, ShouldLogError);
		}

		#endregion

		#region Conversations

		const string ConversationsInviteUrl = "https://slack.com/api/conversations.invite";

		class InviteMessage
		{
			[JsonPropertyName("channel")]
			public string? Channel { get; set; }

			[JsonPropertyName("users")]
			public string? Users { get; set; } // Comma separated list of ids
		}

		/// <summary>
		/// Invite a user to a channel
		/// </summary>
		/// <param name="channel">Channel identifier to invite the user to</param>
		/// <param name="userId">The user id</param>
		public Task InviteUserAsync(string channel, string userId) => InviteUsersAsync(channel, new[] { userId });

		/// <summary>
		/// Invite a set of users to a channel
		/// </summary>
		/// <param name="channel">Channel identifier to invite the user to</param>
		/// <param name="userIds">The user id</param>
		public async Task InviteUsersAsync(string channel, IEnumerable<string> userIds)
		{
			InviteMessage message = new InviteMessage();
			message.Channel = channel;
			message.Users = String.Join(",", userIds);
			await SendRequestAsync<SlackResponse>(ConversationsInviteUrl, message);
		}

		#endregion

		#region Users

		const string UsersInfoUrl = "https://slack.com/api/users.info";
		const string UsersLookupByEmailUrl = "https://slack.com/api/users.lookupByEmail";

		class UserResponse : SlackResponse
		{
			[JsonPropertyName("user")]
			public SlackUser? User { get; set; }
		}

		/// <summary>
		/// Gets a user's profile
		/// </summary>
		/// <param name="userId">The user id</param>
		/// <returns>User profile</returns>
		public async Task<SlackUser> GetUserAsync(string userId)
		{
			using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, $"{UsersInfoUrl}?user={userId}"))
			{
				UserResponse response = await SendRequestAsync<UserResponse>(request, "", ShouldLogError);
				return response.User!;
			}
		}

		/// <summary>
		/// Finds a user by email address
		/// </summary>
		/// <param name="email">The user's email address</param>
		/// <returns>User profile</returns>
		public async Task<SlackUser?> FindUserByEmailAsync(string email)
		{
			using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, $"{UsersLookupByEmailUrl}?email={email}"))
			{
				static bool ShouldLogError(UserResponse response) => !response.Ok && !String.Equals(response.Error, "users_not_found", StringComparison.Ordinal);

				UserResponse response = await SendRequestAsync<UserResponse>(request, "", ShouldLogError);
				return response.User;
			}
		}

		#endregion

		#region Views

		const string ViewsOpenUrl = "https://slack.com/api/views.open";

		class ViewsOpenRequest
		{
			[JsonPropertyName("trigger_id")]
			public string? TriggerId { get; set; }

			[JsonPropertyName("view")]
			public SlackView? View { get; set; }
		}

		/// <summary>
		/// Open a new modal view, in response to a trigger
		/// </summary>
		/// <param name="triggerId">The trigger id, as returned as part of an interaction payload</param>
		/// <param name="view">Definition for the view</param>
		public async Task OpenViewAsync(string triggerId, SlackView view)
		{
			ViewsOpenRequest request = new ViewsOpenRequest();
			request.TriggerId = triggerId;
			request.View = view;
			await SendRequestAsync<PostMessageResponse>(ViewsOpenUrl, request);
		}

		#endregion
	}
}

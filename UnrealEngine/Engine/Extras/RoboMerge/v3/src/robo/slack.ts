// Copyright Epic Games, Inc. All Rights Reserved.

import * as request from '../common/request';
import { ContextualLogger } from '../common/logger';

export interface SlackChannel {
	id: string
	botToken: string
	userToken: string
}

export interface SlackMessageField {
	title: string
	value: string | number | boolean 
	short: boolean
}

// https://api.slack.com/docs/message-attachments#link_buttons
export interface SlackLinkButtonAction {
	type: "button"
	text: string
	url: string
	style?: "default" | "primary" | "danger" //black/white, (green), (red)
	value?: string
}

// https://api.slack.com/docs/interactive-message-field-guide#attachment_fields
export interface SlackAttachment {
	text?: string
	color?: string
	pretext?: string
	mrkdwn_in: string[] // ["pretext", "text", "fields"]
}

export interface SlackLinkButtonsAttachment extends SlackAttachment {
	text: string
	fallback: string
	actions: SlackLinkButtonAction[] // Up to 5 buttons can exist in one attachment before Slack gets mad
}

export enum SlackMessageStyles {
	GOOD = 'good',
	WARNING = 'warning',
	DANGER = 'danger'
}

export interface SlackMessage {
	text: string

	username?: string
	style?: SlackMessageStyles
	fields?: SlackMessageField[]
	title?: string
	title_link?: string
	icon_emoji?: string
	pretext?: string
	footer?: string

	// An array because the Slack API expects it to be
	attachments?: SlackAttachment[]
	// Direct Message support
	channel: string
	// Allows Markdown formatting in messages
	mrkdwn: boolean

	// For the dummy server implementation we store additional information
	cl?: number
	target?: string
}


const MAIN_MESSAGE_FIELDS = new Set(['username', 'icon_emoji', 'channel', 'target', 'cl']);

export class Slack {
	constructor(private channel: SlackChannel, private domain: string, private readonly logger: ContextualLogger) {
	}

	async addUserToChannel(user: string, channel: string, externalUser?: boolean) {
		if (externalUser) {		
			return this.post_user(this.channel.userToken || this.channel.botToken, 'admin.conversations.invite', {channel_id:channel, user_ids:user}, true)
		}
		return this.post('conversations.invite', {channel, users:user}, true)
	}

	async getChannelInfo(channel:string) {
		return this.get('conversations.info', {channel})
	}

	async postMessage(message: SlackMessage) {
		return (await this.post('chat.postMessage', this.makeArgs(message))).ts
	}

	postMessageToDefaultChannel(message: SlackMessage) {
		const args = this.makeArgs(message)
		args.channel = this.channel.id
		return this.post('chat.postMessage', args)
	}

	reply(thread_ts: string, message: SlackMessage) {
		const args = this.makeArgs(message)
		args.thread_ts = thread_ts
		return this.post('chat.postMessage', args)
	}

// why was message optional?
	update(ts: string, message: SlackMessage) {
		const args = this.makeArgs(message)
		args.ts = ts
		return this.post('chat.update', args)
	}

	listMessages(count?: number) {
		const args: any = count ? {count} : count
		// use channels.history if publlic?
		return this.get('groups.history', args)
	}

	async lookupUserIdByEmail(email: string) {
		const userLookupResult = await this.get('users.lookupByEmail', {email}, true)
		return userLookupResult.ok ? userLookupResult.user.id : null
	}

	async openDMConversation(users: string | string[]) : Promise<string> {
		if (users instanceof Array) {
			users = users.join(',')
		}
		return (await this.post('conversations.open', {users})).channel.id
	}

	/*private*/ async post_user(userToken: string, command: string, args: any, canFail? : boolean) {
		const resultJson = await request.post({
			url: this.domain + '/api/' + command,
			body: JSON.stringify(args),
			headers: {Authorization: 'Bearer ' + userToken},
			contentType: 'application/json; charset=utf-8'
		})
		try {
			const result = JSON.parse(resultJson)
			if (result.ok || canFail) {
				return result
			}
		}
		catch {
		}

		this.logger.error(`${command} generated:\n\t${resultJson}`)
		return {ok: false}
	}

	/*private*/ async post(command: string, args: any, canFail? : boolean) {
		return this.post_user(this.channel.botToken, command, args, canFail)
	}

	/*private*/ async get(command: string, args: any, canFail? : boolean) {

		// erg: why am I always passing a channel?
		if (this.channel.id && !args.channel) {
			args.channel = this.channel.id
		}

		const qsBits: string[] = []
		for (const arg in args) {
			qsBits.push(`${encodeURIComponent(arg)}=${encodeURIComponent(args[arg])}`)
		}

		const url = this.domain + `/api/${command}?${qsBits.join('&')}`

		const rawResult = await request.get({url,
			headers: {Authorization: 'Bearer ' + this.channel.botToken}
		})

		try {
			const result = JSON.parse(rawResult)
			if (result.ok || canFail) {
				return result
			}
		}
		catch {
		}
		
		this.logger.error(`url: '${url}' error: '${rawResult}'`)
		return {ok: false}
	}

// behaviour seems to be: put in attachment for green margin if any opts sent at all (e.g. {} different to nothing passed)


// text in messageOpts was different to text argument:
//	former goes in attachment

//	should now overload so that string only puts text in args

// look out for makeArgs with just string - support if necessary
	private makeArgs(message: SlackMessage) {
		const args: {[arg: string]: any} = {}

		// markdown disabled to allow custom links
		// (seems can't have both a link and a user @ without looking up user ids)
		const attch: any = {color: message.style || 'good', text: message.text}
		args.attachments = [attch]

		// Add any explicit attachments
		if (message.attachments) {
			args.attachments = args.attachments.concat(message.attachments)
		}

		const opts = message as unknown as {[key: string]: any}
		for (const key in opts) {
			if (MAIN_MESSAGE_FIELDS.has(key)) {
				args[key] = opts[key]
			}
			else if (key !== 'style') {
				attch[key] = opts[key]
			}
		}

		// parse=client doesn't allow custom links (can't seem to specify it just for main message)
		// args.parse = 'client'
		// args.parse = 'full'
		args.link_names = 1
		return args
	}
}

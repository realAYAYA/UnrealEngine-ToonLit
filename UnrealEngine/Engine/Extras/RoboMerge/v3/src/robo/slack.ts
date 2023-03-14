// Copyright Epic Games, Inc. All Rights Reserved.

import * as request from '../common/request';

export interface SlackChannel {
	id: string
	botToken: string
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
}


const MAIN_MESSAGE_FIELDS = new Set(['username', 'icon_emoji', 'channel']);

export class Slack {
	constructor(private channel: SlackChannel, private domain: string) {
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

	async lookupUserIdByEmail(email: string) : Promise<string> {
		return (await this.get('users.lookupByEmail', {token: this.channel.botToken, email})).user.id
	}

	async openDMConversation(users: string | string[]) : Promise<string> {
		if (users instanceof Array) {
			users = users.join(',')
		}
		return (await this.post('conversations.open', {token: this.channel.botToken, users})).channel.id
	}

	/*private*/ async post(command: string, args: any) {
		const resultJson = await request.post({
			url: this.domain + '/api/' + command,
			body: JSON.stringify(args),
			headers: {Authorization: 'Bearer ' + this.channel.botToken},
			contentType: 'application/json; charset=utf-8'
		})
		try {
			const result = JSON.parse(resultJson)
			if (result.ok) {
				return result
			}
		}
		catch {
		}

		throw new Error(`${command} generated:\n\t${resultJson}`)
	}

	/*private*/ async get(command: string, args: any) {

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

		const result = JSON.parse(rawResult)
		if (!result.ok) {
			throw new Error(rawResult)
		}
		return result
	}

	async* getPages(command: string, limit?: number, inArgs?: any) {
		const args = inArgs || {}
		args.limit = limit || 100
		for (let pageNum = 1;; ++pageNum) {
			// for now limit to 50 pages, just in case
			if (pageNum > 50) {throw new Error('busted safety valve!')}

			const result = await this.get(command, args)
			yield [result, pageNum]

			args.cursor = result.response_metadata && result.response_metadata.next_cursor
			if (!args.cursor) {
				break
			}
		}

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

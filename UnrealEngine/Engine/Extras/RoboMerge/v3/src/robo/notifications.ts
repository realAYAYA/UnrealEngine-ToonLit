// Copyright Epic Games, Inc. All Rights Reserved.

import * as fs from 'fs';
import { DateTimeFormatOptions } from 'intl';
import { Args } from '../common/args';
import { Badge } from '../common/badge';
import { Random } from '../common/helper';
import { ContextualLogger } from '../common/logger';
import { Blockage, Branch, BranchArg, ForcedCl, MergeAction, NodeOpUrlGenerator, resolveBranchArg } from './branch-interfaces';
import { PersistentConflict, Resolution } from './conflict-interfaces';
import { BotEventHandler, BotEvents } from './events';
import { NodeBot } from './nodebot';
import { BlockageNodeOpUrls } from './roboserver';
import { Context } from './settings';
import { Slack, SlackAttachment, SlackLinkButtonAction, SlackLinkButtonsAttachment, SlackMessageField, SlackMessage, SlackMessageStyles } from './slack';
import { WebServer } from '../common/webserver'
import { DummySlackApp } from '../common/dummyslackserver'

const notificationsStartupLogger = new ContextualLogger('Notifications Startup')

// var to enable DMs on blockage
const DIRECT_MESSAGING_ENABLED = true

export const NOTIFICATIONS_PERSISTENCE_KEY = 'notifications'
export const SLACK_MESSAGES_PERSISTENCE_KEY = 'slackMessages'
const CHANGELIST_FIELD_TITLE = 'Change'
const ACKNOWLEDGED_FIELD_TITLE = 'Acknowledged'
const SUNSET_PERSISTED_NOTIFICATIONS_DAYS = 30
const EPIC_TIME_OPTIONS: DateTimeFormatOptions = {timeZone: 'EST5EDT', timeZoneName: 'short'}

const KNOWN_BOT_NAMES = ['buildmachine', 'robomerge'];

let SLACK_TOKENS: {[name: string]: string} = {}
const SLACK_DEV_DUMMY_TOKEN = 'dev'

let args: Args
export function notificationsInit(inArgs: Args) {
	
	args = inArgs

	if (args.devMode) {
		Badge.setDevMode()
	}

	if (args.slackDomain.indexOf('localhost') >= 0) { // todo should parse out port
		const slackServer = new WebServer(new ContextualLogger('dummy Slack'))
		slackServer.addApp(DummySlackApp)
		slackServer.open(8811, 'http')
		return
	}

	let vault
	try {
		const vaultString = fs.readFileSync(args.vault + '/vault.json', 'ascii')
		vault = JSON.parse(vaultString)
	}
	catch (err) {
		notificationsStartupLogger.warn(`Warning, failed to find Slack secrets in vault (ok in dev): ${err.toString()}`)
		return
	}

	const tokensObj = vault['slack-tokens']
	if (tokensObj) {
		SLACK_TOKENS = tokensObj
	}
}

export function isUserAKnownBot(user: string) {
	return KNOWN_BOT_NAMES.indexOf(user) !== -1
}


export async function postToRobomergeAlerts(message: string) {
	// 'CBGFJQEN6' is #robomerge_alerts
	if (!args.devMode) {
		return postMessageToChannel(message, 'CBGFJQEN6')
	}
}

export async function postMessageToChannel(message: string, channel: string, style: SlackMessageStyles = SlackMessageStyles.GOOD) {
	if (SLACK_TOKENS.bot) {
		return new Slack({id: channel, botToken: SLACK_TOKENS.bot}, args.slackDomain).postMessageToDefaultChannel({
			text: message,
			style,
			channel,
			mrkdwn: true
		})
	}
}

//////////
// Utils

// make a Slack notifcation link for a user
function atifyUser(user: string) {
	// don't @ names of bots (currently making them tt style for Slack)
	return isUserAKnownBot(user) ? `\`${user}\`` : '@' + user
}

function generatePersistedSlackMessageKey(sourceCl: number, targetBranchArg: BranchArg, channel: string) {
	const branchName = resolveBranchArg(targetBranchArg, true)
	return `${sourceCl}:${branchName}:${channel}`
}

function formatDuration(durationSeconds: number) {
	const underSixHours = durationSeconds < 6 * 3600
	const durationBits: string[] = []
	if (durationSeconds > 3600) {
		const hoursFloat = durationSeconds / 3600
		const hours = underSixHours ? Math.floor(hoursFloat) : Math.round(hoursFloat)
		durationBits.push(`${hours} hour` + (hours === 1 ? '' : 's'))
	}
	// don't bother with minutes if over six hours
	if (underSixHours) {
		if (durationSeconds < 90) {
			if (durationSeconds > 10) {
				durationSeconds = Math.round(durationSeconds)
			}
			durationBits.push(`${durationSeconds} seconds`)
		}
		else {
			const minutes = Math.round((durationSeconds / 60) % 60)
			durationBits.push(`${minutes} minutes`)
		}
	}
	return durationBits.join(', ')
}

function formatResolution(info: PersistentConflict) {
	// potentially three people involved:
	//	a: original author
	//	b: owner of conflict (when branch resolver overridden)
	//	c: instigator of skip

	// Format message as "a's change was skipped [by c][ on behalf of b][ after N minutes]
	// combinations of sameness:
	// (treat null c as different value, but omit [by c])
	//		all same: 'a skipped own change'
	//		a:	skipped by owner, @a, write 'by owner (b)' instead of b, c
	//		b:	'a skipped own change' @b
	//		c:	resolver not overridden, @a, omit b
	//		all distinct: @a, @b
	// @ a and/or c if they're not the same as b

	const overriddenOwner = info.owner !== info.author

	// display info.resolvingAuthor where possible, because it has correct case
	const resolver = info.resolvingAuthor && info.resolvingAuthor.toLowerCase()
	const bits: string[] = []
	if (resolver === info.author) {
		bits.push(info.resolvingAuthor!, info.resolution!, 'own change')
		if (overriddenOwner) {
			bits.push('on behalf of', info.owner)
		}
	}
	else {
		bits.push(info.author + "'s", 'change was', info.resolution!)
		if (!resolver) {
			// don't know who skipped (shouldn't happen) - notify owner
			if (overriddenOwner) {
				bits.push(`(owner: ${info.owner})`)
			}
		}
		else if (info.owner === resolver) {
			bits.push(`by owner (${info.resolvingAuthor})`)
		}
		else {
			bits.push('by', info.resolvingAuthor!)
			if (overriddenOwner) {
				bits.push('on behalf of', atifyUser(info.owner))
			}
			else {
				// only case we @ author - change has been resolved by another known person, named in message
				bits[0] = atifyUser(info.author) + "'s"
			}
		}
	}

	if (info.timeTakenToResolveSeconds) {
		bits.push('after', formatDuration(info.timeTakenToResolveSeconds))
	}

	if (info.resolvingReason) {
		bits.push(`(Reason: ${info.resolvingReason})`)
	}

	let message = bits.join(' ') + '.'

	if (info.timeTakenToResolveSeconds) {
		// add time emojis!
		if (info.timeTakenToResolveSeconds < 2*60) {
			const poke = Random.choose(['eevee', 'espeon', 'sylveon', 'flareon', 'jolteon', 'leafeon', 'glaceon', 'umbreon', 'vaporeon'])
			message += ` :${poke}_run:`
		}
		else if (info.timeTakenToResolveSeconds < 10*60) {
			message += ' :+1:'
		}
		// else if (info.timeTakenToResolveSeconds > 30*60) {
		// 	message += ' :sadpanda:'
		// }
	}
	return message
}

//////////

type SlackConflictMessage = {
	timestamp: string
	messageOpts: SlackMessage
}

type PersistedSlackMessages = {[key: string]: SlackConflictMessage}

/** Wrapper around Slack, keeping track of messages keyed on target branch and source CL */
class SlackMessages {
	private readonly smLogger: ContextualLogger
	constructor(private slack: Slack, private persistence: Context, parentLogger: ContextualLogger) {
		this.smLogger = parentLogger.createChild('SlackMsgs')
		const slackMessages: PersistedSlackMessages | null = this.persistence.get(SLACK_MESSAGES_PERSISTENCE_KEY)

		if (slackMessages) {
			this.doHouseKeeping(slackMessages)
		}
		else {
			this.persistence.set(SLACK_MESSAGES_PERSISTENCE_KEY, {})
		}
	}

	async postOrUpdate(sourceCl: number, branchArg: BranchArg, message: SlackMessage, persistMessage = true) {
		const findResult = this.find(sourceCl, branchArg, message.channel)

		// If we find a message, simply update the contents
		if (findResult.messageRecord) {
			// keep ack field if present and not in new message

			if (findResult.messageRecord.messageOpts.fields && (
				!message.fields || !message.fields.some(field =>
				field.title === ACKNOWLEDGED_FIELD_TITLE))) {

				for (const field of findResult.messageRecord.messageOpts.fields) {
					if (field.title === ACKNOWLEDGED_FIELD_TITLE) {
						message.fields = [...(message.fields || []), field]
						break
					}
				}
			}
			findResult.messageRecord.messageOpts = message
			await this.update(findResult.messageRecord, findResult.persistedMessages)
		}
		// Otherwise, we will need to create a new one
		else {
			let timestamp
			try {
				timestamp = await this.slack.postMessage(message)
			}
			catch (err) {
				this.smLogger.printException(err, 'Error talking to Slack')
				return
			}

			// Used for messages we don't care to keep, currently the /api/test/directmessage endpoint
			if (persistMessage) {
				findResult.persistedMessages[findResult.conflictKey] = {timestamp, messageOpts: message}
				this.persistence.set(SLACK_MESSAGES_PERSISTENCE_KEY, findResult.persistedMessages)
			}
		}
	}

	async postDM(emailAddress: string, sourceCl: number, branchArg: BranchArg, dm: SlackMessage, persistMessage = true) {
		// The Slack API requires a user ID to open a direct message with users.
		// The most consistent way to do this is getting their email address out of P4.
		if (!emailAddress) {
			console.error("Failed to get email address during notifications for CL " + sourceCl)
			return
		}

		// With their email address, we can get their user ID via Slack API
		let userId : string
		try {
			userId = (await this.slack.lookupUserIdByEmail(emailAddress.toLowerCase()))
		} catch (err) {
			this.smLogger.printException(err, `Failed to get user ID for Slack DM, given email address "${emailAddress}" for CL ${sourceCl}`)
			return
		}

		// Open up a new conversation with the user now that we have their ID
		let channelId : string
		try {
			channelId = (await this.slack.openDMConversation(userId))
			dm.channel = channelId
		} catch (err) {
			this.smLogger.printException(err, `Failed to get Slack conversation ID for user ID "${userId}" given email address "${emailAddress}" for CL ${sourceCl}`)
			return
		}

		// Add the channel/conversation ID to the messageOpts and proceed normally.
		this.smLogger.info(`Creating direct message for ${emailAddress} (key: ${generatePersistedSlackMessageKey(sourceCl, branchArg, channelId)})`)
		this.postOrUpdate(sourceCl, branchArg, dm, persistMessage)
	}

	// 
	findAll(sourceCl: number, branchArg: BranchArg) {
		// Get key to search our list of all messages
		const persistedMessages = this.persistence.get(SLACK_MESSAGES_PERSISTENCE_KEY) as PersistedSlackMessages
		const conflictKey = generatePersistedSlackMessageKey(sourceCl, branchArg, "") // pass empty string as channel name

		// Filter messages
		const messages: SlackConflictMessage[] = []

		for (const key in persistedMessages) {
			if (key.startsWith(conflictKey)) {
				messages.push(persistedMessages[key])
			}
		}

		return {messages, persistedMessages}
	}

	find(sourceCl: number, branchArg: BranchArg, channel: string) {
		// Get key to search our list of all messages
		const persistedMessages = this.persistence.get(SLACK_MESSAGES_PERSISTENCE_KEY) as PersistedSlackMessages
		const conflictKey = generatePersistedSlackMessageKey(sourceCl, branchArg, channel)
		return {messageRecord: persistedMessages[conflictKey], conflictKey, persistedMessages}
	}

	async update(messageRecord: SlackConflictMessage, persistedMessages: PersistedSlackMessages) {
		try {
			await this.slack.update(messageRecord.timestamp, messageRecord.messageOpts)
		}
		catch (err) {
			console.error('Error updating message in Slack! ' + err.toString())
			return
		}

		this.persistence.set(SLACK_MESSAGES_PERSISTENCE_KEY, persistedMessages)
	}

	/** Deliberately ugly function name to avoid accidentally posting duplicate messages about conflicts! */
	postNonConflictMessage(msg: SlackMessage) {
		this.slack.postMessage(msg)
		.catch(err => console.error('Error posting non-conflict to Slack! ' + err.toString()))
	}

	private doHouseKeeping(messages: PersistedSlackMessages) {
		const nowTicks = Date.now() / 1000.

		const keys = Object.keys(messages)
		for (const key of keys) {
			const message = messages[key]
			const messageAgeDays = (nowTicks - parseFloat(message.timestamp))/24/60/60

			// note: serialisation was briefly wrong (11/11/2019), leaving some message records without a message
			if (!message.messageOpts || messageAgeDays > SUNSET_PERSISTED_NOTIFICATIONS_DAYS) {
				this.smLogger.info(`Sunsetting Slack Message "${key}"`)
				delete messages[key]
			}
		}

		// if we removed any messages, persist
		if (Object.keys(messages).length < keys.length) {
			this.persistence.set(SLACK_MESSAGES_PERSISTENCE_KEY, messages)
		}
	}
}

function makeClLink(cl: number, alias?: string) {
	return `<https://p4-swarm.companyname.net/changes/${cl}|${alias ? alias : cl}>`
}

export class BotNotifications implements BotEventHandler {
	private readonly externalRobomergeUrl : string;
	private readonly blockageUrlGenerator : NodeOpUrlGenerator
	private readonly botNotificationsLogger : ContextualLogger
	slackChannel: string;

	constructor(private botname: string, slackChannel: string, persistence: Context, externalUrl: string, 
		blockageUrlGenerator: NodeOpUrlGenerator, parentLogger: ContextualLogger,
		slackChannelOverrides?: [Branch, Branch, string, boolean][]) {
		this.botNotificationsLogger = parentLogger.createChild('Notifications')
		// Hacky way to dynamically change the URL for notifications
		this.externalRobomergeUrl = externalUrl
		this.slackChannel = slackChannel
		this.blockageUrlGenerator = blockageUrlGenerator

		const botToken = args.devMode && SLACK_DEV_DUMMY_TOKEN || SLACK_TOKENS.bot
		if (botToken && slackChannel) {
			this.botNotificationsLogger.info('Enabling Slack messages for ' + botname)
			this.slackMessages = new SlackMessages(new Slack({id: slackChannel, botToken}, args.slackDomain), persistence, this.botNotificationsLogger)
			if (slackChannelOverrides) {
				this.additionalBlockChannelIds = new Map(slackChannelOverrides
					.filter(
						([_1, _2, channel, _3]) => channel !== slackChannel
					)
					.map(
						([source, target, channel, postOnlyToChannel]) => [`${source.upperName}|${target.upperName}`, [channel, postOnlyToChannel]]
					)
				)
			}
		}
	}

	/** Conflict */
	// considered if no Slack set up, should continue to add people to notify, but too complicated:
	//		let's go all in on Slack. Fine for this to be async (but fire and forget)
	async onBlockage(blockage: Blockage) {
		const changeInfo = blockage.change

		if (changeInfo.userRequest) {
			// maybe DM?
			return
		}

		// TODO: Support DMs if we don't have a channel configured
		if (!this.slackMessages) {
			// doing nothing at the moment - don't want to complicate things with fallbacks
			// probably worth having fallback channel, so don't necessarily have to always set up specific channel
			// (in that case would have to show bot as well as branch)
			return
		}

		const cl = blockage.approval ? blockage.approval.shelfCl : changeInfo.cl

		// or integration failure (better wording? exclusive check-out?)
		const sourceBranch = changeInfo.branch
		let targetBranch
		if (blockage.action) {
			targetBranch = blockage.action.branch
		}
		const issue = blockage.failure.kind.toLowerCase()

		// If we get the user's email, we shouldn't ALSO ping them in the channel.
		const userEmail = await blockage.ownerEmail
		const channelPing = userEmail ? blockage.owner : `@${blockage.owner}`

		const isBotUser = isUserAKnownBot(blockage.owner)
		const text =
			blockage.approval ?				`${channelPing}'s change needs to be approved in ${blockage.approval.settings.channelName}` :
			isBotUser ? 									`Blockage caused by \`${blockage.owner}\` commit!` :
			blockage.failure.kind === 'Too many files' ?	`${channelPing}, please request a shelf for this large changelist` :
															`${channelPing}, please resolve the following ${issue}:`

		const message = this.makeSlackChannelMessage(
			`${sourceBranch.name} blocked! (${issue})`,
			text,
			SlackMessageStyles.DANGER, 
			makeClLink(cl), 
			sourceBranch, 
			targetBranch, 
			changeInfo.author
		);

		if (blockage.failure.summary) {
			message.footer = blockage.failure.summary
		}

		if (targetBranch) {
			const additionalChannelInfo = this.additionalBlockChannelIds.get(sourceBranch.upperName + '|' + targetBranch.upperName)
			if (additionalChannelInfo) {
				const [sideChannel, postOnlyToSideChannel] = additionalChannelInfo
				if (!postOnlyToSideChannel) {
					this.slackMessages.postOrUpdate(changeInfo.source_cl, targetBranch.name, message)
				}
				this.slackMessages.postOrUpdate(changeInfo.source_cl, targetBranch.name, { ...message, channel: sideChannel })
			}
			else {
				this.slackMessages.postOrUpdate(changeInfo.source_cl, targetBranch.name, message)
			}
		}
		else {
			// separate message for syntax errors (other blockages have a target branch)
			this.slackMessages.postOrUpdate(changeInfo.source_cl, blockage.failure.kind, message)
		}

		// Post message to owner in DM
		if (DIRECT_MESSAGING_ENABLED && !isBotUser && targetBranch && userEmail) {
			let dm: SlackMessage
			if (blockage.approval) {
				dm = {
					title: 'Approval needed to commit to ' + targetBranch.name,
					text: `Your change has been shelved in ${makeClLink(cl)} and sent to ${blockage.approval.settings.channelName} for approval\n\n` +
							blockage.approval.settings.description,
					channel: "",
					mrkdwn: false
				}
			}
			else {
				const dmText = `Your change (${makeClLink(changeInfo.source_cl)}) ` +
					`hit '${issue}' while merging from *${sourceBranch.name}* to *${targetBranch.name}*.\n\n` +
					'`' + blockage.change.description.substr(0, 80) + '`\n\n' +
					"*_Resolving this blockage is time sensitive._ Please select one of the following:*"
				
				const urls = this.blockageUrlGenerator(blockage)
				if (!urls) {
					const error = `Could not get blockage URLs for blockage -- CL ${blockage.change.cl}`
					this.botNotificationsLogger.printException(error)
					throw error
				}

				dm = this.makeSlackDirectMessage(dmText, changeInfo.source_cl, cl, targetBranch.name, urls)
			}

			this.slackMessages.postDM(userEmail, changeInfo.source_cl, targetBranch, dm)
		}
	}

	onBlockageAcknowledged(info: PersistentConflict) {
		if (this.slackMessages) {
			const targetKey = info.targetBranchName || info.kind
			const title = ACKNOWLEDGED_FIELD_TITLE
			if (info.acknowledger) {
				// hard code to Epic (!) Standard/Daylight Time
				const suffix = info.acknowledgedAt ? ' at ' + info.acknowledgedAt.toLocaleTimeString('en-US', EPIC_TIME_OPTIONS) : ''
				this.tryAddFieldToChannelMessages(info.sourceCl, targetKey, {title, value: info.acknowledger + suffix, short: true})
			}
			else {
				this.tryRemoveFieldFromChannelMessages(info.sourceCl, targetKey, title)
			}
		}
	}

	////////////////////////
	// Conflict resolution
	//
	// For every non-conflicting merge merge operation, no matter whether we committed anything, look for Slack conflict
	// messages that can be set as resolved. This covers the following cases:
	//
	// Normal case (A): user commits CL with resolved unshelved changes.
	//	- RM reparses the change that conflicted and sees nothing to do
	//
	// Corner case (B): user could commit just those files that were conflicted.
	//	- RM will merge the rest of the files and we'll see a non-conflicted commit

	/** On change (case A above) - update message if we see that a conflict has been resolved */
	onBranchUnblocked(info: PersistentConflict) {
		if (this.slackMessages) {
			let newClDesc: string | undefined, messageStyle: SlackMessageStyles
			if (info.resolution === Resolution.RESOLVED) {
				if (info.resolvingCl) {
					newClDesc = `${makeClLink(info.cl)} -> ${makeClLink(info.resolvingCl)}`
				}
				messageStyle = SlackMessageStyles.GOOD
			}
			else {
				messageStyle = SlackMessageStyles.WARNING
			}

			const messageText = formatResolution(info)
			const targetKey = info.targetBranchName || info.kind
			if (!this.updateMessagesAfterUnblock(info.sourceCl, targetKey, '', messageStyle, messageText, newClDesc)) {
				this.botNotificationsLogger.warn(`Conflict message not found to update (${info.blockedBranchName} -> ${targetKey} CL#${info.sourceCl})`)
				const message = this.makeSlackChannelMessage('', messageText, messageStyle, makeClLink(info.cl), info.blockedBranchName, info.targetBranchName, info.author)
				this.slackMessages.postOrUpdate(info.sourceCl, targetKey, message)
			}
		}
	}

	onNonSkipLastClChange(details: ForcedCl) {
		if (this.slackMessages) {
			this.slackMessages.postNonConflictMessage({
				title: details.nodeOrEdgeName + ' forced to new CL',
				text: details.reason,
				style: SlackMessageStyles.WARNING,
				fields: [{
					title: 'By', short: true, value: details.culprit
				}, {
					title: 'Changelists', short: true, value: `${makeClLink(details.previousCl)} -> ${makeClLink(details.forcedCl)}`
				}],
				title_link: this.externalRobomergeUrl + '#' + this.botname,
				mrkdwn: true,
				channel: this.slackChannel // Default to the configured channel
			}) 
		}
	}

	sendTestMessage(username : string) {
		let text = `${username}, please resolve the following test message:\n(source CL: 0, conflict CL: 1, shelf CL: 2)`

		// This doesn't need to be a full Blockage -- just enough to generate required info for message
		const testBlockage: Blockage = {
			action: {
				branch: {
					name: "TARGET_BRANCH",
				} as Branch,
			} as MergeAction,
			failure: {
				kind: "Merge conflict",
				description: ""
			}
		} as Blockage

		const messageOpts = this.makeSlackDirectMessage(text, 0, 1, "TARGETBRANCH",
			NodeBot.getBlockageUrls(
				testBlockage,
				this.externalRobomergeUrl,
				"TEST",
				"SOURCE_BRANCH",
				"0",
				false
			)
		)

		this.slackMessages!.postDM(`${username}@companyname.com`, 0, "TARGETBRANCH", messageOpts, false)
	}

	sendGenericNonConflictMessage(message: string) {
		if (this.slackMessages) {
			this.slackMessages.postNonConflictMessage({
				text: message,
				style: SlackMessageStyles.WARNING,
				mrkdwn: true,
				channel: this.slackChannel
			})
		}
	}

	private makeSlackChannelMessage(title: string, text: string, style: SlackMessageStyles, clDesc: string, sourceBranch: BranchArg,
											targetBranch?: BranchArg, author?: string, buttons?: SlackLinkButtonsAttachment[]) {
		const integrationText = [resolveBranchArg(sourceBranch)]
		if (targetBranch) {
			integrationText.push(resolveBranchArg(targetBranch))
		}
		const fields: SlackMessageField[] = [
			{title: 'Integration', short: true, value: integrationText.join(' -> ')},
			{title: CHANGELIST_FIELD_TITLE, short: true, value: clDesc},
		]

		if (author) {
			fields.push({title: 'Author', short: true, value: author})
		}

		const opts: SlackMessage = {title, text, style, fields,
			title_link: this.externalRobomergeUrl + '#' + this.botname,
			mrkdwn: true,
			channel: this.slackChannel // Default to the configured channel
		}

		if (buttons) {
			opts.attachments = buttons
		}

		return opts
	}

	// This is an extremely opinionated function to send a stylized direct message to the end user.
	//makeSlackChannelMessageOpts(title: string, style: string, clDesc: string, sourceBranch: BranchArg,targetBranch?: BranchArg, author?: string, buttons?: SlackLinkButtonsAttachment[]) 
	private makeSlackDirectMessage(messageText: string, sourceCl: number, conflictCl: number, targetBranch: string, conflictUrls: BlockageNodeOpUrls) : SlackMessage {
		// Start collecting our attachments
		let attachCollection : SlackAttachment[] = []
		
		// Acknowledge button
		const conflictClLink = makeClLink(conflictCl, 'conflict CL #' + conflictCl)
		attachCollection.push(<SlackLinkButtonsAttachment>{
			text: `"I will merge ${conflictClLink} to *${targetBranch}* myself."`,
			fallback: `Please acknowledge blockages at ${this.externalRobomergeUrl}`,
			mrkdwn_in: ["text"],
			actions: [this.generateAcknowledgeButton("Acknowledge Conflict", conflictUrls.acknowledgeUrl)]
		})

		// Create shelf button
		if (conflictUrls.createShelfUrl) {
			attachCollection.push(<SlackLinkButtonsAttachment>{
				text: `"Please create a shelf with the conflicts encountered while merging ${makeClLink(sourceCl)} into *${targetBranch}*"`,
				fallback: `You can create a shelf at ${this.externalRobomergeUrl}`,
				mrkdwn_in: ["text"],
				actions: [this.generateCreateShelfButton(`Create Shelf in ${targetBranch}`, conflictUrls.createShelfUrl)]
			})
		}

		// Skip button
		if (conflictUrls.skipUrl) {
			attachCollection.push(<SlackLinkButtonsAttachment>{
				text: `"${makeClLink(sourceCl)} should not be automatically merged to *${targetBranch}*."`,
				fallback: `You can skip work at ${this.externalRobomergeUrl}`,
				mrkdwn_in: ["text"],
				actions: [this.generateSkipButton(`Skip Merge to ${targetBranch}`, conflictUrls.skipUrl)]
			})
		}

		// Create stomp button if this isn't an exclusive checkout
		if (conflictUrls.stompUrl) {
			attachCollection.push(<SlackLinkButtonsAttachment>{
				text: `"The changes in ${makeClLink(sourceCl)} should stomp the work in *${targetBranch}*."`,
				fallback: `You can stomp work at ${this.externalRobomergeUrl}`,
				mrkdwn_in: ["text"],
				actions: [this.generateStompButton(`Stomp Changes in ${targetBranch}`, conflictUrls.stompUrl)]
			})
		}

		// Append footer
		attachCollection.push({
			pretext: "You can get help via the Slack channel <#C9321FLTU> (if you don't have access to 'robomerge-help', please contact the IT helpdesk)",
			mrkdwn_in: ["pretext"]
		})

		// Return SlackMessage for our direct message
		return {
			text: messageText,
			mrkdwn: true,
			channel: "",
			attachments: attachCollection
		}
	}

	private generateAcknowledgeButton(buttonText: string, url: string) : SlackLinkButtonAction {
		return { 
			type: "button",
			text: buttonText,
			url,
			style: "primary"
		}
	}

	private generateCreateShelfButton(buttonText: string, url: string) : SlackLinkButtonAction {
		return { 
			type: "button",
			text: buttonText,
			url,
			style: "primary"
		}
	}

	private generateSkipButton(buttonText: string, url: string) : SlackLinkButtonAction {
		return { 
			type: "button",
			text: buttonText,
			url,
			style: 'default'
		}
	}

	private generateStompButton(buttonText: string, url: string) : SlackLinkButtonAction {
		return { 
			type: "button",
			text: buttonText,
			url,
			style: "danger"
		}
	}

	

	/** Pre-condition: this.slackMessages must be valid */
	private updateMessagesAfterUnblock(sourceCl: number, targetBranch: BranchArg, newTitle: string,
										newStyle: SlackMessageStyles, newText: string, newClDesc?: string) {
		// Find all messages relating to CL and branch
		const findResult = this.slackMessages!.findAll(sourceCl, targetBranch)

		if (findResult.messages.length == 0) {
			return false
		}

		for (const messageRecord of findResult.messages) {
			const message = messageRecord.messageOpts
			if (newTitle) {
				message.title = newTitle
			}
			else {
				delete message.title
			}

			// e.g. change colour from red to orange
			message.style = newStyle

			// e.g. change source CL to 'source -> dest'
			if (message.fields) {
				const newFields: SlackMessageField[] = []
				for (const field of message.fields) {
					switch (field.title) {
						case CHANGELIST_FIELD_TITLE:
							if (newClDesc) {
								field.value = newClDesc
							}
							break

						case ACKNOWLEDGED_FIELD_TITLE:
							// skip add
							continue
					}
					newFields.push(field)
				}
				message.fields = newFields
			}

			// Delete button attachments sent via Robomerge Slack App
			if (message.attachments) {
				delete message.attachments

				// Hacky: If we remove attachments, we'll no longer have a link to the CL in the message.
				// UE-72320 - Add in a link to the original changelist
				message.text = newText.replace('change', `change (${makeClLink(sourceCl)})`)
			}
			else {
				message.text = newText
			}

			// optionally remove second row of entries
			if (message.fields) {
				// remove shelf entry
				message.fields = message.fields.filter(field => field.title !== 'Shelf' && field.title !== 'Author')
				delete message.footer
			}

			this.slackMessages!.update(messageRecord, findResult.persistedMessages)
		}
		
		return true
	}

	/** Pre-condition: this.slackMessages must be valid */
	private tryAddFieldToChannelMessages(sourceCl: number, targetBranch: BranchArg, newField: SlackMessageField) {
		// Find all messages relating to CL and branch
		const findResult = this.slackMessages!.findAll(sourceCl, targetBranch)

		for (const messageRecord of findResult.messages) {
			const message = messageRecord.messageOpts
			if (message.attachments) {
				// skip DMs
				continue
			}

			if (message.fields && message.fields.find(field => field.title === newField.title)) {
				// do not add same field twice (shouldn't happen, but hey)
				continue
			}

			message.fields = [...(message.fields || []), newField]
			this.slackMessages!.update(messageRecord, findResult.persistedMessages)
		}
	}

	/** Pre-condition: this.slackMessages must be valid */
	private tryRemoveFieldFromChannelMessages(sourceCl: number, targetBranch: BranchArg, fieldTitle: string) {
		// Find all messages relating to CL and branch
		const findResult = this.slackMessages!.findAll(sourceCl, targetBranch)

		for (const messageRecord of findResult.messages) {
			const message = messageRecord.messageOpts
			if (message.attachments || !message.fields) {
				// skip DMs (expecting there to be fields usually, but skipping if not)
				continue
			}

			const replacementFields = message.fields.filter(field => field.title !== fieldTitle)
			if (message.fields.length > replacementFields.length) {
				message.fields = replacementFields
				this.slackMessages!.update(messageRecord, findResult.persistedMessages)
			}
		}
	}

	private readonly slackMessages?: SlackMessages
	private readonly additionalBlockChannelIds = new Map<string, [string, boolean]>()
}

export function bindBotNotifications(events: BotEvents, slackChannelOverrides: [Branch, Branch, string, boolean][], persistence: Context, blockageUrlGenerator: NodeOpUrlGenerator, 
	externalUrl: string, logger: ContextualLogger) {
	events.registerHandler(new BotNotifications(events.botname, events.botConfig.slackChannel, persistence, externalUrl, blockageUrlGenerator, logger, slackChannelOverrides))
}


export function runTests(parentLogger: ContextualLogger) {
	const unitTestLogger = parentLogger.createChild('Notifications')
	const conf: PersistentConflict = {
		blockedBranchName: 'from',
		targetBranchName: 'to',

		cl: 101,
		sourceCl: 1,
		author: 'x',
		owner: 'x',
		kind: 'Unit Test error',

		time: new Date,
		nagged: false,
		ugsIssue: -1,

		resolution: 'pickled' as Resolution
	}

	let nextCl = 101

	const tests = [
		["x's change was pickled.",							'x'],
		["x's change was pickled (owner: y).",				'y'],
		["x pickled own change.",							'x', 'x'],
		["x pickled own change on behalf of y.",			'y', 'x'],
		["@x's change was pickled by y.",					'x', 'y'],
		["x's change was pickled by owner (y).",			'y', 'y'],
		["x's change was pickled by y on behalf of @z.",	'z', 'y'],
	]

	let passed = 0
	for (const test of tests) {
		conf.owner = test[1]
		conf.resolvingAuthor = test[2]
		conf.cl = nextCl++

		const formatted = formatResolution(conf)
		if (test[0] === formatted) {
			++passed
		}
		else {
			unitTestLogger.error('Mismatch!\n' +
				`\tExpected:   ${test[0]}\n` + 
				`\tResult:     ${formatted}\n\n`)
		}
	}

	unitTestLogger.info(`Resolution format: ${passed} out of ${tests.length} correct`)
	return tests.length - passed
}

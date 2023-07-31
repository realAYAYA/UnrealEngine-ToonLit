// Copyright Epic Games, Inc. All Rights Reserved.

import { ContextualLogger } from './logger';
import * as request from '../common/request';

// from UnrealGameSync/PostBadgeStatus
interface BuildData {
	BuildType: string,
	Url: string,
	Project: string,
	ArchivePath: string,
	ChangeNumber: number,
	Result: string // starting, failure, warning, success, or skipped
}

type HordeBadgeData = {
	Stream: string
	Change: number
	Badges: { Name: string, Url: string, State: string }[]
}

const MAX_RETRIES = 3

export const UGS_API_CIS_ROUTE = '<ugs api endpoint>'

export class Badge {
	// static markStarting(badge: string, project: string, cl: number, bot: string, externalUrl: string) { this.mark(Badge.STARTING, badge, project, cl, bot, externalUrl) }
	// static markSuccess(badge: string, project: string, cl: number, bot: string, externalUrl: string) { this.mark(Badge.SUCCESS, badge, project, cl, bot, externalUrl) }
	// static markFailure(badge: string, project: string, cl: number, bot: string, externalUrl: string) { this.mark(Badge.FAILURE, badge, project, cl, bot, externalUrl) }
	// static markWarning(badge: string, project: string, cl: number, bot: string, externalUrl: string) { this.mark(Badge.WARNING, badge, project, cl, bot, externalUrl) }
	// static markSkipped(badge: string, project: string, cl: number, bot: string, externalUrl: string) { this.mark(Badge.SKIPPED, badge, project, cl, bot, externalUrl) }

	static STARTING = 'Starting'
	static SUCCESS = 'Success'
	static FAILURE = 'Failure'
	static WARNING = 'Warning'
	static SKIPPED = 'Skipped'

	private static readonly badgeLogger = new ContextualLogger('Badge')
	private static devMode = false

	static async postWithRetry(args: request.PostArgs, msg: string, method?: string, logResponse?: boolean, shouldRetry?: (response: string) => boolean): Promise<string | null> {
		if (Badge.devMode) {
			Badge.badgeLogger.info('Ignoring badge in dev mode')
			return null
		}

		let retryTimeSeconds = 2 + Math.random()
		let retryNum = 0
		do  {
			let response: string | null = null
			let error = ''
			try {
				response = await request.req(args, method || 'POST')
			}
			catch (err) {
				Badge.badgeLogger.printException(err, 'Unexpected badge error')
				error = err
			}

			if (!error && (!shouldRetry || !shouldRetry(response as string))) {
				if (retryNum > 0) {
					msg += ` after ${retryNum} ${retryNum > 1 ? 'retries' : 'retry'}`
				}
				if (logResponse) {
					msg += ' --- ' + response
				}
				Badge.badgeLogger.info(msg)
				return response
			}

			await new Promise(done => setTimeout(done, retryTimeSeconds * 1000))
			retryTimeSeconds *= 2
		}
		while (++retryNum < MAX_RETRIES)

		Badge.badgeLogger.warn(`Failed to post UGS update after ${MAX_RETRIES} attempts`)
		return null
	}

	// {
	// "Stream": "//Fortnite/Release-15.10",
	// "Change": 14680690,
	// "Badges": [
	// 	{
	// 		"Name": "Merge",
	// 		"Url": "https://robomerge.epicgames.net/",
	// 		"State": "Starting"
	// 	}
	// ]

	static mark(result: string, badge: string, project: string, cl: number, bot: string, externalUrl: string, badgeUrlOverride?: string) {
		const roboUrl = `${externalUrl}#${bot}`
		let data : HordeBadgeData | BuildData
		if (badgeUrlOverride) {
			data = {
				Stream: project,
				Change: cl,
				Badges: [{
					Name: badge,
					Url: roboUrl,
					State: result
				}]
			}
		}
		else {
			data = {
				BuildType: badge,
				Url: roboUrl,
				Project: project,
				ArchivePath: '',
				ChangeNumber: cl,
				Result: result
			}
		}

		const url = badgeUrlOverride || UGS_API_CIS_ROUTE
		const body = JSON.stringify(data)
		return Badge.postWithRetry({
			url,
			body,
			contentType: 'application/json'
		}, `Added '${badge}' (${result}) UGS badge to ${project}@${cl} (${url})\n${body}`)
	}

	static setDevMode() {
		Badge.devMode = true
	}
}

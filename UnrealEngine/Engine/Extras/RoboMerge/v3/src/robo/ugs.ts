// Copyright Epic Games, Inc. All Rights Reserved.

import { Blockage } from './branch-interfaces';
import { Badge } from '../common/badge';
import { ContextualLogger } from '../common/logger';

// need to check whether build health reporting is working
const UGS_URL_ROOT = 'http://ugsapi.epicgames.net'

interface IntegrationIssue {
	Project: string
	Summary: string
	Owner: string
}

interface Diagnostic {
	Message: string
	Url: string
}

export class UGS {
	private static readonly ugsLogger = new ContextualLogger('UGS')

	static async reportBlockage(blockage: Blockage, externalUrl: string) {
		const issue = blockage.failure.kind.toLowerCase()
		const sourceBranch = blockage.change.branch
		const bot = sourceBranch.parent.botname
		let Summary = `RoboMerge ${issue} attempting to integrate CL#${blockage.change.cl} from ${sourceBranch.upperName}`
		if (blockage.action) {
			Summary += ` to ${blockage.action.branch.upperName}`
		}

		const data: IntegrationIssue = {
			Project: bot[0].toUpperCase() + bot.substr(1).toLowerCase(),// slight hack: capitalise bot name to match UGS game projects
			Summary, Owner: blockage.owner
		}
		const responseStr = await Badge.postWithRetry({
			url: UGS_URL_ROOT + '/issues',
			body: JSON.stringify(data),
			contentType: 'application/json'
		}, `Posted issue (${issue}) to UGS for ${bot}:${sourceBranch.upperName}`)

		if (!responseStr) {
			this.ugsLogger.warn('Empty response')
			throw new Error('Empty response')
		}

		let response: any = null
		try {
			response = JSON.parse(responseStr)
		}
		catch (err) {
		}

		if (!response || !response.Id || typeof response.Id !== 'number') {
			const message = response && response.Message || responseStr
			this.ugsLogger.warn('Invalid response: ' + message)
			throw new Error('Invalid response')
		}

		const diagnostic: Diagnostic = {
			Message: blockage.failure.description,
			Url: `${externalUrl}#${bot}`
		}

		// fire and forget
		Badge.postWithRetry({
			url: UGS_URL_ROOT + `/issues/${response.Id}/diagnostics`,
			body: JSON.stringify(diagnostic),
			contentType: 'application/json'
		}, `Posted diagnostic for UGS issue ${response.Id}`)

		return response.Id
	}

	static acknowledge(issue: number, acknowledger: string) {
		return Badge.postWithRetry({
			url: UGS_URL_ROOT + '/issues/' + issue,
			body: `{"Acknowledged":"${acknowledger}"}`,
			contentType: 'application/json'
		}, `Posted UGS issue ${issue} ` + (acknowledger ? `acknowledged by ${acknowledger}` : 'unacknowledged'))
	}

	static reportResolved(issue: number) {
		return Badge.postWithRetry({
			url: UGS_URL_ROOT + '/issues/' + issue,
			body: '{"Resolved":true}',
			contentType: 'application/json'
		}, `Posted issue resolved (${issue}) to UGS`, 'PUT', true, response => response.includes('An error has occurred'))
	}
}

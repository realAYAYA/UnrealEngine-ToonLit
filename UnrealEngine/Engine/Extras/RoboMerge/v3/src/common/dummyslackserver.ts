// Copyright Epic Games, Inc. All Rights Reserved.

import { AppInterface, Handler, WebRequest } from './webserver'
import { setDefault } from './helper'

// channel -> target -> changes
const posted = new Map<string, Map<string, number[]>>()


export class DummySlackApp implements AppInterface {

	constructor(private req: WebRequest) {
	}

	@Handler('POST', '/api/*')
	post(command: string) {
		// @todo (maybe) construct app with logger

		const data = this.req.reqData
		if (!data) {
			throw new Error('Nothing to post!')
		}
		const clMatch = data.match(/changes\/(\d+)\|\d+/)
		const edgeMatch = data.match(/"(\w+) -> (\w+)"/)
		if (clMatch && edgeMatch) {
			let dataObj: any
			try {
				dataObj = JSON.parse(data)
			}
			catch (exc) {
				console.log('Non-JSON sent to dummy-slack: ', data)
				return
			}

			setDefault(
				setDefault(posted, dataObj.channel, new Map),
				edgeMatch[2], []
			).push(parseInt(clMatch[1]))
		}
		else {
			console.log('dummy-slack POST: cl or edge not found', command, this.req.reqData)
		}

		// look for color for block or resolve?

		// careful, two changes for resolutions, luckily first is what we want
	}

	@Handler('GET', '/posted/*')
	channelMessages(channel: string) {
		return [...(posted.get(channel) || [])]
	}
}

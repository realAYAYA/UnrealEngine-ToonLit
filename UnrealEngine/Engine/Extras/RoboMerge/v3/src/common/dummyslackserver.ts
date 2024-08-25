// Copyright Epic Games, Inc. All Rights Reserved.

import { AppInterface, Handler, WebRequest } from './webserver'
import { setDefault } from './helper'

// This must be kept in sync with tests.ts
class DummyThread {
	CL: number
	target: string
	timestamp: number
	messages: string[] // the first message is the original post, the rest are replies
}

// The initial messages of any threads
// channel -> target -> messages for a change
const posted = new Map<string, Map<string, DummyThread[]>>()

// The contents of each thread
// channel -> timestamp -> messages
const threads = new Map<string, Map<number, DummyThread>>()

export class DummySlackApp implements AppInterface {

	constructor(private req: WebRequest) {
	}

	@Handler('POST', '/api/*')
	post(command: string) {
		// @todo (maybe) construct app with logger

		const data = this.req.reqData
		const getData = function() {
			if (!data) {
				throw new Error('No data to parse!')
			}
			let dataObj: any
			try {
				dataObj = JSON.parse(data)
			}
			catch (exc) {
				console.log('Non-JSON sent to dummy-slack: ', data)
				return { error: "Non-JSON sent to dummy-slack" }
			}
			return { dataObj }
		}

		if (command === "conversations.invite") {
			return { ok: true }
		}
		else if (command === "conversations.open") {
			const dataResult = getData()
			if (dataResult.error) {
				return dataResult
			}
			const dataObj = dataResult.dataObj
			return { ok: true, channel: {id: `${dataObj.users}`} }
		}
		else if (command === "chat.update") {
			// Not going to handle updating the stored messages on the dummy server
			// until we have a reason to do so 
			return { ok: true }
		}
		else if (command === "chat.postMessage") {
			if (!data) {
				throw new Error('Nothing to post!')
			}
			const dataResult = getData()
			if (dataResult.error) {
				return dataResult
			}
			const dataObj = dataResult.dataObj

			if (dataObj.thread_ts) {
				let dummyThread = threads.get(dataObj.channel)?.get(dataObj.thread_ts)!
				dummyThread.messages.push(dataObj.attachments[0].text)
				return { ok: true }
			}
			else {
				let cl = dataObj.cl
				let target = dataObj.target
				if (cl && target) {

					const dummyThread = new DummyThread
					dummyThread.CL = cl
					dummyThread.target = target
					dummyThread.timestamp = Date.now()
					dummyThread.messages = [dataObj.attachments[0].text]

					setDefault(
						setDefault(posted, dataObj.channel, new Map),
						dummyThread.target, []
					).push(dummyThread)

					setDefault(threads, dataObj.channel, new Map)
						.set(dummyThread.timestamp, dummyThread)

					return { ok: true, ts: dummyThread.timestamp }
				}
				else {
					console.log('dummy-slack POST: cl or edge not found', command, data)
					return { error: "cl or edge not found"}
				}
			}
		}
		else {
			console.error(`dummy-slack POST: unsupported command '${command}'`)
			return { error: "unsupported command"}
		}
	}

	@Handler('GET', '/posted/*')
	channelMessages(channel: string) {
		return [...(posted.get(channel) || [])]
	}

	@Handler('GET', '/api/*')
	get(command: string) {
		if (command.startsWith("users.lookupByEmail"))
		{
			return { ok: true, user: { id: `${this.req.url.searchParams.get('email')}` } }
		}
		return { ok: false }
	}

}
// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Util, RobomergeBranchSpec, Stream } from '../framework'

export const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Release', streamType: 'release', parent: 'Main'}
]

const TEXT_FILENAME = 'test.txt'
export class Ignore extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		await P4Util.addFileAndSubmit(this.getClient('Main'), TEXT_FILENAME, 'Initial content')

		await this.p4.populate(this.getStreamPath('Release'), 'Initial branch of files from Main')
		await this.getClient('Release').sync()
	}

	run() {
		return P4Util.editFileAndSubmit(this.getClient('Release'), TEXT_FILENAME, 'change', 'deadend')
	}

	verify() {
		return Promise.all([
			this.ensureNotBlocked('Main'),
			this.ensureNotBlocked('Release'),
			this.checkHeadRevision('Main', TEXT_FILENAME, 1)
		])
	}

	allowSyntaxErrors() {
		return true
	}

	getBranches() {
		return [
			this.branch('Main', []),
			this.branch('Release', ['Main'])
		]
	}

	private branch(stream: string, to: string[]): RobomergeBranchSpec {
		return {
			streamDepot: this.constructor.name,
			name: this.fullBranchName(stream),
			streamName: stream,
			flowsTo: to.map(str => this.fullBranchName(str)),
			forceAll: true
		}
	}
}

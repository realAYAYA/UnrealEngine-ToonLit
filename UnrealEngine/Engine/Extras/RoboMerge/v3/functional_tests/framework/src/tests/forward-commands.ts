// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Client, P4Util, RobomergeBranchSpec, Stream } from '../framework'
import { Change, Perforce } from '../test-perforce'

const DEPOT_NAME = 'ForwardCommands'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Release', streamType: 'release', parent: 'Main'},
	{name: 'Release2', streamType: 'release', parent: 'Release'}
]

let fc2: ForwardCommands2 | null = null

function makeBranch(test: FunctionalTest, stream: string, to: string[], force?: string[]): RobomergeBranchSpec {
	const name = test.fullBranchName(stream)
	return {
		streamDepot: DEPOT_NAME,
		name,
		streamName: stream,
		flowsTo: to.map(str => test.fullBranchName(str)),
		forceFlowTo: (force || []).map(str => test.fullBranchName(str))
	}
}

const TEXT_FILENAME = 'test.txt'
export class ForwardCommands extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams, DEPOT_NAME)

		this.mainClient = this.getClientForStream('Main')

		await P4Util.addFileAndSubmit(this.mainClient, TEXT_FILENAME, 'Initial content')
		await this.p4.populate(this.getStreamPath('Release', DEPOT_NAME), 'Initial branch of files from Main')
		await this.p4.populate(this.getStreamPath('Release2', DEPOT_NAME), 'Initial branch of files from Release')
	}

	run() {

		const releaseClient = this.getClientForStream('Release')
		return releaseClient.sync()
		.then(() => P4Util.editFile(releaseClient, TEXT_FILENAME, 'Initial content\n\nMergeable'))
		.then(() => P4Util.submit(releaseClient, `Edit with command\n#robomerge[${fc2!.botName}] ${fc2!.fullBranchName('Release2')}`))
	}

	async verify() {
		let description: string | null = null
		await Promise.all([
			this.checkHeadRevision('Main', TEXT_FILENAME, 2),
			this.mainClient.changes(1)
				.then((changes: Change[]) => description = changes[0]!.description)
		])

		if (description!.indexOf(`[${fc2!.botName}]`) >= 0) {
			throw new Error('forwarded command sent to wrong bot')
		}
	}

	getBranches(): RobomergeBranchSpec[] {
		return [
			makeBranch(this, 'Main', ['Release']),
			makeBranch(this, 'Release', ['Main'], ['Main'])
		]
	}

	private getClientForStream(stream: string) {
		return this.getClient(stream, 'testuser1', DEPOT_NAME)
	}

	private mainClient: P4Client
}

export class ForwardCommands2 extends FunctionalTest {
	constructor(p4: Perforce) {
		super(p4)
		fc2 = this
	}

	async setup() {
		// register a client for parts of the framework that rely on it
		this.p4Client('testuser1', 'Release2', DEPOT_NAME)
	}

	async run() {
	}

	verify() {
		return this.checkHeadRevision('Release2', TEXT_FILENAME, 2, DEPOT_NAME)
	}

	getBranches(): RobomergeBranchSpec[] {
		return [
			makeBranch(this, 'Release', ['Release2']),
			makeBranch(this, 'Release2', [])
		]
	}
}

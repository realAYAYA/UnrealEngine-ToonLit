// Copyright Epic Games, Inc. All Rights Reserved.

import { EdgeProperties, FunctionalTest, P4Util, RobomergeBranchSpec, Stream } from '../framework'
import { Perforce } from '../test-perforce'

const DEPOT_NAME = 'CrossBot'


// @todo make an auto merge target and make sure ~blah works cross bot
const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline', depotName: DEPOT_NAME},
	{name: 'Release', streamType: 'release', parent: 'Main', depotName: DEPOT_NAME},
	{name: 'Dev', streamType: 'development', parent: 'Main', depotName: DEPOT_NAME},


	{name: 'RES', streamType: 'release', parent: 'Main', depotName: DEPOT_NAME},
	{name: 'R26', streamType: 'release', parent: 'RES', depotName: DEPOT_NAME},
	{name: 'Plus', streamType: 'release', parent: 'R26', depotName: DEPOT_NAME},
	{name: 'R25', streamType: 'release', parent: 'Plus', depotName: DEPOT_NAME},
	{name: 'R25Dev', streamType: 'release', parent: 'Plus', depotName: DEPOT_NAME},
	{name: 'DEM', streamType: 'release', parent: 'RES', depotName: DEPOT_NAME},
	{name: 'FNM', streamType: 'release', parent: 'DEM', depotName: DEPOT_NAME},
]

let streamsPromise: Promise<void> | null = null
function createStreams(p4: Perforce, test: FunctionalTest) {
	if (streamsPromise) {
		// add clients to the isRoboMergeIdle list
		return streamsPromise.then(
			() => Promise.all(['Main', 'RES', 'FNM', 'Plus', 'R26', 'R25', 'R25Dev'].map(s => {
				const client = test.p4Client('testuser1', s, DEPOT_NAME)
				return client.create(P4Util.specForClient(client))
			}))
		)
	}
	else {
		streamsPromise =
			(async () => {
				await p4.depot('stream', test.depotSpec(DEPOT_NAME))
				await test.createStreamsAndWorkspaces(streams, DEPOT_NAME)

				const mainClient = test.getClient('Main', 'testuser1', DEPOT_NAME)
				for (const n of [1, 2, 3, 4, 5, 6, 7, 8]) {
					await P4Util.addFileAndSubmit(mainClient, `test${n}.txt`, 'Initial content')
				}


				await Promise.all([
					p4.populate(test.getStreamPath('Release', DEPOT_NAME), 'Initial branch of files from Main'),
					p4.populate(test.getStreamPath('Dev', DEPOT_NAME), 'Initial branch of files from Main'),

					p4.populate(test.getStreamPath('RES', DEPOT_NAME), 'Initial branch of files from Main')
						.then(() => Promise.all([
							p4.populate(test.getStreamPath('R26', DEPOT_NAME), 'Initial branch of files from RES')
								.then(() => p4.populate(test.getStreamPath('Plus', DEPOT_NAME), 'Initial branch of files from R26'))
								.then(() => Promise.all([
									p4.populate(test.getStreamPath('R25', DEPOT_NAME), 'Initial branch of files from Plus'),
									p4.populate(test.getStreamPath('R25Dev', DEPOT_NAME), 'Initial branch of files from Plus')
								])),

							p4.populate(test.getStreamPath('DEM', DEPOT_NAME), 'Initial branch of files from RES')
								.then(() => p4.populate(test.getStreamPath('FNM', DEPOT_NAME), 'Initial branch of files from DEM'))
						]))
				])

			})()
	}

	return streamsPromise
}

let crossBot2: CrossBotTest2 | null = null
export class CrossBotTest extends FunctionalTest {
	async setup() {
		await createStreams(this.p4, this)
		await this.getClientForStream('Release').sync()
	}

	async run() {
		const releaseClient = this.getClientForStream('Release')

		// integrate to 'dev' (different bot)
		await P4Util.editFile(releaseClient, 'test1.txt', 'Initial content\n\nMergeable line')
		await P4Util.submit(releaseClient, `Edit\n#robomerge[${crossBot2!.botName}] dev`)

		await P4Util.editFile(releaseClient, 'test3.txt', 'Initial content\n\nMergeable line')
		await P4Util.submit(releaseClient, `Edit\n#robomerge[${crossBot2!.botName}-alias] dev`)

// not testing invalid bot here - more important to get default syntax error testing
		// await P4Util.editFile(releaseClient, 'test2.txt', 'Initial content\n\nMergeable line')
		// await P4Util.submit(releaseClient, `Edit\n#robomerge[InvalidBot] dev-alias`)
	}

	verify() {
		return Promise.all([
			this.ensureNotBlocked('Release'),
			...[1, 3].map(n => this.checkHeadRevision('Main', `test${n}.txt`, 2, DEPOT_NAME))
		])
	}

	getBranches() {
		const releaseStream = this.makeForceAllBranchDef('Release', ['Main'])
		releaseStream.incognitoMode = true
		return [
			this.makeForceAllBranchDef('Main', []),
			releaseStream
		]
	}

	protected makeForceAllBranchDef(stream: string, to: string[]): RobomergeBranchSpec {
		return {
			streamDepot: DEPOT_NAME,
			name: this.fullBranchName(stream),
			streamName: stream,
			flowsTo: to.map(str => this.fullBranchName(str)),
			forceAll: true
		}
	}

	private getClientForStream(stream: string) {
		return this.getClient(stream, 'testuser1', DEPOT_NAME)
	}
}

// assuming this comes after CrossBotTest in the tests list, and that that
// means they live in different bots
export class CrossBotTest2 extends FunctionalTest {
	async setup() {
		crossBot2 = this

		await createStreams(this.p4, this)
	}

	async run() {}

	verify() {
		return Promise.all([
			this.ensureNotBlocked('Main', 'Dev'),
			this.checkHeadRevision('Release', 'test1.txt', 2, DEPOT_NAME),
			// this.checkHeadRevision('Release', 'test2.txt', 2, DEPOT_NAME),
			this.checkHeadRevision('Dev', 'test1.txt', 2, DEPOT_NAME),
			// this.checkHeadRevision('Dev', 'test2.txt', 1, DEPOT_NAME), // unrelated bot name
			this.checkHeadRevision('Dev', 'test3.txt', 2, DEPOT_NAME)  // alias
		])
	}

	getBranches() {
		const devStream = this.makeBranchDef('Dev', [])
		devStream.aliases = ['dev']
		return [
			this.makeBranchDef('Main', ['Dev']),
			devStream
		]
	}

	protected makeBranchDef(stream: string, to: string[]): RobomergeBranchSpec {
		return {
			streamDepot: DEPOT_NAME,
			name: this.fullBranchName(stream),
			streamName: stream,
			flowsTo: to.map(str => this.fullBranchName(str))
		}
	}
}


abstract class ComplexBase extends FunctionalTest {
	protected makeBranch(stream: string, to: string[], force: string[]): RobomergeBranchSpec {
		const name = this.fullBranchName(stream)
		return {
			streamDepot: DEPOT_NAME,
			name,
			aliases: [stream],
			streamName: stream,
			flowsTo: to.map(str => this.fullBranchName(str)),
			forceFlowTo: force.map(str => this.fullBranchName(str))
		}
	}
}


let complexBot: ComplexCrossBot | null = null
let complexBot2: ComplexCrossBot2 | null = null
let complexBot3: ComplexCrossBot3 | null = null

function getNextgenCommands() {
	return [
		`#robomerge[${complexBot3!.botName}] Plus -R26`,
		`#robomerge[${complexBot2!.botName}] R26`
	]
}

export class ComplexCrossBot extends ComplexBase {
	test8EditRev = -1

	constructor(p4: Perforce) {
		super(p4)
		complexBot = this
	}

	async setup() {
		await createStreams(this.p4, this)

	}

	async run() {
		const client = this.getClient('FNM', 'testuser1', DEPOT_NAME)

		await this.getClient('FNM', 'testuser1', DEPOT_NAME).sync()

		await P4Util.editFile(client, 'test4.txt', 'Initial content\n\nMergeable line')
		await P4Util.submit(client, `Edit\n#robomerge nextgen`)

		// using macro in reconsider
		this.test8EditRev = await P4Util.editFileAndSubmit(client, 'test8.txt', 'Initial content\n\nMergeable line', 'ignore')

		// using macro
		await P4Util.editFile(client, 'test5.txt', 'Initial content\n\nMergeable line')
		await P4Util.submit(client, 'Edit\n' + getNextgenCommands().join('\n'))

		await this.reconsider('FNM', this.test8EditRev, undefined, '#robomerge nextgen, DEM')
	}

	verify() {
		return Promise.all([
			this.checkHeadRevision('R26', 'test4.txt', 2, DEPOT_NAME),
			this.checkHeadRevision('Plus', 'test4.txt', 2, DEPOT_NAME),
			this.checkHeadRevision('R26', 'test8.txt', 2, DEPOT_NAME),
			this.checkHeadRevision('Plus', 'test8.txt', 2, DEPOT_NAME),
			this.checkHeadRevision('R26', 'test5.txt', 2, DEPOT_NAME),
			this.checkHeadRevision('Plus', 'test5.txt', 2, DEPOT_NAME),
			this.checkHeadRevision('FNM', 'test7.txt', 2, DEPOT_NAME),
			this.checkHeadRevision('RES', 'test7.txt', 2, DEPOT_NAME)
		])
	}

	getBranches() {
		return [
			this.makeBranch('RES', ['DEM'], []),
			this.makeBranch('DEM', ['RES', 'FNM'], ['RES']),
			this.makeBranch('FNM', ['DEM'], ['DEM']),
			this.makeBranch('Plus', ['DEM'], []),
		]
	}

	getMacros() {
		return {
			'nextgen': getNextgenCommands()
		}
	}
}

// release
export class ComplexCrossBot2 extends ComplexBase {
	constructor(p4: Perforce) {
		super(p4)
		complexBot2 = this
	}

	async setup() {
		await createStreams(this.p4, this)

		await this.getClient('R25', 'testuser1', DEPOT_NAME).sync()
	}

	async run() {
		const client = this.getClient('R25', 'testuser1', DEPOT_NAME)
		await P4Util.editFile(client, 'test6.txt', 'Initial content\n\nMergeable line')
		await P4Util.submit(client, `Edit\n#robomerge[${complexBot!.botName}] FNM -RES`)
	}

	verify() {
		return this.checkHeadRevision('FNM', 'test6.txt', 2, DEPOT_NAME)
	}

	getBranches() {
		return [
			this.makeBranch('Main', ['RES'], []),
			this.makeBranch('R26', ['RES'], ['RES']),
			this.makeBranch('RES', ['R26', 'Main'], ['Main'])
		]
	}
}

export class ComplexCrossBot3 extends ComplexBase {
	constructor(p4: Perforce) {
		super(p4)
		complexBot3 = this
	}

	async setup() {

		await createStreams(this.p4, this)
	}

	async run() {
		const client = this.getClient('R25Dev', 'testuser1', DEPOT_NAME)
		await client.sync()
		await P4Util.editFileAndSubmit(client, 'test7.txt', 'Initial content\n\nMergeable line', 'nextgen Plus')
	}

	getBranches() {
		return [
			this.makeBranch('Plus', ['R26', 'R25', 'R25Dev'], ['R26', 'R25Dev']),
			this.makeBranch('R25Dev', ['Plus'], []),
			this.makeBranch('R25', ['Plus'], ['Plus']),
			this.makeBranch('R26', [], []),
			this.makeBranch('DEM', ['Plus'], [])
		]
	}

	getEdges(): EdgeProperties[] {
		return [
		  { from: this.fullBranchName('DEM'), to: this.fullBranchName('Plus')
		  , incognitoMode: true
		  }
		]
	}

	async verify() {
	}

	getMacros() {
		return {
			'nextgen': [`#robomerge[${complexBot!.botName}] FNM, -RES`]
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, getRootDataClient, P4Client, P4Util, Stream } from '../framework'
import * as system from '../system'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Dev-NoGate', streamType: 'development', parent: 'Main'},
	{name: 'Dev-PlusOne', streamType: 'development', parent: 'Main'},
	{name: 'Dev-Exact', streamType: 'development', parent: 'Main'},
	{name: 'Dev-Queue', streamType: 'development', parent: 'Main'},
	{name: 'Dev-Window', streamType: 'development', parent: 'Main'}
]

const GATE_FILENAME = 'TestEdgeGate-gate'

export class TestEdgeGate extends FunctionalTest {
	gateClient: P4Client

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)
	
		this.gateClient = await getRootDataClient(this.p4, 'RoboMergeData_' + this.constructor.name)

		const mainClient = this.getClient('Main', 'testuser1')
		await P4Util.addFileAndSubmit(mainClient, 'test.txt', 'Initial content')

		const desc = 'Initial branch of files from Main'
		await Promise.all(
			['Exact', 'PlusOne', 'NoGate', 'Queue', 'Window'].map(s => this.p4.populate(this.getStreamPath('Dev-' + s), desc))
		)

		const firstEditCl = await P4Util.editFileAndSubmit(mainClient, 'test.txt', 'Initial content\n\nFirst addition')

		// must happen in set-up, and for this test, before second addition (i.e. earlier CL numbers)
		await Promise.all([
			P4Util.addFile(this.gateClient, GATE_FILENAME + 'exact.json', JSON.stringify({Change:firstEditCl})),
			P4Util.addFile(this.gateClient, GATE_FILENAME + 'plusone.json', JSON.stringify({Change:firstEditCl + 1})),
			P4Util.addFile(this.gateClient, GATE_FILENAME + 'queue.json', JSON.stringify({Change:firstEditCl, integrationWindow: []})),
			P4Util.addFile(this.gateClient, GATE_FILENAME + 'window.json', JSON.stringify({Change:firstEditCl, integrationWindow: []}))
		])

		await this.gateClient.submit('Added gates')
		const contentsWithTwoAdditions = 'Initial content\n\nFirst addition\n\nSecond addition'
		this.secondEditCl = await P4Util.editFileAndSubmit(mainClient, 'test.txt', contentsWithTwoAdditions)
		await P4Util.editFileAndSubmit(mainClient, 'test.txt', contentsWithTwoAdditions + '\n\nThird addition')
	}

	secondEditCl = -1

	async run() {
		await this.waitForRobomergeIdle()
		await this.checkHeadRevision('Dev-Queue', 'test.txt', 1)
		await P4Util.editFileAndSubmit(this.gateClient, GATE_FILENAME + 'queue.json', JSON.stringify({Change:this.secondEditCl}))
		await P4Util.editFileAndSubmit(this.gateClient, GATE_FILENAME + 'window.json', JSON.stringify({Change:this.secondEditCl, integrationWindow: [{
			startHourUTC: ((new Date).getUTCHours() + 12) % 24,
			durationHours: 1
		}]}))
	}

	verify() {
		this.info('sleeping after removing window')
		return Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 4),
			this.checkHeadRevision('Dev-Exact', 'test.txt', 2),
			this.checkHeadRevision('Dev-PlusOne', 'test.txt', 2),
			system.sleep(10).then(() => this.checkHeadRevision('Dev-Queue', 'test.txt', 3)),
			this.checkHeadRevision('Dev-NoGate', 'test.txt', 4),
			system.sleep(10).then(() => this.checkHeadRevision('Dev-Window', 'test.txt', 1))
		])
	}

	getBranches() {
		const mainSpec = this.makeForceAllBranchDef('Main', ['Dev-Exact', 'Dev-NoGate', 'Dev-PlusOne', 'Dev-Queue', 'Dev-Window'])
		mainSpec.initialCL = 1
		return [
			mainSpec,
			this.makeForceAllBranchDef('Dev-Exact', []),
			this.makeForceAllBranchDef('Dev-PlusOne', []),
			this.makeForceAllBranchDef('Dev-NoGate', []),
			this.makeForceAllBranchDef('Dev-Queue', []),
			this.makeForceAllBranchDef('Dev-Window', []),
		]
	}

	getEdges() {
		return ['Exact', 'PlusOne', 'Queue', 'Window'].map(s =>
		  ({ from: this.fullBranchName('Main'), to: this.fullBranchName('Dev-' + s)
		  , lastGoodCLPath: this.gateClient.stream + '/' + GATE_FILENAME + s.toLowerCase() + '.json'
		  , initialCL: 1
		  }))
	}
}

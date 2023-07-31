// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, getRootDataClient, P4Client, P4Util, RobomergeBranchSpec, Stream } from '../framework'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Dev-Pootle', streamType: 'development', parent: 'Main'},
	{name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
]

const GATE_FILENAME = 'TestGate-gate.json'


export class TestGate extends FunctionalTest {
	gateClient: P4Client
	mainSpec: RobomergeBranchSpec

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)
	
		this.gateClient = await getRootDataClient(this.p4, 'RoboMergeData_' + this.constructor.name)
		this.mainSpec = this.makeForceAllBranchDef('Main', ['Dev-Perkin', 'Dev-Pootle'])
		this.mainSpec.lastGoodCLPath = this.gateClient.stream + '/' + GATE_FILENAME
		this.mainSpec.initialCL = 1

		await P4Util.addFileAndSubmit(this.getClient('Main', 'testuser1'), 'test.txt', 'Initial content')

		const desc = 'Initial branch of files from Main'
		await Promise.all([
			this.p4.populate(this.getStreamPath('Dev-Perkin'), desc),
			this.p4.populate(this.getStreamPath('Dev-Pootle'), desc)
		])

		const mainClient = this.getClient('Main', 'testuser1')
		const firstEditCl = await P4Util.editFileAndSubmit(mainClient, 'test.txt', 'Initial content\n\nFirst addition')

		// must happen in set-up!
		await Promise.all([
			P4Util.editFileAndSubmit(mainClient, 'test.txt', 'Initial content\n\nFirst addition\n\nSecond addition'),
			P4Util.addFileAndSubmit(this.gateClient, GATE_FILENAME, `{"Change":${firstEditCl}}`)
		])
	}

	async run() {
	}

	verify() {
		return Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 3),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 2)
		])
	}

	getBranches() {
		return [
			this.mainSpec,
			this.makeForceAllBranchDef('Dev-Perkin', []),
			this.makeForceAllBranchDef('Dev-Pootle', [])
		]
	}
}

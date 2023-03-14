// Copyright Epic Games, Inc. All Rights Reserved.

import { EdgeProperties, FunctionalTest, P4Util, Stream } from '../framework'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Producer1', streamType: 'release', parent: 'Main'},
	{name: 'Producer2', streamType: 'release', parent: 'Main'},
	{name: 'Consumer', streamType: 'development', parent: 'Main'}
]


export class ImplicitCommands extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test1.txt', 'Initial content')
		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test2.txt', 'Initial content')

		const desc = 'Initial branch of files from Main'
		await Promise.all([
			this.p4.populate(this.getStreamPath('Producer1'), desc),
			this.p4.populate(this.getStreamPath('Producer2'), desc),
			this.p4.populate(this.getStreamPath('Consumer'), desc)
		])

		await Promise.all(['Producer1', 'Producer2'].map(
			branch => this.getClient(branch).sync()
		))
	}

	run() {
		return Promise.all([
			P4Util.editFileAndSubmit(this.getClient('Producer1'), 'test1.txt', 'new content'),
			P4Util.editFileAndSubmit(this.getClient('Producer2'), 'test2.txt', 'new content')
		])
	}

	verify() {
		return Promise.all([
			this.checkHeadRevision('Main', 'test1.txt', 2), // main should have both changes
			this.checkHeadRevision('Main', 'test2.txt', 2),
			this.checkHeadRevision('Producer1', 'test1.txt', 2),
			this.checkHeadRevision('Producer2', 'test2.txt', 2),
			this.checkHeadRevision('Consumer', 'test1.txt', 1), // consumer should only have test2 change
			this.checkHeadRevision('Consumer', 'test2.txt', 2)
		])
	}

	getBranches() {
		return [
			this.makeForceAllBranchDef('Main', ['Consumer']),
			this.makeForceAllBranchDef('Producer1', ['Main']),
			this.makeForceAllBranchDef('Producer2', ['Main']),
			this.makeForceAllBranchDef('Consumer', [])
		]
	}

	getEdges(): EdgeProperties[] {
		return [
		  { from: this.fullBranchName('Producer1'), to: this.fullBranchName('Main')
		  , implicitCommands: ['#robomerge -' + this.fullBranchName('Consumer')]
		  }
		]
	}

}

// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Util, Stream } from '../framework'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Dev-Pootle', streamType: 'development', parent: 'Main'},
	{name: 'Dev-Perkin', streamType: 'development', parent: 'Main'},
	{name: 'Dev-Posie', streamType: 'development', parent: 'Main'}
]

export class TestEdgeReconsider extends FunctionalTest {
	revisionCl = -1
	conflictingRevisionCl = -1

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		const mainClient = this.getClient('Main', 'testuser1')
		await P4Util.addFileAndSubmit(mainClient, 'test.txt', 'Initial content')

		const desc = 'Initial branch of files from Main'
		await Promise.all([
			this.p4.populate(this.getStreamPath('Dev-Perkin'), desc),
			this.p4.populate(this.getStreamPath('Dev-Pootle'), desc),
			this.p4.populate(this.getStreamPath('Dev-Posie'), desc)
		])

		// making edit in set-up, i.e. before added to RoboMerge
		this.revisionCl = await P4Util.editFileAndSubmit(mainClient, 'test.txt', 'Initial content\n\nFirst addition')
		this.conflictingRevisionCl = await P4Util.editFileAndSubmit(mainClient, 'test.txt', 'Will conflict')
	}

	run() {
		return Promise.all([
			this.reconsider('Main', this.revisionCl, 'Dev-Perkin'),
			this.reconsider('Main', this.conflictingRevisionCl, 'Dev-Posie')
		])

			// this.reconsider('Main', this.revisionCl, 'Dev-Perkin')
			// .then(() => this.reconsider('Main', this.conflictingRevisionCl, 'Dev-Posie'))
	}

	verify() {
		return Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 3),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 1), // edge not reconsidered
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 2),
			this.checkHeadRevision('Dev-Posie', 'test.txt', 1) // conflicting reconsider
		])
	}

	getBranches() {
		return [
			this.makeForceAllBranchDef('Main', ['Dev-Perkin', 'Dev-Pootle', 'Dev-Posie']),
			this.makeForceAllBranchDef('Dev-Perkin', []),
			this.makeForceAllBranchDef('Dev-Pootle', []),
			this.makeForceAllBranchDef('Dev-Posie', [])
		]
	}
}

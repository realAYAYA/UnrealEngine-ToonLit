// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Util, Stream } from '../framework'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Dev-Pootle', streamType: 'development', parent: 'Main'},
	{name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
]


// write test so we can eventually test edge independence
export class BlockAssets extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.uasset', 'Initial content', true)

		const desc = 'Initial branch of files from Main'
		await Promise.all([
			this.p4.populate(this.getStreamPath('Dev-Perkin'), desc),
			this.p4.populate(this.getStreamPath('Dev-Pootle'), desc)
		])
	}

	run() {
		return P4Util.editFileAndSubmit(this.getClient('Main'), 'test.uasset', 'New content')
	}	

	verify() {
		// situation before fixes:
		//	- block asset on any target treated as syntax error, so blocks all
		return Promise.all([
			this.checkHeadRevision('Main', 'test.uasset', 2),
			this.checkHeadRevision('Dev-Perkin', 'test.uasset', 1),


// current state
	
/*
	this.ensureBlocked('Main'),
/*/
// once fixed:

			this.checkHeadRevision('Dev-Pootle', 'test.uasset', 2),
			this.ensureBlocked('Main', 'Dev-Perkin'),
			this.ensureNotBlocked('Main', 'Dev-Pootle')
/**/
		])
	}

	getBranches() {
		const mainDef = this.makeForceAllBranchDef('Main', ['Dev-Perkin', 'Dev-Pootle'])
		mainDef.blockAssetFlow = [this.fullBranchName('Dev-Perkin')]
		return [
			mainDef,
			this.makeForceAllBranchDef('Dev-Perkin', []),
			this.makeForceAllBranchDef('Dev-Pootle', [])
		]
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Util } from '../framework'
import { SimpleMainAndReleaseTestBase, streams } from '../SimpleMainAndReleaseTestBase'

// lines 
//  A
//  B
//  C
//  E


// remove - skip this removal
//  A
//  C
//  E

// add
//  A
//  (B)
//  C
//	D
//  E

export class ResolveAfterSkip extends SimpleMainAndReleaseTestBase {

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)
		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.txt', 'A\nB\nC\nE\n')
		await this.initialPopulate()
	}

	async run() {
		const releaseClient = this.getClient('Release')
		await releaseClient.sync()
		await P4Util.editFileAndSubmit(releaseClient, 'test.txt', 'A\nC\nE\n', 'ignore')
		await P4Util.editFileAndSubmit(releaseClient, 'test.txt', 'A\nC\nD\nE\n')
	}

	async verify() {
		const content = await this.getClient('Main').print('test.txt')
		const expected = 'A\nB\nC\nD\nE\n' 

		this.info('Ensuring expected content after merge')
		if (content !== expected) {
			throw new Error(`Expected: ${expected} got ${content}`)
		}
	}
}


// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Client, P4Util } from '../framework'
import { SimpleMainAndReleaseTestBase, streams } from '../SimpleMainAndReleaseTestBase'

export class RequestShelf extends SimpleMainAndReleaseTestBase {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		this.mainClient = this.getClient('Main')

		await P4Util.addFileAndSubmit(this.mainClient, 'test.txt', 'Initial content')

		await this.initialPopulate()
	}

	async run() {
		
		const releaseClient = this.getClient('Release')

		await Promise.all([
			P4Util.editFileAndSubmit(this.mainClient, 'test.txt', 'Main edit'),
			releaseClient.sync().then(() => P4Util.editFileAndSubmit(releaseClient, 'test.txt', 'Release edit'))
		])

		await this.waitForRobomergeIdle()
		this.shelfCl = await this.createShelf('Release', 'Main')

		if (typeof(this.shelfCl) !== 'number') {
			throw new Error(`Did not receive a shelf CL from shelf attempt -- ${this.shelfCl}`)
		}
	}

	async verify() {
		await this.mainClient.unshelve(this.shelfCl)
		await Promise.all([
			this.mainClient.resolve(this.shelfCl, true), // clobber
			this.mainClient.deleteShelved(this.shelfCl)
		])
		await this.mainClient.submit(this.shelfCl)

		await Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 3),
			this.checkHeadRevision('Release', 'test.txt', 2)
		])
	}

	private mainClient: P4Client
	private shelfCl: number
}

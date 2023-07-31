// Copyright Epic Games, Inc. All Rights Reserved.
import { P4Client, P4Util } from '../framework'
import { MultipleDevAndReleaseTestBase } from '../MultipleDevAndReleaseTestBase'

export class RequestShelfIndirectTarget extends MultipleDevAndReleaseTestBase {

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces()

		this.r2client = this.getClient('Release-2.0')

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.txt', 'Initial content')

		await this.initialPopulate()
	}

	async run() {
		const r1client = this.getClient('Release-1.0')

		await Promise.all([
			r1client.sync().then(() => P4Util.editFileAndSubmit(r1client, 'test.txt', '1.0 edit', this.fullBranchName('Dev-Perkin'))),
			this.r2client.sync().then(() => P4Util.editFileAndSubmit(this.r2client, 'test.txt', '2.0 edit'))
		])

		await this.waitForRobomergeIdle()
		this.shelfCl = await this.createShelf('Release-1.0', 'Release-2.0')

		if (typeof this.shelfCl !== 'number') {
			throw new Error(`Did not receive a shelf CL from shelf attempt -- ${this.shelfCl}`)
		}
	}

	async verify() {
		await this.r2client.unshelve(this.shelfCl)
		await Promise.all([
			this.r2client.resolve(this.shelfCl, true), // clobber
			this.r2client.deleteShelved(this.shelfCl)
		])
		await this.r2client.submit(this.shelfCl)

		await this.waitForRobomergeIdle()

		await Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 2),
			this.checkHeadRevision('Release-2.0', 'test.txt', 3),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 1)
		])
	}

	allForceFlow() {
		return false
	}

	private r2client: P4Client
	private shelfCl: number
}

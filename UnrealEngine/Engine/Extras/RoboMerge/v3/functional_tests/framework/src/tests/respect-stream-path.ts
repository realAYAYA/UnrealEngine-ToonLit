// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Client, P4Util, Stream } from '../framework'
import { Perforce } from '../test-perforce'

export class RespectStreamPath extends FunctionalTest {

	private streams: {[key: string]: Stream} = {
		'Main': {name: 'Main', streamType: 'mainline'},
		'Development': {name: 'Development', streamType: 'development', 
			paths: ['isolate ToBeIsolated.isolate', 'exclude ToBeExcluded.exclude' ],
			ignored: ['.ignore'],
			parent: 'Main'
		}
	}

	private mainClient: P4Client
	private developmentClient: P4Client

	constructor(p4: Perforce) {
		super(p4)
	}

	getBranches() {
		return [{
			streamDepot: this.testName,
			name: this.testName + 'Main',
			streamName: 'Main',
			flowsTo: [this.testName + 'Development'],
			forceAll: true
		}, {
			streamDepot: this.testName,
			name: this.testName + 'Development',
			streamName: 'Development'
		}]
	}

	async setup() {
		// Set up depot
		await this.p4.depot('stream', this.depotSpec())

		// Setup streams
		await this.p4.stream(this.streamSpec(this.streams['Main']))
		await this.p4.stream(this.streamSpec(this.streams['Development']))

		this.mainClient = this.p4Client('testuser1', 'Main')
		this.developmentClient = this.p4Client('testuser1', 'Development')

		// Create workspaces
		await this.mainClient.create(P4Util.specForClient(this.mainClient))
		await this.developmentClient.create(P4Util.specForClient(this.developmentClient))

		// Create files
		await Promise.all([
			P4Util.addFile(this.mainClient, 'textfile.txt', 'Simple Functional Test Text File'),
			P4Util.addFile(this.mainClient, 'ToBeIsolated.isolate', 'Simple Functional Test Isolation File'),
			P4Util.addFile(this.mainClient, 'ToBeExcluded.exclude', 'Simple Functional Test Exclusion File'),
			P4Util.addFile(this.mainClient, 'ToBeIgnored.ignore', 'Simple Functional Test Ignored File')
		])
		await P4Util.submit(this.mainClient, "Adding Initial Files")

		// Populate Dev Stream
		await this.p4.populate(this.getStreamPath(this.streams['Development']), 'Initial population')

		// Add isolated file to dev -- this should go through without error if the spec is correct
		await P4Util.addFile(this.developmentClient, 'ToBeIsolated.isolate', 'Adding isolated .isolate file to Development')
		await P4Util.submit(this.developmentClient, 'Submitting isolated file in Development')
	}

	async run() {
		await Promise.all([
			// This file should make it to Dev
			P4Util.editFile(this.mainClient, 'textfile.txt', 'Unit Test File Rev. #2'),
			// The next three files should not be integrated, regardless of their presence in the CL
			P4Util.editFile(this.mainClient, 'ToBeIsolated.isolate', 'Isolation File Rev. #2'),
			P4Util.editFile(this.mainClient, 'ToBeExcluded.exclude', 'Exclusion File Rev. #2'),
			P4Util.editFile(this.mainClient, 'ToBeIgnored.ignore', 'Ignored File Rev. #2')
		])

		await P4Util.submit(this.mainClient, 'Committing CL consisting of mergable and unmergable files Main->Dev')
		
		// Create new CL consisting of ONLY files not to be integrated
		await Promise.all([
			P4Util.editFile(this.mainClient, 'ToBeIsolated.isolate', 'Isolation File Rev. #3'),
			P4Util.editFile(this.mainClient, 'ToBeExcluded.exclude', 'Exclusion File Rev. #3'),
			P4Util.editFile(this.mainClient, 'ToBeIgnored.ignore', 'Ignored File Rev. #3')
		])

		// Robomerge should get a "No Such File(s)" error when resolving this CL, and handle it gracefully.
		await P4Util.submit(this.mainClient, 'Committing CL consisting of unmergable files Main->Dev')
	}

	async verify() {
		await Promise.all([
			this.checkHeadRevision('Development', 'textfile.txt', 2),
			this.checkHeadRevision('Development', 'ToBeIsolated.isolate', 1),
			this.checkHeadRevision('Development', 'ToBeExcluded.exclude', 0),
			this.checkHeadRevision('Development', 'ToBeIgnored.ignore', 0)
		])
	}
}

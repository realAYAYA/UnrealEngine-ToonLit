import { FunctionalTest, P4Util, RobomergeBranchSpec, Stream } from './framework'
import { Perforce } from './test-perforce'


// testDef example
//    command | start |        edges         | expected
// [['c:d',		'a',    'bd', 'c', '', 'c'],	'B:C']

export class GenericTargetTest extends FunctionalTest {
	// private streams: Stream[] = []


	private streamMap = new Map<string, Stream>();
	private branchMap = new Map<string, RobomergeBranchSpec>();
	private source: string;
	private command: string;

	constructor(p4: Perforce, index: number, testDef: string[], private expected: string | null) {
		super(p4, `TargetTest${index}`)

		// copy code from Test constructor/computeTargets in graph.ts!
		const targetsString = testDef[0]
		this.source = testDef[1]
		const nodeStrs = testDef.slice(2)

		// name -> char (variable names same as Test.constructor)
		for (let index = 0; index < nodeStrs.length; ++index) {
			const name = String.fromCharCode('a'.charCodeAt(0) + index)
			const branch = this.addBranch(name)
			this.streamMap.set(name, index === 0
				? {name, streamType: 'mainline'}
				: {name, streamType: 'development', parent: 'a'})

			for (const char of nodeStrs[index]) {
				const targetName = char.toLowerCase()
				this.addBranch(targetName)
				branch.flowsTo!.push(this.fullBranchName(targetName))
				if (char !== targetName) {
					branch.forceFlowTo!.push(this.fullBranchName(targetName))
				}
			}
		}

		const commandBits: string[] = []
		//fillTargetsMap
		const targetBits = targetsString.split(':') // up to three section in target (1st) string: normal, skip, null
		for (const targetChar of targetBits[0]) {
			commandBits.push(this.fullBranchName(targetChar))
		}

		if (targetBits.length > 1) {
			for (const targetChar of targetBits[1]) {
				commandBits.push('-' + this.fullBranchName(targetChar))
			}
		}

		if (targetBits.length > 2) {
			for (const targetChar of targetBits[2]) {
				commandBits.push('!' + this.fullBranchName(targetChar))
			}
		}

		this.command = commandBits.join(' ')
	}

	async setup() {
		const streams = [...this.streamMap.values()]
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)
		await P4Util.addFileAndSubmit(this.getClient('a'), 'test.txt', 'initial content')

		const desc = 'Initial population'
				
		await Promise.all(streams
			.filter(s => s.name !== 'a')
			.map(s => this.p4.populate(this.getStreamPath(s.name), desc))
		)
	}

	async run() {
		const sourceClient = this.getClient(this.source)
		if (this.source !== 'a') {
			await sourceClient.sync()
		}
		await P4Util.editFileAndSubmit(sourceClient, 'test.txt', 'new content', this.command)
	}

	async verify() {
		// at the moment, checking normal and null merges create a revision
		// upper case in expected should be checking for null merge

		if (this.expected || this.expected === '') {
			return Promise.all([...this.streamMap.values()].map(s => 
				this.checkHeadRevision(s.name, 'test.txt',
					1 +
					(s.name === this.source ? 1 : 0) +
					(this.expected!.toLowerCase().indexOf(s.name) >= 0 ? 1 : 0))))
		}
		else {
			return this.ensureBlocked(this.source)
		}
	}

	getBranches() {
		return [...this.branchMap.values()]

	}

	allowSyntaxErrors() {
		return true
	}
	private addBranch(name: string): RobomergeBranchSpec {
		let branch = this.branchMap.get(name)
		if (!branch) {
			branch = this.makeBranchDef(name, [], false)
			this.branchMap.set(name, branch)
		}
		return branch
	}
}

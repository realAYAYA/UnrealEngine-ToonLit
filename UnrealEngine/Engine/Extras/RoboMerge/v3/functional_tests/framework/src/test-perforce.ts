// Copyright Epic Games, Inc. All Rights Reserved.
import { ChildProcess, execFile, ExecFileOptions } from 'child_process'

import * as ztag from './ztag'

class ExecOpts {
	stdin?: string
}

export interface Change {
	change: number
	description: string
}

export interface DescribeResult {
	file: string
	action: string
	rev: number
}

export const VERBOSE = false
export class Perforce {
	static readonly P4_EXE = process.platform === 'win32' ? 'p4.exe' : 'p4'
	static readonly REGEX_NEWLINE = /\r\n|\n|\r/g

	init() {
		return this.exec(process.platform === 'darwin' ? ['-plocalhost:1666', 'login', '-s'] : ['login', '-s'])
	}

	depot(depotType: string, spec: string)	{ return this.exec(['depot', '-t', depotType],	{stdin:spec}) }
	user(spec: string)						{ return this.exec(['user', '-fi'],				{stdin:spec}) }
	stream(spec: string)					{ return this.exec(['stream'],					{stdin:spec}) }
	client(user: string, spec: string)		{ return this.exec(['-u', user, 'client'],		{stdin:spec}) }

	add(user: string, workspace: string, path: string, binary?: boolean)
	{
		const args = ['-u', user, '-c', workspace, 'add', '-t', binary ? 'binary+Cl' : 'text', path]
		return this.exec(args)
	}

	edit(user: string, workspace: string, path: string) { return this.exec(['-u', user, '-c', workspace, 'edit', path]) }
	delete(user: string, workspace: string, path: string) { return this.exec(['-u', user, '-c', workspace, 'delete', path]) }
	sync(user: string, workspace: string) { return this.exec(['-u', user, '-c', workspace, 'sync']) }
	populate(stream: string, description: string) { return this.exec(['populate', '-S', stream, '-r', '-d', description]) }
	print(client: string, path: string) { return this.exec(['-c', client, 'print', '-q', path]) }

	unshelve(user: string, workspace: string, cl: number) {
		return this.exec(['-u', user, '-c', workspace, 'unshelve', '-s', cl.toString(), '-c', cl.toString(), '//...'])
	}

	deleteShelved(user: string, workspace: string, cl: number) {
		return this.exec(['-u', user, '-c', workspace, 'shelve', '-d', '-c', cl.toString()])
	}

	resolve(user: string, workspace: string, cl: number, clobber: boolean) {
		return this.exec(['-u', user, '-c', workspace, 'resolve', clobber ? '-at' : '-ay', '-c', cl.toString(), '//...'])
	}

	fstat(p4DepotPath: string): Promise<string>;
	fstat(client: string, localPath: string): Promise<string>;
	fstat(arg0: string, arg1?: string): Promise<string> {
		if (arg1) {
			return this.exec(['-c', arg0, 'fstat', '-T', 'headRev', arg1])
		} else {
			return this.exec(['fstat', '-T', 'headRev', arg0])
		}
	}

	// submit default changelist
	submit(user: string, workspace: string, description: string): Promise<string>

	// submit specified pending changelist
	submit(user: string, workspace: string, cl: number): Promise<string>

	submit(user: string, workspace: string, arg: string | number)
	{
		let args = ['-u', user, '-c', workspace, 'submit']
		if (typeof arg === 'string') {
			args = [...args, '-d', arg]
		}
		else {
			args = [...args, '-c', arg.toString()]
		}
		return this.exec(args)
	}

	async changes(client: string, stream: string, limit: number, pending?: boolean) {
		const ztagResult = await this.exec(['-ztag', '-c', client, 'changes', '-l', '-m', limit.toString(), '-s', pending ? 'pending' : 'submitted', stream + '/...'])

		const parseResult = ztag.parseZtagOutput(ztagResult, 0, {expected: {change: 'integer', desc: 'string'}, optional: {oldChange: 'integer'}})
		const result: Change[] = []
		for (const entry of parseResult) {
			result.push({change: entry.change as number, description: entry.desc as string})
		}
		return result
	}

	async describe(cl: number) {
		const ztagResult = await this.exec(['-ztag', 'describe', cl.toString()])
		const parseResult = ztag.parseHeaderAndArray(ztagResult, 0,
			{expected: {change: 'integer', user: 'string', client: 'string'}},
			{expected: {action: 'string', rev: 'integer', depotFile: 'string'}}
		)

		const result: DescribeResult[] = []
		for (const entry of parseResult.slice(1)) {
			result.push({file: entry.depotFile as string, action: entry.action as string, rev: entry.rev as number})
		}
		return result
	}


	private exec(args: string[], optOpts?: ExecOpts): Promise<string> {
		if (VERBOSE) console.log('Running: ' + args.join(' '))
		let options: ExecFileOptions = {maxBuffer: 1024*1024}

		const opts: ExecOpts = optOpts || {}
		if (opts.stdin) {
			args.push('-i')
		}

		const cmd = 'p4 ' + args.join(' ')

		let child: ChildProcess
		const execPromise = new Promise<string>((done, fail) => {
			child = execFile(Perforce.P4_EXE, args, options, (err, stdout, stderr) => {

				if (stderr) {
					let errstr = `P4 Error: ${cmd}\n`
						+ `STDERR:\n${stderr}\n`
						+ `STDOUT:\n${stdout}\n`

					if (opts.stdin) {
						errstr += `STDIN:\n${opts.stdin}\n`
					}
					
					fail([new Error(errstr), stderr.toString().replace(Perforce.REGEX_NEWLINE, '\n')])
				}
				else if (err) {
					let errstr = `P4 Error: ${cmd}\n`
						+ err.toString() + '\n'

					if (stdout || stderr) {
						if (stdout) {
							errstr += `STDOUT:\n${stdout}\n`
						}
						if (stderr) {
							errstr += `STDERR:\n${stderr}\n`
						}
					}

					if (opts.stdin) {
						errstr += `STDIN:\n${opts.stdin}\n`
					}

					fail([new Error(errstr), stdout ? stdout.toString() : ''])
				}
				else {
					done(stdout.toString().replace(Perforce.REGEX_NEWLINE, '\n'))
				}
			})
		})

		// write some stdin if requested
		if (opts.stdin) {
			try {
				child!.stdin!.write(opts.stdin)
				child!.stdin!.end()
			}
			catch (ex) {
				// usually means P4 process exited immediately with an error, which should be logged above
				console.log(ex)
			}
		}

		return execPromise
	}
}

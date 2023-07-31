// Copyright Epic Games, Inc. All Rights Reserved.

import { ContextualLogger } from "./logger";

export interface Arg<T> {
	match: RegExp,
	parse?: (str: string) => T,
	env: string,
	dflt?: T
}

export type Args = {[param: string]: any}

export function readProcessArgs(supportedArgs: {[param: string]: Arg<any>}, parentLogger: ContextualLogger) {
	const argsLogger = parentLogger.createChild('Args')
	const args: Args = {}
	for (let argName in supportedArgs) {
		let computedValue = undefined
		let rec = supportedArgs[argName]
		if (rec.env) {
			let envVal = process.env[rec.env]
			if (envVal) {
				computedValue = rec.parse ? rec.parse(envVal) : envVal
			}
		}

		for (const val of process.argv.slice(2)) {
			let match = val.match(rec.match)
			if (match) {
				computedValue = rec.parse ? rec.parse(match[1]) : match[1]
			}
		}

		if (computedValue === undefined) {
			if (rec.dflt !== undefined) {
				computedValue = rec.dflt
			}
			else {
				if (rec.env) {
					argsLogger.error(`Missing required ${rec.env} environment variable or -${argName}=<foo> parameter.`)
				}
				else {
					argsLogger.error(`Missing required -${argName}=<foo> parameter.`)
				}
				return null
			}
		}
		args[argName] = computedValue
	}
	return args
}
// Copyright Epic Games, Inc. All Rights Reserved.

import winston = require('winston')
const { createLogger, format, transports } = winston

const LOG_BUFFER_LINES = 4000
const MESSAGE_SYMBOL = Symbol.for("message")

export const NpmLogLevelValues = { "error": 0, "warn": 1, "info": 2, "troubleshoot": 3, "verbose": 4, "debug": 5, "silly" : 6 }
export type NpmLogLevel = keyof typeof NpmLogLevelValues
export function isNpmLogLevel(level: string): level is NpmLogLevel {
    return level in NpmLogLevelValues
}
export function NpmLogLevelCompare(level1: NpmLogLevel, level2: NpmLogLevel) {
    const val1 = NpmLogLevelValues[level1]
    const val2 = NpmLogLevelValues[level2]
    return val1 === val2 ? 0 :
        val1 < val2 ? -1 :
        1
}

export type RoboLoggerCallback = (message: string) => void | null
export type LogFuncs = Record<NpmLogLevel, (message: string) => winston.Logger>

abstract class RoboLogger {
    private static defaultLogLevel : NpmLogLevel = 
        (process.env.ROBO_LOG_LEVEL && process.env.ROBO_LOG_LEVEL in NpmLogLevelValues) ? process.env.ROBO_LOG_LEVEL as NpmLogLevel : 
        process.env.EPIC_ENV === 'prod' || process.env.NODE_ENV === 'production' ? 'info' : 
        'debug'
    static readonly initLogLevel = RoboLogger.defaultLogLevel

    private static logTransport = new transports.Console({ level: RoboLogger.defaultLogLevel }).on('logged', (log) => {
        const formattedMsg = log[MESSAGE_SYMBOL]
         // Console logger callback functionality is broken. This is a hack to call the callback after log event
        if (RoboLogger.callback) {
            RoboLogger.callback(formattedMsg)
        }
        RoboLogger.addToTail(formattedMsg)
    })
    // Default logger should never be directly used -- implementing classes should use createChildLogger()
    private static defaultLogger : winston.Logger = createLogger({
        defaultMeta: { service: 'robomerge' },
        format: format.combine(
            format.timestamp( { format: 'YYYY/MM/DD HH:mm:ss.SSS' } ),
            format.errors({ stack: true }),
            format.printf(info => {
                const prefix = info.prefix ? ` ${info.prefix}` : ''
                const level = info.level === 'info' ? '' : ` [${info.level}]`
                return `${info.timestamp}${prefix}${level}: ${info.message}`
            })
        ),
        transports: [ RoboLogger.logTransport ],
        levels: NpmLogLevelValues
    })
    protected abstract childLogger : winston.Logger

    private static callback : RoboLoggerCallback
    private static logTail : string[] = []
    private static lastBufferPos = 0

    protected static createChildLogger(meta: Object) {
        return RoboLogger.defaultLogger.child(meta)
    }

    private log(level: NpmLogLevel, message: string) {
        // Console logger callback functionality is broken. See logTransport declaration
        //this.logger.log(level, message, this.callback)

        return this.childLogger.log(level, message)
    }

    static addToTail(message: string) {
        RoboLogger.logTail[RoboLogger.lastBufferPos] = message
	    RoboLogger.lastBufferPos = (RoboLogger.lastBufferPos + 1) % LOG_BUFFER_LINES
    }

    static getLogTail() : string {
        let out = ''
		for (let index = 0; index != RoboLogger.logTail.length; ++index) {
			const idx = (RoboLogger.lastBufferPos + index) % RoboLogger.logTail.length
			const str = RoboLogger.logTail[idx]
			if (str) {
				out += `${str}\n`
			}
		}
		return out
    }

    static restoreInitialLogLevel(logger: RoboLogger) {
        return RoboLogger.setLogLevel(RoboLogger.defaultLogLevel, logger)
    }

    /**
     * Sets the logger's log level and returns the previous level.
     * @param level 
     */

    static setLogLevel(level: NpmLogLevel, logger: RoboLogger) : NpmLogLevel {
        const previousLevel = RoboLogger.logTransport.level as NpmLogLevel
        RoboLogger.logTransport.level = level
        logger[level](`Logging level set to '${level}' (was: '${previousLevel}')`)
        return previousLevel
    }

    static getLogLevel(): NpmLogLevel {
        return RoboLogger.logTransport.level as NpmLogLevel
    }

    static setCallback(cb: RoboLoggerCallback) {
        RoboLogger.callback = cb
    }

    /*
     * Wrapping Functions
     */

    printException(err: any, preface?: any) {
        if (err instanceof Error) {
            const prefaceStr = preface ? `${preface.toString()} ` : ''
            return this.error(`${prefaceStr}${err.name}: ${err.message}\n${err.stack}`)
        }
        return this.error((preface ? `${preface.toString()}: ` : '') + err.toString())
    }

    error(message: any) {
        return this.log('error', String(message))
    }

    warn(message: any) {
        return this.log('warn', String(message))
    }

    info(message: any) {
        return this.log('info', String(message))
    }

    troubleshoot(message: any) {
        return this.log('troubleshoot', String(message))
    }

    verbose(message: any) {
        return this.log('verbose', String(message))
    }

    debug(message: any) {
        return this.log('debug', String(message))
    }

    silly(message: any) {
        return this.log('silly', String(message))
    }
}

export class ContextualLogger extends RoboLogger {
    readonly context: string;
    protected childLogger : winston.Logger

    /**
     * Create a ContextualLogger from root-level logger instance.
     * @param context Preceeding text for every log message
     */
    constructor(context: string);
    /**
     * Create a ContextualLogger parented with a specific winston logger
     * @param context Preceeding text for every log message
     * @param parentLogger winston logger to serve as a parent
     */
    constructor(context: string, parentLogger: winston.Logger);
    constructor(context: string, parentLogger?: winston.Logger) {
        super()
        if (parentLogger) { 
            this.childLogger = parentLogger.child(ContextualLogger.createContextMetaObj(context))
        }
        else {
            this.childLogger = RoboLogger.createChildLogger(ContextualLogger.createContextMetaObj(context))
        }
        this.context = context
    }
    
    addToTail = RoboLogger.addToTail
    setCallback = RoboLogger.setCallback
    getLogTail = RoboLogger.getLogTail


    /**
     * Creates 'meta' object that winston.Logger expects in order to prefix our log messages.
     * @param context Contextual string for log messages. Will be surrounded by brackets in log.
     */
    static createContextMetaObj(context: string) {
        return { prefix: `[${context}]` }
    }

    /**
     * Creates a ContextualLogger whose winston logger is the child of this object's, with appended context.
     * @param context Contextual string to be appended to this object's own context 
     */
    createChild(context: string) : ContextualLogger {
        const newContext = `${this.context}:${context}`
        return new ContextualLogger(newContext, this.childLogger)
    }

}

// Deprecated.
//export const RoboStaticLogger = new ContextualLogger('Static')
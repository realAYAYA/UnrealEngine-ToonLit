// Copyright Epic Games, Inc. All Rights Reserved.

import { action, makeObservable, observable } from 'mobx';
import { AgentData, PoolData } from './Api';
import backend from '.';

export class AgentStore {

    constructor() {
        makeObservable(this);
    }

    @observable
    agentsUpdated: number = 0

    get agents(): AgentData[] {
        // subscribe in any observers
        if (this.agentsUpdated) { }
        return this._agents;
    }
            
	private _agents: AgentData[] = [];
	
    @observable
    poolsUpdated: number = 0;

    get pools(): PoolData[] {
        // subscribe in any observers
        if (this.poolsUpdated) { }
        return this._pools;
    }
    
    private _pools: PoolData[] = [];

	// don't need this for now
    modifiedAfterDate: Date = new Date(0);

    @action 
    private _setPools(data: PoolData[]) {
        this._pools = data;
        this.poolsUpdated++;
    }

    @action
    private _setAgents(data: AgentData[]) {
        if (data.length !== 0) {            
            const toSet: AgentData[] = [...this._agents];
            data.forEach(updatedAgent => {
                const newUpdateIdx = toSet.findIndex(existingAgent => existingAgent.id === updatedAgent.id);
                if(newUpdateIdx !== -1) {
                    toSet[newUpdateIdx] = updatedAgent;
                }
                else {
                    toSet.push(updatedAgent);
                }
            });
            this._agents = toSet;
            this.agentsUpdated++;
        }
	}

    async update(slim = false): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            const promises: any[] = [];
            promises.push(backend.getAgents({modifiedAfter: this.modifiedAfterDate?.toISOString()}));
            if(!slim) {
                promises.push(backend.getPools());
            }
			Promise.all(promises).then(values => {
                this.modifiedAfterDate = new Date();
                this._setAgents(values[0]);
                if(!slim){
                    this._setPools(values[1]);
                }
                resolve();
            }).catch(reason => {
                console.error(`Error getting agents and pools: ${reason}`);
            });
        });
    }
}

export const agentStore = new AgentStore();
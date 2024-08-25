
import { GetJobResponse } from "../../backend/Api";
import graphCache from "../../backend/GraphCache";


export type JobFilterSimple = {
    filterKeyword?: string;
    showOthersPreflights: boolean;
}

const getJobKeywords = (job: GetJobResponse): string[] => {

    let keywords: string[] = [];

    if (job.name) {
        keywords.push(job.name);
    }

    if (job.change) {
        keywords.push(job.change.toString());
    }

    if (job.preflightChange) {
        keywords.push(job.preflightChange.toString());
    }

    if (job.startedByUserInfo) {
        keywords.push(job.startedByUserInfo.name);
    } else {
        keywords.push("Scheduler");
    }

    if (job.graphHash) {
        const graph = graphCache.cache.get(job.graphHash);

        if (graph) {

            const nodes: string[] = [];
            graph.groups?.forEach(g => g.nodes.forEach(n => nodes.push(n.name)));

            job.batches?.forEach(batch => {

                const group = graph.groups?.[batch.groupIdx];

                if (group) {

                    batch.steps.forEach(step => {
                        const name = group.nodes[step.nodeIdx]?.name;
                        if (name && keywords.indexOf(name) === -1) {                            
                            keywords.push(name);
                        }

                    })
                }

            });

        } 
    }


    keywords.push(...job.arguments);

    return keywords;

}

export const filterJob = (job: GetJobResponse, keywordIn?: string, additionalKeywords?: string[]): boolean => {

    const keyword = keywordIn?.toLowerCase();

    if (!keyword) {
        return true;
    }

    let keywords = getJobKeywords(job);


    if (additionalKeywords) {
        keywords.push(...additionalKeywords);
    }

    keywords = keywords.map(k => k.toLowerCase());

    const keys = keyword.indexOf(";") !== -1 ? keyword.split(";").map(k => k.trim()).filter(k => k.length) : [keyword];

    let j = 0;
    for (let i = 0; i < keys.length; i++) {
        if (keywords.find(k => k.indexOf(keys[i]) !== -1)) {
            j++;
        }
    }

    return j === keys.length;
}
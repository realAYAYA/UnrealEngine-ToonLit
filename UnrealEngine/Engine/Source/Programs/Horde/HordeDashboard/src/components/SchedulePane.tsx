// Copyright Epic Games, Inc. All Rights Reserved.
import moment, { Moment } from 'moment-timezone';
import { mergeStyleSets, Stack } from '@fluentui/react';
import { DetailsList, DetailsListLayoutMode, DetailsRow, IColumn, IDetailsListProps, SelectionMode } from '@fluentui/react/lib/DetailsList';
import React, { useState } from 'react';
import { Link } from 'react-router-dom';
import backend from '../backend';
import { GetJobResponse, GetSchedulePatternResponse, GetTemplateRefResponse, JobState } from '../backend/Api';
import dashboard from '../backend/Dashboard';
import { displayTimeZone } from "../base/utilities/timeUtils";

const classes = mergeStyleSets({
    detailsRow: {
        selectors: {
            '.ms-DetailsRow-cell': {
                overflow: "hidden",
                whiteSpace: "nowrap",
                padding: 8
            },
            '.ms-List-cell:nth-child(odd)': {
                background: "rgb(240, 239, 239)",
            },
            '.ms-List-cell:nth-child(even)': {
                background: "#FFFFFF",
            }

        }
    }
});

type ScheduleItem = {
    template: GetTemplateRefResponse
};

const scheduledJobs = new Map<string, GetJobResponse | null>();

const getNextTriggerTime = (pattern: GetSchedulePatternResponse, lastTimeIn: Date | string): Moment | undefined => {

    // Convert last time into the correct timezone for running the scheule
    const lastTime = moment(lastTimeIn);

    // Get the base time (ie. the start of this day) for anchoring the schedule
    let baseTime = moment().year(lastTime.year()).month(lastTime.month()).date(lastTime.day()).hour(0).minute(0).second(0).millisecond(0);

    for (; ;) {

        const day = baseTime.format('dddd');

        if (!pattern.daysOfWeek || !!pattern.daysOfWeek.find(d => {
            return d.toLowerCase() === day.toLowerCase();
        })) {

            // Get the last time in minutes from the start of this day
            const lastTimeMinutes = lastTime.diff(baseTime, "minutes");

            if (lastTimeMinutes < pattern.minTime) {
                return baseTime.add(pattern.minTime, "minutes");
            }

            // Otherwise, get the time for the last trigger in the day.
            if (pattern.interval) {
                const actualMaxTime = pattern.maxTime ? pattern.maxTime : (24 * 60);
                if (lastTimeMinutes < actualMaxTime) {
                    const lastIndex = (lastTimeMinutes - pattern.minTime) / pattern.interval;
                    const nextIndex = lastIndex + 1;

                    const nextTimeMinutes = pattern.minTime + (nextIndex * pattern.interval);
                    if (nextTimeMinutes < actualMaxTime) {
                        return baseTime.add(nextTimeMinutes, "minutes");
                    }
                }
            }
        }

        baseTime = baseTime.add(1, "days");

    }

}

export const SchedulePane: React.FC<{ templates: GetTemplateRefResponse[] }> = ({ templates }) => {

    let [updated, setUpdated] = useState(new Date().getTime());

    const ids = new Set<string>();

    if (new Date().getTime() - updated > 10000 ) {

        scheduledJobs.forEach((job, key) => {
            if (job?.state !== JobState.Complete ) {
                ids.add(key);
            }
        });

    }

    templates.forEach(t => {
        if (t.schedule && t.schedule.enabled && t.schedule.maxActive === 1 && t.schedule.activeJobs.length) {
            const jid = t.schedule.activeJobs[0];
            if (!scheduledJobs.has(jid)) {
                scheduledJobs.set(jid, null);
                ids.add(jid);
            }
        }
    })

    if (ids.size) {
        backend.getJobsByIds(Array.from(ids.values()), { filter: "id,createTime,state" }, false).then(response => {
            response.forEach(j => {
                scheduledJobs.set(j.id, j);
            });

            setUpdated(new Date().getTime());
        });
    }

    const items = templates.filter(t => !!t.schedule?.enabled).map(t => {
        return { template: t };
    });

    const columns = [
        { key: 'schedule_column1', name: 'job_name', minWidth: 320, maxWidth: 320, isResizable: false },
        { key: 'schedule_column3', name: 'time_0', minWidth: 140, maxWidth: 140, isResizable: false },
        { key: 'schedule_column4', name: 'time_1', minWidth: 100, maxWidth: 100, isResizable: false },
        { key: 'schedule_column5', name: 'time_2', minWidth: 100, maxWidth: 100, isResizable: false },
        { key: 'schedule_column6', name: 'time_3', minWidth: 100, maxWidth: 100, isResizable: false },
        { key: 'schedule_column7', name: 'time_4', minWidth: 100, maxWidth: 100, isResizable: false },
        { key: 'schedule_column8', name: 'time_5', minWidth: 100, maxWidth: 100, isResizable: false },
        { key: 'schedule_column9', name: 'time_6', minWidth: 100, maxWidth: 100, isResizable: false },
        { key: 'schedule_column10', name: 'space', minWidth: 2, isResizable: false }

    ];

    const renderItem = (item: ScheduleItem, index?: number, column?: IColumn) => {

        const name = column!.name;
        const template = item.template;
        const schedule = template.schedule!;

        if (name === "job_name") {

            return <Stack horizontalAlign="center" verticalFill verticalAlign="center">{item.template.name}</Stack>;            
        }

        if (name.startsWith("time_")) {

            const index = parseInt(name.slice(5));

            let times: Moment[] = [];
            let triggerTime = moment(schedule.lastTriggerTime);
            times.push(triggerTime);

            schedule.patterns.forEach(p => {
                triggerTime = getNextTriggerTime(p, schedule.lastTriggerTime)!;
                times.push(triggerTime);
                for (let i = 0; i < 5; i++) {
                    triggerTime = getNextTriggerTime(p, moment.utc(triggerTime).toISOString())!;
                    times.push(triggerTime);
                }
            })

            times = times.sort((a, b) => {
                return moment.utc(a).milliseconds() - moment.utc(b).milliseconds();
            })

            if (index >= times.length) {
                return null;
            }

            const displayTime = times[index].tz(displayTimeZone());
            const now = moment.utc().tz(displayTimeZone());
            let timeStr = dashboard.display24HourClock ? displayTime.format('H:mm z') : displayTime.format('h:mma z');
            if (!(now.isSame(displayTime, 'day'))) {
                timeStr += displayTime.format(' (ddd)');
            }
            
            if (index === 0) {

                if (schedule?.activeJobs.length) {
                    const job = scheduledJobs.get(schedule.activeJobs[0]);
                    if (job) {
                        const time = moment(job.createTime).tz(displayTimeZone());
                        const timeStr = dashboard.display24HourClock ? time.format("MMM Do H:mm z") : time.format("MMM Do h:mma z");

                        let state = "Complete";

                        if (job.state === JobState.Waiting || job.state === JobState.Running) {
                            state = "Running";
                        }

                        return <Link to={`/job/${job.id}`}>
                            <Stack horizontalAlign="center" tokens={{childrenGap:4}}>
                            <Stack >{timeStr}</Stack>
                            <Stack >{state}</Stack>
                        </Stack>
                        </Link>;
                    }
                }
            }

            return <Stack horizontalAlign="center" verticalFill verticalAlign="center">{timeStr}</Stack>;
        }


        return null;

    };

    const renderRow: IDetailsListProps['onRenderRow'] = (props) => {

        if (props) {

            const nprops = { ...props, styles: { root: { backgroundColor: "unset", padding: 8 } } };

            return <DetailsRow {...nprops} />

        }
        return null;
    };

    return <div className={classes.detailsRow} style={{ overflowY: 'auto', overflowX: 'hidden', maxHeight: "820px" }} data-is-scrollable={true}>

        <DetailsList
            styles={{root:{overflow:"unset"}}}
            compact={true}
            isHeaderVisible={false}
            indentWidth={0}
            items={items}
            columns={columns}
            setKey="set"
            selectionMode={SelectionMode.none}
            layoutMode={DetailsListLayoutMode.justified}
            onRenderItemColumn={renderItem}
            onRenderRow={renderRow}
        />
    </div>


}

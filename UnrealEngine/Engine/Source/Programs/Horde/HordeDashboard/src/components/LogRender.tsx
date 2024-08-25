// Copyright Epic Games, Inc. All Rights Reserved.
import { Stack, Text } from '@fluentui/react';
import Highlight from 'react-highlighter';
import { IssueData, LogLine } from '../backend/Api';
import backend from '../backend';
import { NavigateFunction } from 'react-router-dom';

enum TagType {
   None,
   SourceFile,
   MSDNCode,
   AgentId,
   LeaseId,
   Link
}

export type LogItem = {
   line?: LogLine;
   lineNumber: number;
   requested: boolean;
   issueId?: number;
   issue?: IssueData;
}

const getLineNumber = (line: LogLine): number | undefined => {

   const properties = line.properties;
   if (!properties) {
      return undefined;
   }

   const lineProp = properties["line"];

   if (!lineProp || typeof (lineProp) !== "object") {
      return undefined;
   }

   return (lineProp as Record<string, number | undefined>).line;

}

const renderMessage = (line: LogLine, lineNumber: number | undefined, logStyle: any, search?: string) => {

   const message = line?.message ?? "";

   if (lineNumber === undefined || lineNumber === null) {
      lineNumber = getLineNumber(line);
   }

   const key = `log_line_${lineNumber}_message`;

   return <Highlight key={key} search={search ? search : ""} className={logStyle.logLine}>{message ?? ""}</Highlight >;

};


const renderTags = (navigate: NavigateFunction, line: LogLine, lineNumber: number | undefined, logStyle: any, tags: string[], search?: string) => {

   if (!line || !line.format || !line.properties) {
      return <Stack styles={{ root: { color: "#000000", paddingLeft: 8, whiteSpace: "pre", tabSize: "3" } }}>Internal log line format error</Stack>;
   }

   const properties = line.properties;
   const lineProp = properties["line"];

   if (lineNumber === undefined || lineNumber === null) {
      lineNumber = (lineProp as Record<string, number | undefined>)?.line ?? 0;
   }

   // render tags
   let renderedTags = tags.map((tag, index) => {

      let tagType = TagType.None;
      let property: Record<string, string | number> | string | undefined;
      let record: Record<string, string> | undefined;
      const key = `log_line_${lineNumber}_${index}`;

      let formatter = "";
      tag = tag.replace("{", "").replace("}", "");
      if (tag.indexOf(":") !== -1) {
         [tag, formatter] = tag.split(":");
      }

      property = properties[tag];

      // C# format specifiers, yes this could be more elegant
      if (formatter) {
         if (typeof (property) === "number") {

            let precision: number | undefined;

            if (formatter.startsWith("n") && formatter.length > 1) {
               precision = parseInt(formatter.slice(1));
            }

            if (formatter.startsWith("0.") && formatter.length > 2) {
               precision = 0;
               for (let i = 2; i < formatter.length; i++) {
                  if (formatter[i] === "0") {
                     precision++;
                  } else {
                     break;
                  }
               }
            }

            if (precision !== undefined && !isNaN(precision)) {
               property = (property as number).toFixed(precision);
            }

         }
      }

      let type = "";
      let text = "";

      if (property && typeof (property) !== "string" && typeof (property) !== "number" && typeof (property) !== "boolean" && property !== null) {
         record = property as Record<string, string>;
      } else {
         if (property === undefined) {
            return null;
         }
         if (property === null) {
            text = "null";
         } else {
            text = property.toString();
         }
      }

      if (record) {

         type = record.type ?? record["$type"] ?? "";
         text = record.text ?? record["$text"] ?? "";

         if (type === "SourceFile") {
            tagType = TagType.SourceFile;
         }

         if (type === "Link") {
            tagType = TagType.Link;
         }

         if (type === "ErrorCode" && text.startsWith("C")) {
            tagType = TagType.MSDNCode;
         }

         if (type === "LeaseId") {
            tagType = TagType.LeaseId;
         }

         if (type === "AgentId") {
            tagType = TagType.AgentId;
         }

      }

      if (tagType === TagType.None || !record) {

         return <Highlight key={key} search={search ? search : ""} className={logStyle.logLine}>{text}</Highlight>;

      } else if (tagType === TagType.LeaseId) {

         const navigateToLeaseLog = async (toplevel: boolean) => {

            const logData = await backend.getLease(text);
            const url = `/log/${logData?.logId}`;
            if (!toplevel) {
               navigate(url)
            } else {
               window.open(url, "_blank");
            }
         }

         return <a key={key} href="/"

            onAuxClick={(ev) => {
               ev.preventDefault();
               ev.stopPropagation()
               navigateToLeaseLog(true)
            }}

            onClick={(ev) => {
               ev.stopPropagation();
               ev.preventDefault();
               navigateToLeaseLog(!!ev?.ctrlKey || !!ev.metaKey)
            }}>
            <Highlight search={search ? search : ""} className={logStyle.logLine}>{text}</Highlight>
         </a>;

      } else if (tagType === TagType.MSDNCode) {

         return <a key={key} target="_blank" rel="noopener noreferrer" href={`https://msdn.microsoft.com/query/dev16.query?appId=Dev16IDEF1&l=EN-US&k=k(${text.toLowerCase()})&rd=true`} onClick={(ev) => ev.stopPropagation()}><Highlight search={search ? search : ""} className={logStyle.logLine}>{text}</Highlight></a>;

      } else if (tagType === TagType.AgentId) {
         const search = new URLSearchParams(window.location.search);
         search.set("agentId", encodeURIComponent(text));
         const url = `${window.location.pathname}?` + search.toString();
         return <a key={key} href="/" onClick={async (ev) => { ev.stopPropagation(); ev.preventDefault(); navigate(url, { replace: true }) }}><Highlight search={search ? search : ""} className={logStyle.logLine}>{text}</Highlight></a>;

      } else if (tagType === TagType.SourceFile) {

         let depotPath = record.depotPath;

         if (!depotPath) {
            return <Highlight key={key} search={search ? search : ""} className={logStyle.logLine}>{text}</Highlight>;
         }

         if (depotPath.indexOf("@") !== -1) {
            depotPath = depotPath.slice(0, depotPath.indexOf("@"));
         }

         depotPath = encodeURIComponent(depotPath);

         // @todo: handle line, it isn't in the source file meta data, and matching isn't obvious (file vs note, etc)
         let tagLine = "";
         if (tagLine) {
            depotPath += `&line=${tagLine}`;
         }

         return <a key={key} href={`ugs://timelapse?depotPath=${(depotPath)}`} onClick={(ev) => ev.stopPropagation()}><Highlight search={search ? search : ""} className={logStyle.logLine}>{text}</Highlight></a>;
      } else if (tagType === TagType.Link) {
         
         return <a key={key} rel="noreferrer" href={record.target} onClick={(ev) => ev.stopPropagation()}><Highlight search={search ? search : ""} className={logStyle.logLine}>{text}</Highlight></a>;
      }

      return <span key={key} />;
   }).filter(t => !!t);

   let remaining = line.format;

   renderedTags = renderedTags.map((t, idx) => {

      let current = remaining;

      const tag = tags[idx];
      const index = remaining.indexOf(tag);

      remaining = remaining.slice(tag.length + (index > 0 ? index : 0));

      if (index < 0) {
         console.error("not able to find tag in format");
         return <Text>Error, unable to find tag</Text>;
      }

      if (index === 0) {
         return t;
      }

      const rtags = [];

      const key = `log_line_${lineNumber}_${idx}_${index}_fragment`;

      rtags.push(<Highlight key={key} search={search ? search : ""} className={logStyle.logLine}>{current.slice(0, index)}</Highlight>);
      rtags.push(t);

      return rtags;

   }).flat();

   if (remaining) {
      const key = `log_line_${lineNumber}_remaining_fragment`;
      renderedTags.push(<Highlight key={key} search={search ? search : ""} className={logStyle.logLine}>{remaining}</Highlight>)
   }

   return <div>
      {renderedTags}
   </div>;

};

export const renderLine = (navigate: NavigateFunction, line: LogLine | undefined, lineNumber: number | undefined, logStyle: any, search?: string) => {

   if (!line) {
      return null;
   }

   const tagRegex = /{[^{}]+}/g
   let format = line?.format;

   let tags: string[] = [];
   if (format) {

      // some automation logs output their own structured logging, which is marked by {{}}
      if (format.indexOf("{{") !== -1) {
         format = format.replaceAll("{", "");
         format = format.replaceAll("}", "");
      }

      const match = format.match(tagRegex);
      if (match?.length)
         tags = match;

      const properties = line.properties;

      // fix issue with tag span, we need to replace react-highlighter as it does not highlight across child nodes
      // should just be using a selector
      if (properties) {
         tags = tags.filter(t => {
            const pname = t.slice(1, -1);

            if (pname !== "WarningCode" && pname !== "WarningMessage") {
               return true;
            }

            const ptext = (properties[pname] as any);
            if (!ptext) {
               return true;
            }

            line.format = line.format?.replaceAll(t, ptext);
            return false;
         });
      }
   }

   // we don't support c# alignment, as this requireds read behinds, etc
   // so strip tags in this case and just output the line
   if (tags.find(t => t.indexOf(",") !== -1)) {
      tags = [];
   }

   if (tags.length && format && line.properties) {
      return renderTags(navigate, line, lineNumber, logStyle, tags, search);
   }

   return renderMessage(line, lineNumber, logStyle, search);

};



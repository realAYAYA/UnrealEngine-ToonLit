# Copyright Epic Games, Inc. All Rights Reserved.
FROM helix-and-node

# configure for Epic's p4 server
ENV P4PORT=perforce:1666

# set this to the robomerge account
ENV P4USER=robomerge

# BOTNAME and P4PASSWD should be set on the run (bot defaults to test just for safety)
ENV BOTNAME=test
ENV P4PASSWD=

# set the APP directory as . in the image
WORKDIR /app

# make the directory to sync branch settings to
RUN mkdir /app/data

# make the root directory for workspaces
RUN mkdir /src

# make an empty settings folder in able to run without error
RUN mkdir -p /root/.robomerge

# expose the robomerge web page
EXPOSE 8080 4433

# install dependencies
COPY ./package.json ./
COPY ./package-lock.json ./
RUN npm install

# copy and compile code
COPY ./tsconfig.json ./
COPY ./src ./src
RUN tsc

# copy configuration
COPY ./version.json ./
COPY ./config ./config

# copy web resources
COPY ./public ./public
COPY ./certs ./certs

# make public folder in advance, so npm install can put things in there
COPY ./install-graphviz.js ./
RUN node ./install-graphviz.js

# copy email templates
COPY ./email_templates ./email_templates

# Copy markdown files
COPY ./*.md ./

# dump versions for easy viewing
RUN echo Node $(node -v)
RUN echo Typescript $(tsc -v)

CMD [ "node", "--require", "source-map-support/register", "dist/robo/watchdog.js" ]

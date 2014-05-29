#!/bin/bash

set -x #echo on

eval "$(/bin/sh ../remote-config.sh)"

PROJECT=line-traffic
EXTRA_SOURCES='line-gui util remote_config.h'
BUILD_DIR=$PWD
SRC_DIR=$SVN_DIR/$PROJECT
REMOTE_USER=$REMOTE_USER_HOSTS
REMOTE_HOST=$REMOTE_HOST_HOSTS
REMOTE_PORT=$REMOTE_PORT_HOSTS
REMOTE_DIR=$( [ "$REMOTE_USER" = "root" ] && echo "/root" || echo "/home/$REMOTE_USER" )
MAKE="qmake $PROJECT.pro -r -spec linux-g++-64 ${BUILD_CONFIG_HOSTS} && make clean && make -w -j12 && install -m 755 -p line-traffic /usr/bin/"
#MAKE="qmake $PROJECT.pro -r -spec linux-g++-64 CONFIG+=debug && make clean && make -w -j12 && install -m 755 -p line-traffic /usr/bin/"

echo "make-remote.sh:1: warning: Compiling on $REMOTE_HOST ($(host $REMOTE_HOST | cut -d ' ' -f 5))"

cd $BUILD_DIR || exit 1
pwd; echo "Copying $PROJECT to $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR..."
for DIR in $PROJECT $EXTRA_SOURCES
do
	rsync -aqcz --delete  -e "ssh -p $REMOTE_PORT" ../$DIR $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR || exit 1
	ssh $REMOTE_USER@$REMOTE_HOST -p $REMOTE_PORT "sh -l -c \"set -x && cd $REMOTE_DIR && touch $DIR && (cd $DIR 2>/dev/null && find . -exec touch {} \; || /bin/true)\""
done
rm -f stdoutfifo
rm -f stderrfifo
mkfifo stdoutfifo || exit 1
cat stdoutfifo &
mkfifo stderrfifo || exit 1
(cat stderrfifo | perl -pi -w -e "s|$REMOTE_DIR/$PROJECT|$SRC_DIR|g;" >&2) &
pwd; echo "Building remotely..."
ssh $REMOTE_USER@$REMOTE_HOST -p $REMOTE_PORT "sh -l -c \"set -x && pwd && cd $REMOTE_DIR && pwd && cd $PROJECT && $MAKE\"" 1>stdoutfifo 2>stderrfifo
rm stdoutfifo
rm stderrfifo

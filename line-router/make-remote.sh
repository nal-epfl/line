#!/bin/bash

set -x #echo on

eval "$(/bin/sh ../remote-config.sh)"

PROJECT=line-router
EXTRA_SOURCES='line-gui util tomo remote_config.h'
BUILD_DIR=$PWD
REMOTE_USER=$REMOTE_USER_ROUTER
REMOTE_HOST=$REMOTE_HOST_ROUTER
REMOTE_PORT=$REMOTE_PORT_ROUTER
REMOTE_DIR=$( [ "$REMOTE_USER" == "root" ] && echo "/root" || echo "/home/$REMOTE_USER" )
MAKE="qmake $PROJECT.pro -r -spec linux-g++-64 ${BUILD_CONFIG_ROUTER} && make clean && make -w -j7 && make install && install -m 755 -p /root/line-router/line-router /usr/bin/"
#MAKE="qmake $PROJECT.pro -r -spec linux-g++-64 CONFIG+=debug && make clean && make -w -j7 && make install && install -m 755 -p /root/line-router/line-router /usr/bin/"

echo "make-remote.sh:1: warning: Local tree in $SRC_DIR "
echo "make-remote.sh:1: warning: Compiling on $REMOTE_HOST ($(host $REMOTE_HOST | cut -d ' ' -f 5))"

# Install PF_RING
PF_RING='PF_RING-5.6.2'
if $($INSTALL_PF_RING)
#if false
then
	cd $BUILD_DIR || exit 1
    pwd; echo "Copying $PF_RING to $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR..."
    rsync -aqcz --delete  -e "ssh -p $REMOTE_PORT" ../$PF_RING $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR || exit 1
  ssh $REMOTE_USER@$REMOTE_HOST -p $REMOTE_PORT "sh -l -c \"set -x && cd $REMOTE_DIR && touch $PF_RING && cd $PF_RING && find . -type f -exec touch {} \; \""
	rm -f stdoutfifo
	rm -f stderrfifo
	mkfifo stdoutfifo || exit 1
	cat stdoutfifo &
	mkfifo stderrfifo || exit 1
	(cat stderrfifo | perl -pi -w -e "s|$REMOTE_DIR/$PF_RING|$SRC_DIR|g;" >&2) &
	pwd; echo "Building remotely..."
	ssh $REMOTE_USER@$REMOTE_HOST -p $REMOTE_PORT "sh -l -c \"set -x && (env | sort) && pwd && cd $REMOTE_DIR && pwd && cd $PF_RING && \
		cd kernel && make && make install && (rmmod pf_ring 2> /dev/null || /bin/true) && cd .. && \
        cd userland && make && cd lib && (make install || /bin/true) && make install && cd .. && \
		cd libpcap && (make install || /bin/true) && make install\"" 1>stdoutfifo 2>stderrfifo
	if [ $? -ne 0 ]
	then
		echo "make-remote.sh:1: error: See the Compile Output panel for details."
		exit 1
	fi
fi

#exit 0

# Install malloc_profile
cd $BUILD_DIR || exit 1
pwd; echo "Copying malloc_profile to $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR..."
rsync -aqcz --delete  -e "ssh -p $REMOTE_PORT" ../malloc_profile $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR || exit 1
ssh $REMOTE_USER@$REMOTE_HOST -p $REMOTE_PORT "sh -l -c \"set -x && cd $REMOTE_DIR && touch malloc_profile && cd malloc_profile && find . -type f -exec touch {} \;\""
rm -f stdoutfifo
rm -f stderrfifo
mkfifo stdoutfifo || exit 1
cat stdoutfifo &
mkfifo stderrfifo || exit 1
(cat stderrfifo | perl -pi -w -e "s|$REMOTE_DIR/$PF_RING|$SRC_DIR|g;" >&2) &
pwd; echo "Building remotely..."
ssh $REMOTE_USER@$REMOTE_HOST -p $REMOTE_PORT "sh -l -c \"set -x && (env | sort) && pwd && cd $REMOTE_DIR && pwd && cd malloc_profile && make clean && make && make install\"" 1>stdoutfifo 2>stderrfifo

# Install line-router

echo "Make command is: $MAKE"

cp $SRC_DIR/$PROJECT/deploycore-template.pl $SRC_DIR/$PROJECT/deploycore.pl && chmod +x $SRC_DIR/$PROJECT/deploycore.pl
perl -pi -e "s/REMOTE_DEDICATED_IF_ROUTER/$REMOTE_DEDICATED_IF_ROUTER/g" $SRC_DIR/$PROJECT/deploycore.pl
perl -pi -e "s/REMOTE_DEDICATED_IP_HOSTS/$REMOTE_DEDICATED_IP_HOSTS/g" $SRC_DIR/$PROJECT/deploycore.pl

cd $BUILD_DIR || exit 1
pwd; echo "Copying $PROJECT to $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR..."
for DIR in $PROJECT $EXTRA_SOURCES
do
	rsync -aqcz --delete  -e "ssh -p $REMOTE_PORT" ../$DIR $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR || exit 1
	ssh $REMOTE_USER@$REMOTE_HOST -p $REMOTE_PORT "sh -l -c \"set -x && cd $REMOTE_DIR && touch $DIR && (cd $DIR 2>/dev/null && find . -type f -exec touch {} \; || /bin/true)\""
done
rm -f stdoutfifo
rm -f stderrfifo
mkfifo stdoutfifo || exit 1
cat stdoutfifo &
mkfifo stderrfifo || exit 1
(cat stderrfifo | perl -pi -w -e "s|$REMOTE_DIR/$PROJECT|$SRC_DIR|g;" >&2) &
pwd; echo "Building remotely..."
ssh $REMOTE_USER@$REMOTE_HOST -p $REMOTE_PORT "sh -l -c \"set -x && pwd && cd $REMOTE_DIR && pwd && cd $PROJECT && $MAKE\"" 1>stdoutfifo 2>stderrfifo
if [ $? -ne 0 ]
then
	echo "make-remote.sh:1: error: See the Compile Output panel for details."
	exit 1
fi
rm stdoutfifo
rm stderrfifo

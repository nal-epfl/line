/*
 *	Copyright (C) 2015 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 is the only version of this
 *  license under which this program may be distributed.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef EVPID_H
#define EVPID_H

#include <ev.h>
#include <QtCore>

typedef void (*ProcessExitCallback)(void*, int);

class Process {
public:
	Process(int pid = -1);
	virtual ~Process();

	int pid;
	struct ev_child *w_child;
	struct ev_loop *loop;
	bool killed;
	bool exited;
	int exitCode;

	// Other parameters
	ProcessExitCallback processExitCallback;
	void *processExitCallbackArg;
};

/** Call this on exit
*/
// NOT usable from ev callbacks!
void process_close_all();

// NOT usable from ev callbacks!
void process_close(int pid);

// Usable from ev callbacks
void process_deferred_close(int pid);

qint32 create_process(struct ev_loop *loop,
					  const char *command,
					  ProcessExitCallback processExitCallback = NULL,
					  void *processExitCallbackArg = NULL);


#endif // EVPID_H

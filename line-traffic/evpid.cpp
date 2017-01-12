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

#include "evpid.h"
#include <unistd.h>
#include "util.h"

// key = pid
static QHash<int, Process*> processes;
static QSet<int> pids_to_close;

Process::Process(int pid)
	: pid(pid)
{
	w_child = NULL;
	loop = NULL;
	processExitCallback = NULL;
	processExitCallbackArg = NULL;
	killed = false;
	exited = false;
	exitCode = 0;
}

Process::~Process()
{
	if (w_child) {
		if (!exited && !killed) {
			kill(pid, SIGINT);
		}
		ev_child_stop(loop, w_child);
		free(w_child);
		w_child = NULL;
	}
}

void process_close_all()
{
	foreach (Process *p, processes.values()) {
		if (!p->exited && !p->killed) {
			kill(p->pid, SIGINT);
			p->killed = true;
		}
		processes.remove(p->pid);
		delete p;
	}
}


void process_close(int pid)
{
	if (processes.contains(pid)) {
		Process *p = processes[pid];
		if (!p->exited && !p->killed) {
			kill(p->pid, SIGINT);
			p->killed = true;
		}
		processes.remove(pid);
		delete p;
	}
}

void process_deferred_close_helper(int, void *)
{
	foreach (int pid, pids_to_close) {
		if (processes.contains(pid)) {
			Process *p = processes[pid];
			if (p->processExitCallback) {
				p->processExitCallback(p->processExitCallbackArg, pid);
			}
		}
		process_close(pid);
	}
	pids_to_close.clear();
}

void process_deferred_close(int pid)
{
	if (processes.contains(pid)) {
		Process *p = processes[pid];
		pids_to_close.insert(p->pid);
		ev_once(p->loop, -1, 0, 0, process_deferred_close_helper, 0);
	}
}

static void process_close_callback(struct ev_loop *, struct ev_child *w, int)
{
	int pid = w->rpid;
	if (processes.contains(pid)) {
		Process *p = processes[pid];
		p->exited = true;
		p->exitCode = w->rstatus;
		process_deferred_close(pid);
	}
}

qint32 create_process(struct ev_loop *loop,
					  const char *command,
					  ProcessExitCallback processExitCallback,
					  void *processExitCallbackArg)
{
	pid_t pid = fork();
	if (pid < 0) {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Fork failed";
		Q_ASSERT_FORCE(false);
	} else if (pid == 0) {
		// child
		execlp("/bin/sh", "sh", "-c", command, NULL);
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Exec failed";
		Q_ASSERT_FORCE(false);
	} else {
		processes[pid] = new Process(pid);
		processes[pid]->processExitCallback = processExitCallback;
		processes[pid]->processExitCallbackArg = processExitCallbackArg;
		processes[pid]->w_child = (struct ev_child*) malloc(sizeof(struct ev_child));
		processes[pid]->loop = loop;

		ev_child_init(processes[pid]->w_child, process_close_callback, pid, 0);
		ev_child_start(EV_DEFAULT_ processes[pid]->w_child);
	}
	return pid;
}

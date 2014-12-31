/*
 *	Copyright (C) 2014 Ovidiu Mara
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

#include "gitinfo.h"

#include <stdio.h>

#include "embed-file.h"

/*
// For this to work, the following needs to be added to the .pro file, just before SOURCES:
//
//
// gitinfobundle.target = gitinfo
// gitinfobundle.commands = cd .. ; \
//                         git status 2>&1 1> git-status.txt ; \
//                         git log --pretty=format:\'%h %an %ae %s (%ci) %d%\' -n 1  2>&1 1> git-log.txt ; \
//                         git diff 2>&1 1> git-diff.txt
// gitinfobundle.depends =
// QMAKE_EXTRA_TARGETS += gitinfobundle
// PRE_TARGETDEPS = gitinfo
//
//
// Projects built with make-remote.sh should not use it in the .pro file,
// and instead run those commands in make-remote.sh.
*/

BINDATA(gitStatus, ../git-status.txt)
BINDATA(gitLog, ../git-log.txt)
BINDATA(gitDiff, ../git-diff.txt)

void showGitInfo()
{
	fprintf(stderr, "--- Git status ---\n%s\n------------------\n", &gitStatus);
	fprintf(stderr, "--- Git log ---\n%s\n------------------\n", &gitLog);
	fprintf(stderr, "--- Git diff ---\n%s\n------------------\n", &gitDiff);
}

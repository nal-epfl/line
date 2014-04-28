/*
 *	Copyright (C) 2011 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
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

#include <QtCore>

#include "tomodata.h"
#include "netquest.h"
#include "binarytomo.h"
#include "tomodata_processing.h"

int main(int argc, char *argv[])
{
	argc--, argv++;

	if (argc != 2) {
		fprintf(stderr, "Wrong args\n");
		exit(1);
	}

	QString inputFile = argv[0];
	QString mode = argv[1];

	TomoData data;
	data.load(QString("%1").arg(inputFile));

	if (mode == "-test-eq" || mode == "-all")
		data.testEquations();

	if (mode == "-netquest" || mode == "-all")
		netquestLoss(data);

	if (mode == "-binary" || mode == "-all")
		binaryTomoLoss(data);

	if (mode == "-full" || mode == "-all")
    {
        //processTomoData(data, exp, writeLog, &htmlLog);
        //processTomoData(data);
    }

	return 0;
}

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

#ifndef SIMULATE_EXPERIMENT_H
#define SIMULATE_EXPERIMENT_H

#include "run_experiment_params.h"
#include "netgraph.h"

// Runs a simulation instead of an emulation. All the data is generated
// in exactly the same format (except logs, packet captures etc.)
bool runSimulation(NetGraph &g, RunParams runParams);

#endif // SIMULATE_EXPERIMENT_H

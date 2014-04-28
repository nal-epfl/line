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

#ifndef NICELABEL_H
#define NICELABEL_H

/*
 * loose_label: demonstrate loose labeling of data range from min to max.
 * (tight method is similar)
 * Params:
 * min = min value of axis
 * max = max value of axis
 * nticks = desired number of ticks (choose a good value based on text & graph width)
 * start = set to the coordinate of the first tick
 * size = set to the distance between 2 ticks
 */
void nice_loose_label(double min, double max, int nticks, double &start, double &size);

#endif // NICELABEL_H

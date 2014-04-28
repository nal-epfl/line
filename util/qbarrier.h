/*
 *	Copyright (C) 2012 alexisdm
 *  https://stackoverflow.com/questions/9637374/qt-synchronization-barrier
 *	Copyright (C) 2014 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 is the only version of this
 *  license which this program may be distributed under.
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

#ifndef QBARRIER_H
#define QBARRIER_H

#include <QMutex>
#include <QWaitCondition>
#include <QSharedPointer>

class QBarrier {
private:
	class QBarrierData
	{
	public:
		QBarrierData(int count)
			: count(count) {}

		void wait() {
			mutex.lock();
			--count;
			if (count > 0)
				condition.wait(&mutex);
			else
				condition.wakeAll();
			mutex.unlock();
		}

	private:
		Q_DISABLE_COPY(QBarrierData)
		int count;
		QMutex mutex;
		QWaitCondition condition;
	};

public:
	// Create a barrier that will wait for count threads
	QBarrier(int count) :
		d(new QBarrierData(count)) {}

	void wait() {
		d->wait();
	}

private:
	QSharedPointer<QBarrierData> d;
};

#endif // QBARRIER_H

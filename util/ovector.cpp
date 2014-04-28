/****************************************************************************
**
** Based on tests/auto/qvector/tst_qvector.cpp:
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
** Copyright (C) 2014 Ovidiu Mara
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "ovector.h"

#include <QtCore>
#include <QtTest/QtTest>

class tst_OVector
{
public:
	void runAllTests() {
		constructors();
		append();
		at();
		capacity();
		clear();
		contains();
		count();
		empty();
		endsWith();
		fill();
		first();
		fromList();
		fromStdVector();
		indexOf();
		insert();
		isEmpty();
		last();
		lastIndexOf();
		mid();
		prepend();
		remove();
		size();
		startsWith();
		swap();
		toList();
		toStdVector();
		value();
		testOperators();
		outOfMemory();
		QTBUG6416_reserve();
		initializeList();
	}

private:
	void constructors() const;
	void append() const;
	void at() const;
	void capacity() const;
	void clear() const;
	void contains() const;
	void count() const;
	void empty() const;
	void endsWith() const;
	void fill() const;
	void first() const;
	void fromList() const;
	void fromStdVector() const;
	void indexOf() const;
	void insert() const;
	void isEmpty() const;
	void last() const;
	void lastIndexOf() const;
	void mid() const;
	void prepend() const;
	void remove() const;
	void size() const;
	void startsWith() const;
	void swap() const;
	void toList() const;
	void toStdVector() const;
	void value() const;
	void testOperators() const;
	void outOfMemory();
	void QTBUG6416_reserve();
	void initializeList();
};

void tst_OVector::constructors() const
{
	// pre-reserve capacity
	{
		OVector<int> myvec(5);

		QVERIFY(myvec.capacity() == 5);
	}

	// default-initialise items
	{
		OVector<int> myvec(5, 42);

		QVERIFY(myvec.capacity() == 5);

		// make sure all items are initialised ok
		foreach (int meaningoflife, myvec) {
			QCOMPARE(meaningoflife, 42);
		}
	}
}

void tst_OVector::append() const
{
	OVector<int> myvec;
	myvec.append(42);
	myvec.append(43);
	myvec.append(44);

	QVERIFY(myvec.size() == 3);
	QCOMPARE(myvec, OVector<int>() << 42 << 43 << 44);
}

void tst_OVector::at() const
{
	OVector<QString> myvec;
	myvec << "foo" << "bar" << "baz";

	QVERIFY(myvec.size() == 3);
	QCOMPARE(myvec.at(0), QLatin1String("foo"));
	QCOMPARE(myvec.at(1), QLatin1String("bar"));
	QCOMPARE(myvec.at(2), QLatin1String("baz"));

	// append an item
	myvec << "hello";
	QVERIFY(myvec.size() == 4);
	QCOMPARE(myvec.at(0), QLatin1String("foo"));
	QCOMPARE(myvec.at(1), QLatin1String("bar"));
	QCOMPARE(myvec.at(2), QLatin1String("baz"));
	QCOMPARE(myvec.at(3), QLatin1String("hello"));

	// remove an item
	myvec.remove(1);
	QVERIFY(myvec.size() == 3);
	QCOMPARE(myvec.at(0), QLatin1String("foo"));
	QCOMPARE(myvec.at(1), QLatin1String("baz"));
	QCOMPARE(myvec.at(2), QLatin1String("hello"));
}

void tst_OVector::capacity() const
{
	OVector<QString> myvec;

	// TODO: is this guaranteed? seems a safe assumption, but I suppose preallocation of a
	// few items isn't an entirely unforseeable possibility.
	QVERIFY(myvec.capacity() == 0);

	// test it gets a size
	myvec << "aaa" << "bbb" << "ccc";
	QVERIFY(myvec.capacity() >= 3);

	// make sure it grows ok
	myvec << "aaa" << "bbb" << "ccc";
	QVERIFY(myvec.capacity() >= 6);

	// let's try squeeze a bit
	myvec.remove(3);
	myvec.remove(3);
	myvec.remove(3);
	// TODO: is this a safe assumption? presumably it won't release memory until shrink(), but can we asser that is true?
	QVERIFY(myvec.capacity() >= 6);
	myvec.squeeze();
	QVERIFY(myvec.capacity() >= 3);

	myvec.remove(0);
	myvec.remove(0);
	myvec.remove(0);
	// TODO: as above note
	QVERIFY(myvec.capacity() >= 3);
	myvec.squeeze();
	QVERIFY(myvec.capacity() == 0);
}

void tst_OVector::clear() const
{
	OVector<QString> myvec;
	myvec << "aaa" << "bbb" << "ccc";

	QVERIFY(myvec.size() == 3);
	myvec.clear();
	QVERIFY(myvec.size() == 0);
}

void tst_OVector::contains() const
{
	OVector<QString> myvec;
	myvec << "aaa" << "bbb" << "ccc";

	QVERIFY(myvec.contains(QLatin1String("aaa")));
	QVERIFY(myvec.contains(QLatin1String("bbb")));
	QVERIFY(myvec.contains(QLatin1String("ccc")));
	QVERIFY(!myvec.contains(QLatin1String("I don't exist")));

	// add it and make sure it does :)
	myvec.append(QLatin1String("I don't exist"));
	QVERIFY(myvec.contains(QLatin1String("I don't exist")));
}

void tst_OVector::count() const
{
	// total size
	{
		// zero size
		OVector<int> myvec;
		QVERIFY(myvec.count() == 0);

		// grow
		myvec.append(42);
		QVERIFY(myvec.count() == 1);
		myvec.append(42);
		QVERIFY(myvec.count() == 2);

		// shrink
		myvec.remove(0);
		QVERIFY(myvec.count() == 1);
		myvec.remove(0);
		QVERIFY(myvec.count() == 0);
	}

	// count of items
	{
		OVector<QString> myvec;
		myvec << "aaa" << "bbb" << "ccc";

		// initial tests
		QVERIFY(myvec.count(QLatin1String("aaa")) == 1);
		QVERIFY(myvec.count(QLatin1String("pirates")) == 0);

		// grow
		myvec.append(QLatin1String("aaa"));
		QVERIFY(myvec.count(QLatin1String("aaa")) == 2);

		// shrink
		myvec.remove(0);
		QVERIFY(myvec.count(QLatin1String("aaa")) == 1);
	}
}

void tst_OVector::empty() const
{
	OVector<int> myvec;

	// starts empty
	QVERIFY(myvec.empty());

	// not empty
	myvec.append(1);
	QVERIFY(!myvec.empty());

	// empty again
	myvec.remove(0);
	QVERIFY(myvec.empty());
}

void tst_OVector::endsWith() const
{
	OVector<int> myvec;

	// empty vector
	QVERIFY(!myvec.endsWith(1));

	// add the one, should work
	myvec.append(1);
	QVERIFY(myvec.endsWith(1));

	// add something else, fails now
	myvec.append(3);
	QVERIFY(!myvec.endsWith(1));

	// remove it again :)
	myvec.remove(1);
	QVERIFY(myvec.endsWith(1));
}

void tst_OVector::fill() const
{
	OVector<int> myvec;

	// resize
	myvec.resize(5);
	myvec.fill(69);
	QCOMPARE(myvec, OVector<int>() << 69 << 69 << 69 << 69 << 69);

	// make sure it can resize itself too
	myvec.fill(42, 10);
	QCOMPARE(myvec, OVector<int>() << 42 << 42 << 42 << 42 << 42 << 42 << 42 << 42 << 42 << 42);
}

void tst_OVector::first() const
{
	OVector<int> myvec;
	myvec << 69 << 42 << 3;

	// test it starts ok
	QCOMPARE(myvec.first(), 69);

	// test removal changes
	myvec.remove(0);
	QCOMPARE(myvec.first(), 42);

	// test prepend changes
	myvec.prepend(23);
	QCOMPARE(myvec.first(), 23);
}

void tst_OVector::fromList() const
{
	QList<QString> list;
	list << "aaa" << "bbb" << "ninjas" << "pirates";

	OVector<QString> myvec;
	myvec = OVector<QString>::fromList(list);

	// test it worked ok
	QCOMPARE(myvec, OVector<QString>() << "aaa" << "bbb" << "ninjas" << "pirates");
	QCOMPARE(list, QList<QString>() << "aaa" << "bbb" << "ninjas" << "pirates");
}

void tst_OVector::fromStdVector() const
{
	// stl = :(
	std::vector<QString> svec;
	svec.push_back(QLatin1String("aaa"));
	svec.push_back(QLatin1String("bbb"));
	svec.push_back(QLatin1String("ninjas"));
	svec.push_back(QLatin1String("pirates"));
	OVector<QString> myvec = OVector<QString>::fromStdVector(svec);

	// test it converts ok
	QCOMPARE(myvec, OVector<QString>() << "aaa" << "bbb" << "ninjas" << "pirates");
}

void tst_OVector::indexOf() const
{
	OVector<QString> myvec;
	myvec << "A" << "B" << "C" << "B" << "A";

	QVERIFY(myvec.indexOf("B") == 1);
	QVERIFY(myvec.indexOf("B", 1) == 1);
	QVERIFY(myvec.indexOf("B", 2) == 3);
	QVERIFY(myvec.indexOf("X") == -1);
	QVERIFY(myvec.indexOf("X", 2) == -1);

	// add an X
	myvec << "X";
	QVERIFY(myvec.indexOf("X") == 5);
	QVERIFY(myvec.indexOf("X", 5) == 5);
	QVERIFY(myvec.indexOf("X", 6) == -1);

	// remove first A
	myvec.remove(0);
	QVERIFY(myvec.indexOf("A") == 3);
	QVERIFY(myvec.indexOf("A", 3) == 3);
	QVERIFY(myvec.indexOf("A", 4) == -1);
}

void tst_OVector::insert() const
{
	OVector<QString> myvec;
	myvec << "A" << "B" << "C";

	// first position
	QCOMPARE(myvec.at(0), QLatin1String("A"));
	myvec.insert(0, QLatin1String("X"));
	QCOMPARE(myvec.at(0), QLatin1String("X"));
	QCOMPARE(myvec.at(1), QLatin1String("A"));

	// middle
	myvec.insert(1, QLatin1String("Z"));
	QCOMPARE(myvec.at(0), QLatin1String("X"));
	QCOMPARE(myvec.at(1), QLatin1String("Z"));
	QCOMPARE(myvec.at(2), QLatin1String("A"));

	// end
	myvec.insert(5, QLatin1String("T"));
	QCOMPARE(myvec.at(5), QLatin1String("T"));
	QCOMPARE(myvec.at(4), QLatin1String("C"));

	// insert a lot of garbage in the middle
	myvec.insert(2, 2, QLatin1String("infinity"));
	QCOMPARE(myvec, OVector<QString>() << "X" << "Z" << "infinity" << "infinity"
			 << "A" << "B" << "C" << "T");
}

void tst_OVector::isEmpty() const
{
	OVector<QString> myvec;

	// starts ok
	QVERIFY(myvec.isEmpty());

	// not empty now
	myvec.append(QLatin1String("hello there"));
	QVERIFY(!myvec.isEmpty());

	// empty again
	myvec.remove(0);
	QVERIFY(myvec.isEmpty());
}

void tst_OVector::last() const
{
	OVector<QString> myvec;
	myvec << "A" << "B" << "C";

	// test starts ok
	QCOMPARE(myvec.last(), QLatin1String("C"));

	// test it changes ok
	myvec.append(QLatin1String("X"));
	QCOMPARE(myvec.last(), QLatin1String("X"));

	// and remove again
	myvec.remove(3);
	QCOMPARE(myvec.last(), QLatin1String("C"));
}

void tst_OVector::lastIndexOf() const
{
	OVector<QString> myvec;
	myvec << "A" << "B" << "C" << "B" << "A";

	QVERIFY(myvec.lastIndexOf("B") == 3);
	QVERIFY(myvec.lastIndexOf("B", 2) == 1);
	QVERIFY(myvec.lastIndexOf("X") == -1);
	QVERIFY(myvec.lastIndexOf("X", 2) == -1);

	// add an X
	myvec << "X";
	QVERIFY(myvec.lastIndexOf("X") == 5);
	QVERIFY(myvec.lastIndexOf("X", 5) == 5);
	QVERIFY(myvec.lastIndexOf("X", 3) == -1);

	// remove first A
	myvec.remove(0);
	QVERIFY(myvec.lastIndexOf("A") == 3);
	QVERIFY(myvec.lastIndexOf("A", 3) == 3);
	QVERIFY(myvec.lastIndexOf("A", 2) == -1);
}

void tst_OVector::mid() const
{
	OVector<QString> list;
	list << "foo" << "bar" << "baz" << "bak" << "buck" << "hello" << "kitty";

	QCOMPARE(list.mid(3, 3), OVector<QString>() << "bak" << "buck" << "hello");
	QCOMPARE(list.mid(4), OVector<QString>() << "buck" << "hello" << "kitty");
}

void tst_OVector::prepend() const
{
	OVector<QString> myvec;
	myvec << "A" << "B" << "C";

	// starts ok
	QVERIFY(myvec.size() == 3);
	QCOMPARE(myvec.at(0), QLatin1String("A"));

	// add something
	myvec.prepend(QLatin1String("X"));
	QCOMPARE(myvec.at(0), QLatin1String("X"));
	QCOMPARE(myvec.at(1), QLatin1String("A"));
	QVERIFY(myvec.size() == 4);

	// something else
	myvec.prepend(QLatin1String("Z"));
	QCOMPARE(myvec.at(0), QLatin1String("Z"));
	QCOMPARE(myvec.at(1), QLatin1String("X"));
	QCOMPARE(myvec.at(2), QLatin1String("A"));
	QVERIFY(myvec.size() == 5);

	// clear and append to an empty vector
	myvec.clear();
	QVERIFY(myvec.size() == 0);
	myvec.prepend(QLatin1String("ninjas"));
	QVERIFY(myvec.size() == 1);
	QCOMPARE(myvec.at(0), QLatin1String("ninjas"));
}

void tst_OVector::remove() const
{
	OVector<QString> myvec;
	myvec << "A" << "B" << "C";

	// remove middle
	myvec.remove(1);
	QCOMPARE(myvec, OVector<QString>() << "A" << "C");

	// remove rest
	myvec.remove(0, 2);
	QCOMPARE(myvec, OVector<QString>());
}

// ::reserve() is really hard to think of tests for, so not doing it.
// ::resize() is tested in ::capacity().

void tst_OVector::size() const
{
	// total size
	{
		// zero size
		OVector<int> myvec;
		QVERIFY(myvec.size() == 0);

		// grow
		myvec.append(42);
		QVERIFY(myvec.size() == 1);
		myvec.append(42);
		QVERIFY(myvec.size() == 2);

		// shrink
		myvec.remove(0);
		QVERIFY(myvec.size() == 1);
		myvec.remove(0);
		QVERIFY(myvec.size() == 0);
	}
}

// ::squeeze() is tested in ::capacity().

void tst_OVector::startsWith() const
{
	OVector<int> myvec;

	// empty vector
	QVERIFY(!myvec.startsWith(1));

	// add the one, should work
	myvec.prepend(1);
	QVERIFY(myvec.startsWith(1));

	// add something else, fails now
	myvec.prepend(3);
	QVERIFY(!myvec.startsWith(1));

	// remove it again :)
	myvec.remove(0);
	QVERIFY(myvec.startsWith(1));
}

void tst_OVector::swap() const
{
	OVector<int> v1, v2;
	v1 << 1 << 2 << 3;
	v2 << 4 << 5 << 6;

	v1.swap(v2);
	QCOMPARE(v1,OVector<int>() << 4 << 5 << 6);
	QCOMPARE(v2,OVector<int>() << 1 << 2 << 3);
}

void tst_OVector::toList() const
{
	OVector<QString> myvec;
	myvec << "A" << "B" << "C";

	// make sure it converts and doesn't modify the original vector
	QCOMPARE(myvec.toList(), QList<QString>() << "A" << "B" << "C");
	QCOMPARE(myvec, OVector<QString>() << "A" << "B" << "C");
}

void tst_OVector::toStdVector() const
{
	OVector<QString> myvec;
	myvec << "A" << "B" << "C";

	std::vector<QString> svec = myvec.toStdVector();
	QCOMPARE(svec.at(0), QLatin1String("A"));
	QCOMPARE(svec.at(1), QLatin1String("B"));
	QCOMPARE(svec.at(2), QLatin1String("C"));

	QCOMPARE(myvec, OVector<QString>() << "A" << "B" << "C");
}

void tst_OVector::value() const
{
	OVector<QString> myvec;
	myvec << "A" << "B" << "C";

	// valid calls
	QCOMPARE(myvec.value(0), QLatin1String("A"));
	QCOMPARE(myvec.value(1), QLatin1String("B"));
	QCOMPARE(myvec.value(2), QLatin1String("C"));

	// default calls
	QCOMPARE(myvec.value(-1), QString());
	QCOMPARE(myvec.value(3), QString());

	// test calls with a provided default, valid calls
	QCOMPARE(myvec.value(0, QLatin1String("default")), QLatin1String("A"));
	QCOMPARE(myvec.value(1, QLatin1String("default")), QLatin1String("B"));
	QCOMPARE(myvec.value(2, QLatin1String("default")), QLatin1String("C"));

	// test calls with a provided default that will return the default
	QCOMPARE(myvec.value(-1, QLatin1String("default")), QLatin1String("default"));
	QCOMPARE(myvec.value(3, QLatin1String("default")), QLatin1String("default"));
}

void tst_OVector::testOperators() const
{
	OVector<QString> myvec;
	myvec << "A" << "B" << "C";
	OVector<QString> myvectwo;
	myvectwo << "D" << "E" << "F";
	OVector<QString> combined;
	combined << "A" << "B" << "C" << "D" << "E" << "F";

	// !=
	QVERIFY(myvec != myvectwo);

	// +
	QCOMPARE(myvec + myvectwo, combined);
	QCOMPARE(myvec, OVector<QString>() << "A" << "B" << "C");
	QCOMPARE(myvectwo, OVector<QString>() << "D" << "E" << "F");

	// +=
	myvec += myvectwo;
	QCOMPARE(myvec, combined);

	// ==
	QVERIFY(myvec == combined);

	// []
	QCOMPARE(myvec[0], QLatin1String("A"));
	QCOMPARE(myvec[1], QLatin1String("B"));
	QCOMPARE(myvec[2], QLatin1String("C"));
	QCOMPARE(myvec[3], QLatin1String("D"));
	QCOMPARE(myvec[4], QLatin1String("E"));
	QCOMPARE(myvec[5], QLatin1String("F"));
}


int fooCtor;
int fooDtor;

struct Foo
{
	int *p;

	Foo() { p = new int; ++fooCtor; }
	Foo(const Foo &other) { Q_UNUSED(other); p = new int; ++fooCtor; }

	void operator=(const Foo & /* other */) { }

	~Foo() { delete p; ++fooDtor; }
};

void tst_OVector::outOfMemory()
{
	fooCtor = 0;
	fooDtor = 0;

	//const int N = 0x7fffffff / sizeof(Foo);
	const int N = 0x7ffffff / sizeof(Foo);

	{
		OVector<Foo> a;

		//QSKIP("This test crashes on many of our machines.", SkipSingle);
		a.resize(N);
		if (a.size() == N) {
			QVERIFY(a.capacity() >= N);
			QCOMPARE(fooCtor, N);
			QCOMPARE(fooDtor, 0);

			for (int i = 0; i < N; i += 35000)
				a[i] = Foo();
		} else {
			// this is the case we're actually testing
			QCOMPARE(a.size(), 0);
			QCOMPARE(a.capacity(), 0);
			QCOMPARE(fooCtor, 0);
			QCOMPARE(fooDtor, 0);

			a.resize(5);
			QCOMPARE(a.size(), 5);
			QVERIFY(a.capacity() >= 5);
			QCOMPARE(fooCtor, 5);
			QCOMPARE(fooDtor, 0);

			const int Prealloc = a.capacity();
			a.resize(Prealloc + 1);
			QCOMPARE(a.size(), Prealloc + 1);
			QVERIFY(a.capacity() >= Prealloc + 1);
			QCOMPARE(fooCtor, Prealloc + 6);
			QCOMPARE(fooDtor, 5);

			a.resize(0x10000000);
			QCOMPARE(a.size(), 0);
			QCOMPARE(a.capacity(), 0);
			QCOMPARE(fooCtor, Prealloc + 6);
			QCOMPARE(fooDtor, Prealloc + 6);
		}
	}

	fooCtor = 0;
	fooDtor = 0;

	{
		OVector<Foo> a(N);
		if (a.size() == N) {
			QVERIFY(a.capacity() >= N);
			QCOMPARE(fooCtor, N);
			QCOMPARE(fooDtor, 0);

			for (int i = 0; i < N; i += 35000)
				a[i] = Foo();
		} else {
			// this is the case we're actually testing
			QCOMPARE(a.size(), 0);
			QCOMPARE(a.capacity(), 0);
			QCOMPARE(fooCtor, 0);
			QCOMPARE(fooDtor, 0);
		}
	}

	Foo foo;

	fooCtor = 0;
	fooDtor = 0;

	{
		OVector<Foo> a(N, foo);
		if (a.size() == N) {
			QVERIFY(a.capacity() >= N);
			QCOMPARE(fooCtor, N);
			QCOMPARE(fooDtor, 0);

			for (int i = 0; i < N; i += 35000)
				a[i] = Foo();
		} else {
			// this is the case we're actually testing
			QCOMPARE(a.size(), 0);
			QCOMPARE(a.capacity(), 0);
			QCOMPARE(fooCtor, 0);
			QCOMPARE(fooDtor, 0);
		}
	}

	fooCtor = 0;
	fooDtor = 0;

	{
		OVector<Foo> a;
		a.resize(10);
		QCOMPARE(fooCtor, 10);
		QCOMPARE(fooDtor, 0);

		OVector<Foo> b(a);
		QCOMPARE(fooCtor, 20);
		QCOMPARE(fooDtor, 0);

		a.resize(N);
		if (a.size() == N) {
			QCOMPARE(fooCtor, N + 20);
		} else {
			QCOMPARE(a.size(), 0);
			QCOMPARE(a.capacity(), 0);
			QCOMPARE(fooCtor, 10);
			QCOMPARE(fooDtor, 0);

			QCOMPARE(b.size(), 10);
			QVERIFY(b.capacity() >= 10);
		}
	}

	{
		OVector<int> a;
		a.resize(10);

		OVector<int> b(a);

		a.resize(N);
		if (a.size() == N) {
			for (int i = 0; i < N; i += 60000)
				a[i] = i;
		} else {
			QCOMPARE(a.size(), 0);
			QCOMPARE(a.capacity(), 0);

			QCOMPARE(b.size(), 10);
			QVERIFY(b.capacity() >= 10);
		}

		b.resize(N - 1);
		if (b.size() == N - 1) {
			for (int i = 0; i < N - 1; i += 60000)
				b[i] = i;
		} else {
			QCOMPARE(b.size(), 0);
			QCOMPARE(b.capacity(), 0);
		}
	}
}

void tst_OVector::QTBUG6416_reserve()
{
	fooCtor = 0;
	fooDtor = 0;
	{
		OVector<Foo> a;
		a.resize(2);
		OVector<Foo> b(a);
		b.reserve(1);
	}
	QCOMPARE(fooCtor, fooDtor);
}

void tst_OVector::initializeList()
{
#ifdef Q_COMPILER_INITIALIZER_LISTS
	OVector<int> v1{2,3,4};
	QCOMPARE(v1, OVector<int>() << 2 << 3 << 4);
	QCOMPARE(v1, (OVector<int>{2,3,4}));

	OVector<OVector<int>> v2{ v1, {1}, OVector<int>(), {2,3,4}  };
	OVector<OVector<int>> v3;
	v3 << v1 << (OVector<int>() << 1) << OVector<int>() << v1;
	QCOMPARE(v3, v2);
#endif
}

static void compareResults(const QVector<int> &ref, const OVector<int> &tst)
{
    if (tst.toVector() != ref) {
        qDebug() << "";
        qDebug() << "ref" << ref;
        qDebug() << "tst" << tst.toVector();
    }
    Q_ASSERT(tst.toVector() == ref);
    qDebug() << "Passed.";
}

template<class T>
void testScalarAppend1(T &v)
{
    v.append(1);
    v.append(2);
    v.append(3);
}

template<class T>
void testScalarAppend2(T &v)
{
    v.append(6);
    v.append(4);
    v.append(5);
}

template<class T>
void testScalarInsert1(T &v)
{
    v.insert(0, 9);
    v.insert(2, 7);
    v.insert(5, 8);
}

template<class T>
void testScalarInsert2(T &v)
{
    v.insert(0, 10, 9);
    v.insert(2, 20, 7);
    v.insert(5, 30, 8);
}

template<class T>
void testClear(T &v)
{
    v.clear();
}

#define BOTH(f, x, y) f(x); f(y); compareResults(x, y);

void testOVector()
{
	tst_OVector tester;
	tester.runAllTests();

    QVector<int> ref;
	OVector<int> tst;

    BOTH(testScalarInsert2, ref, tst);

    for (int i = 0; i < 100; i++) {
        BOTH(testScalarAppend1, ref, tst);
        BOTH(testScalarAppend2, ref, tst);
        BOTH(qSort, ref, tst);

        BOTH(testScalarInsert1, ref, tst);
        BOTH(qSort, ref, tst);

        BOTH(testScalarInsert2, ref, tst);
        BOTH(qSort, ref, tst);
    }

    BOTH(testClear, ref, tst);

    for (int i = 0; i < 100; i++) {
        BOTH(testScalarAppend1, ref, tst);
        BOTH(testScalarAppend2, ref, tst);
        BOTH(qSort, ref, tst);

        BOTH(testScalarInsert1, ref, tst);
        BOTH(testScalarInsert2, ref, tst);
        BOTH(qSort, ref, tst);
    }

    BOTH(testClear, ref, tst);
}

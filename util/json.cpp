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

#include "json.h"

// Qt's Q_ASSERT is removed in release mode, so use this instead
#define Q_ASSERT_FORCE(cond) if (!(cond)) { qt_assert(#cond,__FILE__,__LINE__); }; qt_noop()

static QString toHex(quint16 u) {
	return QString("%1").arg(u, 4, 16, QChar('0'));
}

QString toJson(QString s)
{
	QString result;
	result.reserve(s.length() + 2);

	result += '"';

	foreach (QChar c, s) {
		quint16 u = c.unicode();
		if (u < 0x20 || u == 0x22 || u == 0x5c) {
			result += '\\';
			switch (u) {
				case 0x22:
					result += '"';
					break;
				case 0x5c:
					result += '\\';
					break;
				case 0x8:
					result += 'b';
					break;
				case 0xc:
					result += 'f';
					break;
				case 0xa:
					result += 'n';
					break;
				case 0xd:
					result += 'r';
					break;
				case 0x9:
					result += 't';
					break;
				default:
					result += 'u';
					result += toHex(u);
			}
		} else {
			result += c;
		}
	}
	result += '"';
	return result;
}

QString toJson(QByteArray d)
{
	return toJson(QString::fromLatin1(d.toHex()));
}

QString toJson(quint64 d)
{
	return QString("%1").arg(d);
}

QString toJson(qint64 d)
{
	return QString("%1").arg(d);
}

QString toJson(quint32 d)
{
	return QString("%1").arg(d);
}

QString toJson(qint32 d)
{
	return QString("%1").arg(d);
}

QString toJson(quint16 d)
{
	return QString("%1").arg(d);
}

QString toJson(qint16 d)
{
	return QString("%1").arg(d);
}

QString toJson(quint8 d)
{
	return QString("%1").arg(d);
}

QString toJson(qint8 d)
{
	return QString("%1").arg(d);
}

QString toJson(double d)
{
	return QString("%1").arg(d);
}

QString toJson(float d)
{
	return QString("%1").arg(d);
}

QString toJson(bool d)
{
	return d ? "true" : "false";
}

QString toJson(const QPoint &d)
{
	JsonObjectPrinter p;
	p.addMember("x", d.x());
	p.addMember("y", d.y());
	return p.json();
}

QString toJson(const QPointF &d)
{
	JsonObjectPrinter p;
	p.addMember("x", d.x());
	p.addMember("y", d.y());
	return p.json();
}

/////// Testing

class TestJsonPerson {
public:
	TestJsonPerson(QString name = QString(), int age = 0)
		: name(name), age(age)
	{}

	QString name;
	int age;
};

QString toJson(const TestJsonPerson &d) {
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.name);
	jsonObjectPrinterAddMember(p, d.age);
	return p.json();
}

class TestJsonFamily {
public:
	TestJsonFamily()
		: nanny(NULL)
	{}

	TestJsonPerson mom;
	TestJsonPerson dad;
	QList<TestJsonPerson> children;

	TestJsonPerson *nanny;
};

QString toJson(const struct TestJsonFamily &d) {
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.mom);
	jsonObjectPrinterAddMember(p, d.dad);
	jsonObjectPrinterAddMember(p, d.children);
	jsonObjectPrinterAddMember(p, d.nanny);
	return p.json();
}

void testJson()
{
	int i = 42;
	Q_ASSERT_FORCE(toJson(i) == "42");

	double d = 3.14;
	Q_ASSERT_FORCE(toJson(d) == "3.14");
	Q_ASSERT_FORCE(toJson(&d) == "3.14");

	float f = 3.14;
	Q_ASSERT_FORCE(toJson(f) == "3.14");

	Q_ASSERT_FORCE(toJson(true) == "true");

	Q_ASSERT_FORCE(toJson(false) == "false");

	int *pi = &i;
	Q_ASSERT_FORCE(toJson(pi) == "42");

	pi = 0;
	Q_ASSERT_FORCE(toJson(pi) == "null");

	QString s = "abcdef";
	Q_ASSERT_FORCE(toJson(s) == "\"abcdef\"");

	s = "abc\ndef";
	Q_ASSERT_FORCE(toJson(s) == "\"abc\\ndef\"");

	s = "abc\tdef";
	Q_ASSERT_FORCE(toJson(s) == "\"abc\\tdef\"");

	s = "abc\rdef";
	Q_ASSERT_FORCE(toJson(s) == "\"abc\\rdef\"");

	s = "abc\fdef";
	Q_ASSERT_FORCE(toJson(s) == "\"abc\\fdef\"");

	s = "abc\\def";
	Q_ASSERT_FORCE(toJson(s) == "\"abc\\\\def\"");

	s = "abc\"def";
	Q_ASSERT_FORCE(toJson(s) == "\"abc\\\"def\"");

	s = QString("abc%1def").arg(QChar(0x0001));
	Q_ASSERT_FORCE(toJson(s) == "\"abc\\u0001def\"");

	QList<int> vi = QList<int>() << 1 << 2 << 3 << 4;
	Q_ASSERT_FORCE(toJson(vi) == "[1, 2, 3, 4]");
	Q_ASSERT_FORCE(toJson(vi) == toJson(vi.toVector()));

	QList<QString> vs = QList<QString>() << "ab\nc" << "def";
	Q_ASSERT_FORCE(toJson(vs) == "[\"ab\\nc\", \"def\"]");

	QList<QList<int> > vvi;
	vvi.append(QList<int>() << 1 << 2 << 3);
	vvi.append(QList<int>() << 4 << 5 << 6);
	Q_ASSERT_FORCE(toJson(vvi) == "[\n  [1, 2, 3],\n  [4, 5, 6]\n]");

	QList<QList<QList<int> > > vvvi;
	vvvi.append(QList<QList<int> >()
				<< (QList<int>() << 1 << 2 << 3)
				<< (QList<int>() << 4 << 5 << 6));
	vvvi.append(QList<QList<int> >()
				<< (QList<int>() << 7 << 8 << 9));
	Q_ASSERT_FORCE(toJson(vvvi) == "[\n  [\n    [1, 2, 3],\n    [4, 5, 6]\n  ],\n  [\n    [7, 8, 9]\n  ]\n]");

	TestJsonPerson john("John", 30);
	TestJsonPerson jane("Jane", 35);
	TestJsonPerson jack("Jack", 3);
	TestJsonPerson Jessica("Jessica", 5);
	TestJsonPerson mrsD("Mrs. D", 53);
	TestJsonFamily family;
	family.dad = john;
	family.mom = jane;
	family.children.append(jack);
	family.children.append(Jessica);

	Q_ASSERT_FORCE(toJson(john) == "{\n  \"name\": \"John\",\n  \"age\": 30\n}");
	Q_ASSERT_FORCE(toJson(family) ==
			 "{\n"
			 "  \"mom\": {\n"
			 "    \"name\": \"Jane\",\n"
			 "    \"age\": 35\n"
			 "  },\n"
			 "  \"dad\": {\n"
			 "    \"name\": \"John\",\n"
			 "    \"age\": 30\n"
			 "  },\n"
			 "  \"children\": [\n"
			 "    {\n"
			 "      \"name\": \"Jack\",\n"
			 "      \"age\": 3\n"
			 "    },\n"
			 "    {\n"
			 "      \"name\": \"Jessica\",\n"
			 "      \"age\": 5\n"
			 "    }\n"
			 "  ],\n"
			 "  \"nanny\": null\n"
			 "}"
			 );

	family.nanny = &mrsD;
	Q_ASSERT_FORCE(toJson(family) ==
			 "{\n"
			 "  \"mom\": {\n"
			 "    \"name\": \"Jane\",\n"
			 "    \"age\": 35\n"
			 "  },\n"
			 "  \"dad\": {\n"
			 "    \"name\": \"John\",\n"
			 "    \"age\": 30\n"
			 "  },\n"
			 "  \"children\": [\n"
			 "    {\n"
			 "      \"name\": \"Jack\",\n"
			 "      \"age\": 3\n"
			 "    },\n"
			 "    {\n"
			 "      \"name\": \"Jessica\",\n"
			 "      \"age\": 5\n"
			 "    }\n"
			 "  ],\n"
			 "  \"nanny\": {\n"
			 "    \"name\": \"Mrs. D\",\n"
			 "    \"age\": 53\n"
			 "  }\n"
			 "}"
			 );

	QSet<int> set12;
	set12.insert(1);
	set12.insert(2);
	Q_ASSERT_FORCE(toJson(set12) == "[1, 2]" ||
			 toJson(set12) == "[2, 1]");

	QHash<int, int> hash12;
	hash12[1] = 1;
	hash12[2] = 4;
	Q_ASSERT_FORCE(toJson(hash12) == "[\n"
							   "  {\n"
							   "    \"key\": 1,\n"
							   "    \"value\": 1\n"
							   "  },\n"
							   "  {\n"
							   "    \"key\": 2,\n"
							   "    \"value\": 4\n"
							   "  }\n"
							   "]" ||
			 toJson(hash12) == "[\n"
							   "  {\n"
							   "    \"key\": 2,\n"
							   "    \"value\": 4\n"
							   "  },\n"
							   "  {\n"
							   "    \"key\": 1,\n"
							   "    \"value\": 1\n"
							   "  }\n"
							   "]");

	QPair<int, int> pair12(1, 2);
	Q_ASSERT_FORCE(toJson(pair12) == "{\n"
							   "  \"first\": 1,\n"
							   "  \"second\": 2\n"
							   "}");
}

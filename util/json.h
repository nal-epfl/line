#ifndef JSON_H
#define JSON_H

// See http://json.org and http://www.ecma-international.org/publications/files/ECMA-ST/ECMA-404.pdf

#include <QtCore>

// Strings
QString toJson(QString s);
QString toJson(QByteArray d);

// Numbers
QString toJson(quint64 d);
QString toJson(qint64 d);
QString toJson(quint32 d);
QString toJson(qint32 d);
QString toJson(quint16 d);
QString toJson(qint16 d);
QString toJson(quint8 d);
QString toJson(qint8 d);
QString toJson(double d);
QString toJson(float d);

// Booleans
QString toJson(bool d);

// Pointers
template <typename T>
QString toJson(const T *p)
{
	if (!p)
		return "null";
	return toJson(*p);
}

// Other
QString toJson(const QPoint &d);
QString toJson(const QPointF &d);

// Arrays
template <typename T>
QString toJson(const QList<T> &v)
{
	QStringList parts;
	parts.reserve(v.count());
	bool isComplex = false;
	foreach (T t, v) {
		parts.append(toJson(t));
		isComplex = isComplex ||
					parts.last().contains("\n") ||
					parts.last().contains("[") ||
					parts.last().contains("{");
	}
	if (!isComplex) {
		return QString("[%1]").arg(parts.join(", "));
	}

	QString result = "[\n";
	QString middle = parts.join(",\n");
	foreach (QString line, middle.split('\n')) {
		result += "  " + line + "\n";
	}
	result += "]";
	return result;
}

template <typename T>
QString toJson(const QVector<T> &v)
{
	return toJson(v.toList());
}

template <typename T>
QString toJson(const QLinkedList<T> &v)
{
	return toJson(QList<T>::fromStdList(v.toStdList()));
}

// Objects
// For complex objects, define a function that uses the JsonObjectPrinter helper like this:
//
// QString toJson(const Person &d) {
//	 JsonObjectPrinter p;
//	 jsonObjectPrinterAddMember(p, d.name);
//	 jsonObjectPrinterAddMember(p, d.age);
//	 return p.json();
// }

class JsonObjectPrinter {
public:
	template<typename V>
    void addMember(const QString &name, const V &value) {
		names.append(toJson(name));
		values.append(toJson(value));
	}

	QString json() {
		QString result = "{\n";
		QStringList parts;
		for (int i = 0; i < names.length(); i++) {
			parts.append(QString("%1: %2").arg(names[i], values[i]));
		}
		QString middle = parts.join(",\n");
		foreach (QString line, middle.split('\n')) {
			result += "  " + line + "\n";
		}
		result += "}";
		return result;
	}

	static QString extractMemberName(QString s) {
		return s.split(".").last().split("->").last();
	}

protected:
	QStringList names;
	QStringList values;
};

#define jsonObjectPrinterAddMember(jop,x) jop.addMember(JsonObjectPrinter::extractMemberName(#x), x)

// Special: maps represented as lists of { "key": key, "value": value }
template <typename K, typename V>
QString toJson(const QHash<K, V> &h)
{
	QString result = "[\n";
	QStringList parts;
	foreach (K key, h.uniqueKeys()) {
		JsonObjectPrinter p;
		p.addMember("key", key);
		QList<V> values = h.values(key);
		if (values.count() == 1) {
			p.addMember("value", values.first());
		} else {
			p.addMember("value", values);
		}
		parts.append(p.json());
	}
	QString middle = parts.join(",\n");
	foreach (QString line, middle.split('\n')) {
		result += "  " + line + "\n";
	}
	result += "]";
	return result;
}

template <typename K, typename V>
QString toJson(const QMap<K, V> &h)
{
	QString result = "[\n";
	QStringList parts;
	foreach (K key, h.uniqueKeys()) {
		JsonObjectPrinter p;
		p.addMember("key", key);
		QList<V> values = h.values(key);
		if (values.count() == 1) {
			p.addMember("value", values.first());
		} else {
			p.addMember("value", values);
		}
		parts.append(p.json());
	}
	QString middle = parts.join(",\n");
	foreach (QString line, middle.split('\n')) {
		result += "  " + line + "\n";
	}
	result += "]";
	return result;
}

// Special: sets are represented as lists
template <typename T>
QString toJson(const QSet<T> &set)
{
	return toJson(set.toList());
}

// Special: pairs (a, b) are represented as { first: a, second: b }
template <typename T, typename U>
QString toJson(const QPair<T, U> &pair)
{
	JsonObjectPrinter p;
	p.addMember("first", pair.first);
	p.addMember("second", pair.second);
	return p.json();
}

// Testing
void testJson();

#endif // JSON_H

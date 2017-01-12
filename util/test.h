#ifndef TEST_H
#define TEST_H

#include <QtCore>

#define ASSERT(actual) \
do {\
  if (!(actual)) { \
    qDebug() << __FILE__ << __LINE__ << "Assertion failure:" \
         << "!" << #actual ", value:" << (actual);\
    abort(); \
  } \
} while (0)

#define COMPARE(actual, expected) \
do {\
  if ((actual) != (expected)) { \
    qDebug() << __FILE__ << __LINE__ << "Assertion failure:" \
         << #actual << "!=" << #expected \
         << "values:" << (actual) << "!=" << (expected);\
    abort(); \
  } \
} while (0)

#endif // TEST_H

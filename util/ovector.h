/****************************************************************************
**
** Copyright (C) 2014 Ovidiu Mara
**
** Based on QVector:
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef OVECTOR_H
#define OVECTOR_H

#include <stdlib.h>

#include <iterator>
#ifdef Q_COMPILER_INITIALIZER_LISTS
#include <initializer_list>
#endif
#include <vector>

#include <QDataStream>
#include <QDebug>
#include <QtGlobal>
#include <QList>
#include <QVector>

template <typename T, typename INT = qint32>
class OVector
{
public:
	// Constructs an empty vector.
    inline OVector()
        : data_(nullptr),
          first_(0),
          count_(0),
          size_(0) {}

	// Constructs a vector with an initial size of size elements.
	// The elements are initialized with a default-constructed value.
	explicit OVector(INT count)
        : data_(nullptr),
          first_(0),
          count_(count) {
        allocate(count_);
		for (INT i = 0; i < count_; i++) {
             new (&data_[offset(i)]) T();
        }
    }

	// Constructs a vector with an initial size of size elements. Each element is initialized with initialValue.
	OVector(INT count, const T &initialValue)
        : data_(nullptr),
          first_(0),
          count_(count) {
        allocate(count_);
		for (INT i = 0; i < count_; i++) {
			new (&data_[offset(i)]) T(initialValue);
        }
    }

	// Constructs a copy of other. Complexity: O(count).
    inline OVector(const OVector<T> &other)
        : data_(nullptr),
          first_(0),
          count_(other.count_) {
		allocate(other.size_);
		for (INT i = 0; i < count_; i++) {
            new (&data_[offset(i)]) T(other[i]);
        }
    }

#ifdef Q_COMPILER_INITIALIZER_LISTS
	// Construct a vector from the std::initilizer_list given by args. Complexity: O(count).
    inline OVector(std::initializer_list<T> args)
        : data_(nullptr),
          first_(0),
          count_(args.size()) {
        allocate(count_);

		for (INT i = 0; i < count_; i++) {
			new (&data_[offset(i)]) T(*(args.begin() + i));
        }
    }
#endif

	// Constructs a copy of other. Complexity: O(count).
	inline OVector(const QVector<T> &other)
		: OVector(OVector::fromVector(other)) {}

	// Constructs a copy of other. Complexity: O(count).
	inline OVector(const QList<T> &other)
		: OVector(OVector::fromList(other)) {}

	// Constructs a copy of other. Complexity: O(count).
	inline OVector(const std::vector<T> &other)
		: OVector(OVector::fromStdVector(other)) {}

	// Destroys the vector.
    inline ~OVector() {
        clear();
        deallocate();
    }

	// Assigns other to this vector and returns a reference to this vector. Complexity: O(count).
    OVector<T> &operator=(const OVector<T> &other) {
        clear();
        reallocate(other.size_);
        first_ = 0;
        count_ = other.count_;
		for (INT i = 0; i < count_; i++) {
            data_[offset(i)] = other[i];
        }
        return *this;
    }

#ifdef Q_COMPILER_RVALUE_REFS
	// Assigns other to this vector and returns a reference to this vector. Complexity: O(1).
    inline OVector<T> operator=(OVector<T> &&other) {
        swap(other);
        return *this;
    }
#endif

	// Swaps vector other with this vector. Complexity: O(1).
    inline void swap(OVector<T> &other) {
        qSwap(data_, other.data_);
        qSwap(first_, other.first_);
        qSwap(count_, other.count_);
        qSwap(size_, other.size_);
    }

	// Returns true if other is equal to this vector; otherwise returns false.
	// Two vectors are considered equal if they contain the same values in the same order.
	// This function requires the value type to have an implementation of operator==().
    bool operator==(const OVector<T> &other) const {
        if (this->count_ != other.count_)
            return false;
		for (INT i = first_; i < count_; i++) {
            if ((*this)[i] != other[i]) {
                return false;
            }
        }
        return true;
    }

	// Returns true if other is not equal to this vector; otherwise returns false. See operator==().
    inline bool operator!=(const OVector<T> &other) const {
        return !(*this == other);
    }

	// Returns the number of elements in the vector.
	inline INT size() const {
        return count_;
    }

	// Returns the number of elements in the vector.
	inline INT count() const {
		return count_;
	}

	// Returns true if the vector has size 0; otherwise returns false.
    inline bool isEmpty() const {
        return count_ == 0;
    }

	// Sets the size of the vector to size.
	// If size is greater than the current size, elements are added to the end, initialized with a
	// default-constructed value.
	// If size is less than the current size, elements are removed from the end.
	void resize(INT size) {
		if (size < count_) {
			remove(count_ - size, count_ - size);
		}
        reallocate(size);
        while (count_ < size) {
            append();
		}
    }

	// Returns the maximum number of elements that can be stored in the vector without forcing a reallocation.
	// The sole purpose of this function is to provide a means of fine tuning QVector's memory usage. In general, you
	// will rarely ever need to call this function.
	// If you want to know how many elements are in the vector, call size().
	inline INT capacity() const {
        return size_;
    }

	// Attempts to allocate memory for at least size elements. If you know in advance how large the vector will be, you
	// can call this function, and if you call resize() often you are likely to get better performance. If size is an
	// underestimate, the worst that will happen is that the QVector will be a bit slower.
	// The sole purpose of this function is to provide a means of fine tuning QVector's memory usage. In general, you
	// will rarely ever need to call this function.
	// If you want to change the number of elements of the vector, call resize().
	void reserve(INT size) {
		reallocate(qMax(size, count_), false);
    }

	// Releases any memory not required to store the elements.
	// The sole purpose of this function is to provide a means of fine tuning QVector's memory usage. In general, you
	// will rarely ever need to call this function.
    inline void squeeze() {
        reallocate(count_, true);
    }

	// Removes all the elements from the vector. Does not deallocate memory. This behavior is DIFFERENT compared to
	// QVector.
    void clear() {
		for (INT i = 0; i < count_; i++) {
            (&data_[offset(i)])->~T();
        }
        count_ = 0;
    }

	// Returns the item at index position i in the vector.
	// i must be a valid index position in the vector (i.e., 0 <= i < size()).
	const T &at(INT i) const {
		Q_ASSERT(i >= 0 && i < count_);
        return data_[offset(i)];
    }

	// Returns the item at index position i as a modifiable reference.
	// i must be a valid index position in the vector (i.e., 0 <= i < size()).
	T &operator[](INT i) {
		Q_ASSERT(i >= 0 && i < count_);
		return data_[offset(i)];
    }

	// Returns the item at index position i as a non-modifiable reference.
	// i must be a valid index position in the vector (i.e., 0 <= i < size()).
	const T &operator[](INT i) const {
		Q_ASSERT(i >= 0 && i < count_);
        return data_[offset(i)];
    }

	// Inserts an element at the end of the vector, initialized with a default-constructed value, and returns a
	// referece to the element.
	// This operation is relatively fast, because OVector typically allocates more memory than necessary, so it can
	// grow without reallocating the entire vector each time.
    T &append() {
        if (count_ >= size_) {
            reallocate(bigger());
        }
		count_++;
		new (&data_[offset(count_ - 1)]) T();
        return data_[offset(count_ - 1)];
    }

	// Inserts an element at the end of the vector.
	// This operation is relatively fast, because OVector typically allocates more memory than necessary, so it can
	// grow without reallocating the entire vector each time.
	void append(const T &element) {
        if (count_ >= size_) {
            reallocate(bigger());
		}
		count_++;
		new (&data_[offset(count_ - 1)]) T(element);
    }

	// Inserts an element at the beginning of the vector, initialized with a default-constructed value, and returns a
	// referece to the element.
	// If there is allocated memory available, the complexity is O(1), because OVector uses a circular buffer
	// internally to hold the elements.
	// This operation is relatively fast, because OVector typically allocates more memory than necessary, so it can
	// grow without reallocating the entire vector each time.
    T &prepend() {
        if (count_ >= size_) {
            reallocate(bigger());
        }
		first_ = first_ > 0 ? first_ - 1 : size_ - 1;
		count_++;
		new (&data_[offset(0)]) T();
        return data_[offset(0)];
    }

	// Inserts an element at the beginning of the vector.
	// If there is allocated memory available, the complexity is O(1), because OVector uses a circular buffer
	// internally to hold the elements.
	// This operation is relatively fast, because OVector typically allocates more memory than necessary, so it can
	// grow without reallocating the entire vector each time.
	void prepend(const T &element) {
        if (count_ >= size_) {
            reallocate(bigger());
        }
		first_ = first_ > 0 ? first_ - 1 : size_ - 1;
        count_++;
		new (&data_[offset(0)]) T(element);
    }

	// Inserts a new element, initialized with a default-constructed value, at index position i in the vector.
	// For large vectors, this operation can be slow (linear time), because it requires moving all the items at
	// indexes i and above/below by one position further in memory.
	T &insert(INT i) {
        insert(i, T());
    }

	// Inserts a new element at index position i in the vector.
	// For large vectors, this operation can be slow (linear time), because it requires moving all the items at
	// indexes i and above/below by one position further in memory.
	void insert(INT i, const T &element) {
		insert(i, 1, element);
    }

	// Inserts n elements, initialized with value, at index position i in the vector.
	// For large vectors, this operation can be slow (linear time), because it requires moving all the items at
	// indexes i and above/below by one position further in memory.
	// i must be a valid position in the vector (i.e., 0 <= i <= size()).
	void insert(INT i, INT n, const T &value) {
		Q_ASSERT(i >= 0 && i <= count_);
		if (n <= 0)
			return;
        if (count_ + n > size_) {
            reallocate(bigger(count_ + n));
        }
		if (i + n / 2 < count_ / 2) {
			for (INT j = 0 - n; j + n < i; j++) {
				qSwap(data_[offsetUnsafe(j)], data_[offsetUnsafe(j + n)]);
			}
			count_ += n;
			first_ -= n;
			while (first_ < 0)
				first_ += size_;
			for (INT j = i; j < i + n; j++) {
				new (&data_[offset(j)]) T(value);
			}
		} else {
			for (INT j = count_ - 1 + n; j - n >= i; j--) {
				qSwap(data_[offsetUnsafe(j)], data_[offsetUnsafe(j - n)]);
			}
			count_ += n;
			for (INT j = i; j < i + n; j++) {
				new (&data_[offset(j)]) T(value);
			}
		}
    }

	// Replaces the item at index position i with value.
	// i must be a valid index position in the vector (i.e., 0 <= i < size()).
	void replace(INT i, const T &t) {
		Q_ASSERT(i >= 0 && i < count_);
        data_[offset(i)] = t;
    }

	// Removes the element at index position i.
	// i must be a valid index position in the vector (i.e., 0 <= i < size()).
	void remove(INT i) {
		Q_ASSERT(i >= 0 && i < count_);
		remove(i, 1);
    }

	// Removes n elements starting from index position i.
	// i must be a valid index position in the vector (i.e., 0 <= i < size()).
	void remove(INT i, INT n) {
		Q_ASSERT(i >= 0 && i < count_);
		if (n <= 0)
			return;
		if (i + n / 2 < count_ / 2) {
			for (INT j = i; j >= 0; j--) {
				if (j - n >= 0) {
					qSwap(data_[offset(j)], data_[offset(j - n)]);
				} else {
					(&data_[offset(j)])->~T();
				}
			}
			first_ = (first_ + 1) % size_;
		} else {
			for (INT j = i; j < count_; j++) {
				if (j + n < count_) {
					qSwap(data_[offset(j)], data_[offset(j + n)]);
				} else {
					(&data_[offset(j)])->~T();
				}
			}
		}
		count_ -= n;
    }

	// Assigns value to all items in the vector.
	// If count is different from -1 (the default), the vector is resized to size count beforehand.
	OVector<T> &fill(const T &value, INT count = -1) {
        if (count > 0) {
            while (count < count_) {
                remove(count_ - 1);
            }
			reallocate(count);
			for (INT i = 0; i < count_; i++) {
				data_[offset(i)] = value;
            }
            while (count > count_) {
				append(value);
            }
        } else {
			for (INT i = 0; i < count_; i++) {
				data_[offset(i)] = value;
            }
        }
        return *this;
    }

	// Returns the index position of the first occurrence of value in the vector, searching forward from
	// index position from (by default, 0).
	// Returns -1 if no item matched.
	// This function requires the value type to have an implementation of operator==().
	INT indexOf(const T &value, INT from = 0) const {
		for (INT i = from; i < count_; i++) {
			if (data_[offset(i)] == value)
                return i;
        }
        return -1;
    }

	// Returns the index position of the last occurrence of the value value in the vector, searching backward
	// from index position from (by default, -1, signifying the last element).
	// Returns -1 if no item matched.
	// This function requires the value type to have an implementation of operator==().
	INT lastIndexOf(const T &value, INT from = -1) const {
		if (from < 0)
			from = count_ - 1;
		for (INT i = from; i >= 0; i--) {
			if (data_[offset(i)] == value)
                return i;
        }
        return -1;
    }

	// Returns true if the vector contains an occurrence of value; otherwise returns false.
	// This function requires the value type to have an implementation of operator==().
	bool contains(const T &value) const {
		return indexOf(value) >= 0;
    }

	// Returns the number of occurrences of value in the vector.
	// This function requires the value type to have an implementation of operator==().
	INT count(const T &value) const {
		INT result = 0;
		for (INT i = 0; i < count_; i++) {
			if (data_[offset(i)] == value)
                result++;
        }
        return result;
    }

	// Removes the first element from the vector.
	// The vector must not be empty.
	void removeFirst() {
		Q_ASSERT(!isEmpty());
		remove(0);
	}

	// Removes and returns the first element from the vector.
	// The vector must not be empty.
	T takeFirst() {
		Q_ASSERT(!isEmpty());
		T result(at(0));
		remove(0);
		return result;
	}

	// Removes the last element from the vector.
	// The vector must not be empty.
	void removeLast() {
		Q_ASSERT(!isEmpty());
		remove(count_ - 1);
	}

	// Removes and returns the last element from the vector.
	// The vector must not be empty.
	T takeLast() {
		Q_ASSERT(!isEmpty());
		T result(at(count_ - 1));
		remove(count_ - 1);
		return result;
	}

	// An STL-style non-const iterator.
    class iterator {
    public:
		OVector *v;
		INT i;

        typedef std::random_access_iterator_tag  iterator_category;
        typedef qptrdiff difference_type;
        typedef T value_type;
        typedef T *pointer;
        typedef T &reference;

        inline iterator()
            : v(nullptr),
              i(0) {}

		inline iterator(OVector *v, INT index)
            : v(v),
              i(index) {}

        inline iterator(const iterator &other)
            : v(other.v),
              i(other.i) {}

        inline T &operator*() const {
            return (*v)[i];
        }

        inline T *operator->() const {
            return &(*v)[i];
        }

		inline T &operator[](INT j) const {
            return (*v)[i + j];
        }

        inline bool operator==(const iterator &other) const {
            return v == other.v && i == other.i;
        }

        inline bool operator!=(const iterator &other) const {
            return !(*this == other);
        }

        inline bool operator<(const iterator& other) const {
            return i < other.i;
        }

        inline bool operator<=(const iterator& other) const {
            return i <= other.i;
        }

        inline bool operator>(const iterator& other) const {
            return i > other.i;
        }

        inline bool operator>=(const iterator& other) const {
            return i >= other.i;
        }

        inline iterator &operator++() {
            ++i;
            return *this;
        }

        inline iterator operator++(int) {
            iterator copy(*this);
            ++i;
            return copy;
        }

        inline iterator &operator--() {
            i--;
            return *this;
        }

        inline iterator operator--(int) {
            iterator copy(*this);
            i--;
            return copy;
        }

		inline iterator &operator+=(INT j) {
            i += j;
            return *this;
        }

		inline iterator &operator-=(INT j) {
            i -= j;
            return *this;
        }

		inline iterator operator+(INT j) const {
            return iterator(v, i + j);
        }

		inline iterator operator-(INT j) const {
            return iterator(v, i - j);
        }

		inline INT operator-(iterator j) const {
            return i - j.i;
        }
    };
    friend class iterator;

	// An STL-style const iterator.
    class const_iterator {
    public:
		const OVector *v;
		INT i;
        typedef std::random_access_iterator_tag  iterator_category;
        typedef qptrdiff difference_type;
        typedef T value_type;
        typedef const T *pointer;
        typedef const T &reference;

        inline const_iterator()
            : v(nullptr),
              i(0) {}

		inline const_iterator(const OVector *v, INT index)
            : v(v),
              i(index) {}

        inline const_iterator(const const_iterator &other)
            : v(other.v),
              i(other.i) {}

		inline const_iterator(const iterator &other)
            : v(other.v),
              i(other.i) {}

        inline const T &operator*() const {
            return (*v)[i];
        }

        inline const T *operator->() const {
            return &(*v)[i];
        }

		inline const T &operator[](INT j) const {
            return (*v)[i + j];
        }

        inline bool operator==(const const_iterator &other) const {
            return v == other.v && i == other.i;
        }

        inline bool operator!=(const const_iterator &other) const {
            return !(*this == other);
        }

        inline bool operator<(const const_iterator& other) const {
            return i < other.i;
        }

        inline bool operator<=(const const_iterator& other) const {
            return i <= other.i;
        }

        inline bool operator>(const const_iterator& other) const {
            return i > other.i;
        }

        inline bool operator>=(const const_iterator& other) const {
            return i >= other.i;
        }

        inline const_iterator &operator++() {
            ++i;
            return *this;
        }

        inline const_iterator operator++(int) {
            const_iterator n(*this);
            ++i;
            return n;
        }

        inline const_iterator &operator--() {
            i--;
            return *this;
        }

        inline const_iterator operator--(int) {
            const_iterator n(*this);
            i--;
            return n;
        }

		inline const_iterator &operator+=(INT j) {
            i += j;
            return *this;
        }

		inline const_iterator &operator-=(INT j) {
            i -= j;
            return *this;
        }

		inline const_iterator operator+(INT j) const {
            return const_iterator(i+j);
        }

		inline const_iterator operator-(INT j) const {
            return const_iterator(i-j);
        }

		inline INT operator-(const_iterator j) const {
            return i - j.i;
        }
    };
    friend class const_iterator;

	// Returns an STL-style iterator pointing to the first element in the vector.
    inline iterator begin() {
        return iterator(this, 0);
    }

	// Returns an STL-style const iterator pointing to the first element in the vector.
    inline const_iterator begin() const {
        return const_iterator(this, 0);
    }

	// Returns an STL-style const iterator pointing to the first element in the vector.
    inline const_iterator constBegin() const {
        return begin();
    }

	// Returns an STL-style iterator pointing to the imaginary element after the last element in the vector.
    inline iterator end() {
        return iterator(this, count_);
    }

	// Returns an STL-style const iterator pointing to the imaginary element after the last element in the vector.
    inline const_iterator end() const {
        return const_iterator(this, count_);
    }

	// Returns an STL-style const iterator pointing to the imaginary element after the last element in the vector.
    inline const_iterator constEnd() const {
        return end();
    }

	// Inserts n copies of value in front of the item pointed to by the iterator before.
	// Returns an iterator pointing at the first of the inserted items.
	iterator insert(iterator before, INT n, const T &value) {
		if (n > 0) {
			insert(before.i, n, value);
			return before++;
		} else {
			return before;
		}
    }

	// Inserts an element in front of the item pointed to by the iterator before.
	// Returns an iterator pointing at the inserted element.
	inline iterator insert(iterator before, const T &element) {
		return insert(before, 1, element);
    }

	// Removes all the elements from begin up to (but not including) end.
	// Returns an iterator to the same element that end referred to before the call.
    iterator erase(iterator begin, iterator end) {
        remove(begin.i, end.i - begin.i);
		return iterator(this, begin.i);
    }

	// Removes the element pointed to by the iterator pos from the vector.
	// Returns an iterator to the next element in the vector (or end()).
    inline iterator erase(iterator pos) {
        return erase(pos, pos + 1);
    }

	// Returns a reference to the first item in the vector.
	// The vector must not be empty.
    inline T& first() {
        Q_ASSERT(!isEmpty());
        return data_[offset(0)];
    }

	// Returns a const reference to the first item in the vector.
	// The vector must not be empty.
    inline const T &first() const {
        Q_ASSERT(!isEmpty());
        return data_[offset(0)];
    }

	// Returns a reference to the last item in the vector.
	// The vector must not be empty.
    inline T& last() {
        Q_ASSERT(!isEmpty());
        return data_[offset(count_ - 1)];
    }

	// Returns a const reference to the last item in the vector.
	// The vector must not be empty.
    inline const T &last() const {
        Q_ASSERT(!isEmpty());
        return data_[offset(count_ - 1)];
    }

	// Returns true if this vector is not empty and its first element is equal to value.
	inline bool startsWith(const T &value) const {
		return !isEmpty() && first() == value;
    }

	// Returns true if this vector is not empty and its last element is equal to value.
	inline bool endsWith(const T &value) const {
		return !isEmpty() && last() == value;
    }

	// Returns a vector whose elements are copied from this vector, starting at position pos.
	// If length is -1 (the default), all elements after pos are copied; otherwise length elements
	// (or all remaining elements if there are less than length elements) are copied.
	OVector<T> mid(INT pos, INT length = -1) const {
        if (length == -1)
            length = count_;
        OVector<T> other;
        other.reserve(length);
		for (INT i = pos; i < count_ && length > 0; i++, length--) {
            other.append(data_[offset(i)]);
        }
        return other;
    }

	// Returns the value at index position i in the vector.
	// If the index i is out of bounds, returns a default-constructed value.
	T value(INT i) const {
		return value(i, T());
    }

	// Returns the value at index position i in the vector.
	// If the index i is out of bounds, returns a defaultValue.
	T value(INT i, const T &defaultValue) const {
        if (i < 0 || i >= count_)
            return defaultValue;
        return at(i);
    }

    // STL compatibility
    typedef T value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef qptrdiff difference_type;
    typedef iterator Iterator;
    typedef const_iterator ConstIterator;
	typedef INT size_type;

	// Appends value to the vector.
	inline void push_back(const T &value) {
		append(value);
    }

	// Prepends value to the vector.
	inline void push_front(const T &value) {
		prepend(value);
    }

	// Removes the last element from the vector.
	// The vector must not be empty.
    void pop_back() {
        Q_ASSERT(!isEmpty());
        remove(count_ - 1);
    }

	// Removes the first element from the vector.
	// The vector must not be empty.
    void pop_front() {
        Q_ASSERT(!isEmpty());
        remove(0);
    }

	// Returns true if the vector is empty.
    inline bool empty() const {
        return isEmpty();
    }

	// Returns a reference to the first element.
	// The vector must not be empty.
    inline T& front() {
		Q_ASSERT(!isEmpty());
        return data_[offset(0)];
    }

	// Returns a const reference to the first element.
	// The vector must not be empty.
    inline const_reference front() const {
        return first();
    }

	// Returns a reference to the last element.
	// The vector must not be empty.
    inline reference back() {
        return last();
    }

	// Returns a const reference to the last element.
	// The vector must not be empty.
    inline const_reference back() const {
        return last();
    }

	// Appends the items of the other vector to this vector and returns a reference to this vector.
    OVector<T> &operator+=(const OVector<T> &other) {
		for (INT i = 0; i < other.count(); i++) {
            append(other[i]);
        }
        return *this;
    }

	// Appends the items of the other vector to a copy of this vector and returns the result.
    inline OVector<T> operator+(const OVector<T> &l) const {
        OVector n(*this);
        n += l;
        return n;
    }

	// Appends an element to this vector and returns a reference to this vector.
	inline OVector<T> &operator+=(const T &element) {
		append(element);
        return *this;
    }

	// Appends an element to this vector and returns a reference to this vector.
	inline OVector<T> &operator<<(const T &element) {
		append(element);
        return *this;
    }

	// Appends the items of the other vector to this vector and returns a reference to this vector.
	inline OVector<T> &operator<<(const OVector<T> &element) {
		*this += element;
        return *this;
    }

	// Returns a QVector containing the same elements as this vector.
    QVector<T> toVector() const {
        QVector<T> result;
		for (INT i = 0; i < count_; i++) {
            result.append(data_[offset(i)]);
        }
        return result;
    }

	// Returns an OVector containing the same elements as the QVector.
	static OVector<T> fromVector(const QVector<T> &other) {
        OVector<T> result;
        result.reserve(other.count());
		for (INT i = 0; i < other.count(); i++) {
            result.append(other[i]);
        }
        return result;
    }

	// Returns a QList containing the same elements as this vector.
    QList<T> toList() const {
        QList<T> result;
		for (INT i = 0; i < count_; i++) {
            result.append(data_[offset(i)]);
        }
        return result;
    }

	// Returns an OVector containing the same elements as the QList.
    static OVector<T> fromList(const QList<T> &other) {
        OVector<T> result;
        result.reserve(other.count());
		for (INT i = 0; i < other.count(); i++) {
            result.append(other[i]);
        }
        return result;
    }

	// Returns an std::vector containing the same elements as this vector.
	inline std::vector<T> toStdVector() const {
		std::vector<T> tmp;
		tmp.reserve(size());
		qCopy(constBegin(), constEnd(), std::back_inserter(tmp));
		return tmp;
	}

	// Returns an OVector containing the same elements as the std::vector.
    static inline OVector<T> fromStdVector(const std::vector<T> &vector) {
        OVector<T> tmp;
        tmp.reserve(vector.size());
        qCopy(vector.begin(), vector.end(), std::back_inserter(tmp));
        return tmp;
    }

protected:
	// Circular buffer containing the elements (or null).
	T *data_;

	// The index of the first element in data_.
	INT first_;

	// The number of elements.
	INT count_;

	// The amount of memory allocated, expressed as a number of elements.
	INT size_;

	// Allocates memory to hold size elements.
	// The buffer data_ must be null.
	void allocate(INT size) {
		Q_ASSERT(data_ == nullptr);
		if (size == 0) {
			size_ = 0;
		} else {
			size_ = size;
			data_ = static_cast<T*>(malloc(size_ * sizeof(T)));
		}
	}

	// Reallocates memory to hold a new number of elements, newSize.
	// Unless allowSqueeze is true, it does not release excess memory.
	// The new size muse be >= count().
	inline void reallocate(INT newSize, bool allowSqueeze = false) {
		Q_ASSERT(newSize >= count_);
		if (newSize <= size_ && !allowSqueeze) {
			return;
		}
		T *newData = static_cast<T*>(malloc(newSize * sizeof(T)));
		for (INT i = 0; i < count_; i++) {
			if (i < newSize) {
				new (&newData[i]) T(data_[offset(i)]);
			} else {
				(&data_[offset(i)])->~T();
			}
		}
		first_ = 0;
		free(data_);
		data_ = newData;
		size_ = newSize;
	}

	// Frees all memory.
	// The vector must be empty. If you need to empty it, use clear().
	void deallocate() {
		Q_ASSERT(count_ == 0);
		free(data_);
		data_ = nullptr;
		size_ = 0;
		first_ = 0;
		count_ = 0;
	}

	// Returns the index in the circular buffer, given a logical index index.
	// The index must be valid (no out of bounds access!).
	INT offset(INT index) const {
		Q_ASSERT(index >= 0 && index < count_);
		return offsetUnsafe(index);
	}

	// Returns the index in the circular buffer, given a logical index index.
	INT offsetUnsafe(INT index) const {
		if (size_ == 0)
			return 0;
		INT adjusted = first_ + index;
		while (adjusted < 0) {
			adjusted += size_;
		}
		return adjusted % size_;
	}

	// Computes a "nice" memory size bigger than the current size and minSize. Used to allocate memory in advance (not
	// for every insert/append).
	INT bigger(INT minSize = -1) const {
		if (minSize <= size_) {
			minSize = size_ + 1;
		}
		if (minSize < 1000000LL)
			return qMax(minSize + minSize, 1024);
		if (minSize < 100000000LL)
			return minSize + minSize / 2;
		if (minSize < 1000000000LL)
			return minSize + minSize / 4;
		return minSize + minSize / 10;
	}
};

#if defined(FORCE_UREF)
template <typename T>
inline QDebug &operator<<(QDebug debug, const OVector<T> &vec)
#else
template <typename T>
inline QDebug operator<<(QDebug debug, const OVector<T> &vec)
#endif
{
	debug.nospace() << "OVector";
	return operator<<(debug, vec.toList());
}

template<typename T>
QDataStream& operator<<(QDataStream& s, const OVector<T>& v)
{
	s << quint32(v.size());
	for (typename OVector<T>::const_iterator it = v.begin(); it != v.end(); ++it)
		s << *it;
	return s;
}

template<typename T>
QDataStream& operator>>(QDataStream& s, OVector<T>& v)
{
	v.clear();
	qint64 c;
	s >> c;
	v.resize(c);
	for (qint64 i = 0; i < c; ++i) {
		T t;
		s >> t;
		v[i] = t;
	}
	return s;
}

#ifdef TEST_OVECTOR
// Runs a battery of tests to check if OVector is working correctly. Slow.
void testOVector();
#endif

#endif // OVECTOR_H

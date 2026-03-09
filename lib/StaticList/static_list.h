#ifndef STATIC_LIST_H
#define STATIC_LIST_H

template<typename T, unsigned int N>
class StaticList {
private:
    T data[N];
    unsigned int size;

public:
    StaticList();

    bool push(const T& value);
    bool insert(unsigned int index, const T& value);
    bool remove(unsigned int index);

    T& operator[](unsigned int index);
    const T& operator[](unsigned int index) const;

    unsigned int length() const;
    bool isEmpty() const;
    bool isFull() const;

    void clear();
};

#include "static_list.tpp"

#endif

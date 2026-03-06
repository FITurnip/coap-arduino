template<typename T, size_t Capacity>
CircularQueue<T, Capacity>::CircularQueue()
: head(0), tail(0), count(0) {}

template<typename T, size_t Capacity>
bool CircularQueue<T, Capacity>::push(const T& item) {
    if(full()) return false;

    buffer[tail] = item;
    tail = (tail + 1) % Capacity;
    count++;
    return true;
}

template<typename T, size_t Capacity>
bool CircularQueue<T, Capacity>::pop() {
    if(empty()) return false;

    head = (head + 1) % Capacity;
    count--;
    return true;
}

template<typename T, size_t Capacity>
T& CircularQueue<T, Capacity>::front() {
    return buffer[head];
}

template<typename T, size_t Capacity>
const T& CircularQueue<T, Capacity>::front() const {
    return buffer[head];
}

template<typename T, size_t Capacity>
bool CircularQueue<T, Capacity>::empty() const {
    return count == 0;
}

template<typename T, size_t Capacity>
bool CircularQueue<T, Capacity>::full() const {
    return count == Capacity;
}

template<typename T, size_t Capacity>
size_t CircularQueue<T, Capacity>::size() const {
    return count;
}

template<typename T, size_t Capacity>
size_t CircularQueue<T, Capacity>::capacity() const {
    return Capacity;
}

template<typename T, size_t Capacity>
void CircularQueue<T, Capacity>::clear() {
    head = 0;
    tail = 0;
    count = 0;
}

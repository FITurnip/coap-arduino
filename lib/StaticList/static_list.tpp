#ifndef STATIC_LIST_TPP
#define STATIC_LIST_TPP

template<typename T, unsigned int N>
StaticList<T, N>::StaticList() : size(0) {}

// Menambahkan elemen di akhir
template<typename T, unsigned int N>
bool StaticList<T, N>::push(const T& value) {
    if (size >= N) return false;
    data[size++] = value;
    return true;
}

// Menyisipkan elemen pada posisi tertentu
template<typename T, unsigned int N>
bool StaticList<T, N>::insert(unsigned int index, const T& value) {
    if (size >= N || index > size) return false;
    for (unsigned int i = size; i > index; --i) {
        data[i] = data[i - 1];
    }
    data[index] = value;
    ++size;
    return true;
}

// Menghapus elemen pada posisi tertentu
template<typename T, unsigned int N>
bool StaticList<T, N>::remove(unsigned int index) {
    if (index >= size) return false;
    for (unsigned int i = index; i < size - 1; ++i) {
        data[i] = data[i + 1];
    }
    --size;
    return true;
}

// Operator akses elemen tanpa throw
template<typename T, unsigned int N>
T& StaticList<T, N>::operator[](unsigned int index) {
    if (index >= size) index = size ? size - 1 : 0; // fallback ke elemen terakhir atau 0
    return data[index];
}

template<typename T, unsigned int N>
const T& StaticList<T, N>::operator[](unsigned int index) const {
    if (index >= size) index = size ? size - 1 : 0; // fallback ke elemen terakhir atau 0
    return data[index];
}

// Mengembalikan jumlah elemen saat ini
template<typename T, unsigned int N>
unsigned int StaticList<T, N>::length() const {
    return size;
}

// Cek apakah kosong
template<typename T, unsigned int N>
bool StaticList<T, N>::isEmpty() const {
    return size == 0;
}

// Cek apakah penuh
template<typename T, unsigned int N>
bool StaticList<T, N>::isFull() const {
    return size >= N;
}

// Menghapus semua elemen
template<typename T, unsigned int N>
void StaticList<T, N>::clear() {
    size = 0;
}

#endif

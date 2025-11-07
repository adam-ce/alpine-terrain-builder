#include <array>
#include <initializer_list>
#include <stdexcept>

template <typename T, std::size_t N>
class FixedVector {
public:
    FixedVector() = default;

    FixedVector(std::initializer_list<T> init) {
        if (init.size() > N) {
            throw std::out_of_range("capacity exceeded");
        }
        for (auto &v : init) {
            this->_data[this->_size] = v;
            this->_size++;
        }
    }

    void push_back(const T &value) {
        if (this->_size >= N) {
            throw std::out_of_range("capacity exceeded");
        }
        this->_data[this->_size] = value;
        this->_size++;
    }

    void pop_back() {
        if (this->_size == 0) {
            throw std::out_of_range("empty vector");
        }
        this->_size--;
    }

    T &operator[](std::size_t i) {
        return this->_data[i];
    }
    const T &operator[](std::size_t i) const {
        return this->_data[i];
    }

    std::size_t size() const {
        return this->_size;
    }
    constexpr std::size_t capacity() const {
        return N;
    }
    bool empty() const {
        return this->_size == 0;
    }
    bool full() const {
        return this->_size == N;
    }

    T *begin() {
        return this->_data.data();
    }
    T *end() {
        return this->_data.data() + this->_size;
    }
    const T *begin() const {
        return this->_data.data();
    }
    const T *end() const {
        return this->_data.data() + this->_size;
    }

private:
    std::array<T, N> _data;
    std::size_t _size = 0;
};

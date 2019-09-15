#include <array>
#include <ostream>

template <typename T, size_t size> class WindowHistory {
public:
  WindowHistory() : _beg(0), _end(0), window(), _curr_size(0) {}

  void push(const T &item) {
    window[_end] = item;
    update_indices();
  }

  template <typename... Args> void emplace(const Args &... args) {
    new (&window[_end]) T(args...);
    update_indices();
  }

  T &operator[](size_t idx) { return window[idx]; }
  const T &operator[](size_t idx) const { return window[idx]; }
  T beg() const { return window[_beg]; }
  T end() const { return window[(size + _end - 1) % size]; }
  size_t curr_size() const { return _curr_size; }
  const size_t win_size = size;

private:
  std::array<T, size> window;
  size_t _beg;
  size_t _end;
  size_t _curr_size;

  void update_indices() {
    if (_curr_size < size) {
      ++_curr_size;
    } else {
      _beg = (_beg + 1) % size;
    }
    _end = (_end + 1) % size;
  }
};
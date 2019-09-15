#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include "windowhistory.hpp"

enum class DisplayComponent {
  None,
  ElapsedTime,
  RawProgress,
  ProgressBar,
  Percentage,
  EstimatedTime,
  Speed,
  All
};

struct PrinterBase {};

template <DisplayComponent dc> struct ComponentFormatter;

template <DisplayComponent... dcs> struct Printer;

template <typename T, DisplayComponent... dcs> class ProgressBar {
public:
  ProgressBar(const T &progress, const T &max_progress)
      : progress(progress), max_progress(max_progress), speed_history() {}

  void init() {
    start = std::chrono::steady_clock::now();
    history.emplace(start, progress);
    while (true) {
      print_progressbar();
      std::this_thread::sleep_for(
          std::chrono::milliseconds(sampling_freq_millis));
      if (progress == max_progress) {
        print_progressbar();
        plot_speed_history();
        break;
      }
    }
    time_point end = std::chrono::steady_clock::now();
    // std::cout << "\nJob completed in " << duration_sec(start, end) << " s" <<
    // std::endl;
  }

private:
  template <DisplayComponent dc> friend struct ComponentFormatter;
  typedef std::chrono::time_point<std::chrono::steady_clock> time_point;
  /* configurations */
  static constexpr const size_t bar_width = 50;
  static constexpr const size_t sampling_freq_millis = 100;
  static constexpr const char *fillchar = "█";
  static constexpr const char *emptychar = "―";
  static constexpr const size_t history_window_size = 20;
  static constexpr const size_t speed_history_size = 20;
  static constexpr const size_t speed_history_plot_height = 15;
  /* end configurations */

  /* members */
  const T &progress;
  const T &max_progress;
  WindowHistory<std::pair<time_point, T>, history_window_size> history;
  std::array<double, speed_history_size> speed_history;
  double current_speed;
  time_point start;
  time_point current;
  /* end members */

  double current_progress() const {
    return static_cast<double>(progress) / max_progress;
  }

  size_t map_progress(size_t upper) const { return current_progress() * upper; }

  double calculate_speed() const {
    T window_progress = history.end().second - history.beg().second;
    double time_taken = duration_sec(history.beg().first, history.end().first);
    return window_progress / time_taken;
  }

  static double duration_sec(const time_point &t1, const time_point &t2) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1)
               .count() /
           1000.0;
  }

  static std::string fmt_time(double tot_seconds) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << std::setw(2);
    size_t days = tot_seconds / (60 * 60 * 24);
    size_t hours = std::fmod(tot_seconds, 60 * 60 * 24) / (60 * 60);
    size_t minutes = std::fmod(tot_seconds, 60 * 60) / 60;
    double seconds = std::fmod(tot_seconds, 60);
    bool started = false;
    if (days > 0) {
      ss << days << 'd' << std::setfill('0');
      started = true;
    }
    if (started || hours > 0) {
      ss << hours << 'h' << std::setfill('0');
      started = true;
    }
    if (started || minutes > 0) {
      ss << minutes << 'm' << std::setfill('0');
    }
    ss << seconds << 's';
    std::string out = ss.str();
    if (out.size() > 15) {
      out = "-";
    }
    return out;
  }

  void store_speeds() {
    current_speed = calculate_speed();
    speed_history[map_progress(speed_history_size)] = current_speed;
  }

  void print_progressbar() {
    current = std::chrono::steady_clock::now();
    history.emplace(current, progress);
    store_speeds();
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    Printer<dcs...>::print(this, ss);
    std::string output = ss.str();
#if 0
        struct winsize size;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
        size_t columns = size.ws_col;
        size_t output_lines = (output.size() + columns - 1)/ columns;
        // for(size_t i = 0; i < output_lines - 1; i++) {
        //     std::cout << "\033[F";
        // }
#endif
    std::cout << "\33[2K\r" << output << std::flush;
  }

  void plot_speed_history() const {
    std::array<std::array<std::string, speed_history_size>,
               speed_history_plot_height>
        plot;
    for (auto &s : plot)
      for (auto &c : s)
        c = "·";
    double max_speed = 0;
    for (const auto &speed : speed_history) {
      max_speed = (max_speed < speed &&
                   speed != std::numeric_limits<double>::infinity())
                      ? speed
                      : max_speed;
    }
    for (size_t i = 0; i < speed_history_size; i++) {
      if (speed_history[i] != std::numeric_limits<double>::infinity()) {
        plot[(size_t)((speed_history[i] / max_speed) *
                      (speed_history_plot_height - 1))][i] = "⏺";
      }
    }
    std::cout << '\n';
    for (size_t i = 0; i < speed_history_plot_height; i++) {
      std::cout << "│ ";
      for (size_t j = 0; j < speed_history_size; j++) {
        std::cout << plot[speed_history_plot_height - 1 - i][j] << "  ";
      }
      std::cout << '\n';
    }
    std::cout << "└─";
    for (size_t i = 0; i < speed_history_size; i++) {
      std::cout << "───";
    }
    std::cout << "\n";
    for (size_t i = 1; i <= speed_history_size; i++) {
      std::cout << std::setw(3) << ((i * 100) / speed_history_size)
                << std::flush;
    }
    std::cout << std::endl;
  }
};

template <typename T, DisplayComponent... dcs>
constexpr const size_t ProgressBar<T, dcs...>::bar_width;
template <typename T, DisplayComponent... dcs>
constexpr const size_t ProgressBar<T, dcs...>::sampling_freq_millis;
template <typename T, DisplayComponent... dcs>
constexpr const char *ProgressBar<T, dcs...>::fillchar;
template <typename T, DisplayComponent... dcs>
constexpr const char *ProgressBar<T, dcs...>::emptychar;
template <typename T, DisplayComponent... dcs>
constexpr const size_t ProgressBar<T, dcs...>::history_window_size;
template <typename T, DisplayComponent... dcs>
constexpr const size_t ProgressBar<T, dcs...>::speed_history_size;
template <typename T, DisplayComponent... dcs>
constexpr const size_t ProgressBar<T, dcs...>::speed_history_plot_height;

template <DisplayComponent dc> struct ComponentFormatter {
  template <typename T, DisplayComponent... dcs>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {}
};

template <> struct ComponentFormatter<DisplayComponent::ElapsedTime> {
  template <typename T, DisplayComponent... dcs>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {
    double current_time =
        ProgressBar<T, dcs...>::duration_sec(pb->start, pb->current);
    ss << "[Elapsed: " << ProgressBar<T, dcs...>::fmt_time(current_time) << "]";
  }
};

template <> struct ComponentFormatter<DisplayComponent::RawProgress> {
  template <typename T, DisplayComponent... dcs>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {
    ss << "[Progress: " << pb->progress << '/' << pb->max_progress << " Ticks]";
  }
};

template <> struct ComponentFormatter<DisplayComponent::ProgressBar> {
  template <typename T, DisplayComponent... dcs>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {
    ss << "|";
    for (size_t i = 0; i < pb->bar_width; i++) {
      ss << ((i <= (pb->progress * ProgressBar<T, dcs...>::bar_width) /
                       pb->max_progress)
                 ? ProgressBar<T, dcs...>::fillchar
                 : ProgressBar<T, dcs...>::emptychar);
    }
    ss << "|";
  }
};

template <> struct ComponentFormatter<DisplayComponent::Percentage> {
  template <typename T, DisplayComponent... dcs>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {
    double current_progress_percentage =
        (pb->progress * 100.0) / pb->max_progress;
    ss << current_progress_percentage << "%";
  }
};

template <> struct ComponentFormatter<DisplayComponent::EstimatedTime> {
  template <typename T, DisplayComponent... dcs>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {
    double estimated_remaining_time =
        (pb->max_progress - pb->progress) / pb->current_speed;
    ss << "[Est.Remaining: "
       << ProgressBar<T, dcs...>::fmt_time(estimated_remaining_time) << "]";
  }
};

template <> struct ComponentFormatter<DisplayComponent::Speed> {
  template <typename T, DisplayComponent... dcs>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {
    ss << "[Speed: " << pb->current_speed << " Tick/s]";
  }
};

template <DisplayComponent dc> struct Enum {
  static constexpr const DisplayComponent previous =
      static_cast<DisplayComponent>(static_cast<size_t>(dc) - 1);
};

template <DisplayComponent dc> struct FormatAllImpl {
  template <typename T, DisplayComponent... dcs>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {
    FormatAllImpl<Enum<dc>::previous>::print(pb, ss);
    ComponentFormatter<Enum<dc>::previous>::print(pb, ss);
  }
};

template <> struct FormatAllImpl<DisplayComponent::None> {
  template <typename T, DisplayComponent... dcs>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {}
};

template <> struct ComponentFormatter<DisplayComponent::All> {
  template <typename T, DisplayComponent... dcs>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {
    FormatAllImpl<DisplayComponent::All>::print(pb, ss);
  }
};

template <DisplayComponent... dcs> struct Container {
  static constexpr const DisplayComponent dcsa[sizeof...(dcs)] = {dcs...};
};

template <size_t idx, DisplayComponent... dcs> struct Selector {
  static constexpr const DisplayComponent dc = Container<dcs...>::dcsa[idx];
};

template <size_t idx, DisplayComponent... dcs> struct PrinterImpl {
  template <typename T>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {
    PrinterImpl<idx - 1, dcs...>::print(pb, ss);
    ComponentFormatter<Selector<idx - 1, dcs...>::dc>::print(pb, ss);
  }
};

template <DisplayComponent... dcs> struct PrinterImpl<0, dcs...> {
  template <typename T>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {}
};

template <DisplayComponent... dcs> struct Printer {
  template <typename T>
  static void print(const ProgressBar<T, dcs...> *pb, std::ostringstream &ss) {
    PrinterImpl<sizeof...(dcs), dcs...>::print(pb, ss);
  }
};

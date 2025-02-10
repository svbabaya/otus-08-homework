#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>
#include <thread>

#include "CRC32.hpp"
#include "IO.hpp"


static bool is_success = false;
std::mutex search_mutex;

struct Range {
  Range(uint32_t start, uint32_t finish) : m_start_number(start), m_finish_number(finish) {}
  uint32_t m_start_number{};
  uint32_t m_finish_number{};
};

std::vector<Range> get_ranges(const uint32_t threads_amount, const uint32_t maxVal) {
  std::vector<Range> ranges;
  uint32_t part = maxVal / threads_amount;
  uint32_t start = 0;
  uint32_t finish = 0;
  for (size_t i = 0; i < threads_amount; ++i) {
    uint32_t s = start;
    if (i == threads_amount - 1) {
      finish = maxVal;
    } else {
      finish = start + part - 1;
    }
    start += part;
    Range range(s, finish);
    ranges.push_back(range);
  }
  return ranges;
}

/// @brief Переписывает последние 4 байта значением value
void replaceLastFourBytes(std::vector<char> &data, uint32_t value) {
  std::copy_n(reinterpret_cast<const char *>(&value), 4, data.end() - 4);
}

/**
 * 
 * @brief Формирует новый вектор с тем же CRC32, добавляя в конец оригинального
 * строку injection и дополнительные 4 байта
 * @details При формировании нового вектора последние 4 байта не несут полезной
 * нагрузки и подбираются таким образом, чтобы CRC32 нового и оригинального
 * вектора совпадали
 * @param original оригинальный вектор
 * @param injection произвольная строка, которая будет добавлена после данных
 * оригинального вектора
 * @return новый вектор
 */

void search_result(const uint32_t start,
                    const uint32_t finish, 
                    const uint32_t originalCrc32, 
                    std::vector<char>& result) {    

    std::lock_guard<std::mutex> lock(search_mutex);  
    if (is_success == true) {
      return;
    }

    for (size_t i = start; i <= finish; ++i) {  
    replaceLastFourBytes(result, uint32_t(i));
    auto currentCrc32 = crc32(result.data(), result.size());

    if (currentCrc32 == originalCrc32) {
      std::cout << "Success\n";
      is_success = true;
      return;
    }

    if (i % 1000 == 0) {
      std::cout << "progress: "
                << static_cast<double>(i) / static_cast<double>(finish)
                << std::endl;
    }
  }                  
}

std::vector<char> hack(const std::vector<char> &original,
                       const std::string &injection,
                       std::vector<Range>& ranges) {
  const uint32_t originalCrc32 = crc32(original.data(), original.size());

  std::vector<char> result(original.size() + injection.size() + 4);
  auto it = std::copy(original.begin(), original.end(), result.begin());
  std::copy(injection.begin(), injection.end(), it);

  std::vector<std::thread> threads;
  for (size_t i = 0; i < ranges.size(); ++i) {
    threads.push_back(std::thread(search_result, 
                      ranges[i].m_start_number, 
                      ranges[i].m_finish_number,
                      originalCrc32,
                      ref(result)));      
  }
  for (size_t i = 0; i < ranges.size(); ++i) {
    threads[i].join();
  }
  return result;
}

int main(int argc, char **argv) {
  int threads_amount = 1;
  if (argc != 4) {
    std::cerr << "Call with three args: " << argv[0]
              << " <input file> <output file> <threads amount 1...100>\n";
    return 1;
  }
  if (atoi(argv[3]) >= 1 && atoi(argv[3]) <= 100) {
    threads_amount = atoi(argv[3]);
  } else {
    std::cerr << "The third argument must be a number 1...100\n";
    return 1;
  }

  const std::string extra = "He-he-he";
  const size_t maxVal = std::numeric_limits<uint32_t>::max();
  std::vector<Range> ranges = get_ranges(threads_amount, maxVal);

  const auto start_time = std::chrono::steady_clock::now();
  try {
    const std::vector<char> data = readFromFile(argv[1]);

    const std::vector<char> badData = hack(data, extra, ranges);

    writeToFile(argv[2], badData);
  } catch (std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 2;
  }
  const std::chrono::duration<double> dur = std::chrono::steady_clock::now() - start_time;
  std::cout << "Duration: " << dur.count() << " sec" << std::endl;
  return 0;
}

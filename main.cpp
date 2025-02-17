#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>
#include <thread>

#include "CRC32.hpp"
#include "IO.hpp"

static bool is_success = false;

struct Range {
    Range(uint32_t start, uint32_t finish) : m_start_number(start), m_finish_number(finish) {}
    uint32_t m_start_number{};
    uint32_t m_finish_number{};
  };
  
std::vector<Range> get_ranges(const int threads_amount, const uint32_t maxVal) {
    std::vector<Range> ranges;
    uint32_t part = maxVal / static_cast<uint32_t>(threads_amount);
    uint32_t start = 0;
    uint32_t finish = 0;
    for (size_t i = 0; i < static_cast<size_t>(threads_amount); ++i) {
      uint32_t s = start;
      if (i == static_cast<size_t>(threads_amount) - 1) {
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
void hack(const std::vector<char> &original,
                    const std::string &injection,
                    const char* out_path,
                    const uint32_t start,
                    const uint32_t finish) {
  const uint32_t originalCrc32 = crc32(original.data(), original.size());
  std::vector<char> result(original.size() + injection.size() + 4);
  auto it = std::copy(original.begin(), original.end(), result.begin());
  std::copy(injection.begin(), injection.end(), it);

  /*
   * Внимание: код ниже крайне не оптимален.
   * В качестве доп. задания устраните избыточные вычисления
   */

  for (size_t i = start; i < finish; ++i) {
    if (is_success) {
        return;
    }
    // Заменяем последние четыре байта на значение i
    replaceLastFourBytes(result, uint32_t(i));
    // Вычисляем CRC32 текущего вектора result
    auto currentCrc32 = crc32(result.data(), result.size());

    if (currentCrc32 == originalCrc32) {
        writeToFile(out_path, result);
        is_success = true;
        std::cout << "Success\n";
        return;
    }
    // Отображаем прогресс
    if (i % 1000 == 0) {
      std::cout << "progress: "
                << static_cast<double>(i) / static_cast<double>(finish)
                << std::endl;
    }
  }
  throw std::logic_error("Can't hack");
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
    std::vector<std::thread> threads;
  
    const auto start_time = std::chrono::steady_clock::now();

  try {
    const std::vector<char> data = readFromFile(argv[1]);

    for (Range rng : ranges) {
        threads.emplace_back(hack, 
                                ref(data), 
                                ref(extra), 
                                argv[2], 
                                rng.m_start_number, 
                                rng.m_finish_number);
    }
    for (std::thread& t : threads) {
        t.join();
    }

  } catch (std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 2;
  }

  const std::chrono::duration<double> dur = std::chrono::steady_clock::now() - start_time;
  std::cout << "Duration: " << dur.count() << " sec" << std::endl;
  return 0;
}

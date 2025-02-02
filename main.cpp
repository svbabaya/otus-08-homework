#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>
#include <thread>

#include "CRC32.hpp"
#include "IO.hpp"

struct Range {
  Range(uint32_t start, uint32_t finish) : m_start_number(start), m_finish_number(finish) {}
  uint32_t m_start_number{};
  uint32_t m_finish_number{};
};

std::vector<Range> get_ranges(const size_t threads_amount, const size_t maxVal) {
  std::vector<Range> ranges;
  uint32_t part = maxVal / threads_amount;
  uint32_t start = 0;
  uint32_t finish = 0;
  for (size_t i = 0; i < threads_amount; ++i) {
    size_t s = start;
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
void replaceLastFourBytes(std::vector<char>& data, uint32_t value) {
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
// std::vector<char> hack(const std::vector<char> &original,
//                        const std::string &injection, 
//                        size_t maxVal) {
//   const uint32_t originalCrc32 = crc32(original.data(), original.size());
//   std::vector<char> result(original.size() + injection.size() + 4);
//   auto it = std::copy(original.begin(), original.end(), result.begin());
//   std::copy(injection.begin(), injection.end(), it);
//   /*
//    * Внимание: код ниже крайне не оптимален.
//    * В качестве доп. задания устраните избыточные вычисления
//    */
//   for (size_t i = 0; i < maxVal; ++i) {
//     // Заменяем последние четыре байта на значение i
//     replaceLastFourBytes(result, uint32_t(i));
//     // Вычисляем CRC32 текущего вектора result
//     auto currentCrc32 = crc32(result.data(), result.size());
//     if (currentCrc32 == originalCrc32) {
//       std::cout << "Success\n";
//       return result;
//     }
//     // Отображаем прогресс
//     if (i % 1000 == 0) {
//       std::cout << "progress: "
//                 << static_cast<double>(i) / static_cast<double>(maxVal)
//                 << std::endl;
//     }
//   }
//   throw std::logic_error("Can't hack");
// }

void search_result(const uint32_t start,
                    const uint32_t finish, 
                    const uint32_t originalCrc32, 
                    std::vector<char> &result) {
                      
    for (size_t i = start; i <= finish; ++i) {
    // Заменяем последние четыре байта на значение i
    replaceLastFourBytes(result, uint32_t(i));
    // Вычисляем CRC32 текущего вектора result
    auto currentCrc32 = crc32(result.data(), result.size());

    if (currentCrc32 == originalCrc32) {
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

std::vector<char> hack2(const std::vector<char> &original,
                       const std::string &injection, 
                       const std::vector<Range> &rng) {

const uint32_t originalCrc32 = crc32(original.data(), original.size());
std::vector<char> result(original.size() + injection.size() + 4);
auto it = std::copy(original.begin(), original.end(), result.begin());
std::copy(injection.begin(), injection.end(), it);

  uint32_t start1 = rng[0].m_start_number; 
  uint32_t finish1 = rng[0].m_finish_number;

  std::thread t1(search_result, start1, finish1, originalCrc32, ref(result));
  t1.join();

  return result;
  // search_result(start, finish, originalCrc32, result);

  // for (size_t i = start; i <= finish; ++i) {
  //   // Заменяем последние четыре байта на значение i
  //   replaceLastFourBytes(result, uint32_t(i));
  //   // Вычисляем CRC32 текущего вектора result
  //   auto currentCrc32 = crc32(result.data(), result.size());
  //   if (currentCrc32 == originalCrc32) {
  //     std::cout << "Success\n";
  //     return result;
  //   }
  //   // Отображаем прогресс
  //   if (i % 1000 == 0) {
  //     std::cout << "progress: "
  //               << static_cast<double>(i) / static_cast<double>(finish)
  //               << std::endl;
  //   }
  // }
  // throw std::logic_error("Can't hack");
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

  std::vector<Range> ranges;
  const std::string extra = "He-he-he";
  const size_t maxVal = std::numeric_limits<uint32_t>::max();
  try {
    const std::vector<char> data = readFromFile(argv[1]);
    // const std::vector<char> badData = hack(data, extra, maxVal);
    const std::vector<char> badData = hack2(data, extra, get_ranges(threads_amount, maxVal));
    writeToFile(argv[2], badData);
  } catch (std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 2;
  }
  return 0;
}

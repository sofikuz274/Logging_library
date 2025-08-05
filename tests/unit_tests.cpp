#include "logging/Logger.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <filesystem>
#include <chrono>
#include <functional>

/**
 * @brief Простой фреймворк для тестирования
 */
class TestRunner {
private:
    int totalTests = 0;
    int passedTests = 0;

public:
    void runTest(const std::string& testName, std::function<void()> testFunc) {
        totalTests++;
        std::cout << "Выполняется тест: " << testName << "... ";
        
        try {
            testFunc();
            passedTests++;
            std::cout << "PASSED" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "FAILED: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "FAILED: Неизвестная ошибка" << std::endl;
        }
    }
    
    void printSummary() {
        std::cout << "\n=== РЕЗУЛЬТАТЫ ТЕСТИРОВАНИЯ ===" << std::endl;
        std::cout << "Всего тестов: " << totalTests << std::endl;
        std::cout << "Прошло: " << passedTests << std::endl;
        std::cout << "Провалилось: " << (totalTests - passedTests) << std::endl;
        
        if (passedTests == totalTests) {
            std::cout << "Все тесты ПРОШЛИ!" << std::endl;
        } else {
            std::cout << "Некоторые тесты ПРОВАЛИЛИСЬ!" << std::endl;
        }
        std::cout << "===============================" << std::endl;
    }
    
    bool allTestsPassed() const {
        return passedTests == totalTests;
    }
};

/**
 * @brief Утверждение для тестов
 */
#define ASSERT(condition, message) \
    if (!(condition)) { \
        throw std::runtime_error(message); \
    }

/**
 * @brief Чтение содержимого файла
 */
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return "";
    }
    
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/**
 * @brief Подсчет строк в файле
 */
size_t countLines(const std::string& content) {
    return std::count(content.begin(), content.end(), '\n');
}

/**
 * @brief Удаление тестового файла
 */
void cleanupFile(const std::string& filename) {
    std::filesystem::remove(filename);
}

/**
 * @brief Тест базовой функциональности логирования
 */
void testBasicLogging() {
    const std::string testFile = "test_basic.log";
    cleanupFile(testFile);
    
    {
        logging::Logger logger(testFile, logging::LogLevel::INFO);
        ASSERT(logger.isValid(), "Логгер должен быть валидным");
        
        ASSERT(logger.log("Test message"), "Запись сообщения должна пройти успешно");
        ASSERT(logger.info("Info message"), "Запись INFO сообщения должна пройти успешно");
        ASSERT(logger.warning("Warning message"), "Запись WARNING сообщения должна пройти успешно");
    }
    
    std::string content = readFile(testFile);
    ASSERT(!content.empty(), "Файл лога должен содержать данные");
    ASSERT(content.find("Test message") != std::string::npos, "Файл должен содержать 'Test message'");
    ASSERT(content.find("Info message") != std::string::npos, "Файл должен содержать 'Info message'");
    ASSERT(content.find("Warning message") != std::string::npos, "Файл должен содержать 'Warning message'");
    
    cleanupFile(testFile);
}

/**
 * @brief Тест фильтрации по уровням важности
 */
void testLogLevelFiltering() {
    const std::string testFile = "test_levels.log";
    cleanupFile(testFile);
    
    {
        logging::Logger logger(testFile, logging::LogLevel::WARNING);
        
        // Эти сообщения не должны записаться
        logger.debug("Debug message");
        logger.info("Info message");
        
        // Это сообщение должно записаться
        logger.warning("Warning message");
    }
    
    std::string content = readFile(testFile);
    ASSERT(content.find("Debug message") == std::string::npos, "DEBUG сообщения не должны записываться");
    ASSERT(content.find("Info message") == std::string::npos, "INFO сообщения не должны записываться");
    ASSERT(content.find("Warning message") != std::string::npos, "WARNING сообщения должны записываться");
    
    cleanupFile(testFile);
}

/**
 * @brief Тест изменения уровня важности по умолчанию
 */
void testChangeDefaultLevel() {
    const std::string testFile = "test_change_level.log";
    cleanupFile(testFile);
    
    {
        logging::Logger logger(testFile, logging::LogLevel::WARNING);
        ASSERT(logger.getDefaultLevel() == logging::LogLevel::WARNING, "Начальный уровень должен быть WARNING");
        
        logger.setDefaultLevel(logging::LogLevel::DEBUG);
        ASSERT(logger.getDefaultLevel() == logging::LogLevel::DEBUG, "Уровень должен измениться на DEBUG");
        
        // Теперь все сообщения должны записываться
        logger.debug("Debug message");
        logger.info("Info message");
        logger.warning("Warning message");
    }
    
    std::string content = readFile(testFile);
    ASSERT(content.find("Debug message") != std::string::npos, "DEBUG сообщения должны записываться");
    ASSERT(content.find("Info message") != std::string::npos, "INFO сообщения должны записываться");
    ASSERT(content.find("Warning message") != std::string::npos, "WARNING сообщения должны записываться");
    
    cleanupFile(testFile);
}

/**
 * @brief Тест многопоточной записи
 */
void testMultithreading() {
    const std::string testFile = "test_multithreading.log";
    cleanupFile(testFile);
    
    {
        logging::Logger logger(testFile, logging::LogLevel::INFO);
        
        const int numThreads = 5;
        const int messagesPerThread = 10;
        std::vector<std::thread> threads;
        
        // Запуск потоков
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([&logger, i, messagesPerThread]() {
                for (int j = 0; j < messagesPerThread; ++j) {
                    std::ostringstream oss;
                    oss << "Thread " << i << " message " << j;
                    logger.info(oss.str());
                }
            });
        }
        
        // Ожидание завершения всех потоков
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
    std::string content = readFile(testFile);
    size_t lineCount = countLines(content);
    
    // Должно быть записано numThreads * messagesPerThread сообщений
    ASSERT(lineCount == 50, "Должно быть записано 50 строк");
    
    cleanupFile(testFile);
}

/**
 * @brief Тест форматирования сообщений
 */
void testMessageFormatting() {
    const std::string testFile = "test_formatting.log";
    cleanupFile(testFile);
    
    {
        logging::Logger logger(testFile, logging::LogLevel::DEBUG);
        logger.log("Test message", logging::LogLevel::INFO);
    }
    
    std::string content = readFile(testFile);
    
    // Проверяем наличие основных компонентов формата
    ASSERT(content.find("[INFO]") != std::string::npos, "Сообщение должно содержать уровень [INFO]");
    ASSERT(content.find("Test message") != std::string::npos, "Сообщение должно содержать текст");
    
    // Проверяем наличие временной метки (формат YYYY-MM-DD)
    ASSERT(content.find("[2") != std::string::npos, "Сообщение должно содержать временную метку");
    
    cleanupFile(testFile);
}

/**
 * @brief Тест преобразования уровней логирования
 */
void testLogLevelConversion() {
    ASSERT(logging::logLevelToString(logging::LogLevel::DEBUG) == "DEBUG", "DEBUG должен преобразовываться в 'DEBUG'");
    ASSERT(logging::logLevelToString(logging::LogLevel::INFO) == "INFO", "INFO должен преобразовываться в 'INFO'");
    ASSERT(logging::logLevelToString(logging::LogLevel::WARNING) == "WARNING", "WARNING должен преобразовываться в 'WARNING'");
    
    ASSERT(logging::stringToLogLevel("DEBUG") == logging::LogLevel::DEBUG, "'DEBUG' должен преобразовываться в DEBUG");
    ASSERT(logging::stringToLogLevel("info") == logging::LogLevel::INFO, "'info' должен преобразовываться в INFO");
    ASSERT(logging::stringToLogLevel("Warning") == logging::LogLevel::WARNING, "'Warning' должен преобразовываться в WARNING");
    ASSERT(logging::stringToLogLevel("unknown") == logging::LogLevel::INFO, "Неизвестный уровень должен возвращать INFO");
}

/**
 * @brief Тест обработки ошибок
 */
void testErrorHandling() {
    // Тест с недоступным файлом
    const std::string invalidFile = "/root/inaccessible.log";
    logging::Logger logger(invalidFile, logging::LogLevel::INFO);
    
    // Логгер может не быть валидным, если файл недоступен
    // Но это не должно приводить к краху программы
    logger.log("This should not crash");
}

/**
 * @brief Тест логирования пустых и очень больших сообщений
 */
void testEdgeCaseMessages() {
    const std::string testFile = "test_edge_cases.log";
    cleanupFile(testFile);
    
    {
        logging::Logger logger(testFile, logging::LogLevel::DEBUG);
        
        // Пустое сообщение
        ASSERT(logger.log(""), "Пустое сообщение должно быть записано");
        
        // Очень большое сообщение (1KB)
        std::string largeMessage(1024, 'A');
        ASSERT(logger.log(largeMessage), "Большое сообщение должно быть записано");
        
        // Сообщение с специальными символами
        ASSERT(logger.log("Message with \t tabs \n newlines \r returns"), 
               "Сообщение со специальными символами должно быть записано");
        
        // Сообщение с unicode символами
        ASSERT(logger.log("Unicode: привет мир 你好世界 🌍"), 
               "Сообщение с unicode должно быть записано");
    }
    
    std::string content = readFile(testFile);
    ASSERT(content.find("Unicode: привет мир 你好世界 🌍") != std::string::npos, 
           "Unicode сообщение должно быть в файле");
    
    cleanupFile(testFile);
}

/**
 * @brief Тест производительности логирования
 */
void testPerformance() {
    const std::string testFile = "test_performance.log";
    cleanupFile(testFile);
    
    {
        logging::Logger logger(testFile, logging::LogLevel::INFO);
        
        const int numMessages = 1000; // Уменьшаем количество сообщений для реалистичного теста
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numMessages; ++i) {
            logger.info("Performance test message " + std::to_string(i));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Проверяем, что 1000 сообщений записались за разумное время (< 5 секунд)
        ASSERT(duration.count() < 5000, "Производительность должна быть приемлемой");
        
        std::string content = readFile(testFile);
        size_t lineCount = countLines(content);
        ASSERT(lineCount == numMessages, "Должно быть записано все сообщения");
    }
    
    cleanupFile(testFile);
}

/**
 * @brief Тест стрессового многопоточного логирования
 */
void testStressMultithreading() {
    const std::string testFile = "test_stress_threads.log";
    cleanupFile(testFile);
    
    {
        logging::Logger logger(testFile, logging::LogLevel::INFO);
        
        const int numThreads = 20;
        const int messagesPerThread = 100;
        std::vector<std::thread> threads;
        
        // Запуск потоков с разными типами сообщений
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([&logger, i, messagesPerThread]() {
                for (int j = 0; j < messagesPerThread; ++j) {
                    switch (j % 3) {
                        case 0:
                            logger.debug("Debug from thread " + std::to_string(i) + " message " + std::to_string(j));
                            break;
                        case 1:
                            logger.info("Info from thread " + std::to_string(i) + " message " + std::to_string(j));
                            break;
                        case 2:
                            logger.warning("Warning from thread " + std::to_string(i) + " message " + std::to_string(j));
                            break;
                    }
                }
            });
        }
        
        // Ожидание завершения всех потоков
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
    std::string content = readFile(testFile);
    size_t lineCount = countLines(content);
    
    // Должно быть записано 20 * 100 = 2000 сообщений
    ASSERT(lineCount == 2000, "Должно быть записано все сообщения из всех потоков");
    
    cleanupFile(testFile);
}

/**
 * @brief Тест инициализации логгера с различными параметрами
 */
void testLoggerInitialization() {
    // Тест с пустым именем файла
    {
        logging::Logger logger("", logging::LogLevel::INFO);
        ASSERT(!logger.isValid(), "Логгер с пустым именем файла должен быть невалидным");
    }
    
    // Тест с очень длинным именем файла
    {
        std::string longFilename(300, 'a');
        longFilename += ".log";
        logging::Logger logger(longFilename, logging::LogLevel::INFO);
        // Должен быть валидным, если система поддерживает такие длинные имена
    }
    
    // Тест с именем файла содержащим специальные символы
    {
        const std::string specialFile = "test_special_chars_!@#$%^&().log";
        cleanupFile(specialFile);
        
        logging::Logger logger(specialFile, logging::LogLevel::INFO);
        ASSERT(logger.isValid(), "Логгер с специальными символами в имени должен быть валидным");
        ASSERT(logger.log("Test message"), "Сообщение должно быть записано");
        
        cleanupFile(specialFile);
    }
}

/**
 * @brief Тест последовательного открытия/закрытия логгера
 */
void testRepeatedOpenClose() {
    const std::string testFile = "test_repeated.log";
    cleanupFile(testFile);
    
    // Многократное создание и уничтожение логгера
    for (int i = 0; i < 10; ++i) {
        logging::Logger logger(testFile, logging::LogLevel::INFO);
        ASSERT(logger.isValid(), "Логгер должен быть валидным при каждом создании");
        ASSERT(logger.log("Message " + std::to_string(i)), 
               "Сообщение должно быть записано");
    }
    
    std::string content = readFile(testFile);
    ASSERT(content.find("Message 9") != std::string::npos, 
           "Последнее сообщение должно быть в файле");
    
    cleanupFile(testFile);
}

/**
 * @brief Тест логирования с различными уровнями важности
 */
void testAllLogLevels() {
    const std::string testFile = "test_all_levels.log";
    cleanupFile(testFile);
    
    {
        logging::Logger logger(testFile, logging::LogLevel::DEBUG);
        
        // Тест всех уровней
        ASSERT(logger.debug("Debug message"), "DEBUG сообщение должно быть записано");
        ASSERT(logger.info("Info message"), "INFO сообщение должно быть записано");
        ASSERT(logger.warning("Warning message"), "WARNING сообщение должно быть записано");
        
        // Тест явного указания уровня
        ASSERT(logger.log("Explicit debug", logging::LogLevel::DEBUG), 
               "Явное DEBUG сообщение должно быть записано");
        ASSERT(logger.log("Explicit info", logging::LogLevel::INFO), 
               "Явное INFO сообщение должно быть записано");
        ASSERT(logger.log("Explicit warning", logging::LogLevel::WARNING), 
               "Явное WARNING сообщение должно быть записано");
    }
    
    std::string content = readFile(testFile);
    ASSERT(content.find("[DEBUG]") != std::string::npos, 
           "DEBUG сообщения должны быть помечены");
    ASSERT(content.find("[INFO]") != std::string::npos, 
           "INFO сообщения должны быть помечены");
    ASSERT(content.find("[WARNING]") != std::string::npos, 
           "WARNING сообщения должны быть помечены");
    
    cleanupFile(testFile);
}

/**
 * @brief Тест логирования с форматированием времени
 */
void testTimestampFormatting() {
    const std::string testFile = "test_timestamp.log";
    cleanupFile(testFile);
    
    {
        logging::Logger logger(testFile, logging::LogLevel::INFO);
        
        // Записываем сообщение и проверяем временную метку
        auto before = std::chrono::system_clock::now();
        logger.info("Timestamp test message");
        auto after = std::chrono::system_clock::now();
        
        auto before_time_t = std::chrono::system_clock::to_time_t(before);
        auto after_time_t = std::chrono::system_clock::to_time_t(after);
        
        std::tm before_tm = *std::localtime(&before_time_t);
        std::tm after_tm = *std::localtime(&after_time_t);
        
        char expected_date[20];
        std::strftime(expected_date, sizeof(expected_date), "%Y-%m-%d", &before_tm);
        
        std::string content = readFile(testFile);
        ASSERT(content.find(expected_date) != std::string::npos, 
               "Сообщение должно содержать корректную дату");
    }
    
    cleanupFile(testFile);
}

int main() {
    TestRunner runner;
    
    std::cout << "Запуск юнит-тестов для библиотеки логирования\n" << std::endl;
    
    runner.runTest("Базовое логирование", testBasicLogging);
    runner.runTest("Фильтрация по уровням", testLogLevelFiltering);
    runner.runTest("Изменение уровня по умолчанию", testChangeDefaultLevel);
    runner.runTest("Многопоточность", testMultithreading);
    runner.runTest("Форматирование сообщений", testMessageFormatting);
    runner.runTest("Преобразование уровней", testLogLevelConversion);
    runner.runTest("Обработка ошибок", testErrorHandling);
    
    // Новые тесты
    runner.runTest("Граничные случаи сообщений", testEdgeCaseMessages);
    runner.runTest("Производительность", testPerformance);
    runner.runTest("Стрессовое многопоточное логирование", testStressMultithreading);
    runner.runTest("Инициализация логгера", testLoggerInitialization);
    runner.runTest("Повторное открытие/закрытие", testRepeatedOpenClose);
    runner.runTest("Все уровни логирования", testAllLogLevels);
    runner.runTest("Форматирование времени", testTimestampFormatting);
    
    runner.printSummary();
    
    return runner.allTestsPassed() ? 0 : 1;
}

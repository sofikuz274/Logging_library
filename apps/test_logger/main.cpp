#include "logging/Logger.h"
#include <iostream>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <sstream>

 // Структура для передачи данных между потоками
struct LogMessage {
    std::string text;
    logging::LogLevel level;
    
    LogMessage(const std::string& t, logging::LogLevel l) : text(t), level(l) {}
};

 // Потокобезопасная очередь для сообщений
class ThreadSafeQueue {
private:
    std::queue<LogMessage> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_{false};

public:
    void push(const LogMessage& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(message);
        condition_.notify_one();
    }
    
    bool pop(LogMessage& message) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !queue_.empty() || shutdown_.load(); });
        
        if (queue_.empty()) {
            return false; // shutdown
        }
        
        message = queue_.front();
        queue_.pop();
        return true;
    }
    
    void shutdown() {
        shutdown_.store(true);
        condition_.notify_all();
    }
};

// Функция рабочего потока для записи логов
void loggerWorker(logging::Logger& logger, ThreadSafeQueue& queue) {
    LogMessage message{"", logging::LogLevel::INFO};
    
    while (queue.pop(message)) {
        if (!logger.log(message.text, message.level)) {
            std::cerr << "Ошибка записи в журнал: " << message.text << std::endl;
        }
    }
}

// Парсинг уровня логирования из строки
logging::LogLevel parseLogLevel(const std::string& input, bool& hasLevel) {
    hasLevel = false;
    
    if (input.empty()) {
        return logging::LogLevel::INFO;
    }
    
    // Ищем уровень в начале строки (формат: "LEVEL: message" или "LEVEL message")
    size_t colonPos = input.find(':');
    size_t spacePos = input.find(' ');
    
    size_t separatorPos = std::string::npos;
    if (colonPos != std::string::npos && spacePos != std::string::npos) {
        separatorPos = std::min(colonPos, spacePos);
    } else if (colonPos != std::string::npos) {
        separatorPos = colonPos;
    } else if (spacePos != std::string::npos) {
        separatorPos = spacePos;
    }
    
    if (separatorPos == std::string::npos) {
        return logging::LogLevel::INFO;
    }
    
    std::string levelStr = input.substr(0, separatorPos);
    
    // Проверяем, является ли это валидным уровнем
    if (levelStr == "DEBUG" || levelStr == "debug") {
        hasLevel = true;
        return logging::LogLevel::DEBUG;
    } else if (levelStr == "INFO" || levelStr == "info") {
        hasLevel = true;
        return logging::LogLevel::INFO;
    } else if (levelStr == "WARNING" || levelStr == "warning" || levelStr == "WARN" || levelStr == "warn") {
        hasLevel = true;
        return logging::LogLevel::WARNING;
    }
    
    return logging::LogLevel::INFO;
}


// Извлечение текста сообщения после уровня
std::string extractMessage(const std::string& input, bool hasLevel) {
    if (!hasLevel) {
        return input;
    }
    
    size_t colonPos = input.find(':');
    size_t spacePos = input.find(' ');
    
    size_t separatorPos = std::string::npos;
    if (colonPos != std::string::npos && spacePos != std::string::npos) {
        separatorPos = std::min(colonPos, spacePos);
    } else if (colonPos != std::string::npos) {
        separatorPos = colonPos;
    } else if (spacePos != std::string::npos) {
        separatorPos = spacePos;
    }
    
    if (separatorPos == std::string::npos) {
        return input;
    }
    
    std::string message = input.substr(separatorPos + 1);
    
    // Убираем ведущие пробелы
    size_t start = message.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return "";
    }
    
    return message.substr(start);
}

// Отображение справки по использованию
void showUsage(const std::string& programName) {
    std::cout << "Использование: " << programName << " <файл_журнала> [уровень_по_умолчанию]\n\n";
    std::cout << "Параметры:\n";
    std::cout << "  файл_журнала          - имя файла для записи журнала\n";
    std::cout << "  уровень_по_умолчанию  - DEBUG, INFO или WARNING (по умолчанию: INFO)\n\n";
    std::cout << "Формат ввода сообщений:\n";
    std::cout << "  <сообщение>                    - использует уровень по умолчанию\n";
    std::cout << "  <УРОВЕНЬ>: <сообщение>        - использует указанный уровень\n";
    std::cout << "  <УРОВЕНЬ> <сообщение>         - использует указанный уровень\n\n";
    std::cout << "Для выхода введите 'quit', 'exit' или нажмите Ctrl+C\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        showUsage(argv[0]);
        return 1;
    }

    std::string logFile = argv[1];
    logging::LogLevel defaultLevel = logging::LogLevel::DEBUG;

    // Парсинг уровня по умолчанию
    if (argc >= 3) {
        defaultLevel = logging::stringToLogLevel(argv[2]);
    }

    // Создание логгера
    logging::Logger logger(logFile, defaultLevel);
    if (!logger.isValid()) {
        std::cerr << "Ошибка: не удалось создать логгер для файла " << logFile << std::endl;
        return 1;
    }

    std::cout << "Логгер инициализирован. Файл: " << logFile 
              << ", уровень по умолчанию: " << logging::logLevelToString(defaultLevel) << std::endl;
    std::cout << "Введите сообщения для записи в журнал (quit для выхода):\n";

    // Создание очереди и запуск рабочего потока
    ThreadSafeQueue messageQueue;
    std::thread workerThread(loggerWorker, std::ref(logger), std::ref(messageQueue));

    std::string input;
    
    while (std::getline(std::cin, input)) {
        // Проверка команд выхода
        if (input == "quit" || input == "exit") {
            break;
        }
        
        if (input.empty()) {
            continue;
        }
        
        // Парсинг уровня и сообщения
        bool hasLevel = false;
        logging::LogLevel level = parseLogLevel(input, hasLevel);
        std::string message = extractMessage(input, hasLevel);
        
        if (message.empty()) {
            std::cout << "Пустое сообщение, пропускаем.\n";
            continue;
        }
        
        // Добавление сообщения в очередь
        messageQueue.push(LogMessage(message, level));
        
        std::cout << "Сообщение добавлено: [" << logging::logLevelToString(level) 
                  << "] " << message << std::endl;
    }

    // Остановка рабочего потока
    messageQueue.shutdown();
    workerThread.join();

    std::cout << "Программа завершена.\n";
    return 0;
}

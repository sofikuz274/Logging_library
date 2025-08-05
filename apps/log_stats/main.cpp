#include "logging/Logger.h"
#include <iostream>
#include <string>
#include <map>
#include <chrono>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iomanip>

/**
 * @brief Структура для хранения статистики
 */
struct LogStatistics {
    // Счетчики сообщений
    size_t totalMessages = 0;
    std::map<logging::LogLevel, size_t> messagesByLevel;
    size_t messagesLastHour = 0;
    
    // Статистика длин сообщений
    size_t minLength = SIZE_MAX;
    size_t maxLength = 0;
    double avgLength = 0.0;
    size_t totalLength = 0;
    
    // Временные метки для подсчета сообщений за последний час
    std::vector<std::chrono::system_clock::time_point> timestamps;
    
    // Время последней выдачи статистики
    std::chrono::system_clock::time_point lastStatsOutput = std::chrono::system_clock::now();
    bool statsChanged = false;

    void addMessage(const std::string& message, logging::LogLevel level) {
        auto now = std::chrono::system_clock::now();
        
        // Обновляем общую статистику
        totalMessages++;
        messagesByLevel[level]++;
        
        // Обновляем статистику длин
        size_t messageLength = message.length();
        totalLength += messageLength;
        minLength = std::min(minLength, messageLength);
        maxLength = std::max(maxLength, messageLength);
        avgLength = static_cast<double>(totalLength) / totalMessages;
        
        // Добавляем временную метку
        timestamps.push_back(now);
        
        // Очищаем старые метки (старше часа)
        auto hourAgo = now - std::chrono::hours(1);
        timestamps.erase(
            std::remove_if(timestamps.begin(), timestamps.end(),
                [hourAgo](const auto& timestamp) { return timestamp < hourAgo; }),
            timestamps.end()
        );
        
        messagesLastHour = timestamps.size();
        statsChanged = true;
    }
    
    void printStatistics() {
        std::cout << "\n=== СТАТИСТИКА ЛОГОВ ===" << std::endl;
        std::cout << "Всего сообщений: " << totalMessages << std::endl;
        
        std::cout << "По уровням важности:" << std::endl;
        for (const auto& [level, count] : messagesByLevel) {
            std::cout << "  " << logging::logLevelToString(level) << ": " << count << std::endl;
        }
        
        std::cout << "За последний час: " << messagesLastHour << std::endl;
        
        if (totalMessages > 0) {
            std::cout << "Длины сообщений:" << std::endl;
            std::cout << "  Минимум: " << (minLength == SIZE_MAX ? 0 : minLength) << std::endl;
            std::cout << "  Максимум: " << maxLength << std::endl;
            std::cout << "  Среднее: " << std::fixed << std::setprecision(2) << avgLength << std::endl;
        }
        
        std::cout << "========================\n" << std::endl;
        
        lastStatsOutput = std::chrono::system_clock::now();
        statsChanged = false;
    }
    
    bool shouldPrintStats(size_t messagesInterval, int timeoutSeconds) const {
        if (totalMessages % messagesInterval == 0 && totalMessages > 0) {
            return true;
        }
        
        auto now = std::chrono::system_clock::now();
        auto timeSinceLastOutput = std::chrono::duration_cast<std::chrono::seconds>(
            now - lastStatsOutput).count();
            
        return statsChanged && (timeSinceLastOutput >= timeoutSeconds);
    }
};

/**
 * @brief Парсинг полученного лог-сообщения
 */
bool parseLogMessage(const std::string& rawMessage, std::string& message, logging::LogLevel& level) {
    // Ожидаемый формат: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message
    
    size_t firstBracket = rawMessage.find('[');
    if (firstBracket == std::string::npos) return false;
    
    size_t secondBracket = rawMessage.find(']', firstBracket);
    if (secondBracket == std::string::npos) return false;
    
    size_t thirdBracket = rawMessage.find('[', secondBracket);
    if (thirdBracket == std::string::npos) return false;
    
    size_t fourthBracket = rawMessage.find(']', thirdBracket);
    if (fourthBracket == std::string::npos) return false;
    
    // Извлекаем уровень
    std::string levelStr = rawMessage.substr(thirdBracket + 1, fourthBracket - thirdBracket - 1);
    level = logging::stringToLogLevel(levelStr);
    
    // Извлекаем сообщение
    size_t messageStart = rawMessage.find_first_not_of(" \t", fourthBracket + 1);
    if (messageStart == std::string::npos) {
        message = "";
    } else {
        message = rawMessage.substr(messageStart);
    }
    
    return true;
}

/**
 * @brief Создание серверного сокета для прослушивания
 */
int createServerSocket(const std::string& host, int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Ошибка создания сокета" << std::endl;
        return -1;
    }
    
    // Позволяем переиспользовать адрес
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Ошибка установки опций сокета" << std::endl;
        close(server_fd);
        return -1;
    }
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Ошибка привязки сокета к порту " << port << std::endl;
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Ошибка прослушивания сокета" << std::endl;
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

/**
 * @brief Функция для периодической проверки вывода статистики по таймауту
 */
void timeoutChecker(LogStatistics& stats, int timeoutSeconds, size_t messagesInterval, 
                   std::atomic<bool>& running) {
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (stats.shouldPrintStats(messagesInterval, timeoutSeconds)) {
            stats.printStatistics();
        }
    }
}

/**
 * @brief Отображение справки по использованию
 */
void showUsage(const std::string& programName) {
    std::cout << "Использование: " << programName << " <порт> <N> <T>\n\n";
    std::cout << "Параметры:\n";
    std::cout << "  порт  - порт для прослушивания подключений\n";
    std::cout << "  N     - выводить статистику после каждого N-го сообщения\n";
    std::cout << "  T     - таймаут в секундах для вывода статистики\n\n";
    std::cout << "Пример: " << programName << " 12345 10 30\n";
    std::cout << "  - слушает порт 12345\n";
    std::cout << "  - выводит статистику каждые 10 сообщений\n";
    std::cout << "  - выводит статистику каждые 30 секунд, если она изменилась\n";
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        showUsage(argv[0]);
        return 1;
    }
    
    int port = std::atoi(argv[1]);
    size_t messagesInterval = std::atoi(argv[2]);
    int timeoutSeconds = std::atoi(argv[3]);
    
    if (port <= 0 || port > 65535) {
        std::cerr << "Ошибка: неверный порт " << port << std::endl;
        return 1;
    }
    
    if (messagesInterval == 0) {
        std::cerr << "Ошибка: N должно быть больше 0" << std::endl;
        return 1;
    }
    
    if (timeoutSeconds <= 0) {
        std::cerr << "Ошибка: T должно быть больше 0" << std::endl;
        return 1;
    }
    
    std::cout << "Запуск сервера статистики логов..." << std::endl;
    std::cout << "Порт: " << port << std::endl;
    std::cout << "Интервал сообщений: " << messagesInterval << std::endl;
    std::cout << "Таймаут: " << timeoutSeconds << " секунд" << std::endl;
    
    // Создание серверного сокета
    int server_fd = createServerSocket("0.0.0.0", port);
    if (server_fd < 0) {
        return 1;
    }
    
    std::cout << "Сервер запущен и ожидает подключений на порту " << port << "..." << std::endl;
    
    LogStatistics stats;
    std::atomic<bool> running{true};
    
    // Запуск потока для проверки таймаута
    std::thread timeoutThread(timeoutChecker, std::ref(stats), timeoutSeconds, 
                             messagesInterval, std::ref(running));
    
    while (true) {
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        
        int client_socket = accept(server_fd, (struct sockaddr*)&address, 
                                 (socklen_t*)&addrlen);
        
        if (client_socket < 0) {
            std::cerr << "Ошибка принятия соединения" << std::endl;
            continue;
        }
        
        std::cout << "Клиент подключился" << std::endl;
        
        char buffer[4096];
        std::string partialMessage;
        
        while (true) {
            ssize_t bytesRead = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytesRead <= 0) {
                if (bytesRead == 0) {
                    std::cout << "Клиент отключился" << std::endl;
                } else {
                    std::cerr << "Ошибка чтения из сокета" << std::endl;
                }
                break;
            }
            
            buffer[bytesRead] = '\0';
            std::string data = partialMessage + std::string(buffer);
            partialMessage.clear();
            
            // Обработка полученных данных построчно
            size_t pos = 0;
            while ((pos = data.find('\n')) != std::string::npos) {
                std::string line = data.substr(0, pos);
                data.erase(0, pos + 1);
                
                if (line.empty()) continue;
                
                // Удаляем символ возврата каретки если есть
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                
                std::cout << "Получено: " << line << std::endl;
                
                // Парсинг и обработка сообщения
                std::string message;
                logging::LogLevel level;
                
                if (parseLogMessage(line, message, level)) {
                    stats.addMessage(message, level);
                    
                    // Проверяем, нужно ли вывести статистику
                    if (stats.shouldPrintStats(messagesInterval, timeoutSeconds)) {
                        stats.printStatistics();
                    }
                } else {
                    std::cerr << "Не удалось распарсить сообщение: " << line << std::endl;
                }
            }
            
            // Сохраняем неполное сообщение
            if (!data.empty()) {
                partialMessage = data;
            }
        }
        
        close(client_socket);
    }
    
    running.store(false);
    timeoutThread.join();
    close(server_fd);
    
    return 0;
}
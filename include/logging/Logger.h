#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <memory>
#include <queue>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace logging {

    /**
     * @brief Уровни важности логируемых сообщений
     */
    enum class LogLevel : int {
        DEBUG = 0,    // Отладочные сообщения
        INFO = 1,     // Информационные сообщения
        WARNING = 2   // Предупреждения и ошибки
    };

    /**
     * @brief Преобразование уровня логирования в строку
     */
    std::string logLevelToString(LogLevel level);

    /**
     * @brief Преобразование строки в уровень логирования
     */
    LogLevel stringToLogLevel(const std::string& levelStr);

    /**
     * @brief Абстрактный интерфейс для вывода логов
     */
    class LogOutput {
    public:
        virtual ~LogOutput() = default;
        virtual bool writeLog(const std::string& formattedMessage) = 0;
        virtual bool isValid() const = 0;
    };

    /**
     * @brief Вывод логов в файл
     */
    class FileOutput : public LogOutput {
    private:
        std::ofstream file_;
        std::string filename_;

    public:
        explicit FileOutput(const std::string& filename);
        ~FileOutput() override;
        
        bool writeLog(const std::string& formattedMessage) override;
        bool isValid() const override;
    };

    /**
     * @brief Вывод логов в сокет (дополнительная функциональность)
     */
    class SocketOutput : public LogOutput {
    private:
        int socket_fd_;
        std::string host_;
        int port_;
        bool connected_;

    public:
        SocketOutput(const std::string& host, int port);
        ~SocketOutput() override;
        
        bool writeLog(const std::string& formattedMessage) override;
        bool isValid() const override;
        
    private:
        bool connect();
        void disconnect();
    };

    /**
     * @brief Comprehensive error codes for logging system
     */
    enum class LoggingError : int {
        SUCCESS = 0,
        FILE_OPEN_FAILED = 1001,
        FILE_WRITE_FAILED = 1002,
        SOCKET_CONNECTION_FAILED = 2001,
        SOCKET_WRITE_FAILED = 2002,
        CONFIG_PARSE_ERROR = 3001,
        QUEUE_OVERFLOW = 5001,
        ROTATION_FAILED = 6001
    };

    /**
     * @brief Configuration structure for logger
     */
    struct LoggerConfig {
        LogLevel defaultLevel = LogLevel::INFO;
        bool enableAsync = false;
        size_t asyncQueueSize = 10000;
        size_t maxFileSizeMB = 100;
        size_t maxFiles = 10;
        bool enableRotation = true;
        bool compressOldLogs = false;
        std::string timestampFormat = "%Y-%m-%d %H:%M:%S";
        int reconnectIntervalMs = 5000;
        int maxReconnectAttempts = 10;
    };

    /**
     * @brief Async logging queue
     */
    template<typename T>
    class AsyncQueue {
    private:
        std::queue<T> queue_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::atomic<bool> shutdown_{false};
        size_t max_size_;

    public:
        explicit AsyncQueue(size_t max_size = 10000) : max_size_(max_size) {}

        bool push(const T& item) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.size() >= max_size_) return false;
            queue_.push(item);
            cv_.notify_one();
            return true;
        }

        bool pop(T& item) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || shutdown_.load(); });
            if (shutdown_.load() && queue_.empty()) return false;
            item = queue_.front();
            queue_.pop();
            return true;
        }

        void shutdown() {
            shutdown_.store(true);
            cv_.notify_all();
        }
    };

    /**
     * @brief Log rotation handler
     */
    class LogRotator {
    private:
        std::string base_filename_;
        size_t max_size_bytes_;
        size_t max_files_;
        bool compress_;

    public:
        LogRotator(const std::string& base_filename, size_t max_size_mb, size_t max_files, bool compress);
        bool shouldRotate(size_t current_size) const;
        bool rotate();
    };

    /**
     * @brief Enhanced socket output with reconnection
     */
    class EnhancedSocketOutput : public LogOutput {
    private:
        int socket_fd_;
        std::string host_;
        int port_;
        std::atomic<bool> connected_;
        std::atomic<bool> reconnecting_;
        int reconnect_interval_ms_;
        int max_reconnect_attempts_;
        std::thread reconnect_thread_;
        std::mutex reconnect_mutex_;

        bool connect();
        void disconnect();
        void reconnectLoop();

    public:
        EnhancedSocketOutput(const std::string& host, int port, 
                           int reconnect_interval_ms = 5000, 
                           int max_reconnect_attempts = 10);
        ~EnhancedSocketOutput() override;
        
        bool writeLog(const std::string& formattedMessage) override;
        bool isValid() const override;
    };

    /**
     * @brief Enhanced file output with rotation
     */
    class EnhancedFileOutput : public LogOutput {
    private:
        std::ofstream file_;
        std::string filename_;
        std::unique_ptr<LogRotator> rotator_;
        size_t current_size_;
        std::mutex file_mutex_;

    public:
        EnhancedFileOutput(const std::string& filename, 
                          size_t max_size_mb = 100, 
                          size_t max_files = 10,
                          bool compress = false);
        ~EnhancedFileOutput() override;
        
        bool writeLog(const std::string& formattedMessage) override;
        bool isValid() const override;
    };

    /**
     * @brief Основной класс логгера с расширенными функциями
     */
    class Logger {
    private:
        std::unique_ptr<LogOutput> output_;
        LogLevel defaultLevel_;
        mutable std::mutex mutex_;
        LoggerConfig config_;
        
        // Async logging support
        std::unique_ptr<AsyncQueue<std::pair<std::string, LogLevel>>> async_queue_;
        std::unique_ptr<std::thread> async_worker_;
        std::atomic<bool> async_running_{false};
        
        // Error tracking
        LoggingError last_error_;
        std::string last_error_message_;

        std::string formatMessage(const std::string& message, LogLevel level) const;
        std::string getCurrentTime() const;
        void startAsyncWorker();
        void stopAsyncWorker();
        void asyncWorkerLoop();

    public:
        Logger(const std::string& filename, LogLevel defaultLevel = LogLevel::INFO);
        Logger(const std::string& host, int port, LogLevel defaultLevel = LogLevel::INFO);
        Logger(const LoggerConfig& config);
        
        ~Logger();

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) = default;
        Logger& operator=(Logger&&) = default;

        bool log(const std::string& message, LogLevel level);
        bool log(const std::string& message);
        
        void setDefaultLevel(LogLevel level);
        LogLevel getDefaultLevel() const;
        bool isValid() const;
        
        // Error handling
        LoggingError getLastError() const;
        std::string getLastErrorMessage() const;
        
        // Async control
        void enableAsync(bool enable = true);
        bool isAsyncEnabled() const;
        
        // Configuration
        void setConfig(const LoggerConfig& config);
        LoggerConfig getConfig() const;

        // Extended helper methods
        bool debug(const std::string& message) { return log(message, LogLevel::DEBUG); }
        bool info(const std::string& message) { return log(message, LogLevel::INFO); }
        bool warning(const std::string& message) { return log(message, LogLevel::WARNING); }
    };

} // namespace logging
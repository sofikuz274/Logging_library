#include "logging/Logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <algorithm>

namespace logging {

    std::string logLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE:   return "TRACE";
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR:   return "ERROR";
            case LogLevel::FATAL:   return "FATAL";
            default:                return "UNKNOWN";
        }
    }

    LogLevel stringToLogLevel(const std::string& levelStr) {
        std::string upper = levelStr;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        if (upper == "TRACE") return LogLevel::TRACE;
        if (upper == "DEBUG") return LogLevel::DEBUG;
        if (upper == "INFO") return LogLevel::INFO;
        if (upper == "WARNING") return LogLevel::WARNING;
        if (upper == "ERROR") return LogLevel::ERROR;
        if (upper == "FATAL") return LogLevel::FATAL;
        
        return LogLevel::INFO; // По умолчанию
    }

    // FileOutput implementation
    FileOutput::FileOutput(const std::string& filename) : filename_(filename) {
        file_.open(filename_, std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "Ошибка: не удалось открыть файл журнала: " << filename_ << std::endl;
        }
    }

    FileOutput::~FileOutput() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    bool FileOutput::writeLog(const std::string& formattedMessage) {
        if (!file_.is_open()) {
            return false;
        }
        
        file_ << formattedMessage << std::endl;
        file_.flush();
        return file_.good();
    }

    bool FileOutput::isValid() const {
        return file_.is_open() && file_.good();
    }

    // SocketOutput implementation
    SocketOutput::SocketOutput(const std::string& host, int port) 
        : socket_fd_(-1), host_(host), port_(port), connected_(false) {
        connected_ = connect();
    }

    SocketOutput::~SocketOutput() {
        disconnect();
    }

    bool SocketOutput::connect() {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Ошибка создания сокета" << std::endl;
            return false;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);
        
        if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Неверный адрес: " << host_ << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }

        if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Ошибка подключения к " << host_ << ":" << port_ << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }

        return true;
    }

    void SocketOutput::disconnect() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        connected_ = false;
    }

    bool SocketOutput::writeLog(const std::string& formattedMessage) {
        if (!connected_ || socket_fd_ < 0) {
            return false;
        }

        std::string message = formattedMessage + "\n";
        ssize_t sent = send(socket_fd_, message.c_str(), message.length(), 0);
        
        if (sent < 0) {
            std::cerr << "Ошибка отправки данных в сокет" << std::endl;
            connected_ = false;
            return false;
        }

        return sent == static_cast<ssize_t>(message.length());
    }

    bool SocketOutput::isValid() const {
        return connected_ && socket_fd_ >= 0;
    }

    // Logger implementation
    Logger::Logger(const std::string& filename, LogLevel defaultLevel) 
        : output_(std::make_unique<FileOutput>(filename)), defaultLevel_(defaultLevel) {
    }

    Logger::Logger(const std::string& host, int port, LogLevel defaultLevel)
        : output_(std::make_unique<SocketOutput>(host, port)), defaultLevel_(defaultLevel) {
    }

    Logger::~Logger() {
        stopAsyncWorker();
    }

    bool Logger::log(const std::string& message, LogLevel level) {
        // Проверяем, нужно ли записывать сообщение
        if (static_cast<int>(level) < static_cast<int>(defaultLevel_)) {
            return true; // Сообщение отфильтровано, но это не ошибка
        }

        if (!output_ || !output_->isValid()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string formattedMessage = formatMessage(message, level);
        return output_->writeLog(formattedMessage);
    }

    bool Logger::log(const std::string& message) {
        return log(message, defaultLevel_);
    }

    void Logger::setDefaultLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        defaultLevel_ = level;
    }

    LogLevel Logger::getDefaultLevel() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return defaultLevel_;
    }

    bool Logger::isValid() const {
        return output_ && output_->isValid();
    }

    LoggingError Logger::getLastError() const {
        return last_error_;
    }

    std::string Logger::getLastErrorMessage() const {
        return last_error_message_;
    }

    void Logger::enableAsync(bool enable) {
        if (enable && !async_running_) {
            startAsyncWorker();
        } else if (!enable && async_running_) {
            stopAsyncWorker();
        }
    }

    bool Logger::isAsyncEnabled() const {
        return async_running_;
    }

    void Logger::setConfig(const LoggerConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        defaultLevel_ = config.defaultLevel;
    }

    LoggerConfig Logger::getConfig() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    std::string Logger::formatMessage(const std::string& message, LogLevel level) const {
        std::ostringstream oss;
        oss << "[" << getCurrentTime() << "] "
            << "[" << logLevelToString(level) << "] "
            << message;
        return oss.str();
    }

    std::string Logger::getCurrentTime() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << ms.count();
        
        return oss.str();
    }

} 

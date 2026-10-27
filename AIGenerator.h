#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <SFML/System/Vector2.hpp>

class AiGenerator {
public:
    AiGenerator();
    ~AiGenerator() = default;

    // Запуск генерации в фоновом потоке
    void generateAsync(const std::string& userPrompt, sf::Vector2f canvasCenter, const std::string& apiKey);

    // Проверка состояния из главного потока SFML
    bool isLoading() const { return m_isLoading.load(); }
    bool hasResult() const { return m_hasResult.load(); }

    // Забирает готовый JSON (или текст ошибки)
    bool consumeResult(std::string& outJson, std::string& outError);

private:
    std::atomic<bool> m_isLoading{ false };
    std::atomic<bool> m_hasResult{ false };

    std::mutex m_mutex;
    std::string m_resultJson;
    std::string m_errorMessage;

    static std::string buildSystemPrompt(sf::Vector2f center);
    static std::string sendHttpsPost(const std::wstring& host, const std::wstring& path, const std::string& body, std::string& outErr);
};
#include "AiGenerator.h"
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <sstream>
#include "json/json.hpp"

#pragma comment(lib, "winhttp.lib")

AiGenerator::AiGenerator() {}

std::string AiGenerator::buildSystemPrompt(sf::Vector2f center) {
    std::ostringstream ss;
    ss << "You are a 2D vector graphics generator for a custom C++ SFML editor.\n"
        << "Convert the user's description into a valid JSON object matching this exact schema:\n"
        << "{\n"
        << "  \"version\": 1,\n"
        << "  \"shapes\": [ ... ]\n"
        << "}\n\n"
        << "Coordinate System Rules:\n"
        << "- Screen X goes RIGHT, Y goes DOWN. Center the generated object around (" << (int)center.x << ", " << (int)center.y << ").\n"
        << "- Available shape types:\n"
        << "  1. \"Polygon\":\n"
        << "     - \"position\": {\"x\": float, \"y\": float} (center of the polygon)\n"
        << "     - \"size\": {\"x\": halfWidth, \"y\": halfHeight}\n"
        << "     - \"basePoints\": array of {\"x\": float, \"y\": float} in normalized range [-1.0, 1.0].\n"
        << "       World vertex = position + basePoint * size.\n"
        << "       Examples: Rectangle = [{\"x\":-1,\"y\":-1}, {\"x\":1,\"y\":-1}, {\"x\":1,\"y\":1}, {\"x\":-1,\"y\":1}].\n"
        << "                 Triangle pointing up = [{\"x\":0,\"y\":-1}, {\"x\":1,\"y\":1}, {\"x\":-1,\"y\":1}].\n"
        << "     - \"isClosed\": true (for filled shapes) or false (for lines).\n"
        << "  2. \"Ellipse\":\n"
        << "     - \"position\": {\"x\": float, \"y\": float}\n"
        << "     - \"radiusX\": float, \"radiusY\": float\n"
        << "     - \"size\": {\"x\": radiusX * 2, \"y\": radiusY * 2}\n"
        << "     - \"isClosed\": true\n"
        << "  3. \"Group\":\n"
        << "     - If composite object (house, car, tree, etc.), ALWAYS wrap shapes inside a single \"Group\" in \"children\": [ ... ].\n\n"
        << "Common properties for every shape:\n"
        << "- \"name\": descriptive string (e.g. \"Roof\", \"Wall\")\n"
        << "- \"rotation\": float (degrees, default 0.0)\n"
        << "- \"anchorOffset\": {\"x\": 0.0, \"y\": 0.0}\n"
        << "- \"fillColor\": [R, G, B, A] (0-255)\n"
        << "- \"sideColors\": [[R, G, B, A]]\n"
        << "- \"sideThicknesses\": [float]\n"
        << "CRITICAL: Return ONLY a valid JSON object. Do NOT wrap the JSON in markdown code blocks (like ```json). Do NOT write any introduction or explanation. Start directly with '{' and end with '}'.";
    return ss.str();
}

std::string AiGenerator::sendHttpsPost(const std::wstring& host, const std::wstring& path, const std::string& body, std::string& outErr) {
    // В заголовке уже будут переданы Content-Type и Authorization
    return ""; // Используется перегрузка с заголовками
}

void AiGenerator::generateAsync(const std::string& userPrompt, sf::Vector2f canvasCenter, const std::string& apiKey) {
    if (m_isLoading.load()) return;

    m_isLoading.store(true);
    m_hasResult.store(false);

    std::thread([this, userPrompt, canvasCenter, apiKey]() {
        std::string err;
        std::string generatedSceneJson;

        try {
            // Формируем OpenAI-совместимый JSON запрос
            nlohmann::json reqBody;
            // Бесплатная модель на OpenRouter:
            reqBody["model"] = "openrouter/free";
            reqBody["messages"] = nlohmann::json::array({
                { {"role", "system"}, {"content", buildSystemPrompt(canvasCenter)} },
                { {"role", "user"}, {"content", userPrompt} }
                });
            reqBody["temperature"] = 0.2;

            std::wstring host = L"openrouter.ai";
            std::wstring path = L"/api/v1/chat/completions";

            std::wstring headers = L"Content-Type: application/json\r\n"
                L"Authorization: Bearer " + std::wstring(apiKey.begin(), apiKey.end()) + L"\r\n";

            // WinHTTP запрос
            HINTERNET hSession = WinHttpOpen(L"VectorEditorAI/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            HINTERNET hConnect = hSession ? WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
            HINTERNET hRequest = hConnect ? WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;

            if (hRequest) {
                WinHttpSetTimeouts(hRequest, 5000, 5000, 10000, 40000);
                std::string bodyStr = reqBody.dump();

                BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1L, (LPVOID)bodyStr.data(), (DWORD)bodyStr.size(), (DWORD)bodyStr.size(), 0);
                if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);

                if (bResults) {
                    DWORD statusCode = 0;
                    DWORD statusSize = sizeof(statusCode);
                    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

                    std::string response;
                    DWORD dwSize = 0;
                    do {
                        if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;
                        std::string buffer(dwSize, '\0');
                        DWORD dwDownloaded = 0;
                        if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                            response.append(buffer.data(), dwDownloaded);
                        }
                    } while (dwSize > 0);

                    if (statusCode == 200) {
                        auto respJson = nlohmann::json::parse(response);
                        std::string rawContent = respJson["choices"][0]["message"]["content"].get<std::string>();

                        // Очистка от случайных ```json ... ``` если модель все же добавила маркдаун
                        size_t firstBrace = rawContent.find('{');
                        size_t lastBrace = rawContent.rfind('}');
                        if (firstBrace != std::string::npos && lastBrace != std::string::npos && lastBrace > firstBrace) {
                            generatedSceneJson = rawContent.substr(firstBrace, lastBrace - firstBrace + 1);
                        }
                        else {
                            generatedSceneJson = rawContent;
                        }
                    }
                    else {
                        err = "HTTP Error " + std::to_string(statusCode) + ": " + response;
                    }
                }
                else {
                    err = "Failed to connect to openrouter.ai";
                }
            }
            else {
                err = "WinHttp initialization failed";
            }

            if (hRequest) WinHttpCloseHandle(hRequest);
            if (hConnect) WinHttpCloseHandle(hConnect);
            if (hSession) WinHttpCloseHandle(hSession);

        }
        catch (const std::exception& e) {
            err = std::string("Error: ") + e.what();
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_resultJson = generatedSceneJson;
            m_errorMessage = err;
        }
        m_isLoading.store(false);
        m_hasResult.store(true);
        }).detach();
}

bool AiGenerator::consumeResult(std::string& outJson, std::string& outError) {
    if (!m_hasResult.load()) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    outJson = m_resultJson;
    outError = m_errorMessage;
    m_hasResult.store(false);
    return true;
}
#pragma once

class Logger
{
public:
	enum class LogLevel {
		Info,
		Warning,
		Error
	};

	static Logger& Instance();

	void AddLog(LogLevel level, const std::string& message)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_logs.push_back({ level, message });
		m_scrollToBottom = true;

		std::string debugStr = "[LOG] " + message + "\n";
		OutputDebugStringA(debugStr.c_str());

		FILE* f;
		fopen_s(&f, "debug_log.txt", "a");
		if (f) {
			fprintf(f, "%s", debugStr.c_str());
			fclose(f);
		}
	}

	void AddLog(LogLevel level, const char* format, ...)
	{
		char buffer[1024];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);

		AddLog(level, std::string(buffer));
	}

	std::string SjisToUtf8(const std::string& sjisStr)
	{
		if (sjisStr.empty()) return "";
		int size = MultiByteToWideChar(932, 0, sjisStr.c_str(), -1, nullptr, 0);
		std::wstring utf16Str(size, 0);
		MultiByteToWideChar(932, 0, sjisStr.c_str(), -1, &utf16Str[0], size);

		size = WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string utf8Str(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), -1, &utf8Str[0], size, nullptr, nullptr);

		if (!utf8Str.empty() && utf8Str.back() == '\0') {
			utf8Str.pop_back();
		}
		return utf8Str;
	}

	void DrawImGuiWindow(bool* pOpen = nullptr)
	{
		ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Log Window", pOpen))
		{
			ImGui::End();
			return;
		}

		if (ImGui::Button("Clear")) {
			Clear();
		}
		ImGui::SameLine();
		bool copy = ImGui::Button("Copy");

		ImGui::Separator();
		ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		if (copy) {
			ImGui::LogToClipboard();
		}

		std::lock_guard<std::mutex> lock(m_mutex);
		for (const auto& log : m_logs)
		{
			ImVec4 color;
			switch (log.level) {
			case LogLevel::Info:    color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break; // White
			case LogLevel::Warning: color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break; // Yellow
			case LogLevel::Error:   color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break; // Red
			}
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			std::string utf8Msg = SjisToUtf8(log.message);
			ImGui::TextUnformatted(utf8Msg.c_str());
			ImGui::PopStyleColor();
		}

		if (m_scrollToBottom) {
			ImGui::SetScrollHereY(1.0f);
			m_scrollToBottom = false;
		}

		ImGui::EndChild();
		ImGui::End();
	}

	void Clear()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_logs.clear();
	}

private:
	Logger() {}
	~Logger() {}
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	struct LogEntry {
		LogLevel level;
		std::string message;
	};

	std::vector<LogEntry> m_logs;
	std::mutex m_mutex;
	bool m_scrollToBottom = true;
};

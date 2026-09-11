#include "../build_run.h"
#include "../editor.h"
#include "../i18n.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

// A parsed compiler diagnostic: file path, 1-based line and column. Returns
// true when the line looks like "path(line,col): error/warning" (MSVC) or
// "path:line:col: error/warning" (clang/gcc).
struct Diagnostic {
	std::string file;
	int			line;
	int			column;
	bool		error;
};

bool parse_diagnostic(const std::string &text, Diagnostic &out)
{
	// MSVC:  C:\path\file.c(42,7): error C2143: ...
	//         C:\path\file.c(42): error C2143: ...
	size_t open = text.find('(');
	if (open != std::string::npos && open > 0) {
		size_t close = text.find(')', open);
		if (close != std::string::npos) {
			std::string inner = text.substr(open + 1, close - open - 1);
			size_t comma	 = inner.find(',');
			int	 line		 = 0;
			int	 column		 = 0;
			if (comma != std::string::npos) {
				line	= std::atoi(inner.substr(0, comma).c_str());
				column	= std::atoi(inner.substr(comma + 1).c_str());
			} else {
				line = std::atoi(inner.c_str());
			}
			if (line > 0) {
				std::string tail = text.substr(close + 1);
				bool		error = tail.find(": error") != std::string::npos ||
								 tail.find(": fatal error") != std::string::npos;
				bool warning = tail.find(": warning") != std::string::npos;
				if (error || warning) {
					out.file	  = text.substr(0, open);
					out.line	  = line;
					out.column	  = column;
					out.error	  = error;
					return true;
				}
			}
		}
	}
	// clang/gcc: /path/file.c:42:7: error: ...
	size_t first_colon = text.find(':');
	if (first_colon != std::string::npos) {
		size_t second = text.find(':', first_colon + 1);
		if (second != std::string::npos) {
			size_t third = text.find(':', second + 1);
			if (third != std::string::npos) {
				int line   = std::atoi(text.substr(first_colon + 1, second - first_colon - 1).c_str());
				int column = std::atoi(text.substr(second + 1, third - second - 1).c_str());
				if (line > 0) {
					std::string tail = text.substr(third + 1);
					bool		error = tail.find("error") != std::string::npos;
					bool		warning = tail.find("warning") != std::string::npos;
					if (error || warning) {
						out.file	= text.substr(0, first_colon);
						out.line	= line;
						out.column	= column;
						out.error	= error;
						return true;
					}
				}
			}
		}
	}
	return false;
}

void open_diagnostic(const Diagnostic &diag)
{
#ifdef _WIN32
	// Reaching a specific line would require a real code editor; opening the
	// file in the system editor is a pragmatic jump target.
	ShellExecuteA(NULL, "open", diag.file.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
	(void)diag;
#endif
}

} // namespace

void EditorBuildOutputPanel(EditorState &state)
{
	if (!ImGui::Begin(L("Build Output"))) {
		ImGui::End();
		return;
	}

	if (state.build_running) {
		ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "%s", L("build.running"));
	} else if (state.build_succeeded) {
		ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", L("build.success"));
	} else if (!state.build_log.empty()) {
		ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", L("build.failed"));
	}
	ImGui::SameLine();
	if (ImGui::Button(L("Clear"), ImVec2(0, 0))) {
		std::lock_guard<std::mutex> lock(state.build_log_mutex);
		state.build_log.clear();
	}

	ImGui::Separator();

	std::string text;
	{
		std::lock_guard<std::mutex> lock(state.build_log_mutex);
		text = state.build_log;
	}
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
	ImGui::BeginChild("build_log_scroll", ImVec2(0, 0), false,
					  ImGuiWindowFlags_HorizontalScrollbar);
	{
		size_t start = 0;
		while (start <= text.size()) {
			size_t end = text.find('\n', start);
			if (end == std::string::npos) end = text.size();
			std::string line = text.substr(start, end - start);
			if (start < text.size()) {
				// Trim trailing '\r' so MSVC output renders cleanly.
				if (!line.empty() && line.back() == '\r') line.pop_back();
			}
			Diagnostic diag;
			bool		is_diag = !line.empty() && parse_diagnostic(line, diag);
			if (is_diag) {
				ImVec4 color = diag.error ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
										  : ImVec4(0.85f, 0.75f, 0.35f, 1.0f);
				ImGui::TextColored(color, "%s", line.c_str());
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("build.jump_tooltip"));
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) open_diagnostic(diag);
			} else {
				ImGui::TextUnformatted(line.c_str());
			}
			if (end >= text.size()) break;
			start = end + 1;
		}
	}
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::End();
}

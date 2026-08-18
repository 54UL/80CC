#ifndef SELECTABLE_TEXT_VIEW_HPP
#define SELECTABLE_TEXT_VIEW_HPP

#include <imgui.h>
#include <string>
#include <vector>
#include <regex>
#include <algorithm>
#include <unordered_map>
#include <cmath>

namespace ettycc {
namespace widgets {

    // -- Highlight rule --------------------------------------------------------
    // A regex matched against each line. First match wins.
    struct HighlightRule
    {
        std::regex  pattern;
        ImVec4      color;
    };

    using HighlightRuleset = std::vector<HighlightRule>;

    // -- Internal selection state (per widget ID) ------------------------------
    struct SelectableTextViewState
    {
        // Selection is stored as (line, column) pairs.
        int selStartLine = -1, selStartCol = 0;
        int selEndLine   = -1, selEndCol   = 0;
        bool selecting   = false;
        bool hasFocus    = false;
    };

    // Retrieve per-ID persistent state via ImGui storage.
    inline SelectableTextViewState& GetState(ImGuiID id)
    {
        // Use a small static map keyed by ImGuiID.
        // ImGui::GetStateStorage could work too, but it only stores scalars.
        static std::unordered_map<ImGuiID, SelectableTextViewState> sStates;
        return sStates[id];
    }

    // -- Helpers ---------------------------------------------------------------

    inline ImVec4 ColorForLine(const std::string& line,
                               const HighlightRuleset& rules,
                               const ImVec4& defaultColor)
    {
        for (const auto& rule : rules)
        {
            if (std::regex_match(line, rule.pattern))
                return rule.color;
        }
        return defaultColor;
    }

    // Clamp a (line, col) pair to valid range.
    inline void ClampPos(int& line, int& col, const std::vector<std::string>& lines)
    {
        if (lines.empty()) { line = 0; col = 0; return; }
        line = std::max(0, std::min(line, (int)lines.size() - 1));
        col  = std::max(0, std::min(col,  (int)lines[line].size()));
    }

    // Order two positions so that (aLine,aCol) <= (bLine,bCol).
    inline void OrderPositions(int& aLine, int& aCol, int& bLine, int& bCol)
    {
        if (aLine > bLine || (aLine == bLine && aCol > bCol))
        {
            std::swap(aLine, bLine);
            std::swap(aCol,  bCol);
        }
    }

    // Build the selected text string.
    inline std::string BuildSelectedText(const std::vector<std::string>& lines,
                                          int sLine, int sCol, int eLine, int eCol)
    {
        if (sLine < 0 || lines.empty()) return {};
        ClampPos(sLine, sCol, lines);
        ClampPos(eLine, eCol, lines);
        OrderPositions(sLine, sCol, eLine, eCol);

        std::string result;
        for (int i = sLine; i <= eLine; ++i)
        {
            const auto& l = lines[i];
            int from = (i == sLine) ? sCol : 0;
            int to   = (i == eLine) ? eCol : (int)l.size();
            from = std::min(from, (int)l.size());
            to   = std::min(to,   (int)l.size());
            if (from < to)
                result.append(l, from, to - from);
            if (i < eLine)
                result += '\n';
        }
        return result;
    }

    // Compute which column a mouse X position maps to for a given line.
    inline int ColFromMouseX(const std::string& line, float mouseX, float textStartX)
    {
        if (line.empty()) return 0;
        float rel = mouseX - textStartX;
        if (rel <= 0.0f) return 0;

        // Walk character by character using ImGui's font metrics.
        const ImFont* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();
        float x = 0.0f;
        for (int i = 0; i < (int)line.size(); ++i)
        {
            float charW = font->GetCharAdvance(line[i]) * (fontSize / font->FontSize);
            if (rel < x + charW * 0.5f)
                return i;
            x += charW;
        }
        return (int)line.size();
    }

    // Measure text width up to `count` characters.
    inline float MeasureText(const std::string& line, int from, int count)
    {
        if (count <= 0) return 0.0f;
        from  = std::max(0, std::min(from, (int)line.size()));
        count = std::min(count, (int)line.size() - from);
        return ImGui::CalcTextSize(line.c_str() + from, line.c_str() + from + count).x;
    }

    // -----------------------------------------------------------------
    // SelectableTextView
    //
    // Renders colored text lines inside a scrollable child region.
    // Text is selectable via mouse click+drag and copyable with Ctrl+C.
    // Ctrl+A selects all.
    //
    // Parameters:
    //   id          - unique ImGui string ID (e.g. "##BuildLog")
    //   lines       - text lines to display
    //   rules       - highlight rules (first match wins per line)
    //   defaultCol  - color for lines that match no rule
    //   bgCol       - background color (use alpha=0 for transparent)
    //   size        - widget size (ImVec2, use -1 for fill)
    //   scrollToEnd - if true, scroll to bottom this frame
    // -----------------------------------------------------------------
    inline void SelectableTextView(const char* id,
                                    const std::vector<std::string>& lines,
                                    const HighlightRuleset& rules,
                                    const ImVec4& defaultCol,
                                    const ImVec4& bgCol,
                                    const ImVec2& size = ImVec2(-1, -1),
                                    bool scrollToEnd = false)
    {
        const ImGuiID wid = ImGui::GetID(id);
        auto& state = GetState(wid);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, bgCol);
        if (!ImGui::BeginChild(id, size, false,
                               ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::EndChild();
            ImGui::PopStyleColor();
            return;
        }

        // Focus handling: clicking inside the child gives us focus.
        const bool childHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        if (childHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            state.hasFocus = true;
        if (!childHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            state.hasFocus = false;

        const ImVec2 windowPos  = ImGui::GetCursorScreenPos();
        const float  lineHeight = ImGui::GetTextLineHeightWithSpacing();
        const float  textStartX = windowPos.x;
        const ImVec2 mousePos   = ImGui::GetMousePos();

        // -- Mouse selection ---------------------------------------------------
        if (childHovered || state.selecting)
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && childHovered)
            {
                int clickLine = (int)((mousePos.y - windowPos.y) / lineHeight);
                clickLine = std::max(0, std::min(clickLine, (int)lines.size() - 1));
                int clickCol  = lines.empty() ? 0
                              : ColFromMouseX(lines[clickLine], mousePos.x, textStartX);

                state.selStartLine = clickLine;
                state.selStartCol  = clickCol;
                state.selEndLine   = clickLine;
                state.selEndCol    = clickCol;
                state.selecting    = true;
            }

            if (state.selecting && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                int dragLine = (int)((mousePos.y - windowPos.y) / lineHeight);
                dragLine = std::max(0, std::min(dragLine, (int)lines.size() - 1));
                int dragCol = lines.empty() ? 0
                            : ColFromMouseX(lines[dragLine], mousePos.x, textStartX);
                state.selEndLine = dragLine;
                state.selEndCol  = dragCol;
            }

            if (state.selecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                state.selecting = false;
        }

        // -- Keyboard shortcuts ------------------------------------------------
        if (state.hasFocus)
        {
            const ImGuiIO& io = ImGui::GetIO();
            // Ctrl+A: select all
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))
            {
                state.selStartLine = 0;
                state.selStartCol  = 0;
                state.selEndLine   = std::max(0, (int)lines.size() - 1);
                state.selEndCol    = lines.empty() ? 0 : (int)lines.back().size();
            }
            // Ctrl+C: copy selection
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
            {
                int sL = state.selStartLine, sC = state.selStartCol;
                int eL = state.selEndLine,   eC = state.selEndCol;
                std::string sel = BuildSelectedText(lines, sL, sC, eL, eC);
                if (!sel.empty())
                    ImGui::SetClipboardText(sel.c_str());
            }
        }

        // -- Compute ordered selection range -----------------------------------
        int selALine = state.selStartLine, selACol = state.selStartCol;
        int selBLine = state.selEndLine,   selBCol = state.selEndCol;
        bool hasSelection = (selALine >= 0 && (selALine != selBLine || selACol != selBCol));
        if (hasSelection)
            OrderPositions(selALine, selACol, selBLine, selBCol);

        // -- Render lines ------------------------------------------------------
        const ImVec4 selectionBg = ImGui::GetStyleColorVec4(ImGuiCol_TextSelectedBg);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        for (int i = 0; i < (int)lines.size(); ++i)
        {
            const auto& line = lines[i];
            const ImVec4 col = ColorForLine(line, rules, defaultCol);
            const ImVec2 linePos = ImGui::GetCursorScreenPos();

            // Draw selection highlight behind the text.
            if (hasSelection && i >= selALine && i <= selBLine)
            {
                int from = (i == selALine) ? selACol : 0;
                int to   = (i == selBLine) ? selBCol : (int)line.size();
                from = std::min(from, (int)line.size());
                to   = std::min(to,   (int)line.size());

                if (from < to)
                {
                    float x1 = linePos.x + MeasureText(line, 0, from);
                    float x2 = linePos.x + MeasureText(line, 0, to);
                    ImVec2 p1(x1, linePos.y);
                    ImVec2 p2(x2, linePos.y + lineHeight);
                    drawList->AddRectFilled(p1, p2,
                        ImGui::ColorConvertFloat4ToU32(selectionBg));
                }
                else if (i > selALine && i < selBLine && !line.empty())
                {
                    // Full line selected (middle lines of multi-line selection).
                    float w = MeasureText(line, 0, (int)line.size());
                    ImVec2 p1(linePos.x, linePos.y);
                    ImVec2 p2(linePos.x + w, linePos.y + lineHeight);
                    drawList->AddRectFilled(p1, p2,
                        ImGui::ColorConvertFloat4ToU32(selectionBg));
                }
            }

            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        }

        if (scrollToEnd)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

} // namespace widgets
} // namespace ettycc

#endif // SELECTABLE_TEXT_VIEW_HPP

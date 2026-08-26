#include "composition_session.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

using nakl::windows::CompositionSession;
using nakl::windows::EditAction;
using nakl::windows::EventKind;
using nakl::windows::InputEvent;
using nakl::windows::TextExpansion;
using nakl::windows::TypingMethod;
using nakl::windows::Utf16UnitsForTrailingScalars;

[[noreturn]] void Fail(std::string_view message)
{
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

void Expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        Fail(message);
    }
}

void AppendScalar(std::u16string &text, char32_t scalar)
{
    if (scalar <= 0xFFFF)
    {
        text.push_back(static_cast<char16_t>(scalar));
        return;
    }
    scalar -= 0x10000;
    text.push_back(static_cast<char16_t>(0xD800 + (scalar >> 10)));
    text.push_back(static_cast<char16_t>(0xDC00 + (scalar & 0x3FF)));
}

class FakeEditor
{
  public:
    explicit FakeEditor(TypingMethod method, const std::vector<TextExpansion> &expansions = {})
        : session_(method, expansions)
    {
        Expect(session_.valid(), "core session must be created");
    }

    EditAction Type(char32_t scalar)
    {
        auto action = session_.Process(InputEvent{.kind = EventKind::Text, .scalar = scalar});
        Expect(action.has_value(), "core processing must succeed");
        if (action->kind == EditAction::Kind::PassThrough)
        {
            AppendScalar(text_, scalar);
        }
        else
        {
            const std::size_t units = Utf16UnitsForTrailingScalars(text_, action->delete_scalars);
            Expect(units <= text_.size(), "replacement must remain inside document");
            text_.erase(text_.size() - units);
            text_.append(action->insert);
        }
        return *action;
    }

    [[nodiscard]] EditAction Preview(char32_t scalar) const
    {
        auto action = session_.Preview(InputEvent{.kind = EventKind::Text, .scalar = scalar});
        Expect(action.has_value(), "preview must succeed");
        return *action;
    }

    void Boundary()
    {
        session_.Reset();
    }

    [[nodiscard]] const std::u16string &text() const
    {
        return text_;
    }

  private:
    CompositionSession session_;
    std::u16string text_;
};

void VerifyRegression(TypingMethod method, std::u32string_view prefix, char32_t modifier,
                      std::u16string_view expected, std::size_t expected_delete,
                      std::u16string_view expected_insert)
{
    FakeEditor editor(method);
    for (char32_t scalar : prefix)
    {
        editor.Type(scalar);
    }

    const EditAction preview = editor.Preview(modifier);
    Expect(preview.kind == EditAction::Kind::ReplaceBefore, "modifier preview must consume");
    Expect(preview.delete_scalars == expected_delete, "preview deletion count must match");
    Expect(preview.insert == expected_insert, "preview insertion must match");

    const EditAction action = editor.Type(modifier);
    Expect(action.kind == EditAction::Kind::ReplaceBefore, "modifier must consume");
    Expect(action.delete_scalars == expected_delete, "ordered deletion count must match");
    Expect(action.insert == expected_insert, "ordered insertion must match");
    Expect(editor.text() == expected, "fake editor must reach canonical output");
}

} // namespace

int main()
{
    VerifyRegression(TypingMethod::Telex, U"cha", U'f', u"chà", 1, u"à");
    VerifyRegression(TypingMethod::Telex, U"giai", U'r', u"giải", 2, u"ải");
    VerifyRegression(TypingMethod::Telex, U"hai", U'r', u"hải", 2, u"ải");
    VerifyRegression(TypingMethod::Telex, U"hoai", U'r', u"hoải", 2, u"ải");
    VerifyRegression(TypingMethod::Vni, U"giai", U'3', u"giải", 2, u"ải");

    FakeEditor reset_editor(TypingMethod::Telex);
    reset_editor.Type(U'a');
    reset_editor.Boundary();
    reset_editor.Type(U's');
    Expect(reset_editor.text() == u"as", "focus boundary must prevent stale composition");

    const std::u16string supplementary = u"a\U0001F600";
    Expect(Utf16UnitsForTrailingScalars(supplementary, 1) == 2,
           "one supplementary scalar must occupy two UTF-16 units");
    Expect(Utf16UnitsForTrailingScalars(supplementary, 2) == 3,
           "two trailing scalars must include the preceding BMP unit");

    FakeEditor expansion_editor(TypingMethod::Telex, {{"btw", "by the way"}});
    for (char32_t scalar : std::u32string_view(U"btw "))
    {
        expansion_editor.Type(scalar);
    }
    Expect(expansion_editor.text() == u"by the way ", "text expansion contract must apply");

    std::cout << "PASS: Windows composition contract R1-R5 and expansion\n";
    return 0;
}

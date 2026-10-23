#include "composition_session.h"
#include "committed_edit.h"
#include "settings.h"
#include "surrounding_tracker.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

using vigo::linux::CompositionSession;
using vigo::linux::CommittedTextClient;
using vigo::linux::EditAction;
using vigo::linux::EventKind;
using vigo::linux::InputEvent;
using vigo::linux::SurroundingTracker;
using vigo::linux::TextExpansion;
using vigo::linux::TypingMethod;

class RecordingClient final : public CommittedTextClient
{
  public:
    [[nodiscard]] bool CanReplaceBefore(std::size_t, std::string_view) const noexcept override
    {
        return can_delete;
    }

    void DeleteBefore(std::size_t scalar_count) override
    {
        calls.append("delete:").append(std::to_string(scalar_count)).append(";");
    }

    void Commit(std::string_view text) override
    {
        calls.append("commit:").append(text).append(";");
    }

    bool can_delete = true;
    std::string calls;
};

class TrackedRecordingClient final : public CommittedTextClient
{
  public:
    explicit TrackedRecordingClient(SurroundingTracker &tracker) : tracker_(tracker) {}

    [[nodiscard]] bool CanReplaceBefore(std::size_t scalar_count,
                                        std::string_view expected_before) const noexcept override
    {
        return tracker_.CanReplaceBefore(scalar_count, expected_before);
    }

    void DeleteBefore(std::size_t scalar_count) override
    {
        calls.append("delete:").append(std::to_string(scalar_count)).append(";");
    }

    void Commit(std::string_view text) override
    {
        calls.append("commit:").append(text).append(";");
    }

    std::string calls;

  private:
    SurroundingTracker &tracker_;
};

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

[[nodiscard]] std::size_t TrailingUtf8Bytes(std::string_view text, std::size_t scalar_count)
{
    std::size_t index = text.size();
    while (index > 0 && scalar_count > 0)
    {
        --index;
        while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80)
        {
            --index;
        }
        --scalar_count;
    }
    return text.size() - index;
}

class FakeCommittedEditor
{
  public:
    explicit FakeCommittedEditor(TypingMethod method,
                                 const std::vector<TextExpansion> &expansions = {})
        : session_(method, expansions)
    {
        Expect(session_.valid(), "core session must be created");
    }

    EditAction Type(char32_t scalar)
    {
        const auto preview = session_.Preview(InputEvent{.kind = EventKind::Text, .scalar = scalar});
        Expect(preview.has_value(), "preview must succeed");
        auto action = session_.Process(InputEvent{.kind = EventKind::Text, .scalar = scalar});
        Expect(action.has_value(), "core processing must succeed");
        Expect(action->kind == preview->kind, "preview must preserve live action kind");
        Expect(action->delete_scalars == preview->delete_scalars,
               "preview must preserve live deletion count");
        Expect(action->insert == preview->insert, "preview must preserve live insertion");
        if (action->kind == EditAction::Kind::PassThrough)
        {
            Expect(scalar <= 0x7F, "test pass-through input must be ASCII");
            text_.push_back(static_cast<char>(scalar));
        }
        else
        {
            const std::size_t bytes = TrailingUtf8Bytes(text_, action->delete_scalars);
            Expect(bytes <= text_.size(), "replacement must remain inside document");
            text_.erase(text_.size() - bytes);
            text_.append(action->insert);
        }
        return *action;
    }

    void Boundary()
    {
        session_.Reset();
    }

    [[nodiscard]] const std::string &text() const
    {
        return text_;
    }

  private:
    CompositionSession session_;
    std::string text_;
};

void VerifyRegression(TypingMethod method, std::u32string_view prefix, char32_t modifier,
                      std::string_view expected, std::size_t expected_delete,
                      std::string_view expected_before, std::string_view expected_insert)
{
    FakeCommittedEditor editor(method);
    for (const char32_t scalar : prefix)
    {
        editor.Type(scalar);
    }

    const EditAction action = editor.Type(modifier);
    Expect(action.kind == EditAction::Kind::ReplaceBefore, "modifier must be consumed");
    Expect(action.delete_scalars == expected_delete, "ordered deletion count must match");
    Expect(action.expected_before == expected_before, "replacement must carry a suffix guard");
    Expect(action.insert == expected_insert, "ordered insertion must match");
    Expect(editor.text() == expected, "fake committed editor must reach canonical output");
}

} // namespace

int main()
{
    VerifyRegression(TypingMethod::Telex, U"cha", U'f', "chà", 1, "a", "à");
    VerifyRegression(TypingMethod::Telex, U"giai", U'r', "giải", 2, "ai", "ải");
    VerifyRegression(TypingMethod::Telex, U"hai", U'r', "hải", 2, "ai", "ải");
    VerifyRegression(TypingMethod::Telex, U"hoai", U'r', "hoải", 2, "ai", "ải");
    VerifyRegression(TypingMethod::Vni, U"giai", U'3', "giải", 2, "ai", "ải");

    FakeCommittedEditor literal_editor(TypingMethod::Telex);
    for (const char32_t scalar : std::u32string_view(U"verrsion"))
    {
        literal_editor.Type(scalar);
    }
    Expect(literal_editor.text() == "version",
           "repeated tone modifier must keep the rest of the word literal");

    FakeCommittedEditor reset_editor(TypingMethod::Telex);
    reset_editor.Type(U'a');
    reset_editor.Boundary();
    reset_editor.Type(U's');
    Expect(reset_editor.text() == "as", "focus boundary must prevent stale composition");

    FakeCommittedEditor expansion_editor(TypingMethod::Telex, {{"btw", "by the way"}});
    for (const char32_t scalar : std::u32string_view(U"btw "))
    {
        expansion_editor.Type(scalar);
    }
    Expect(expansion_editor.text() == "by the way ", "text expansion contract must apply");

    RecordingClient client;
    Expect(vigo::linux::ApplyCommittedEdit(
               client, {.kind = EditAction::Kind::ReplaceBefore,
                        .delete_scalars = 2,
                        .expected_before = "ai",
                        .insert = "ải"}),
           "capable client must accept a committed replacement");
    Expect(client.calls == "delete:2;commit:ải;", "adapter order must be delete then commit");

    RecordingClient unsupported;
    unsupported.can_delete = false;
    Expect(!vigo::linux::ApplyCommittedEdit(
               unsupported, {.kind = EditAction::Kind::ReplaceBefore,
                             .delete_scalars = 1,
                             .expected_before = "a",
                             .insert = "à"}),
           "client without native deletion must fail closed");
    Expect(unsupported.calls.empty(), "failed committed edit must have no partial effects");

    CompositionSession unsupported_session(TypingMethod::Telex);
    for (const char32_t scalar : std::u32string_view(U"cha"))
    {
        auto ignored = unsupported_session.Process({.kind = EventKind::Text, .scalar = scalar});
        Expect(ignored.has_value(), "unsupported-client setup must process");
    }
    const auto unsupported_preview =
        unsupported_session.Preview({.kind = EventKind::Text, .scalar = U'f'});
    Expect(unsupported_preview.has_value() &&
               unsupported_preview->kind == EditAction::Kind::ReplaceBefore,
           "unsupported client must detect the replacement without advancing live state");
    unsupported_session.Reset();
    const auto after_reset = unsupported_session.Process({.kind = EventKind::Text, .scalar = U'f'});
    Expect(after_reset.has_value() && after_reset->kind == EditAction::Kind::PassThrough,
           "failed capability check must reset stale composition");

    FakeCommittedEditor first_context(TypingMethod::Telex);
    FakeCommittedEditor second_context(TypingMethod::Telex);
    for (const char32_t scalar : std::u32string_view(U"giai"))
    {
        first_context.Type(scalar);
    }
    for (const char32_t scalar : std::u32string_view(U"cha"))
    {
        second_context.Type(scalar);
    }
    first_context.Type(U'r');
    second_context.Type(U'f');
    Expect(first_context.text() == "giải" && second_context.text() == "chà",
           "input contexts must own independent sessions");

    const auto settings = vigo::linux::ParseSettings(
        "version=1\nenabled=false\nexpansion=btw\tby the way\nexcluded=code\n");
    Expect(!settings.enabled && settings.expansions.size() == 1,
           "settings parser must create one immutable expansion snapshot");
    Expect(vigo::linux::IsApplicationExcluded(settings, "code"),
           "settings parser must preserve application exclusions");
    Expect(vigo::linux::IsApplicationExcluded(settings, "gtk3-im:code"),
           "IBus frontend prefixes must not bypass application exclusions");
    const auto future_settings = vigo::linux::ParseSettings("version=2\nenabled=false\n");
    Expect(future_settings.enabled && future_settings.expansions.empty(),
           "unknown settings versions must fail to safe defaults");

    SurroundingTracker tracker;
    Expect(!tracker.Observe("giai and ai", 4, 4), "first surrounding snapshot must initialize");
    Expect(tracker.CanReplaceBefore(2, "ai"), "tracked suffix must be replaceable");
    Expect(tracker.Observe("giai and ai", 11, 11),
           "caret move to an identical suffix must be unexpected");
    Expect(tracker.CanReplaceBefore(2, "ai"), "new caret snapshot must replace only its own text");

    tracker.Clear();
    Expect(!tracker.Observe("gia", 3, 3), "pass-through tracker must initialize");
    Expect(tracker.ExpectPassThrough({.kind = EventKind::Text, .scalar = U'i'}),
           "printable pass-through must predict a native insertion");
    Expect(tracker.pending(), "predicted insertion must remain pending until observed");
    Expect(!tracker.Observe("gia", 3, 3) && tracker.pending(),
           "duplicate native snapshots must not cancel a predicted insertion");
    Expect(!tracker.Observe("giai", 4, 4), "predicted native insertion must be accepted");
    Expect(!tracker.pending(), "accepted insertion must clear the pending update");

    const EditAction replacement{.kind = EditAction::Kind::ReplaceBefore,
                                 .delete_scalars = 2,
                                 .expected_before = "ai",
                                 .insert = "ải"};
    TrackedRecordingClient tracked_client(tracker);
    Expect(vigo::linux::ApplyTrackedCommittedEdit(tracked_client, tracker, replacement),
           "validated replacement must be predicted and dispatched exactly once");
    Expect(tracked_client.calls == "delete:2;commit:ải;",
           "tracked adapter order must be delete then commit");
    Expect(tracker.pending(), "tracked dispatch must await native surrounding updates");
    Expect(!tracker.Observe("gi", 2, 2), "predicted delete snapshot must be accepted");
    Expect(!tracker.Observe("giải", 4, 4), "predicted commit snapshot must be accepted");

    Expect(tracker.ExpectPassThrough({.kind = EventKind::Text, .scalar = U'!'}),
           "second pass-through must be tracked");
    tracker.CancelPending();
    Expect(!tracker.CanReplaceBefore(2, "ải"),
           "unconfirmed native update must invalidate replacement capability");

    std::cout << "PASS: Linux committed-edit contract R1-R6 and expansion\n";
    return 0;
}

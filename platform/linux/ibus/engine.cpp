#include "engine.h"

#include "composition_session.h"
#include "committed_edit.h"
#include "settings.h"
#include "surrounding_tracker.h"

#include <cstring>
#include <limits>
#include <new>
#include <vector>

struct _NaklIBusEngine
{
    IBusEngine parent_instance;
    nakl::linux::CompositionSession *session;
    nakl::linux::SettingsSnapshot *settings;
    nakl::linux::SurroundingTracker *surrounding;
    std::string *client;
    gboolean excluded;
    nakl::linux::TypingMethod method;
    gboolean secure;
};

struct _NaklIBusEngineClass
{
    IBusEngineClass parent_class;
};

using NaklIBusEngine = struct _NaklIBusEngine;
using NaklIBusEngineClass = struct _NaklIBusEngineClass;

G_DEFINE_TYPE(NaklIBusEngine, nakl_ibus_engine, IBUS_TYPE_ENGINE)

namespace
{

[[nodiscard]] nakl::linux::TypingMethod RequestedMethod(IBusEngine *engine)
{
    const gchar *name = ibus_engine_get_name(engine);
    return name != nullptr && std::strstr(name, "vni") != nullptr
               ? nakl::linux::TypingMethod::Vni
               : nakl::linux::TypingMethod::Telex;
}

void ResetSession(NaklIBusEngine *self)
{
    if (self->session != nullptr)
    {
        self->session->Reset();
    }
}

[[nodiscard]] bool EnsureSession(NaklIBusEngine *self)
{
    if (self->settings != nullptr && (!self->settings->enabled || self->excluded != FALSE))
    {
        return false;
    }
    const auto requested = RequestedMethod(IBUS_ENGINE(self));
    if (self->session != nullptr && self->method == requested && self->session->valid())
    {
        return true;
    }
    delete self->session;
    self->method = requested;
    const std::vector<nakl::linux::TextExpansion> empty_expansions;
    const auto &expansions =
        self->settings != nullptr ? self->settings->expansions : empty_expansions;
    self->session = new (std::nothrow) nakl::linux::CompositionSession(requested, expansions);
    return self->session != nullptr && self->session->valid();
}

void ReloadSettings(NaklIBusEngine *self)
{
    delete self->session;
    self->session = nullptr;
    delete self->settings;
    self->settings = new (std::nothrow) nakl::linux::SettingsSnapshot(nakl::linux::LoadSettings());
    self->excluded = self->settings != nullptr && self->client != nullptr &&
                             nakl::linux::IsApplicationExcluded(*self->settings, *self->client)
                         ? TRUE
                         : FALSE;
    if (self->settings != nullptr && self->settings->enabled && self->excluded == FALSE)
    {
        static_cast<void>(EnsureSession(self));
    }
}

[[nodiscard]] bool HasCommandModifier(guint state)
{
    constexpr guint CommandModifiers =
        IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_SUPER_MASK | IBUS_HYPER_MASK | IBUS_META_MASK;
    return (state & CommandModifiers) != 0;
}

class IBusCommittedClient final : public nakl::linux::CommittedTextClient
{
  public:
    explicit IBusCommittedClient(IBusEngine *engine) : engine_(engine) {}

    [[nodiscard]] bool CanReplaceBefore(std::size_t scalar_count,
                                        std::string_view expected_before) const noexcept override
    {
        const auto *self = reinterpret_cast<const NaklIBusEngine *>(engine_);
        if (scalar_count > static_cast<std::size_t>(std::numeric_limits<gint>::max()) ||
            (engine_->client_capabilities & IBUS_CAP_SURROUNDING_TEXT) == 0 ||
            self->surrounding == nullptr)
        {
            return false;
        }
        return self->surrounding->CanReplaceBefore(scalar_count, expected_before);
    }

    void DeleteBefore(std::size_t scalar_count) override
    {
        const auto count = static_cast<guint>(scalar_count);
        ibus_engine_delete_surrounding_text(engine_, -static_cast<gint>(count), count);
    }

    void Commit(std::string_view text) override
    {
        const std::string owned(text);
        ibus_engine_commit_text(engine_, ibus_text_new_from_string(owned.c_str()));
    }

  private:
    IBusEngine *engine_;
};

[[nodiscard]] gboolean ProcessKeyEvent(IBusEngine *engine, guint keyval, guint keycode, guint state)
{
    static_cast<void>(keycode);
    auto *self = reinterpret_cast<NaklIBusEngine *>(engine);
    try
    {
        if ((state & IBUS_RELEASE_MASK) != 0)
        {
            return FALSE;
        }
        switch (keyval)
        {
        case IBUS_KEY_Shift_L:
        case IBUS_KEY_Shift_R:
        case IBUS_KEY_Control_L:
        case IBUS_KEY_Control_R:
        case IBUS_KEY_Caps_Lock:
        case IBUS_KEY_Shift_Lock:
        case IBUS_KEY_Meta_L:
        case IBUS_KEY_Meta_R:
        case IBUS_KEY_Alt_L:
        case IBUS_KEY_Alt_R:
        case IBUS_KEY_Super_L:
        case IBUS_KEY_Super_R:
        case IBUS_KEY_Hyper_L:
        case IBUS_KEY_Hyper_R:
            return FALSE;
        default:
            break;
        }
        if (self->secure != FALSE || HasCommandModifier(state))
        {
            ResetSession(self);
            return FALSE;
        }
        if (!EnsureSession(self))
        {
            return FALSE;
        }
        if (self->surrounding != nullptr && self->surrounding->pending())
        {
            // The frontend never confirmed the previous committed edit. The
            // current key must pass through without using speculative state.
            self->surrounding->CancelPending();
            ResetSession(self);
            return FALSE;
        }

        nakl::linux::InputEvent event{.kind = nakl::linux::EventKind::Boundary};
        if (keyval == IBUS_KEY_BackSpace)
        {
            event.kind = nakl::linux::EventKind::Backspace;
        }
        else
        {
            const auto scalar = static_cast<char32_t>(ibus_keyval_to_unicode(keyval));
            if (scalar >= U' ' && scalar != 0x7F)
            {
                event = {.kind = nakl::linux::EventKind::Text, .scalar = scalar};
            }
        }

        auto preview = self->session->Preview(event);
        if (!preview.has_value())
        {
            delete self->session;
            self->session = nullptr;
            return FALSE;
        }
        IBusCommittedClient client(engine);
        if (preview->kind == nakl::linux::EditAction::Kind::ReplaceBefore &&
            !client.CanReplaceBefore(preview->delete_scalars, preview->expected_before))
        {
            // A committed replacement is atomic only when the client supports
            // delete-surrounding. Do not synthesize Backspace or advance the
            // live session when the native capability is unavailable.
            ResetSession(self);
            return FALSE;
        }

        auto action = self->session->Process(event);
        if (!action.has_value())
        {
            delete self->session;
            self->session = nullptr;
            return FALSE;
        }
        if (action->kind == nakl::linux::EditAction::Kind::PassThrough)
        {
            if (self->surrounding == nullptr || !self->surrounding->ExpectPassThrough(event))
            {
                ResetSession(self);
            }
            return FALSE;
        }
        if (self->surrounding == nullptr)
        {
            ResetSession(self);
            return FALSE;
        }
        if (nakl::linux::ApplyTrackedCommittedEdit(client, *self->surrounding, *action))
        {
            return TRUE;
        }
        self->surrounding->CancelPending();
        ResetSession(self);
        return FALSE;
    }
    catch (...)
    {
        delete self->session;
        self->session = nullptr;
        return FALSE;
    }
}

void Reset(IBusEngine *engine)
{
    ResetSession(reinterpret_cast<NaklIBusEngine *>(engine));
}

void FocusIn(IBusEngine *engine)
{
    auto *self = reinterpret_cast<NaklIBusEngine *>(engine);
    try
    {
        ReloadSettings(self);
    }
    catch (...)
    {
        delete self->session;
        self->session = nullptr;
    }
}

void FocusInId(IBusEngine *engine, const gchar *object_path, const gchar *client)
{
    static_cast<void>(object_path);
    auto *self = reinterpret_cast<NaklIBusEngine *>(engine);
    try
    {
        delete self->client;
        self->client = client != nullptr ? new std::string(client) : nullptr;
        ReloadSettings(self);
    }
    catch (...)
    {
        delete self->session;
        self->session = nullptr;
    }
}

void FocusOut(IBusEngine *engine)
{
    auto *self = reinterpret_cast<NaklIBusEngine *>(engine);
    if (self->surrounding != nullptr)
    {
        self->surrounding->Clear();
    }
    Reset(engine);
}

void FocusOutId(IBusEngine *engine, const gchar *object_path)
{
    static_cast<void>(object_path);
    FocusOut(engine);
}

void Enable(IBusEngine *engine)
{
    auto *self = reinterpret_cast<NaklIBusEngine *>(engine);
    try
    {
        ReloadSettings(self);
        ibus_engine_get_surrounding_text(engine, nullptr, nullptr, nullptr);
    }
    catch (...)
    {
        delete self->session;
        self->session = nullptr;
    }
}

void Disable(IBusEngine *engine)
{
    Reset(engine);
}

void SetContentType(IBusEngine *engine, guint purpose, guint hints)
{
    auto *self = reinterpret_cast<NaklIBusEngine *>(engine);
    self->secure = purpose == IBUS_INPUT_PURPOSE_PASSWORD || purpose == IBUS_INPUT_PURPOSE_PIN ||
                   (hints & IBUS_INPUT_HINT_PRIVATE) != 0;
    if (self->secure != FALSE)
    {
        ResetSession(self);
    }
}

void SetSurroundingText(IBusEngine *engine, IBusText *text, guint cursor, guint anchor)
{
    auto *self = reinterpret_cast<NaklIBusEngine *>(engine);
    try
    {
        const gchar *contents = text != nullptr ? ibus_text_get_text(text) : nullptr;
        if (contents == nullptr || !g_utf8_validate(contents, -1, nullptr))
        {
            if (self->surrounding != nullptr)
            {
                self->surrounding->Clear();
            }
            ResetSession(self);
            return;
        }
        if (self->surrounding == nullptr)
        {
            self->surrounding = new nakl::linux::SurroundingTracker();
        }
        if (self->surrounding->Observe(contents, cursor, anchor))
        {
            ResetSession(self);
        }
    }
    catch (...)
    {
        if (self->surrounding != nullptr)
        {
            self->surrounding->Clear();
        }
        ResetSession(self);
    }
}

void Finalize(GObject *object)
{
    auto *self = reinterpret_cast<NaklIBusEngine *>(object);
    delete self->session;
    self->session = nullptr;
    delete self->settings;
    self->settings = nullptr;
    delete self->surrounding;
    self->surrounding = nullptr;
    delete self->client;
    self->client = nullptr;
    G_OBJECT_CLASS(nakl_ibus_engine_parent_class)->finalize(object);
}

} // namespace

static void nakl_ibus_engine_init(NaklIBusEngine *self)
{
    self->session = nullptr;
    self->settings = nullptr;
    self->surrounding = nullptr;
    self->client = nullptr;
    self->excluded = FALSE;
    self->method = nakl::linux::TypingMethod::Telex;
    self->secure = FALSE;
}

static void nakl_ibus_engine_class_init(NaklIBusEngineClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = Finalize;

    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);
    engine_class->process_key_event = ProcessKeyEvent;
    engine_class->focus_in = FocusIn;
    engine_class->focus_in_id = FocusInId;
    engine_class->focus_out = FocusOut;
    engine_class->focus_out_id = FocusOutId;
    engine_class->reset = Reset;
    engine_class->enable = Enable;
    engine_class->disable = Disable;
    engine_class->set_content_type = SetContentType;
    engine_class->set_surrounding_text = SetSurroundingText;
}

#include "committed_edit.h"
#include "composition_session.h"
#include "settings.h"
#include "surrounding_tracker.h"

#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using vigo::linux::CompositionSession;
using vigo::linux::EditAction;
using vigo::linux::EventKind;
using vigo::linux::InputEvent;
using vigo::linux::TypingMethod;

class ContextState final : public fcitx::InputContextProperty
{
  public:
    void Reload(TypingMethod method, std::string_view application)
    {
        // Invalidate the old context before any allocation or settings I/O so
        // an exception cannot preserve composition from the previous focus.
        active_ = false;
        session_.reset();
        surrounding_.Clear();
        expansions_.clear();
        const auto settings = vigo::linux::LoadSettings();
        expansions_ = settings.expansions;
        active_ = settings.enabled && !vigo::linux::IsApplicationExcluded(settings, application);
        if (active_)
        {
            static_cast<void>(Ensure(method));
        }
    }

    [[nodiscard]] bool Ensure(TypingMethod method)
    {
        if (!active_)
        {
            return false;
        }
        if (session_ != nullptr && method_ == method && session_->valid())
        {
            return true;
        }
        method_ = method;
        session_ = std::make_unique<CompositionSession>(method, expansions_);
        return session_->valid();
    }

    void Reset() noexcept
    {
        if (session_ != nullptr)
        {
            session_->Reset();
        }
    }

    void ObserveSurrounding(const fcitx::SurroundingText &surrounding)
    {
        if (!surrounding.isValid() ||
            surrounding_.Observe(surrounding.text(), surrounding.cursor(), surrounding.anchor()))
        {
            surrounding_.Clear();
            Reset();
        }
    }

    [[nodiscard]] vigo::linux::SurroundingTracker &surrounding() noexcept
    {
        return surrounding_;
    }

    [[nodiscard]] CompositionSession *session() noexcept
    {
        return session_.get();
    }

  private:
    TypingMethod method_ = TypingMethod::Telex;
    bool active_ = true;
    std::vector<vigo::linux::TextExpansion> expansions_;
    std::unique_ptr<CompositionSession> session_;
    vigo::linux::SurroundingTracker surrounding_;
};

class FcitxCommittedClient final : public vigo::linux::CommittedTextClient
{
  public:
    FcitxCommittedClient(fcitx::InputContext *context,
                         vigo::linux::SurroundingTracker *surrounding)
        : context_(context), surrounding_(surrounding)
    {
    }

    [[nodiscard]] bool CanReplaceBefore(std::size_t scalar_count,
                                        std::string_view expected_before) const noexcept override
    {
        if (scalar_count > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            !context_->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText) ||
            surrounding_ == nullptr)
        {
            return false;
        }
        return surrounding_->CanReplaceBefore(scalar_count, expected_before);
    }

    void DeleteBefore(std::size_t scalar_count) override
    {
        const auto count = static_cast<unsigned int>(scalar_count);
        context_->deleteSurroundingText(-static_cast<int>(count), count);
    }

    void Commit(std::string_view text) override
    {
        context_->commitString(std::string(text));
    }

  private:
    fcitx::InputContext *context_;
    vigo::linux::SurroundingTracker *surrounding_;
};

[[nodiscard]] TypingMethod MethodFor(const fcitx::InputMethodEntry &entry)
{
    return entry.uniqueName().find("vni") != std::string::npos ? TypingMethod::Vni
                                                               : TypingMethod::Telex;
}

[[nodiscard]] bool IsSecureOrDisabled(const fcitx::InputContext &context)
{
    const auto capabilities = context.capabilityFlags();
    return capabilities.testAny(fcitx::CapabilityFlag::PasswordOrSensitive) ||
           capabilities.test(fcitx::CapabilityFlag::Disable);
}

[[nodiscard]] bool HasCommandModifier(const fcitx::Key &key)
{
    const fcitx::KeyStates command_states{
        fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,   fcitx::KeyState::Super,
        fcitx::KeyState::Hyper, fcitx::KeyState::Meta,
    };
    return key.states().testAny(command_states);
}

class VigoEngine final : public fcitx::InputMethodEngineV2
{
  public:
    explicit VigoEngine(fcitx::Instance *instance)
        : instance_(instance), factory_([](fcitx::InputContext &) { return new ContextState(); })
    {
        instance_->inputContextManager().registerProperty("vigoCompositionSession", &factory_);
        surrounding_watcher_ = instance_->watchEvent(
            fcitx::EventType::InputContextSurroundingTextUpdated,
            fcitx::EventWatcherPhase::Default, [this](fcitx::Event &event) {
                auto &updated = static_cast<fcitx::SurroundingTextUpdatedEvent &>(event);
                auto *context = updated.inputContext();
                context->propertyFor(&factory_)->ObserveSurrounding(context->surroundingText());
            });
    }

    void keyEvent(const fcitx::InputMethodEntry &entry, fcitx::KeyEvent &event) override
    {
        auto *context = event.inputContext();
        auto *state = context->propertyFor(&factory_);
        try
        {
            if (event.isRelease())
            {
                return;
            }
            if (event.key().isModifier())
            {
                return;
            }
            if (IsSecureOrDisabled(*context) || HasCommandModifier(event.key()))
            {
                state->Reset();
                return;
            }
            if (!state->Ensure(MethodFor(entry)))
            {
                return;
            }
            if (state->surrounding().pending())
            {
                state->surrounding().CancelPending();
                state->Reset();
                return;
            }

            InputEvent input{.kind = EventKind::Boundary};
            if (event.key().sym() == FcitxKey_BackSpace)
            {
                input.kind = EventKind::Backspace;
            }
            else
            {
                const auto scalar =
                    static_cast<char32_t>(fcitx::Key::keySymToUnicode(event.key().sym()));
                if (scalar >= U' ' && scalar != 0x7F)
                {
                    input = {.kind = EventKind::Text, .scalar = scalar};
                }
            }

            auto preview = state->session()->Preview(input);
            if (!preview.has_value())
            {
                state->Reset();
                return;
            }
            FcitxCommittedClient client(context, &state->surrounding());
            if (preview->kind == EditAction::Kind::ReplaceBefore &&
                !client.CanReplaceBefore(preview->delete_scalars, preview->expected_before))
            {
                state->Reset();
                return;
            }

            auto action = state->session()->Process(input);
            if (!action.has_value() || action->kind == EditAction::Kind::PassThrough)
            {
                if (action.has_value() &&
                    !state->surrounding().ExpectPassThrough(input))
                {
                    state->Reset();
                }
                return;
            }
            if (vigo::linux::ApplyTrackedCommittedEdit(client, state->surrounding(), *action))
            {
                event.filterAndAccept();
            }
            else
            {
                state->surrounding().CancelPending();
                state->Reset();
            }
        }
        catch (...)
        {
            state->Reset();
        }
    }

    void activate(const fcitx::InputMethodEntry &entry, fcitx::InputContextEvent &event) override
    {
        auto *context = event.inputContext();
        auto *state = context->propertyFor(&factory_);
        try
        {
            state->Reload(MethodFor(entry), context->program());
            state->ObserveSurrounding(context->surroundingText());
        }
        catch (...)
        {
            state->Reset();
        }
    }

    void reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override
    {
        event.inputContext()->propertyFor(&factory_)->Reset();
    }

  private:
    fcitx::Instance *instance_;
    fcitx::FactoryFor<ContextState> factory_;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>> surrounding_watcher_;
};

class VigoEngineFactory final : public fcitx::AddonFactory
{
  public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override
    {
        return new VigoEngine(manager->instance());
    }
};

} // namespace

FCITX_ADDON_FACTORY(VigoEngineFactory)

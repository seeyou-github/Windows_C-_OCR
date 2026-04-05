#pragma once

#include "darkui/button.h"
#include "darkui/static.h"

#include <memory>
#include <string>

namespace darkui {

// Modal dark dialog with a custom title bar, body area, and confirm/cancel buttons.
// Usage:
// - Fill Dialog::Options and call Create(owner, id, theme, options).
// - Use ThemedWindowHost for the owner window when you want one shared top-level theme workflow.
// - For custom grouped body sections, parent child controls to darkui::Panel inside content_hwnd().
// - Inside custom bodies, prefer per-control `variant` presets before manually tuning low-level styling.
// - Use SetMessage() for a simple centered message body, or create your own child
//   controls inside content_hwnd() for custom layouts.
// - Call ShowModal() to display the popup and run a local modal message loop.
// - Read values from child controls after ShowModal() returns and before the Dialog
//   object is destroyed when the dialog contains input fields.
// Notes:
// - Unknown WM_COMMAND / WM_NOTIFY messages are forwarded to the owner window.
// - The built-in message label can be hidden when you want a fully custom body.
// - EndDialog() now ends the modal loop and hides the dialog window first; final
//   destruction still happens in Destroy() or the Dialog destructor.
// - This avoids a real bug seen in production: if the dialog destroyed its child
//   controls inside EndDialog(), callers that read Edit/ComboBox values after
//   ShowModal() returned could get empty text even though the user had typed input.
class Dialog {
public:
    struct Options {
        std::wstring title = L"Dialog";
        std::wstring message{};
        std::wstring confirmText = L"Confirm";
        std::wstring cancelText = L"Cancel";
        int width = 480;
        int height = 280;
        bool messageVisible = true;
        bool cancelVisible = true;
    };

    enum class Result {
        None = 0,
        Confirm = 1,
        Cancel = 2
    };

    struct Impl;

    Dialog();
    ~Dialog();

    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;

    // Creates the dialog window and its built-in controls.
    bool Create(HWND owner, int controlId, const Theme& theme, const Options& options);
    // Destroys the popup window and all built-in child controls.
    void Destroy();

    // Returns the popup window handle.
    HWND hwnd() const { return dialogHwnd_; }
    // Returns the owner window handle passed to Create().
    HWND owner() const { return ownerHwnd_; }
    // Returns the central content host window.
    // Create custom child controls inside this host when you do not want to use
    // the built-in message label.
    HWND content_hwnd() const { return contentHwnd_; }
    // Returns the built-in confirm button handle.
    HWND confirm_button() const { return confirmButton_.hwnd(); }
    // Returns the built-in cancel button handle.
    HWND cancel_button() const { return cancelButton_.hwnd(); }
    // Returns the built-in message label handle.
    HWND message_hwnd() const { return messageLabel_.hwnd(); }
    // Returns the current modal result.
    Result modal_result() const { return modalResult_; }
    // Returns the active theme.
    const Theme& theme() const { return theme_; }

    // Low-level theme hook used by ThemeManager.
    void SetTheme(const Theme& theme);
    // Updates the title-bar text.
    void SetTitle(const std::wstring& title);
    // Updates the built-in centered message body and makes it visible.
    void SetMessage(const std::wstring& text);
    // Returns the current built-in message text.
    std::wstring GetMessage() const;
    // Updates the confirm button text.
    void SetConfirmText(const std::wstring& text);
    // Updates the cancel button text.
    void SetCancelText(const std::wstring& text);
    // Shows or hides the built-in message label.
    void SetMessageVisible(bool visible);
    // Shows or hides the cancel button.
    void SetCancelVisible(bool visible);
    // Sets the modal result, exits the modal loop, and hides the dialog window.
    // Notes:
    // - This does not immediately destroy the dialog window.
    // - Delayed destruction lets caller code safely read child-control values after
    //   ShowModal() returns.
    void EndDialog(Result result);
    // Shows the dialog modally and returns the final modal result.
    // Notes:
    // - After this returns, custom child controls are still available until Destroy()
    //   or the Dialog destructor runs.
    // - For input dialogs, read control values immediately after ShowModal() returns,
    //   then let the Dialog object fall out of scope or call Destroy().
    Result ShowModal();

private:
    std::unique_ptr<Impl> impl_;
    HWND ownerHwnd_ = nullptr;
    HWND dialogHwnd_ = nullptr;
    HWND contentHwnd_ = nullptr;
    int controlId_ = 0;
    int width_ = 480;
    int height_ = 280;
    bool messageVisible_ = true;
    bool cancelVisible_ = true;
    bool modalRunning_ = false;
    Result modalResult_ = Result::None;
    Theme theme_{};
    Button confirmButton_{};
    Button cancelButton_{};
    Static titleLabel_{};
    Static messageLabel_{};
};

}  // namespace darkui

// SPDX-License-Identifier: MIT
// Test-only virtual keyboard. Uses evdev keycodes from the standard US layout.
#include "virtual-keyboard-client.h"
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <sys/mman.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

struct Globals {
    wl_seat *seat = nullptr;
    zwp_virtual_keyboard_manager_v1 *manager = nullptr;
};

int main()
{
    auto *display = wl_display_connect(nullptr);
    if (!display)
        return 1;
    Globals globals;
    auto *registry = wl_display_get_registry(display);
    const wl_registry_listener listener{
        [](void *data, wl_registry *reg, uint32_t name, const char *interface, uint32_t) {
            auto &g = *static_cast<Globals *>(data);
            if (std::strcmp(interface, "wl_seat") == 0)
                g.seat = static_cast<wl_seat *>(wl_registry_bind(reg, name, &wl_seat_interface, 1));
            if (std::strcmp(interface, "zwp_virtual_keyboard_manager_v1") == 0)
                g.manager = static_cast<zwp_virtual_keyboard_manager_v1 *>(wl_registry_bind(
                    reg, name, &zwp_virtual_keyboard_manager_v1_interface, 1));
        },
        [](void *, wl_registry *, uint32_t) {}};
    wl_registry_add_listener(registry, &listener, &globals);
    if (wl_display_roundtrip(display) < 0 || !globals.seat || !globals.manager)
        return 1;
    auto *keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(globals.manager, globals.seat);
    auto *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    const xkb_rule_names names{"evdev", "pc105", "us", "", ""};
    auto *keymap = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap)
        return 1;
    char *map = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    const auto size = std::strlen(map) + 1;
    const int fd = memfd_create("schnellertype-test-keymap", MFD_CLOEXEC);
    if (fd < 0 || write(fd, map, size) != static_cast<ssize_t>(size))
        return 1;
    zwp_virtual_keyboard_v1_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, static_cast<uint32_t>(size));
    close(fd);
    free(map);
    if (wl_display_roundtrip(display) < 0)
        return 1;
    std::cout << "ready" << std::endl;
    const auto modifier = [&](const char *name) { return 1u << xkb_keymap_mod_get_index(keymap, name); };
    const auto send = [&](xkb_keysym_t symbol, bool ctrl, bool shift) {
        for (auto code = xkb_keymap_min_keycode(keymap); code <= xkb_keymap_max_keycode(keymap); ++code) {
            for (xkb_level_index_t level = 0; level < 2; ++level) {
                const xkb_keysym_t *symbols = nullptr;
                if (xkb_keymap_key_get_syms_by_level(keymap, code, 0, level, &symbols) != 1 || symbols[0] != symbol)
                    continue;
                const auto mods = (ctrl ? modifier(XKB_MOD_NAME_CTRL) : 0)
                    | ((shift || level == 1) ? modifier(XKB_MOD_NAME_SHIFT) : 0);
                zwp_virtual_keyboard_v1_modifiers(keyboard, mods, 0, 0, 0);
                const auto now = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
                zwp_virtual_keyboard_v1_key(keyboard, now, code - 8, WL_KEYBOARD_KEY_STATE_PRESSED);
                zwp_virtual_keyboard_v1_key(keyboard, now + 1, code - 8, WL_KEYBOARD_KEY_STATE_RELEASED);
                zwp_virtual_keyboard_v1_modifiers(keyboard, 0, 0, 0, 0);
                wl_display_flush(display);
                QThread::msleep(20);
                return true;
            }
        }
        return false;
    };
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto command = QJsonDocument::fromJson(QByteArray::fromStdString(line)).object();
        bool success = !command.isEmpty();
        if (command.contains("text")) {
            for (const auto value : command.value("text").toString().toUcs4())
                success = send(xkb_utf32_to_keysym(value), false, false) && success;
        } else if (command.contains("key")) {
            success = send(xkb_keysym_from_name(command.value("key").toString().toUtf8().constData(),
                                                XKB_KEYSYM_NO_FLAGS),
                           command.value("ctrl").toBool(), command.value("shift").toBool()) && success;
        } else {
            success = false;
        }
        success = wl_display_roundtrip(display) >= 0 && success;
        std::cout << (success ? "ok" : "error") << std::endl;
    }
    zwp_virtual_keyboard_v1_destroy(keyboard);
    wl_display_roundtrip(display);
    wl_display_disconnect(display);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    return 0;
}

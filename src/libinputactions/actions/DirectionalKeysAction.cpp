// SPDX-License-Identifier: GPL-3.0-or-later
#include "DirectionalKeysAction.h"
#include <algorithm>
#include <cmath>
#include <libinputactions/conditions/Condition.h>
#include <libinputactions/helpers/QThread.h>
#include <libinputactions/input/backends/InputBackend.h>
#include <libinputactions/input/devices/VirtualKeyboard.h>
#include <numbers>
#include <stdexcept>

namespace InputActions
{
namespace
{
int direction(qreal x, qreal y)
{
    return std::abs(x) >= std::abs(y) ? (x < 0 ? 0 : 1) : (y < 0 ? 2 : 3);
}
void accumulate(qreal &total, qreal delta)
{
    total = delta != 0 && total * delta < 0 ? delta : total + delta;
}
}
DirectionalKeysAction::DirectionalKeysAction(qreal distance, Bindings keys, Bindings initial)
    : m_distance(distance)
    , m_keys(std::move(keys))
    , m_initial(std::move(initial))
{
    if (!std::isfinite(distance) || distance <= 0)
        throw std::invalid_argument("Invalid directional-key distance");
    for (const auto *bindings : {&m_keys, &m_initial}) {
        for (const auto &shortcut : *bindings) {
            for (const auto key : shortcut)
                g_inputBackend->addVirtualKeyboardKey(key);
        }
    }
}
void DirectionalKeysAction::reset()
{
    Action::reset();
    m_firstUpdate = true;
    m_x = m_y = 0;
}
void DirectionalKeysAction::emitKeys(const std::vector<KeyboardKey> &keys)
{
    for (const auto key : keys)
        g_inputBackend->virtualKeyboard()->keyboardKey(key, true);
    for (auto it = keys.rbegin(); it != keys.rend(); ++it)
        g_inputBackend->virtualKeyboard()->keyboardKey(*it, false);
}
void DirectionalKeysAction::doExecute(const ActionExecutionArguments &args)
{
    QThreadHelpers::runOnThread(
        QThreadHelpers::mainThread(),
        [this, args] {
            move(args);
        },
        true);
}
void DirectionalKeysAction::move(const ActionExecutionArguments &args)
{
    const auto delta = args.inputActionArgs.motionPointDelta;
    if (!std::isfinite(delta.x()) || !std::isfinite(delta.y()))
        return;
    const bool repeatAllowed = !m_repeatCondition || m_repeatCondition->satisfied();
    if (m_firstUpdate) {
        m_firstUpdate = false;
        const bool hasInitial = std::ranges::any_of(m_initial, [](const auto &keys) {
            return !keys.empty();
        });
        // Already inside the controlled UI: navigate it instead of opening again.
        if (hasInitial && !(m_repeatCondition && repeatAllowed)) {
            auto initialDirection = direction(delta.x(), delta.y());
            if (args.inputActionArgs.motionAngle) {
                const auto angle = *args.inputActionArgs.motionAngle * std::numbers::pi / 180;
                initialDirection = direction(std::cos(angle), -std::sin(angle));
            }
            emitKeys(m_initial[initialDirection]);
            return; // The opening movement is not also a repeat selection.
        }
    }
    // Check on the main thread at execution time: Escape must not leak arrow keys
    // into an application or cause the initial opening shortcut to run again.
    if (!repeatAllowed) {
        m_x = m_y = 0;
        return;
    }
    accumulate(m_x, delta.x());
    accumulate(m_y, delta.y());
    for (int emitted = 0; emitted < 8 && std::max(std::abs(m_x), std::abs(m_y)) >= m_distance; ++emitted) {
        const auto selected = direction(m_x, m_y);
        emitKeys(m_keys[selected]);
        if (selected <= 1) {
            m_x += m_x < 0 ? m_distance : -m_distance;
            m_y = 0;
        } else {
            m_y += m_y < 0 ? m_distance : -m_distance;
            m_x = 0;
        }
    }
}
}

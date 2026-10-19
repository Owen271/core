// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Action.h"
#include <array>
#include <libinputactions/input/KeyboardKey.h>
#include <vector>

namespace InputActions
{
// Convert two-axis motion to discrete keyboard shortcuts, without shell commands.
class DirectionalKeysAction : public Action
{
public:
    // Ordered left, right, up, down. Keys are pressed in order, released in reverse.
    using Bindings = std::array<std::vector<KeyboardKey>, 4>;
    DirectionalKeysAction(qreal distance, Bindings keys, Bindings initial = {});
    void setRepeatCondition(std::shared_ptr<Condition> value) { m_repeatCondition = std::move(value); }
    void reset() override;

protected:
    void doExecute(const ActionExecutionArguments &args) override;

private:
    void emitKeys(const std::vector<KeyboardKey> &keys);
    void move(const ActionExecutionArguments &args);
    qreal m_distance;
    Bindings m_keys;
    Bindings m_initial;
    std::shared_ptr<Condition> m_repeatCondition;
    bool m_firstUpdate = true;
    qreal m_x = 0, m_y = 0;
};
}

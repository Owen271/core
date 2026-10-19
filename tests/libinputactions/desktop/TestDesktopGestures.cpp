// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <iostream>
#include <libinputactions/actions/ActionExecutor.h>
#include <libinputactions/config/Node.h>
#include <libinputactions/handlers/MotionTriggerHandler.h>
#include <libinputactions/input/Delta.h>
#include <libinputactions/input/backends/InputBackend.h>
#include <libinputactions/input/devices/InputDevice.h>
#include <libinputactions/input/devices/VirtualKeyboard.h>
#include <libinputactions/triggers/touchpad/TouchpadTrigger.h>
#include <libinputactions/variables/VariableRegistry.h>
#include <linux/input-event-codes.h>
#include <set>
#include <stdexcept>

using namespace InputActions;
void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
struct Keyboard : VirtualKeyboard
{
    bool switcher = false;
    std::set<int> held;
    std::vector<int> arrows;
    int overview = 0, desktop = 0, selections = 0, lastSelection = 0;
    void doKeyboardKey(KeyboardKey key, bool down) override
    {
        const int code = key.scanCode();
        if (down) {
            held.insert(code);
            if (code == KEY_TAB && held.contains(KEY_LEFTMETA))
                ++overview;
            if (code == KEY_D && held.contains(KEY_LEFTMETA))
                ++desktop;
            if (code == KEY_TAB && held.contains(KEY_LEFTALT)) {
                ++selections;
                switcher = true;
                lastSelection = held.contains(KEY_LEFTSHIFT) ? -1 : 1;
            }
            if (code == KEY_LEFT || code == KEY_RIGHT || code == KEY_UP || code == KEY_DOWN)
                arrows.push_back(code);
        } else {
            held.erase(code);
            if (code == KEY_LEFTALT)
                switcher = false;
        }
    }
};
struct Backend : InputBackend
{
    Keyboard keyboard;
    VirtualKeyboard *virtualKeyboard() override { return &keyboard; }
};
struct Handler : MotionTriggerHandler
{
    using MotionTriggerHandler::handleMotion;
    using TriggerHandler::activateTriggers;
    using TriggerHandler::addTrigger;
    using TriggerHandler::cancelTriggers;
    using TriggerHandler::endTriggers;
};
int main(int argc, char **argv)
try {
    QCoreApplication app(argc, argv);
    g_variableRegistry = std::make_shared<VariableRegistry>();
    g_variableRegistry->registerStored(BuiltinVariables::Fingers);
    g_variableRegistry->registerStored(BuiltinVariables::LastTriggerId);
    g_variableRegistry->registerStored(BuiltinVariables::LastTriggerTimestamp);
    g_variableRegistry->registerStored(BuiltinVariables::WindowId);
    g_variableRegistry->registerStored(BuiltinVariables::WindowUnderFingersId);
    g_variableRegistry->registerStored(BuiltinVariables::WindowUnderPointerId);
    g_variableRegistry->registerStored(BuiltinVariables::InitialWindowId);
    g_variableRegistry->registerStored(BuiltinVariables::InitialWindowUnderFingersId);
    g_variableRegistry->registerStored(BuiltinVariables::InitialWindowUnderPointerId);
    g_variableRegistry->variable(BuiltinVariables::Fingers)->setValue(3.0);
    g_actionExecutor = std::make_unique<ActionExecutor>();
    auto backend = std::make_unique<Backend>();
    auto &keyboard = backend->keyboard;
    g_inputBackend = std::move(backend);
    g_variableRegistry->registerComputed<bool>("plasma_window_switcher_active", [&keyboard](auto &value) {
        value = keyboard.switcher;
    });
    const auto config = YAML::Load(R"(touchpad:
  gestures:
    - type: swipe
      id: overview
      fingers: 3
      angle: 25-155
      lock_direction: true
      actions:
        - on: begin
          input:
            - keyboard: [leftmeta+tab]

    - type: swipe
      id: show_desktop
      fingers: 3
      direction: down
      lock_direction: true
      actions:
        - on: begin
          input:
            - keyboard: [leftmeta+d]

    - type: swipe
      id: app_switch
      fingers: 3
      direction: left_right
      lock_direction: true
      actions:
        - on: begin
          input:
            - keyboard: [+leftalt]
        - on: update
          directional_keys:
            distance: 75
            initial:
              left: [leftshift, tab]
              right: [tab]
            keys:
              left: [left]
              right: [right]
              up: [up]
              down: [down]
            repeat_conditions: $plasma_window_switcher_active
        - on: end_cancel
          input:
            - keyboard: [-leftalt]
)");
    auto makeHandler = [&](YAML::Node config) {
        auto handler = std::make_unique<Handler>();
        for (const auto &entry : config["touchpad"]["gestures"])
            handler->addTrigger(Node::create(YAML::Clone(entry))->as<std::unique_ptr<TouchpadTrigger>>());
        return handler;
    };
    auto handler = makeHandler(config);
    InputDevice touchpad(InputDeviceType::Touchpad);
    touchpad.properties().setMotionThreshold(5);
    touchpad.properties().setSwipeAngleTolerance(45);
    auto begin = [&] {
        handler->activateTriggers(TriggerType::Swipe, TriggerActivationEvent{});
    };
    auto move = [&](qreal x, qreal y) {
        return handler->handleMotion(&touchpad, PointDelta(QPointF(x, y)));
    };
    auto end = [&] {
        handler->endTriggers(TriggerType::Swipe);
        require(keyboard.held.empty(), "A modifier remained held after lift.");
    };
    for (int sign : {-1, 1}) {
        begin();
        const auto before = keyboard.selections;
        move(sign * 10, 0);
        require(keyboard.selections == before + 1 && keyboard.lastSelection == sign, "First app selection was delayed or reversed.");
        const auto arrows = keyboard.arrows.size();
        move(sign * 74, 0);
        require(keyboard.arrows.size() == arrows, "Repeat fired before 75 units.");
        move(sign, 0);
        require(keyboard.arrows.size() == arrows + 1, "Repeat did not fire at 75 units.");
        move(0, 75);
        move(0, -75);
        move(-sign * 75, 0);
        require(keyboard.arrows[arrows + 1] == KEY_DOWN && keyboard.arrows[arrows + 2] == KEY_UP, "Vertical motion did not navigate rows.");
        require(keyboard.overview == 0 && keyboard.desktop == 0, "App switching activated a vertical action.");
        keyboard.switcher = false; // Escape closes the UI while fingers remain down.
        const auto closedArrows = keyboard.arrows.size();
        move(-sign * 150, 0);
        move(sign * 150, 0);
        move(0, 150);
        require(keyboard.arrows.size() == closedArrows && keyboard.selections == before + 1, "Closing leaked keys or reopened the switcher.");
        end();
    }
    const auto upward = [&](std::initializer_list<QPointF> deltas) {
        const auto before = keyboard.overview;
        const auto desktop = keyboard.desktop;
        const auto selections = keyboard.selections;
        begin();
        for (const auto delta : deltas)
            move(delta.x(), delta.y());
        end();
        require(keyboard.overview == before + 1 && keyboard.desktop == desktop && keyboard.selections == selections,
                "Upward swipe changed actions or toggled twice.");
    };
    upward({{0, -10}, {100, 0}, {0, 100}, {0, -100}});
    upward({{4.047, -2.428}, {2.968, -1.619}, {3.777, -2.968}, {5.127, -4.047}});
    upward({{0, -10}, {0, 6}, {0, -20}});
    const auto desktop = keyboard.desktop;
    begin();
    move(0, 10);
    move(100, 0);
    move(0, -100);
    end();
    require(keyboard.desktop == desktop + 1, "Downward swipe changed actions.");
    begin();
    move(10, 0);
    handler->cancelTriggers(TriggerType::Swipe);
    require(keyboard.held.empty(), "Cancellation left Alt held.");
    g_variableRegistry->variable(BuiltinVariables::Fingers)->setValue(4.0);
    const auto selections = keyboard.selections;
    begin();
    require(!move(0, -100), "Four-finger swipe was blocked.");
    end();
    require(keyboard.selections == selections, "Four fingers activated the preset.");
    g_variableRegistry->variable(BuiltinVariables::Fingers)->setValue(3.0);
    // Default behavior remains chainable unless a user opts in to locking.
    auto unlocked = YAML::Clone(config);
    for (auto entry : unlocked["touchpad"]["gestures"])
        entry.remove("lock_direction");
    handler = makeHandler(unlocked);
    const auto overview = keyboard.overview;
    begin();
    move(0, -10);
    move(0, 10);
    move(0, -10);
    end();
    require(keyboard.overview == overview + 2, "Default gesture chaining was changed.");
    for (const char *distance : {"0", "-1", ".nan", ".inf"}) {
        auto invalid = YAML::Clone(config);
        invalid["touchpad"]["gestures"][2]["actions"][1]["directional_keys"]["distance"] = distance;
        bool threw = false;
        try {
            makeHandler(invalid);
        } catch (const std::exception &) {
            threw = true;
        }
        require(threw, "Invalid distance was accepted.");
    }
    std::cout << "PASS: immediate signed opening, 75-unit repeats, two-axis keys, Escape/cancel/lift cleanup, locked vertical gestures, diagonal start, "
                 "four-finger passthrough, unchanged defaults, invalid settings.\n";
} catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
}

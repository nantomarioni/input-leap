/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2023-2024 InputLeap Developers
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2008 Volker Lanz (vl@fidra.de)
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Action.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QTextStream>

const char* Action::action_type_names_[] =
{
    "keyDown", "keyUp", "keystroke",
    "switchToScreen", "toggleScreen",
    "switchInDirection", "lockCursorToScreen",
    "mouseDown", "mouseUp", "mousebutton"
};

const char* Action::switch_direction_names_[] = { "left", "right", "up", "down" };
const char* Action::lock_cursor_mode_names_[] = { "toggle", "on", "off" };
const QString Action::command_template_ = QStringLiteral("(%1)");

Action::Action() :
    key_sequence_(),
    type_(keystroke),
    type_screen_names_(),
    switch_screen_name_(),
    switch_direction_(switchLeft),
    lock_cursor_mode_(lockCursorToggle),
    active_on_release_(false),
    has_screens_(false)
{
}

QString Action::text() const
{
    /* This function is used to save to config file which is for InputLeap server to
     * read. However the server config parse does not support functions with ()
     * in the end but now argument inside. If you need a function with no
     * argument, it can not have () in the end.
     */
    QString text = QString(action_type_names_[key_sequence_.isMouseButton() ?
                                             type() + int(mouseDown) : type()]);

    switch (type())
    {
        case keyDown:
        case keyUp:
        case keystroke:
            {
                QString commandArgs = key_sequence_.toString();
                if (!key_sequence_.isMouseButton())
                {
                    const QStringList& screens = typeScreenNames();
                    if (haveScreens() && !screens.isEmpty())
                    {
                        QString screenList;
                        for (int i = 0; i < screens.size(); i++)
                        {
                            screenList.append(screens[i]);
                            if (i != screens.size() - 1)
                                screenList.append(QStringLiteral(":"));
                        }
                        commandArgs.append(QStringLiteral(",%1").arg(screenList));
                    }
                    else
                    {
                        commandArgs.append(QStringLiteral(",*"));
                    }
                }
                text.append(command_template_.arg(commandArgs));
            }
            break;

        case switchToScreen:
            text.append(command_template_.arg(switchScreenName()));
            break;

        case switchInDirection:
            text.append(command_template_.arg(switch_direction_names_[switch_direction_]));
            break;

        case lockCursorToScreen:
            text.append(command_template_.arg(lock_cursor_mode_names_[lock_cursor_mode_]));
            break;

        default:
            break;
    }


    return text;
}

void Action::loadSettings(QSettings& settings)
{
    key_sequence_.loadSettings(settings);
    setType(settings.value("type", keyDown).toInt());

    type_screen_names_.clear();
    int numTypeScreens = settings.beginReadArray("typeScreenNames");
    for (int i = 0; i < numTypeScreens; i++)
    {
        settings.setArrayIndex(i);
        type_screen_names_.append(settings.value(SettingsKeys::SCREEN_NAME).toString());
    }
    settings.endArray();

    setSwitchScreenName(settings.value(SettingsKeys::SWITCH_TO_SCREEN).toString());
    setSwitchDirection(settings.value(SettingsKeys::SWITCH_DIRECTION, switchLeft).toInt());
    setLockCursorMode(settings.value(SettingsKeys::LOCKTOSCREEN, lockCursorToggle).toInt());
    setActiveOnRelease(settings.value(SettingsKeys::ACTIVEONRELEASE, false).toBool());
    setHaveScreens(settings.value(SettingsKeys::HASSCREENS, false).toBool());
}

void Action::saveSettings(QSettings& settings) const
{
    key_sequence_.saveSettings(settings);
    settings.setValue(SettingsKeys::ACTION_TYPE, type());

    settings.beginWriteArray(SettingsKeys::SCREEN_NAMES);
    for (int i = 0; i < typeScreenNames().size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue(SettingsKeys::SCREEN_NAME, typeScreenNames()[i]);
    }
    settings.endArray();

    settings.setValue(SettingsKeys::SWITCH_TO_SCREEN, switchScreenName());
    settings.setValue(SettingsKeys::SWITCH_DIRECTION, switchDirection());
    settings.setValue(SettingsKeys::LOCKTOSCREEN, lockCursorMode());
    settings.setValue(SettingsKeys::ACTIVEONRELEASE, activeOnRelease());
    settings.setValue(SettingsKeys::HASSCREENS, haveScreens());
}

QJsonObject Action::toJson() const
{
    QJsonObject json = key_sequence_.toJson();
    json.insert(SettingsKeys::ACTION_TYPE, type());
    json.insert(SettingsKeys::SCREEN_NAMES, QJsonArray::fromStringList(typeScreenNames()));
    json.insert(SettingsKeys::SWITCH_TO_SCREEN, switchScreenName());
    json.insert(SettingsKeys::SWITCH_DIRECTION, switchDirection());
    json.insert(SettingsKeys::LOCKTOSCREEN, lockCursorMode());
    json.insert(SettingsKeys::ACTIVEONRELEASE, activeOnRelease());
    json.insert(SettingsKeys::HASSCREENS, haveScreens());
    return json;
}

void Action::loadJson(const QJsonObject& json)
{
    key_sequence_.loadJson(json);
    setType(qBound(int(keyDown), json.value(SettingsKeys::ACTION_TYPE).toInt(keyDown), int(mousebutton)));
    type_screen_names_.clear();
    for (const QJsonValue& name : json.value(SettingsKeys::SCREEN_NAMES).toArray())
        type_screen_names_.append(name.toString());
    setSwitchScreenName(json.value(SettingsKeys::SWITCH_TO_SCREEN).toString());
    setSwitchDirection(qBound(int(switchLeft), json.value(SettingsKeys::SWITCH_DIRECTION).toInt(switchLeft), int(switchDown)));
    setLockCursorMode(qBound(int(lockCursorToggle), json.value(SettingsKeys::LOCKTOSCREEN).toInt(lockCursorToggle), int(lockCursorOff)));
    setActiveOnRelease(json.value(SettingsKeys::ACTIVEONRELEASE).toBool(false));
    setHaveScreens(json.value(SettingsKeys::HASSCREENS).toBool(false));
}

QTextStream& operator<<(QTextStream& outStream, const Action& action)
{
    if (action.activeOnRelease())
        outStream << ";";

    outStream << action.text();

    return outStream;
}

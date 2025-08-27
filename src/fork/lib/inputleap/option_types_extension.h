/*  InputLeap (nantomarioni fork) -- mouse and keyboard sharing utility
    Copyright (C) - Nicolas Antomarioni (nantomarioni@gmail.com)

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include "base/EventTypes.h"
#include <vector>

//! Option ID
/*!
Type to hold an option identifier.
*/
typedef std::uint32_t OptionID;

//! Option Value
/*!
Type to hold an option value.
*/
typedef std::int32_t OptionValue;

// for now, options are just pairs of integers
typedef std::vector<std::uint32_t> OptionsList;

// macro for packing 4 character strings into 4 byte integers
#define OPTION_CODE(_s)                                             \
    (static_cast<std::uint32_t>(static_cast<unsigned char>(_s[0]) << 24) |    \
     static_cast<std::uint32_t>(static_cast<unsigned char>(_s[1]) << 16) |    \
     static_cast<std::uint32_t>(static_cast<unsigned char>(_s[2]) <<  8) |    \
     static_cast<std::uint32_t>(static_cast<unsigned char>(_s[3])      ))

namespace inputleap {

static const OptionID    kOptionScreenDimmingEnabled        = OPTION_CODE("DMEN");
static const OptionID    kOptionScreenDimmingPercentage    = OPTION_CODE("DMPC");

} // namespace inputleap

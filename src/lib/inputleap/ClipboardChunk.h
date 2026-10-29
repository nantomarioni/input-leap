/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2015-2016 Symless Ltd.
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

#pragma once

#include "inputleap/clipboard_types.h"

#include <cstdint>
#include <string>

#define CLIPBOARD_CHUNK_META_SIZE 7

namespace inputleap {

class IStream;

// Progress of the clipboard transfer arriving on one connection.
struct ClipboardChunkAssemblyState {
    std::size_t expected_size = 0;
    bool active = false;
    // declared size is over the receive limit: chunks are dropped, not cached
    bool oversize = false;
};

class ClipboardChunk {
public:

    static ClipboardChunk start(ClipboardID id, std::uint32_t sequence, const std::size_t& size);
    static ClipboardChunk data(ClipboardID id, std::uint32_t sequence, const std::string& data);
    static ClipboardChunk end(ClipboardID id, std::uint32_t sequence);

    // Reads one clipboard chunk from the stream into dataCached. Never caches
    // more than the declared size, and drops transfers declared larger than
    // max_size (returns kOversize for each of their chunks).
    static int assemble(inputleap::IStream* stream, std::string& dataCached, ClipboardID& id,
                        std::uint32_t& sequence, ClipboardChunkAssemblyState& state,
                        std::size_t max_size);

    std::uint8_t id_ = 0;
    std::uint32_t sequence_ = 0;
    std::uint8_t mark_ = 0;
    std::string data_;
};

} // namespace inputleap

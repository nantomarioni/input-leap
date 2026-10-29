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

#include "inputleap/ClipboardChunk.h"

#include "inputleap/ProtocolUtil.h"
#include "inputleap/protocol_types.h"
#include "io/IStream.h"
#include "base/Log.h"
#include "base/String.h"
#include <cstring>

namespace inputleap {

ClipboardChunk ClipboardChunk::start(ClipboardID id, std::uint32_t sequence,
                                     const std::size_t& size)
{
    ClipboardChunk chunk;
    chunk.id_ = id;
    chunk.sequence_ = sequence;
    chunk.mark_ = kDataStart;
    chunk.data_ = std::to_string(size);
    return chunk;
}

ClipboardChunk ClipboardChunk::data(ClipboardID id, std::uint32_t sequence,
                                    const std::string& data)
{
    ClipboardChunk chunk;
    chunk.id_ = id;
    chunk.sequence_ = sequence;
    chunk.mark_ = kDataChunk;
    chunk.data_ = data;
    return chunk;
}

ClipboardChunk ClipboardChunk::end(ClipboardID id, std::uint32_t sequence)
{
    ClipboardChunk chunk;
    chunk.id_ = id;
    chunk.sequence_ = sequence;
    chunk.mark_ = kDataEnd;
    return chunk;
}

int ClipboardChunk::assemble(inputleap::IStream* stream, std::string& dataCached,
                             ClipboardID& id, std::uint32_t& sequence,
                             ClipboardChunkAssemblyState& state, std::size_t max_size)
{
    std::uint8_t mark;
    std::string data;

    auto reset = [&]() {
        state = {};
        dataCached.clear();
        dataCached.shrink_to_fit();
    };

    if (!ProtocolUtil::readf(stream, kMsgDClipboard + 4, &id, &sequence, &mark, &data)) {
        reset();
        return kError;
    }

    if (id >= kClipboardEnd) {
        LOG_ERR("clipboard transmission failed: invalid clipboard id %d", id);
        reset();
        return kError;
    }

    if (mark == kDataStart) {
        reset();
        state.expected_size = inputleap::string::stringToSizeType(data);
        state.active = true;
        if (state.expected_size > max_size) {
            // the sender keeps streaming chunks; they are dropped as they arrive
            LOG_WARN("not receiving clipboard %d: size %zu is over the %zu byte limit",
                     id, state.expected_size, max_size);
            state.oversize = true;
            return kOversize;
        }
        LOG_DEBUG("start receiving clipboard data");
        return kStart;
    }
    else if (mark == kDataChunk) {
        if (!state.active) {
            LOG_ERR("clipboard transmission failed: data before start");
            reset();
            return kError;
        }
        if (state.oversize) {
            return kOversize;
        }
        // dataCached never exceeds expected_size, so this can't underflow
        if (data.size() > state.expected_size - dataCached.size()) {
            LOG_ERR("clipboard transmission failed: more data than the declared size=%zu",
                    state.expected_size);
            reset();
            return kError;
        }
        dataCached.append(data);
        return kNotFinish;
    }
    else if (mark == kDataEnd) {
        if (!state.active) {
            LOG_ERR("clipboard transmission failed: end before start");
            reset();
            return kError;
        }
        if (state.oversize) {
            reset();
            return kOversize;
        }
        if (state.expected_size != dataCached.size()) {
            LOG_ERR("corrupted clipboard data, expected size=%zd actual size=%zd",
                    state.expected_size, dataCached.size());
            reset();
            return kError;
        }
        state = {};
        return kFinish;
    }

    LOG_ERR("clipboard transmission failed: unknown error");
    reset();
    return kError;
}

} // namespace inputleap

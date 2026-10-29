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

#include "inputleap/ClipboardChunk.h"
#include "inputleap/ProtocolUtil.h"
#include "inputleap/protocol_types.h"
#include "io/IStream.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <cstring>

namespace inputleap {

namespace {

// In-memory stream: writef() appends wire bytes, assemble() reads them back.
class ByteStream : public IStream {
public:
    void close() override {}
    std::uint32_t read(void* buffer, std::uint32_t n) override
    {
        n = std::min<std::uint32_t>(n, static_cast<std::uint32_t>(bytes_.size() - pos_));
        std::memcpy(buffer, bytes_.data() + pos_, n);
        pos_ += n;
        return n;
    }
    void write(const void* buffer, std::uint32_t n) override
    {
        bytes_.append(static_cast<const char*>(buffer), n);
    }
    void flush() override {}
    void shutdownInput() override {}
    void shutdownOutput() override {}
    const EventTarget* get_event_target() const override { return nullptr; }
    bool isReady() const override { return pos_ < bytes_.size(); }
    std::uint32_t getSize() const override
    {
        return static_cast<std::uint32_t>(bytes_.size() - pos_);
    }

    // queue one DCLP message; assemble() expects the 4-byte code already read
    void queue_chunk(const ClipboardChunk& chunk)
    {
        ProtocolUtil::writef(this, kMsgDClipboard, chunk.id_, chunk.sequence_, chunk.mark_,
                             &chunk.data_);
        pos_ += 4;
    }

private:
    std::string bytes_;
    std::size_t pos_ = 0;
};

struct Receiver {
    ByteStream stream;
    std::string cached;
    ClipboardChunkAssemblyState state;
    ClipboardID id = 0;
    std::uint32_t sequence = 0;

    int receive(const ClipboardChunk& chunk, std::size_t max_size)
    {
        stream.queue_chunk(chunk);
        return ClipboardChunk::assemble(&stream, cached, id, sequence, state, max_size);
    }
};

} // namespace

TEST(ClipboardChunkTests, assemble_withinLimit_finishesWithData)
{
    Receiver r;
    EXPECT_EQ(kStart, r.receive(ClipboardChunk::start(kClipboardClipboard, 7, 6), 100));
    EXPECT_EQ(kNotFinish, r.receive(ClipboardChunk::data(kClipboardClipboard, 7, "abc"), 100));
    EXPECT_EQ(kNotFinish, r.receive(ClipboardChunk::data(kClipboardClipboard, 7, "def"), 100));
    EXPECT_EQ(kFinish, r.receive(ClipboardChunk::end(kClipboardClipboard, 7), 100));
    EXPECT_EQ("abcdef", r.cached);
    EXPECT_EQ(7u, r.sequence);
}

TEST(ClipboardChunkTests, assemble_declaredOverLimit_dropsWithoutCaching)
{
    Receiver r;
    EXPECT_EQ(kOversize, r.receive(ClipboardChunk::start(kClipboardClipboard, 1, 1000), 100));
    EXPECT_EQ(kOversize, r.receive(ClipboardChunk::data(kClipboardClipboard, 1, "abc"), 100));
    EXPECT_TRUE(r.cached.empty());
    EXPECT_EQ(kOversize, r.receive(ClipboardChunk::end(kClipboardClipboard, 1), 100));
    EXPECT_FALSE(r.state.active);
}

TEST(ClipboardChunkTests, assemble_moreDataThanDeclared_errorsAndClears)
{
    Receiver r;
    EXPECT_EQ(kStart, r.receive(ClipboardChunk::start(kClipboardClipboard, 1, 4), 100));
    EXPECT_EQ(kNotFinish, r.receive(ClipboardChunk::data(kClipboardClipboard, 1, "abc"), 100));
    EXPECT_EQ(kError, r.receive(ClipboardChunk::data(kClipboardClipboard, 1, "def"), 100));
    EXPECT_TRUE(r.cached.empty());
    EXPECT_FALSE(r.state.active);
}

TEST(ClipboardChunkTests, assemble_dataBeforeStart_errors)
{
    Receiver r;
    EXPECT_EQ(kError, r.receive(ClipboardChunk::data(kClipboardClipboard, 1, "abc"), 100));
    EXPECT_TRUE(r.cached.empty());
}

TEST(ClipboardChunkTests, assemble_invalidClipboardId_errors)
{
    Receiver r;
    EXPECT_EQ(kError, r.receive(ClipboardChunk::start(kClipboardEnd, 1, 3), 100));
}

TEST(ClipboardChunkTests, assemble_endWithMissingData_errors)
{
    Receiver r;
    EXPECT_EQ(kStart, r.receive(ClipboardChunk::start(kClipboardClipboard, 1, 6), 100));
    EXPECT_EQ(kNotFinish, r.receive(ClipboardChunk::data(kClipboardClipboard, 1, "abc"), 100));
    EXPECT_EQ(kError, r.receive(ClipboardChunk::end(kClipboardClipboard, 1), 100));
    EXPECT_TRUE(r.cached.empty());
}

TEST(ClipboardChunkTests, assemble_newTransferAfterOversize_isReceived)
{
    Receiver r;
    EXPECT_EQ(kOversize, r.receive(ClipboardChunk::start(kClipboardClipboard, 1, 1000), 100));
    EXPECT_EQ(kOversize, r.receive(ClipboardChunk::end(kClipboardClipboard, 1), 100));
    EXPECT_EQ(kStart, r.receive(ClipboardChunk::start(kClipboardClipboard, 2, 2), 100));
    EXPECT_EQ(kNotFinish, r.receive(ClipboardChunk::data(kClipboardClipboard, 2, "ok"), 100));
    EXPECT_EQ(kFinish, r.receive(ClipboardChunk::end(kClipboardClipboard, 2), 100));
    EXPECT_EQ("ok", r.cached);
}

} // namespace inputleap

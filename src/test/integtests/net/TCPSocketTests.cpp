/*  InputLeap -- mouse and keyboard sharing utility
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

#include "test/global/TestEventQueue.h"
#include "net/ISocketMultiplexerJob.h"
#include "net/NetworkAddress.h"
#include "net/SocketMultiplexer.h"
#include "net/TCPListenSocket.h"
#include "net/TCPSocket.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#define TEST_TCP_PORT 24813

namespace inputleap {

namespace {

class ProbeSocket : public TCPSocket {
public:
    using TCPSocket::TCPSocket;
    bool connected()
    {
        std::lock_guard<std::mutex> lock(tcp_mutex_);
        return m_connected;
    }
};

template <class Ready>
bool waitFor(Ready ready)
{
    for (int i = 0; i < 500; ++i) {
        if (ready()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

} // namespace

// A writer thread builds the next job outside the socket's lock, so close()
// can run between that and the multiplexer running it. The service then
// found m_socket null and wrote through it: the service crash when the
// window disconnected (input-leapd.exe+0x139c6, writeSocket(nullptr)).
TEST(TCPSocketTests, aJobQueuedBeforeCloseStopsInsteadOfWriting)
{
    TestEventQueue events;
    SocketMultiplexer multiplexer;

    TCPListenSocket listener(&events, &multiplexer, IArchNetwork::kINET);
    NetworkAddress address("127.0.0.1", TEST_TCP_PORT);
    address.resolve();
    listener.bind(address);

    ProbeSocket client(&events, &multiplexer, IArchNetwork::kINET);
    client.connect(address);

    std::unique_ptr<IDataSocket> accepted;
    ASSERT_TRUE(waitFor([&]() { accepted = listener.accept(); return accepted != nullptr; }));
    ASSERT_TRUE(waitFor([&]() { return client.connected(); }));

    const char line[] = "ILOG";
    client.write(line, sizeof(line));
    auto stale = client.newJob();
    ASSERT_NE(nullptr, stale);

    client.close();

    MultiplexerJobStatus status = stale->run(false, true, false);
    EXPECT_FALSE(status.continue_servicing);
    EXPECT_EQ(nullptr, status.new_job);
}

} // namespace inputleap

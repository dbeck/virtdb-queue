#include <gtest/gtest.h>
#include <queue/queue.hh>

using namespace virtdb::queue;

namespace virtdb { namespace test {
  class QueueTest : public ::testing::Test { };
}}

using namespace virtdb::test;

TEST_F(QueueTest, Dummy)
{
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  auto ret = RUN_ALL_TESTS();
  return ret;
  return 0;
}

#include <gtest/gtest.h>
#include <queue/exception.hh>
#include <queue/sync_object.hh>
#include <queue/simple_queue.hh>
#include <queue/mmapped_file.hh>
#include <future>
#include <iostream>

using namespace virtdb::queue;

namespace virtdb { namespace test {
  
  class SyncObjectTest : public ::testing::Test { };
  class SimpleQueueTest : public ::testing::Test { };
  class MmappedFileTest : public ::testing::Test { };
  
}}

using namespace virtdb::test;

TEST_F(MmappedFileTest, SimpleWrite)
{
  const char * file_name = "/tmp/MmappedFileTest.SimpleWrite";
  {
    params p;
    p.mmap_writable_ = true;
    mmapped_file f(file_name,p);
    f.write("Hello", 5);
    f.seek_to(5);
    f.write(" World\n\n", 8);
    char x[5000];
    ::memset(x,' ',5000);
    sprintf(x,"%ld",time(NULL));
    f.write(x,5000);
    f.seek_to(4999);
    f.write("second\n",7);
  }
  ::unlink(file_name);
}

TEST_F(MmappedFileTest, WriteLoop)
{
  const char * file_name = "/tmp/MmappedFileTest.WriteLoop";
  {
    params p;
    p.mmap_writable_ = true;
    mmapped_file f(file_name,p);
    size_t i = 0;
    while( f.can_fit(6) )
    {
      f.write("\r",   1);
      f.write("hell", 4);
      f.write("oo\n", 3);
      ++i;
    }
    std::cout << "did " << i << " loops\n";
  }
  // ::unlink(file_name);
}

TEST_F(SimpleQueueTest, CreatePublisherAndSubscriber)
{
  const char * name = "/tmp/SimpleQueueTest.CreatePublisherAndSubscriber.test";
  simple_publisher pub{name};
  simple_subscriber sub{name};
}

TEST_F(SyncObjectTest, Parallel2)
{
  const char * name = "/tmp/SyncObjectTest.Parallel2.test";
  sync_server svr{name};
  
  for( int k=0; k<5; ++k )
  {
    std::promise<void> when_started1;
    std::future<void> on_start1{when_started1.get_future()};
    
    std::promise<void> when_started2;
    std::future<void> on_start2{when_started2.get_future()};

    uint64_t lim = 1+(k*(k+10))*12345;
    svr.set(0);
    
    std::thread thr1([&](){
      when_started1.set_value();
      for( uint64_t i=0;i<lim;++i )
        svr.signal();
    });
    
    on_start1.wait();
    
    std::thread thr2([&](){
      when_started2.set_value();
      sync_client c{name};
      uint64_t v=0;
      while( v!=lim)
        v = c.wait_next(v);
    });
    
    on_start2.wait();
    
    uint64_t v = 0;
    sync_client d{name};
    while( v!=lim )
      v = d.wait_next(v);
    
    thr1.join();
    thr2.join();
    
    EXPECT_EQ(v, lim);
    std::cout << "done: " << lim << "\n";
  }
  svr.cleanup_all();
}

TEST_F(SyncObjectTest, UseRootFolder)
{
  auto fun = [](){ sync_server svr("/"); };
  EXPECT_ANY_THROW(fun());
}

TEST_F(SyncObjectTest, UseNewTmpFolder)
{
  srand(time(NULL));
  std::string path{"/tmp/"}; path += std::to_string(rand()) + std::to_string(time(NULL));
  auto fun = [&path](){
    sync_server svr(path);
  };
  EXPECT_NO_THROW(fun());
  EXPECT_NO_THROW(fun());
  try
  {
    sync_server svr(path);
    svr.cleanup_all();
  }
  catch(...)
  {
  }
}

TEST_F(SyncObjectTest, UseSameTmpFolder)
{
  std::string path{"/tmp/SyncObjectTest.UseSameTmpFolder.test"};
  auto fun = [&path](){
    sync_server svr(path);
    svr.cleanup_all();
  };
  EXPECT_NO_THROW(fun());
}

TEST_F(SyncObjectTest, RoundTrip)
{
  std::string here{"/tmp/SyncObjectTest.RoundTrip.test"};
  std::string peer{"/tmp/SyncObjectTest.RoundTrip.test.peer"};
  sync_server svr_here{here};
  svr_here.set(0);
  
  std::promise<void> when_started;
  std::future<void> on_start{when_started.get_future()};
  
  std::thread peer_thread([&](){
    sync_server svr_peer{peer};
    sync_client cli_here{here};
    uint64_t v=0;
    svr_peer.set(0);
    when_started.set_value();
    while( v < 1000 )
    {
      v = cli_here.wait_next(v);
      svr_peer.signal();
    }
  });
  
  on_start.wait();
  sync_client cli_peer{peer};
  
  uint64_t v = 0;
  for( uint64_t i=0;i<1000; ++i )
  {
    svr_here.signal();
    v = cli_peer.wait_next(v);
  }
  
  peer_thread.join();
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  auto ret = RUN_ALL_TESTS();
  return ret;
  return 0;
}

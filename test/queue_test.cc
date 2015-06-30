#include <gtest/gtest.h>
#include <queue/exception.hh>
#include <queue/sync_object.hh>
#include <queue/simple_queue.hh>
#include <queue/mmapped_file.hh>
#include <queue/varint.hh>
#include <future>
#include <iostream>
#include <string.h>
#include <map>

using namespace virtdb::queue;

namespace virtdb { namespace test {
  
  class SyncObjectTest : public ::testing::Test { };
  class SimpleQueueTest : public ::testing::Test { };
  class MmappedFileTest : public ::testing::Test { };
  class VarIntTest : public ::testing::Test { };
  
}}

using namespace virtdb::test;

TEST_F(VarIntTest, TwoWay16)
{
  for( uint16_t i=0; i<65535; ++i )
  {
    varint v{i};
    varint v2{v.buf(), v.len()};
    EXPECT_EQ(v2.get16(), i);
    EXPECT_EQ(v2.get64(), i);
    EXPECT_EQ(v.get16(), v2.get16());
    EXPECT_EQ(v.get64(), v2.get64());
    EXPECT_EQ(v.len(), v2.len());
  }
}

TEST_F(VarIntTest, TwoWay64)
{
  for( uint64_t i=0; i<(65535<<15); i+=(i+3) )
  {
    varint v{i};
    std::string stmp{v.buf(), v.buf()+v.len()};
    varint v2{stmp};
    EXPECT_EQ(v2.get64(), i);
    EXPECT_EQ(v.get64(), v2.get64());
    EXPECT_EQ(v.len(), v2.len());
  }
}


TEST_F(MmappedFileTest, RevampedWriteLoop)
{
  const char * file_name = "/tmp/MmappedFileTest.RevampedWriteLoop";
  
  char pattern[24] = {
    'A', '0', '1', '2',   '3', '4', '5', '6',
    'A', '0', '1', '2',   '3', '4', '5', '6',
    'A', '0', '1', '2',   '3', '4', '5', '6'
  };
  
  uint64_t last_pos = 0;
  {
    params p;
    p.mmap_writable_ = true;
    mmapped_writer wr(file_name, p);
    
    for( int i=0; i<4*1024*1024; ++i )
    {
      last_pos = wr.write(pattern,    1);
      EXPECT_EQ(last_pos, wr.last_position());
      last_pos = wr.write(pattern+1,  18);
      EXPECT_EQ(last_pos, wr.last_position());
      last_pos = wr.write(pattern+19, 5);
      EXPECT_EQ(last_pos, wr.last_position());
    }
    EXPECT_EQ(last_pos, 4*1024*1024*24);
  }
  
  {
    params p;
    mmapped_reader rd(file_name, p);
    
    for( int i=0; i<4*1024*1024; ++i )
    {
      uint64_t remaining = 0;
      const uint8_t * ptr = rd.get(remaining);
      if( remaining < 24 )
      {
        rd.seek(rd.last_position());
        ptr = rd.get(remaining);
      }
      
      EXPECT_EQ(::memcmp(pattern, ptr, 1), 0);
      ptr = rd.move_by(1, remaining);
      EXPECT_EQ(::memcmp(pattern+1, ptr, 18), 0);
      ptr = rd.move_by(18, remaining);
      EXPECT_EQ(::memcmp(pattern+19, ptr, 5), 0);
      ptr = rd.move_by(5, remaining);
    }
    
    EXPECT_EQ(rd.last_position(), last_pos);
    std::cout << "last_pos: " << last_pos  << " msgs: " << 4*1024*1024*3 << "\n";
  }
  ::unlink(file_name);
}

TEST_F(MmappedFileTest, RevampedIntegerLoop)
{
  const char * file_name = "/tmp/MmappedFileTest.RevampedIntegerLoop";
  
  {
    params p;
    p.mmap_writable_ = true;
    mmapped_writer wr(file_name, p);
    
    for( uint32_t i=0; i<23*1024*1024; ++i )
      wr.write(&i, sizeof(i));
  }
  
  {
    params p;
    mmapped_reader rd(file_name, p);
    uint64_t remaining = 0;
    const uint32_t * ptr = rd.get<uint32_t>(remaining);
    
    for( uint32_t i=0; i<23*1024*1024; ++i )
    {
      EXPECT_EQ(*ptr, i);
      ptr = rd.move_by<uint32_t>(sizeof(i), remaining);
      
      if( remaining < sizeof(i) )
      {
        rd.seek(rd.last_position());
        ptr = rd.get<uint32_t>(remaining);
      }
    }
    
    std::cout << "last_pos: " << rd.last_position() << " msgs: " << 23*1024*1024 << "\n";
  }
  ::unlink(file_name);
}

TEST_F(SimpleQueueTest, SeekToEnd)
{
  const char * name = "/tmp/SimpleQueueTest.SeekToEnd.test";

  {
    params p;
    p.mmap_max_file_size_ = 4*1024*1024;

    simple_publisher pub{name,p};
  
    char tmp[99];
    ::memset(tmp,0,99);
  
    for( int i=0;i<1024*1024;++i )
      pub.push(tmp, 99);
    
    {
      simple_subscriber sub{name,p};
      std::cout << "sub position:" << sub.position() << " mmap count:" << sub.mmap_count() << "\n";
      sub.seek_to_end();
      std::cout << "sub position:" << sub.position() << " mmap count:" << sub.mmap_count() << "\n";
      std::cout << "pub position:" << pub.position() << " mmap count:" << pub.mmap_count() << "\n";
    }
  }

  {
    simple_publisher::cleanup_all(name);
  }
}

TEST_F(SimpleQueueTest, CreatePublisherAndSubscriber)
{
  const char * name = "/tmp/SimpleQueueTest.CreatePublisherAndSubscriber.test";
  std::string act_file;
  {
    params p;
    p.mmap_max_file_size_ = 4*1024*1024;

    simple_publisher pub{name, p};
    simple_subscriber sub{name, p};
  
    for( uint32_t i=0; i<23*1024*1024; ++i )
      pub.push(&i, sizeof(i));
  
    act_file = pub.act_file();
  }

  EXPECT_FALSE(act_file.empty());
  simple_publisher::cleanup_all(name);
}

TEST_F(SimpleQueueTest, SlowPublish)
{
  const char * name = "/tmp/SimpleQueueTest.SlowPublish.test";
  {
    using namespace std::chrono;
    steady_clock::time_point lim = steady_clock::now() + seconds{3};
    uint64_t sent = 0;
    std::promise<void> on_done;
    std::future<void> done{on_done.get_future()};
    std::promise<void> on_started;
    std::future<void> started{on_started.get_future()};
    
    std::thread t([&](){
      simple_publisher pub{name};
      on_started.set_value();
      
      while(steady_clock::now() < lim) {
        pub.push(&sent, sizeof(sent));
        std::this_thread::sleep_for(milliseconds(100));
        ++sent;
      }
      on_done.set_value();
    });

    started.wait();
    simple_subscriber sub{name};
    
    uint64_t from = 0;
    std::map<uint64_t, uint64_t> value_to_id;
    
    auto on_data = [&](uint64_t id,
                       const uint8_t * data,
                       uint64_t len) {
      EXPECT_EQ(len, sizeof(sent));
      if( len == sizeof(sent) )
      {
        uint64_t recvd = 0;
        ::memcpy(&recvd, data, len);
        value_to_id[recvd] = id;
        std::cout << "received: " << recvd << " @" << id << "\n";
      }
      return true;
    };
    
    while( true )
    {
      from = sub.pull(from, on_data, 5000);
      if( done.wait_for(milliseconds{10}) == std::future_status::ready )
        break;
    }

    done.wait();
    t.join();
    
    EXPECT_EQ(value_to_id.size(), sent);
  }
  simple_publisher::cleanup_all(name);
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
        svr.signal(i+1);
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
    uint64_t i=0;
    while( v < 1000 )
    {
      v = cli_here.wait_next(v);
      svr_peer.signal(++i);
    }
  });
  
  on_start.wait();
  sync_client cli_peer{peer};
  
  uint64_t v = 0;
  for( uint64_t i=0;i<1000; ++i )
  {
    svr_here.signal(i+1);
    v = cli_peer.wait_next(v);
  }
  
  peer_thread.join();
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  auto ret = RUN_ALL_TESTS();
  return ret;
}

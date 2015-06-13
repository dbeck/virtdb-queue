
#include <queue/simple_queue.hh>
#include <queue/exception.hh>
#include <iostream>
#include <string>
#include <string.h>

using namespace virtdb::queue;

namespace
{
  void usage(const char * msg = nullptr)
  {
    if( msg )
      std::cout << "ERROR: " << msg << "\n\n";
    std::cout
      << "usage:\n"
      << "simple_q_client_test <folder> <count>\n";
  }
}

int main(int argc, char ** argv)
{
  try
  {
    if( argc < 3 ) { THROW_("missing parameters"); }
    std::string folder(argv[1]);
    long long count = ::atoll(argv[2]);
    if( !count ) { THROW_("count must be positive integer"); }
    
    simple_subscriber c{folder};
    
    uint64_t from=0;
    long long prev = -1;
    long long last = 0;
    while ( true )
    {
      from = c.pull(from, [&](uint64_t id, const uint8_t * ptr, uint64_t len) {
        if( len != sizeof(last) )
        {
          std::cerr << "invalid length: " << len << " @" << id << " last=" << last << "\n";
        }
        else
        {
          ::memcpy(&last,ptr,len);
          if( last != prev+1 )
          {
            std::cerr << "invalid value: " << last << " @" << id << " should be:" << prev+1 << " (prev)\n";
          }
          prev = last;
        }
        return last < (count-1);
      }, 20000);
      if( last == count -1 ) break;
    }
    
    if( last != count -1 )
    {
      std::cerr << "invalid value: " << last << " @" << from << " should be:" << count-1 << " (target)\n";
    }
    
    std::cout << "performed " << c.mmap_count() << " mmap syscalls\n";
  }
  catch( const std::exception & e )
  {
    usage(e.what());
    return 1;
  }
  return 0;
}

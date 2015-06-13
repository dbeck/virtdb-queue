
#include <queue/simple_queue.hh>
#include <queue/exception.hh>
#include <iostream>
#include <string>

using namespace virtdb::queue;

namespace
{
  void usage(const char * msg = nullptr)
  {
    if( msg )
      std::cout << "ERROR: " << msg << "\n\n";
    std::cout
      << "usage:\n"
      << "simple_q_server_test <folder> <count>\n";
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
    
    params p;
    p.mmap_max_file_size_ = 3*p.mmap_buffer_size_;
    
    simple_publisher s{folder, p};
    
    for( long long i=0; i<count; ++i )
      s.push(&i, sizeof(i));
    
    std::cout << "performed " << s.mmap_count() << " mmap syscalls\n";
    std::cout << "performed " << s.sync_update_count() << " sync object updates\n";
  }
  catch( const std::exception & e )
  {
    usage(e.what());
    return 1;
  }
  return 0;
}

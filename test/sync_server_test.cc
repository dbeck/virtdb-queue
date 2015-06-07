
#include <queue/sync_object.hh>
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
    << "sync_server_test <folder> <count>\n";
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
    
    sync_server s(folder);
    s.set(0);
    
    for( long long i=0; i<count; ++i )
      s.signal();
  }
  catch( const std::exception & e )
  {
    usage(e.what());
    return 1;
  }
  return 0;
}

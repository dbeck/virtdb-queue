#include <queue/simple_publisher.hh>

namespace virtdb { namespace queue {
  
  simple_publisher::simple_publisher(const std::string & path)
  : sync_{path}
  {
  }
  
  simple_publisher::~simple_publisher()
  {
  }
}}

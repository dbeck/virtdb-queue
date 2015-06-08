#include <queue/simple_queue.hh>

namespace virtdb { namespace queue {
  
  simple_publisher::simple_publisher(const std::string & path)
  : sync_{path}
  {
  }
  
  simple_publisher::~simple_publisher()
  {
  }
  
  simple_subscriber::simple_subscriber(const std::string & path)
  : sync_{path}
  {
  }
  
  simple_subscriber::~simple_subscriber()
  {
  }
}}

#include <queue/simple_subscriber.hh>

namespace virtdb { namespace queue {
  
  simple_subscriber::simple_subscriber(const std::string & path)
  : sync_{path}
  {
  }
  
  simple_subscriber::~simple_subscriber()
  {
  }
  
}}

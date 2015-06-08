#pragma once

#include <queue/sync_object.hh>

namespace virtdb { namespace queue {
  
  class simple_publisher
  {
    sync_server sync_;
    
    // disable copying and default consturction
    // until properly implemented
    simple_publisher() = delete;
    simple_publisher(const simple_publisher &) = delete;
    simple_publisher& operator=(const simple_publisher &) = delete;
    
  public:
    simple_publisher(const std::string & path);
    virtual ~simple_publisher();
  };
  
}}

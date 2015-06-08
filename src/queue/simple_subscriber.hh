#pragma once

#include <queue/sync_object.hh>

namespace virtdb { namespace queue {
  
  class simple_subscriber
  {
    sync_server sync_;
    
    // disable copying and default consturction
    // until properly implemented
    simple_subscriber() = delete;
    simple_subscriber(const simple_subscriber &) = delete;
    simple_subscriber& operator=(const simple_subscriber &) = delete;
    
  public:
    simple_subscriber(const std::string & path);
    virtual ~simple_subscriber();
  };
  
}}

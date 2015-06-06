#pragma once

namespace virtdb { namespace queue {
  
  class subscriber
  {
  public:
    subscriber(const std::string & path) {}
    virtual ~subscriber() {}
  };
  
}}

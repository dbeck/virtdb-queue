#pragma once

#include <string>
#include <queue/params.hh>

namespace virtdb { namespace queue {
  
  class mmapped_file
  {
    std::string   name_;
    int           fd_;
    void *        buffer_;
    uint64_t      offset_;
    params        params_;
    
    // disable copying and default consturction
    // until properly implemented
    mmapped_file() = delete;
    mmapped_file(const mmapped_file &) = delete;
    
  public:
    mmapped_file(const std::string & filename,
                 const params & p = params()) {}
    virtual ~mmapped_file() {}
  };
  
}}

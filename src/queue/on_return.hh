#pragma once

#include <functional>

namespace virtdb { namespace queue {
  
  class on_return final
  {
  public:
    typedef std::function<void(void)> fun;
    
  private:
    fun cb_;
    on_return() = delete;
    on_return(const on_return &) = delete;
    on_return & operator=(const on_return &) = delete;
    
  public:
    on_return(fun f) : cb_{f} {}
    ~on_return() { cb_(); }
    
    void reset(fun f=[](){}) { cb_ = f; }
  };
  
}}

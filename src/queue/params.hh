#pragma once

#include <unistd.h>
#include <cstdint>

namespace virtdb { namespace queue {

  struct params
  {
    uint64_t   sync_throttle_ms_;
    uint64_t   mmap_throttle_ms_;
    uint64_t   mmap_buffer_size_;
    uint64_t   mmap_max_file_size_;
    bool       mmap_writable_;
    long       sys_page_size_;
        
    // set default values
    params()
    : sync_throttle_ms_{1},
      mmap_throttle_ms_{1},
      mmap_buffer_size_{80*1024*1024},
      mmap_max_file_size_{1024*1024*1024},
      mmap_writable_{false},
      sys_page_size_{::sysconf(_SC_PAGESIZE)}
    {
    }
  };
  
}}

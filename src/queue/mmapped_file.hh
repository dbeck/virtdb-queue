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
    uint64_t      next_position_;
    params        parameters_;
    void *        real_buffer_;
    uint64_t      real_size_;
    
    // disable copying and default consturction
    // until properly implemented
    mmapped_file() = delete;
    mmapped_file(const mmapped_file &) = delete;
    mmapped_file& operator=(const mmapped_file &) = delete;
    
  public:
    mmapped_file(const std::string & filename,
                 const params & prms = params());
    
    virtual ~mmapped_file();
    
    // accessors
    inline const std::string & name()    const { return name_; }
    inline uint64_t offset()             const { return offset_; }
    inline uint64_t next_position()      const { return next_position_; }
    inline const params & parameters()   const { return parameters_; }
    inline uint64_t mapped_max()         const { return offset()+parameters().mmap_buffer_size_; }
    inline bool can_fit(uint64_t sz)     const { return (mapped_max()+sz) < parameters().mmap_max_file_size_; }
    
    // seek to position
    void seek_to(uint64_t pos);
    
    // throws exception if can_fit() would fail
    void write(const void * data,
               uint64_t sz);
    
    // throws exception if data is not available
    void read(void * data, uint64_t sz);
    
    const void * get(uint64_t sz);
    
    template <typename T>
    const T * get(uint64_t sz)
    {
      return reinterpret_cast<T*>(get(sz));
    }
  };
  
}}

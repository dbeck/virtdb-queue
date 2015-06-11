#pragma once

#include <string>
#include <queue/params.hh>

namespace virtdb { namespace queue {

  class mmapped_file
  {
    std::string   name_;
    params        parameters_;
    int           fd_;
    uint64_t      absolute_position_;
    uint64_t      relative_position_;
    uint64_t      min_known_size_;
    // these has to be aligned due to mmap limitations
    uint8_t *     aligned_ptr_;
    uint64_t      aligned_size_;
    uint64_t      aligned_offset_;
    
    // disable copying and default consturction
    // until properly implemented
    mmapped_file() = delete;
    mmapped_file(const mmapped_file &) = delete;
    mmapped_file& operator=(const mmapped_file &) = delete;

  protected:
    mmapped_file(const std::string & filename,
                 const params & prms = params());
    
    void set_writeable(bool yesno);
    bool exists();
    
    // these throw if fail:
    void create_and_open_file();
    void open_file_for_writing();
    void open_file_for_reading();
    void mmap_file_for_writing(uint64_t offset,
                               uint64_t len);
    void mmap_file_for_reading(uint64_t offset,
                               uint64_t len);
    void extend_file_for_writing(uint64_t len);
    void unmap_all();
    
    // these throw too:
    uint8_t * get_ptr(uint64_t & remaining);
    uint8_t * move_ptr(uint64_t by,
                       uint64_t & remaining);
    uint64_t last_position() const;
    
  public:
    const std::string & name() const;
    const params & parameters() const;
    uint64_t size();
    uint64_t min_known_size() const;
    
    virtual ~mmapped_file();
  };
  
  class mmapped_writer : public mmapped_file
  {
  public:
    mmapped_writer(const std::string & filename,
                   const params & prms = params());

    virtual ~mmapped_writer();
    
    // the main operation are a sequence of:
    // endpos = write(buffer, size) ...
    uint64_t write(const void * data,
                   uint64_t len);
    
    // on restarts we may need to seek to last data
    // position within the existing file:
    void seek(uint64_t pos);
  };
  
  class mmapped_reader : public mmapped_file
  {
  public:
    mmapped_reader(const std::string & filename,
                   const params & prms = params());
    
    virtual ~mmapped_reader();
    
    // the main operations are a sequence of:
    //   ptr = get(required_size)
    // where min_size tells how much data we optimally
    // need. if there is not enough data, reader will try to
    // remap the memory segment. if there is not enough
    // space in the file, it throws an exception.
    //
    // returns: a valid pointer or throws exception
    //          required_size shows how much data is available
    //          in the pointed region
    const uint8_t * get(uint64_t & required_size);
    
    // when a part has been processed we move our position
    // forward
    uint8_t * move_by(uint64_t by,
                      uint64_t & remaining);
    
    // convenience wrapper over get()
    template <typename T>
    const T * get(uint64_t & sz)
    {
      return reinterpret_cast<T*>(get(sz));
    }
    
    // on restarts we may need to seek to a specific
    // position within the existing file:
    void seek(uint64_t pos);
  };
  
  // OLD IMPLEMENTATION
  
  class mmapped_file_old
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
    mmapped_file_old() = delete;
    mmapped_file_old(const mmapped_file_old &) = delete;
    mmapped_file_old& operator=(const mmapped_file_old &) = delete;
    
  public:
    mmapped_file_old(const std::string & filename,
                     const params & prms = params());
    
    virtual ~mmapped_file_old();
    
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

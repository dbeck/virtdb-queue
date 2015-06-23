#pragma once

#include <queue/params.hh>
#include <string>
#include <memory>

namespace virtdb { namespace queue {

  class mmapped_file
  {
    std::string   name_;
    params        parameters_;
    int           fd_;
    uint64_t      relative_position_;
    uint64_t      min_known_size_;
    // these has to be aligned due to mmap limitations
    uint8_t *     aligned_ptr_;
    uint64_t      aligned_size_;
    uint64_t      aligned_offset_;
    // stats
    uint64_t      mmap_count_;
    
    // disable copying and default construction
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
    
  public:
    const std::string & name() const;
    const params & parameters() const;
    uint64_t size();
    uint64_t min_known_size() const;
    uint64_t last_position() const;
    uint64_t mmap_count() const;
    
    virtual ~mmapped_file();
  };
  
  class mmapped_writer : public mmapped_file
  {
  public:
    typedef std::shared_ptr<mmapped_writer> sptr;
    
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
    typedef std::shared_ptr<mmapped_reader> sptr;

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
    
    // convenience wrapper over get()
    template <typename T>
    const T * get(uint64_t & sz)
    {
      return reinterpret_cast<const T*>(get(sz));
    }
    
    // when a part has been processed we move our position
    // forward
    uint8_t * move_by(uint64_t by,
                      uint64_t & remaining);

    // convenience wrapper over move_by()
    template <typename T>
    const T * move_by(uint64_t by,
                      uint64_t & remaining)
    {
      return reinterpret_cast<const T*>(move_by(by, remaining));
    }

    // on restarts we may need to seek to a specific
    // position within the existing file:
    void seek(uint64_t pos);
  };
    
}}

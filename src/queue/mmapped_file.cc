
#include <queue/mmapped_file.hh>
#include <queue/exception.hh>
#include <queue/on_return.hh>

// C lib
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <iostream>

namespace virtdb { namespace queue {
  
  // BASE Implementation
  
  mmapped_file::mmapped_file(const std::string & filename,
                             const params & prms)
  : name_{filename},
    parameters_{prms},
    fd_{-1},
    absolute_position_{0},
    relative_position_{0},
    min_known_size_{0},
    aligned_ptr_{nullptr},
    aligned_size_{0},
    aligned_offset_{0}
  {
  }
  
  mmapped_file::~mmapped_file()
  {
    if( fd_ != -1 ) ::close(fd_);
    fd_ = -1;
    
    try
    {
      unmap_all();
    }
    catch (...)
    {
      perror("unmap operation failed");
    }
  }
  
  void
  mmapped_file::set_writeable(bool yesno)
  {
    parameters_.mmap_writable_ = yesno;
  }
  
  bool
  mmapped_file::exists()
  {
    if( min_known_size_ > 0 ) return true;
    
    struct stat file_stat;
    
    if( !::lstat(name_.c_str(), &file_stat) )
    {
      min_known_size_ = file_stat.st_size;
      return true;
    }
    else
    {
      return false;
    }
  }
  
  void
  mmapped_file::create_and_open_file()
  {
    if( !parameters_.mmap_writable_ )
    {
      THROW_(std::string{"file should be opened as read only: "}+name_);
    }
    
    if( fd_ != -1 )
    {
      ::close(fd_);
    }
    
    fd_ = open(name_.c_str(), O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
    
    if( fd_ < 0 )
    {
      THROW_(std::string{"failed to create file: "}+name_);
    }
  }
  
  void
  mmapped_file::open_file_for_writing()
  {
    if( !parameters_.mmap_writable_ )
    {
      THROW_(std::string{"file should be opened as read only: "}+name_);
    }
    
    if( fd_ != -1 )
    {
      ::close(fd_);
    }
    
    fd_ = open(name_.c_str(), O_RDWR);
    
    if( fd_ < 0 )
    {
      perror("failed to open");
      THROW_(std::string{"failed to open file for writing: "}+name_);
    }
  }

  void
  mmapped_file::open_file_for_reading()
  {
    if( parameters_.mmap_writable_ )
    {
      THROW_(std::string{"file should be opened as write enabled: "}+name_);
    }
    
    if( fd_ != -1 )
    {
      ::close(fd_);
    }
    
    fd_ = open(name_.c_str(), O_RDONLY);
    
    if( fd_ < 0 )
    {
      THROW_(std::string{"failed to open file for writing: "}+name_);
    }
  }

  void
  mmapped_file::extend_file_for_writing(uint64_t len)
  {
    if( !parameters_.mmap_writable_ )
    {
      THROW_(std::string{"file opened as read only: "}+name_);
    }
    
    if( fd_ < 0 )
    {
      THROW_(std::string{"file descriptor is negative for file: "}+name_);
    }
    
    // check if requested size is not smaller than we already know to have
    if( len > min_known_size_ )
    {
      // check actual size
      uint64_t sz = size();
      
      if( len > sz )
      {
        if( ::ftruncate(fd_, len) )
        {
          THROW_(std::string{"couldn't extend file: "}+name_+" to: "+std::to_string(len));
        }
      }
    }
  }
  
  void
  mmapped_file::mmap_file_for_writing(uint64_t offset,
                                      uint64_t len)
  {
    if( !parameters_.mmap_writable_ )
    {
      THROW_(std::string{"file opened as read only: "}+name_);
    }
    
    if( fd_ < 0 )
    {
      THROW_(std::string{"file descriptor is negative for file: "}+name_);
    }
    
    // check offset and length
    uint64_t real_offset = offset;
    uint64_t real_len    = len;
    uint64_t page_size   = parameters_.sys_page_size_;
    
    // make sure offset and length are aligned on pages
    if( offset % page_size )
    {
      real_len    += (2*page_size);
      real_offset  = (offset/page_size)*page_size;
    }

    // make sure we have the right size
    extend_file_for_writing(real_offset+real_len);

    // reset previous mapping if any
    unmap_all();
    
    // do the actual mapping
    void * buff = ::mmap(nullptr,
                         real_len,
                         PROT_READ|PROT_WRITE,
                         MAP_SHARED,
                         fd_,
                         real_offset);
    
    if( buff == MAP_FAILED ||
        buff == nullptr )
    {
      THROW_(std::string{"failed to mmap file: "}+name_+" pos: "+std::to_string(offset));
    }
    
    aligned_ptr_     = (uint8_t *)buff;
    aligned_offset_  = real_offset;
    aligned_size_    = real_len;
    
    absolute_position_   = offset;
    relative_position_   = offset-real_offset;
  }
  
  void
  mmapped_file::mmap_file_for_reading(uint64_t offset,
                                      uint64_t len)
  {
    if( parameters_.mmap_writable_ )
    {
      THROW_(std::string{"file should opened for writing: "}+name_);
    }
    
    if( fd_ < 0 )
    {
      THROW_(std::string{"file descriptor is negative for file: "}+name_);
    }
    
    // check offset and length
    uint64_t real_offset = offset;
    uint64_t real_len    = len;
    uint64_t page_size   = parameters_.sys_page_size_;
    
    // make sure offset and length are aligned on pages
    if( offset % page_size )
    {
      real_len    += (2*page_size);
      real_offset  = (offset/page_size)*page_size;
    }

    // reset previous mapping if any
    unmap_all();
    
    // do the actual mapping
    void * buff = ::mmap(nullptr,
                         real_len,
                         PROT_READ,
                         MAP_SHARED,
                         fd_,
                         real_offset);
    
    if( buff == MAP_FAILED ||
        buff == nullptr )
    {
      THROW_(std::string{"failed to mmap file: "}+name_+" pos: "+std::to_string(offset));
    }
    
    aligned_ptr_     = (uint8_t *)buff;
    aligned_offset_  = real_offset;
    aligned_size_    = real_len;
    
    absolute_position_   = offset;
    relative_position_   = offset-real_offset;
  }
  
  void
  mmapped_file::unmap_all()
  {
    if( aligned_ptr_ )
    {
      // flush out unwritten content
      if( !parameters_.mmap_writable_ && relative_position_ > 0 )
      {
        uint64_t page_size = parameters_.sys_page_size_;
        uint64_t sync_len = relative_position_;
        sync_len = ((relative_position_+page_size)/page_size)*page_size;
        if( sync_len > aligned_size_ )
          sync_len = aligned_size_;
        
        if( ::msync(aligned_ptr_, sync_len, MS_SYNC) )
        {
          THROW_(std::string{"failed to sync file: "}+name()+" sync len: "+std::to_string(sync_len));
        }
      }
      
      if( ::munmap(aligned_ptr_, aligned_size_) )
      {
        THROW_(std::string{"failed to unmap file: "}+name());
      }
    }
    
    // reset all related variables
    aligned_ptr_      = nullptr;
    aligned_size_     = 0;
    aligned_offset_   = 0;

    absolute_position_   = 0;
    relative_position_   = 0;
  }
  
  uint8_t *
  mmapped_file::get_ptr(uint64_t & remaining)
  {
    if( !aligned_ptr_ )
    {
      THROW_(std::string{"invalid pointer for mmapped file: "}+name());
    }
    if( relative_position_ >= aligned_size_ )
    {
      THROW_(std::string{"no space in buffer fo mmapped file: "}+name());
    }
    remaining = (aligned_size_ - relative_position_);
    return (aligned_ptr_+relative_position_);
  }
  
  uint8_t *
  mmapped_file::move_ptr(uint64_t by,
                         uint64_t & remaining)
  {
    if( !aligned_ptr_ )
    {
      THROW_(std::string{"invalid pointer for mmapped file: "}+name());
    }
    if( (relative_position_+by) > aligned_size_ )
    {
      THROW_(std::string{"not enough space in buffer fo mmapped file: "}+name());
    }
    relative_position_ += by;
    remaining = (aligned_size_ - relative_position_);
    return (aligned_ptr_+relative_position_);
  }
  
  uint64_t
  mmapped_file::last_position() const
  {
    return aligned_offset_+relative_position_;
  }
  
  const std::string &
  mmapped_file::name() const
  {
    return name_;
  }
  
  const params &
  mmapped_file::parameters() const
  {
    return parameters_;
  }
  
  uint64_t
  mmapped_file::size()
  {
    struct stat file_stat;
    
    if( !::lstat(name_.c_str(), &file_stat) )
    {
      min_known_size_ = file_stat.st_size;
      return min_known_size_;
    }
    else
    {
      return 0;
    }
  }
  
  uint64_t
  mmapped_file::min_known_size() const
  {
    return min_known_size_;
  }

  // WRITER Implementation
  
  mmapped_writer::mmapped_writer(const std::string & filename,
                                 const params & prms)
  : mmapped_file{filename, prms}
  {
    set_writeable(true);
    
    if( !exists() )
    {
      create_and_open_file();
    }
    else
    {
      open_file_for_writing();
    }
    
    mmap_file_for_writing(0, prms.mmap_buffer_size_);
  }
  
  mmapped_writer::~mmapped_writer()
  {
    try
    {
      unmap_all();
    }
    catch (...)
    {
      perror("failed to unmap");
    }
  }
  
  uint64_t
  mmapped_writer::write(const void * data,
                        uint64_t len)
  {
    if( !len || !data )
    {
      THROW_("invalid parameters");
    }
    
    uint64_t remaining    = 0;
    uint8_t * buffer_ptr  = get_ptr(remaining);
    uint8_t * data_ptr    = (uint8_t*)data;
    uint64_t last_pos     = last_position();
    
    if( remaining == 0 )
    {
      mmap_file_for_writing(last_pos,
                            parameters().mmap_buffer_size_);
      
      buffer_ptr = get_ptr(remaining);
      
      if( !remaining )
      {
        THROW_(std::string{"couldn't map new region for file: "}+name());
      }
    }
    
    while( len > 0 )
    {
      uint64_t copy_len = remaining;
      if( copy_len > len )
      {
        copy_len = len;
      }
      
      ::memcpy(buffer_ptr, data_ptr, copy_len);
      
      len        -= copy_len;
      remaining  -= copy_len;
      data_ptr   += copy_len;
      
      if( remaining == 0 )
      {
        mmap_file_for_writing(last_pos+copy_len,
                              parameters().mmap_buffer_size_);
        buffer_ptr = get_ptr(remaining);
      }
      else
      {
        buffer_ptr = move_ptr(copy_len, remaining);
        last_pos = last_position();
      }
    }
    
    if( !remaining )
    {
      mmap_file_for_writing(last_pos,
                            parameters().mmap_buffer_size_);
    }
    
    return last_position();
  }
  
  void
  mmapped_writer::seek(uint64_t pos)
  {
    mmap_file_for_writing(pos, parameters().mmap_buffer_size_);
  }
  
  // READER Implementation

  mmapped_reader::mmapped_reader(const std::string & filename,
                                 const params & prms)
  : mmapped_file{filename, prms}
  {
    set_writeable(true);
    if( !exists() )
    {
      THROW_(std::string{"no such file : "}+filename);
    }
    uint64_t map_size = prms.mmap_buffer_size_;
    uint64_t sz = size();
    if( map_size < sz )
    {
      map_size = sz;
    }
    
    if( !map_size )
    {
      THROW_(std::string{"file has zero size : "}+filename);
    }
    
    open_file_for_reading();
    mmap_file_for_reading(0, map_size);
  }
  
  mmapped_reader::~mmapped_reader()
  {
    try
    {
      unmap_all();
    }
    catch (...)
    {
      perror("failed to unmap");
    }
  }
  
  const uint8_t *
  mmapped_reader::get(uint64_t & required_size)
  {
    uint64_t remaining    = 0;
    uint8_t * buffer_ptr  = get_ptr(remaining);
    uint64_t last_pos     = last_position();
    
    if( remaining < required_size )
    {
      mmap_file_for_reading(last_pos,
                            parameters().mmap_buffer_size_);
      
      buffer_ptr = get_ptr(remaining);
      
      if( !remaining )
      {
        THROW_(std::string{"couldn't map new region for file: "}+name());
      }
    }
    
    required_size = remaining;
    return buffer_ptr;
  }
  
  uint8_t *
  mmapped_reader::move_by(uint64_t by,
                          uint64_t & remaining)
  {
    return  move_ptr(by, remaining);
  }
  
  void
  mmapped_reader::seek(uint64_t pos)
  {
    uint64_t sz = size();
    if( pos > sz )
    {
      THROW_(std::string{"insufficient space available in mmapped file: "}+name());
    }

    uint64_t remaining = sz - pos;
    if( remaining > parameters().mmap_buffer_size_ )
      remaining = parameters().mmap_buffer_size_;
    
    mmap_file_for_reading(pos, remaining);
  }
  
  // OLD IMPLEMANTATION
  
  mmapped_file_old::mmapped_file_old(const std::string & filename,
                                     const params & prms)
  : name_{filename},
    fd_{-1},
    buffer_{nullptr},
    offset_{0},
    next_position_{0},
    parameters_{prms},
    real_buffer_{nullptr},
    real_size_{0}
  {
    bool exists = true;
    struct stat file_stat;
    
    if( ::lstat(filename.c_str(), &file_stat) )
    {
      exists = false;
    }

    if( prms.mmap_writable_ && !exists )
    {
      fd_ = open(filename.c_str(), O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
      if( fd_ < 0 ) { THROW_(std::string{"failed to create file: "}+filename); }
    }
    else if( prms.mmap_writable_ && exists )
    {
      fd_ = open(filename.c_str(), O_RDWR);
      if( fd_ < 0 ) { THROW_(std::string{"failed to open file for writing: "}+filename); }
    }
    else
    {
      fd_ = open(filename.c_str(), O_RDONLY);
      if( fd_ < 0 ) { THROW_(std::string{"failed to open file for reading: "}+filename); }
    }
    
    seek_to(0);
  }
  
  mmapped_file_old::~mmapped_file_old()
  {
    if( buffer_ )
    {
      if( next_position_ > offset_ && parameters().mmap_writable_ )
      {
        uint64_t sync_len = ((next_position_-offset_)+parameters().sys_page_size_)/parameters().sys_page_size_;
        sync_len *= parameters().sys_page_size_;
        if( sync_len > real_size_ ) sync_len = real_size_;
        ::msync(real_buffer_, sync_len, MS_SYNC);
        std::cout << "synced " << sync_len << "  bytes\n";
      }
      ::munmap(real_buffer_, real_size_);
      buffer_       = nullptr;
      real_buffer_  = nullptr;
    }
    if( fd_ >= 0 )
    {
      ::close(fd_);
      fd_ = -1;
    }
  }
  
  void
  mmapped_file_old::seek_to(uint64_t pos)
  {
    if( fd_ < 0 )
    {
      THROW_(std::string{"cannot map file: "}+name()+" bacause fd_ is negative");
    }
    
    auto const & prms = parameters();
    
    size_t  len         = prms.mmap_buffer_size_;
    size_t  real_len    = len;
    off_t   off         = pos;
    off_t   real_off    = off;
    int     prot        = PROT_READ;
    
    if( pos%prms.sys_page_size_ )
    {
      real_len += (2*prms.sys_page_size_);
      real_off = (pos/prms.sys_page_size_)*prms.sys_page_size_;
    }
    
    if( buffer_ )
    {
      if( prms.mmap_writable_ )
      {
        uint64_t sync_len = ((next_position_-offset_)+parameters().sys_page_size_)/parameters().sys_page_size_;
        sync_len *= parameters().sys_page_size_;
        if( sync_len > real_size_ ) sync_len = real_size_;
        if( ::msync(real_buffer_, sync_len, MS_SYNC) )
        {
          perror("sync failed");
          // THROW_(std::string{"failed to sync file: "}+name());
        }
        std::cout << "synced " << sync_len << "  bytes\n";
      }
      if( ::munmap(real_buffer_, real_size_) )
      {
        THROW_(std::string{"failed to unmap file: "}+name());
      }
      buffer_          = nullptr;
      next_position_   = 0;
      offset_          = 0;
      real_buffer_     = nullptr;
      real_size_       = 0;
    }
    
    if( prms.mmap_writable_ )
    {
      struct stat file_stat;
      if( ::lstat(name().c_str(), &file_stat) )
      {
        THROW_(std::string{"cannot stat file:"}+name());
      }
      
      prot |= PROT_WRITE;
      
      if( file_stat.st_size < real_len+real_off )
      {
        if( ftruncate(fd_, real_len+real_off) )
        {
          THROW_(std::string{"couldn't extend file: "}+name()+" to: "+std::to_string(pos+len));
        }
      }
    }
    
    void * buff = ::mmap(buffer_,
                         real_len,
                         prot,
                         MAP_SHARED,
                         fd_,
                         real_off);
    
    if( buff == MAP_FAILED || buff == nullptr )
    {
      THROW_(std::string{"failed to mmap file: "}+name()+" pos: "+std::to_string(pos));
    }
    
    if( len == real_len )
      buffer_ = buff;
    else
      buffer_ = ((char *)buff)+(pos%prms.sys_page_size_);
    
    offset_         = pos;
    next_position_  = pos;
    real_buffer_    = buff;
    real_size_      = real_len;
  }

  void
  mmapped_file_old::write(const void * data,
                      uint64_t sz)
  {
    if( !buffer_ )
    {
      THROW_(std::string{"cannot write data. file: "}+name()+" not yet mapped");
    }
    auto const & prms = parameters();
    if( !prms.mmap_writable_ )
    {
      THROW_(std::string{"cannot write data. file: "}+name()+" opened for read");
    }
    if( sz > prms.mmap_buffer_size_ )
    {
      THROW_(std::string{"cannot write bigger than: "}+std::to_string(prms.mmap_buffer_size_));
    }
    
    uint64_t written_already = next_position_ - offset_;
    uint64_t free_space = prms.mmap_buffer_size_ - written_already;
    
    if( free_space < sz )
    {
      seek_to(next_position_);
    }
    
    if( sz && data )
    {
      ::memcpy((char *)buffer_+(next_position_-offset_), data, sz);
      next_position_ += sz;
    }
  }
  
  void
  mmapped_file_old::read(void * data,
                     uint64_t sz)
  {
  }
  
  const void *
  mmapped_file_old::get(uint64_t sz)
  {
    return nullptr;
  }

}}

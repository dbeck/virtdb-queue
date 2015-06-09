
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
  
  mmapped_file::mmapped_file(const std::string & filename,
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
  
  mmapped_file::~mmapped_file()
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
  mmapped_file::seek_to(uint64_t pos)
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
  mmapped_file::write(const void * data,
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
  mmapped_file::read(void * data,
                     uint64_t sz)
  {
  }
  
  const void *
  mmapped_file::get(uint64_t sz)
  {
    return nullptr;
  }

}}

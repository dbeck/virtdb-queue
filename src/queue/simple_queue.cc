#include <queue/simple_queue.hh>
#include <queue/exception.hh>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

namespace virtdb { namespace queue {
  
  struct hexconv
  {
    char chars[256];
    hexconv()
    {
      ::memset(chars,0,sizeof(chars));
      for( char c=0;c<=10; ++c )
      {
        chars['0'+c] = c;
      }
      for( char c=0; c<6; ++c )
      {
        chars['a'+c] = 10+c;
        chars['A'+c] = 10+c;
      }
    }
    
    uint64_t operator()(const std::string & in) const
    {
      uint64_t ret = 0;
      size_t i = 0;
      for( auto c = in.begin(); c!=in.end() && i < 16; ++c, ++i )
      {
        ret = ret<<4;
        ret += chars[*c];
      }
      return ret;
    }
    
    std::string operator()(uint64_t v)
    {
      static char map[16] = {
        '0','1','2','3',  '4','5','6','7',
        '8','9','A','B',  'C','D','E','F',
      };
      char res[17];
      ::memset(res, '0', 16);
      res[16] = 0;
      for( int i=0; i<16; ++i )
      {
        res[i] = map[(v>>((15-i)*4))&0x0f];
      }
      return std::string(res);
    }
  };
  
  static hexconv hex_conv;
  
  struct varintconv
  {
    uint64_t operator()(const uint8_t * ptr, uint64_t len) const
    {
      uint64_t ret   = 0;
      uint64_t shift = 0;
      while( len > 0 )
      {
        uint64_t t  = *ptr;
        uint64_t tv = (t&127)<<shift;
        ret |= tv;
        if( !(t & 128) ) break;
        ++ptr;
        --len;
        shift += 7;
      }
      return ret;
    }
    
    void operator()(uint64_t in, uint8_t * out, uint8_t & len) const
    {
      while( in )
      {
        if( in < 128 ) *out = (in&127);
        else           *out = (in&127) | 128;
        in >>= 7;
        ++len;
        ++out;
      }
    }
  };
  
  static varintconv varint_conv;
  
  simple_queue::simple_queue(const std::string & path,
                             const params & p)
  : path_{path},
    parameters_{p}
  {
  }
  
  simple_queue::~simple_queue()
  {
  }
  
  bool
  simple_queue::list_files(std::set<std::string> & results) const
  {
    bool            ret{false};
    DIR*            dp{nullptr};
    struct dirent*  dirp{nullptr};
    
    if((dp  = opendir(path_.c_str())) == NULL) {
      THROW_(std::string{"cannot list folder:"}+path_);
    }
    
    while ((dirp = readdir(dp)) != NULL) {
      std::string name{dirp->d_name};
      if( name.size() == 19 && name.find(".sq") == 16 )
      {
        results.insert(name);
        ret = true;
      }
    }
    
    closedir(dp);
    return ret;
  }
  
  std::string
  simple_queue::last_file() const
  {
    std::string ret;
    std::set<std::string> files;
    if( list_files(files) )
    {
      if( !files.empty() )
        ret = *(files.rbegin());
    }
    return ret;
  }
  
  simple_publisher::simple_publisher(const std::string & path,
                                     const params & p)
  : simple_queue{path, p},
    sync_{path, p},
    file_offset_{0}
  {
    // check what is the last file
    auto name               = last_file();
    bool find_position      = false;
    uint64_t last_position  = 0;
    
    if( name.empty() )
    {
      name = "0000000000000000.sq";
    }
    else
    {
      file_offset_ = hex_conv(name);
      find_position = true;
    }
    
    std::string filename = path + "/" + name;
    
    // seek to last position
    if( find_position )
    {
      mmapped_reader reader{filename};
      
      uint64_t remaining   = 0;
      const uint8_t * ptr  = reader.get(remaining);
      
      while( true && ptr != nullptr )
      {
        // check magic
        if( ((*ptr) & 0xf0) != 0xf0 )
          break;
        
        uint8_t vlen  = (*ptr)&0x0f;
        uint64_t dlen = 0;
        
        // this is the minimum size we need for a message
        if( remaining < 1+vlen )
        {
          reader.seek(reader.last_position());
          ptr = reader.get(remaining);
          if( remaining < 1+vlen )
            break;
        }
        
        dlen = varint_conv(ptr+1, vlen);
        
        // check if we can jump over the header and the data
        if( remaining < dlen+1+vlen )
          reader.seek(reader.last_position());
          
        ptr = reader.move_by(dlen+1+vlen, remaining);
      }
      
      last_position = reader.last_position();
      
      if( last_position > p.mmap_max_file_size_ &&
          last_position > p.mmap_buffer_size_ )
      {
        // create a new file because the existing one is too big
        name           = hex_conv(file_offset_+last_position) + ".sq";
        filename       = path + "/" + name;
        last_position  = 0;
        file_offset_  += last_position;
      }
    }
    
    // update the semaphore to be at least as big as that
    sync_.set(file_offset_+last_position);
    
    // Open mmapped file for writing
    writer_sptr_.reset(new mmapped_writer(filename ,p));
    if( last_position )
      writer_sptr_->seek(last_position);
  }
  
  void
  simple_publisher::push(const void * data,
                         uint64_t len)
  {
    if( !writer_sptr_ )
    {
      THROW_(std::string{"no file opened in: "}+path());
    }
    
    // 1 byte magic: 0xf0 + size of varlen
    // size: in varint format
    // data

    uint8_t vdata[12];
    uint8_t vlen = 0;
    varint_conv(len, vdata+1, vlen);
    vdata[0] = 0xf0 | vlen;
    
    writer_sptr_->write(vdata, vlen+1);
    if( data && len )
      writer_sptr_->write(data, len);
    
    uint64_t last_position = writer_sptr_->last_position();
    sync_.signal(file_offset_+last_position);
    
    auto const & prms = parameters();
    if( last_position > prms.mmap_max_file_size_ &&
        last_position > prms.mmap_buffer_size_ )
    {
      std::string name = hex_conv(file_offset_+last_position) + ".sq";
      std::string filename = path() + "/" + name;
      writer_sptr_.reset(new mmapped_writer(filename ,prms));
      file_offset_ += last_position;
    }
  }
  
  std::string
  simple_publisher::act_file() const
  {
    std::string ret;
    if( writer_sptr_ )
      ret = writer_sptr_->name();
    return ret;
  }
  
  simple_publisher::~simple_publisher()
  {
  }
  
  simple_subscriber::simple_subscriber(const std::string & path,
                                       const params & p)
  : simple_queue{path, p},
    sync_{path, p}
  {
  }
  
  simple_subscriber::~simple_subscriber()
  {
  }
}}

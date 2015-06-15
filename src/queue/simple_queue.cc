#include <queue/simple_queue.hh>
#include <queue/exception.hh>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <chrono>

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
    parameters_{p},
    mmap_count_{0}
  {
  }
  
  simple_queue::~simple_queue()
  {
  }
  
  void
  simple_queue::add_mmap_count(uint64_t v)
  {
    mmap_count_ += v;
  }
  
  uint64_t
  simple_queue::mmap_count() const
  {
    return mmap_count_;
  }
  
  bool
  simple_queue::list_files(std::set<std::string> & results,
                           const std::string & path)
  {
    bool            ret{false};
    DIR*            dp{nullptr};
    struct dirent*  dirp{nullptr};
    
    if((dp  = opendir(path.c_str())) == NULL) {
      THROW_(std::string{"cannot list folder:"}+path);
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
  
  bool
  simple_queue::list_files(std::set<std::string> & results) const
  {
    return list_files(results, path());
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
  
  void
  simple_publisher::cleanup_all(const std::string & path)
  {
    // cleanup sync object
    sync_server s{path};
    s.cleanup_all();
    
    // gather list of file
    std::set<std::string> files;
    list_files(files, path);
    for( auto const & f: files)
    {
      std::string filename = path + "/" + f;
      ::unlink(filename.c_str());
    }
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
      
      while( ptr != nullptr && remaining )
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
    
    // update stats
    if( writer_sptr_ )
      add_mmap_count(writer_sptr_->mmap_count());
      
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
    
    auto const & prms = parameters();
    
    // NOTE: here I assume that all writes go to the same file and
    //       new file is not created between writes
    writer_sptr_->write(vdata, vlen+1);
    if( data && len )
      writer_sptr_->write(data, len);
    
    uint64_t last_position = writer_sptr_->last_position();
    sync_.signal(file_offset_+last_position);
    
    if( last_position > prms.mmap_max_file_size_ &&
        last_position > prms.mmap_buffer_size_ )
    {
      std::string name = hex_conv(file_offset_+last_position) + ".sq";
      std::string filename = path() + "/" + name;
      
      // update stats
      if( writer_sptr_ )
        add_mmap_count(writer_sptr_->mmap_count());
      
      // open file for writing
      writer_sptr_.reset(new mmapped_writer(filename ,prms));
      file_offset_ += last_position;
    }
  }
  
  void
  simple_publisher::push(const buffer_vector & buffers)
  {
    if( !writer_sptr_ )
    {
      THROW_(std::string{"no file opened in: "}+path());
    }
    
    uint64_t len = 0;
    for( auto const & b : buffers )
    {
      if( b.first && b.second )
      {
        len += b.second;
      }
    }
    
    // 1 byte magic: 0xf0 + size of varlen
    // size: in varint format
    // data
    
    uint8_t vdata[12];
    uint8_t vlen = 0;
    varint_conv(len, vdata+1, vlen);
    vdata[0] = 0xf0 | vlen;

    auto const & prms = parameters();
    
    // NOTE: here I assume that all writes go to the same file and
    //       new file is not created between writes
    
    writer_sptr_->write(vdata, vlen+1);
    for( auto const & b : buffers )
    {
      if( b.first && b.second )
      {
        writer_sptr_->write(b.first, b.second);
      }
    }
    
    uint64_t last_position = writer_sptr_->last_position();
    sync_.signal(file_offset_+last_position);
    
    // we may need to open a new file if the current one became too big
    if( last_position > prms.mmap_max_file_size_ &&
       last_position > prms.mmap_buffer_size_ )
    {
      std::string name = hex_conv(file_offset_+last_position) + ".sq";
      std::string filename = path() + "/" + name;
      
      // update stats
      if( writer_sptr_ )
        add_mmap_count(writer_sptr_->mmap_count());
      
      // open file for writing
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
  
  uint64_t
  simple_publisher::sync_update_count() const
  {
    return sync_.update_count();
  }
  
  simple_publisher::~simple_publisher()
  {
  }
  
  void
  simple_subscriber::update_ids()
  {
    std::set<std::string> files;
    if( list_files(files) )
    {
      std::vector<uint64_t> ids;
      for( auto const & f : files )
      {
        if( !f.empty() )
        {
          ids.push_back(hex_conv(f));
        }
      }
      file_ids_.swap(ids);
    }
  }
  
  simple_subscriber::simple_subscriber(const std::string & path,
                                       const params & p)
  : simple_queue{path, p},
    sync_{path, p},
    next_{0},
    act_file_{0}
  {
    update_ids();
  }
  
  uint64_t
  simple_subscriber::pull_from(uint64_t from,
                               pull_fun f)
  {
    // decide which file to read from
    auto decide_file = [this](uint64_t from_val) {
      uint64_t ret = 0;
      for( auto it=file_ids_.begin(); it!=file_ids_.end(); ++it )
      {
        if( *it <= from_val )
          ret = *it;
        else
          break;
      }
      return ret;
    };

    uint64_t read_from = decide_file(from);
    
    if( act_file_ != read_from || !reader_sptr_ )
    {
      // re-check file list
      update_ids();
      read_from = decide_file(from);
      
      // calc filename
      std::string name        = hex_conv(read_from) + ".sq";
      std::string full_name   = path() + "/" + name;

      // update stats
      if( reader_sptr_ )
        add_mmap_count(reader_sptr_->mmap_count());

      // open the file
      reader_sptr_.reset(new mmapped_reader{full_name});
      act_file_ = read_from;
    }

    // try to seek to the position
    reader_sptr_->seek(from-act_file_);
    
    {
      uint64_t remaining   = 0;
      const uint8_t * ptr  = reader_sptr_->get(remaining);
      
      while( ptr != nullptr && remaining )
      {
        // check magic
        if( ((*ptr) & 0xf0) != 0xf0 )
          break;
        
        uint8_t vlen  = (*ptr)&0x0f;
        uint64_t dlen = 0;
        
        // this is the minimum size we need for a message
        if( remaining < 1+vlen )
        {
          reader_sptr_->seek(reader_sptr_->last_position());
          ptr = reader_sptr_->get(remaining);
          if( remaining < 1+vlen )
            break;
        }
        
        dlen = varint_conv(ptr+1, vlen);
        
        // check if we can jump over the header and the data
        if( remaining < dlen+1+vlen )
          reader_sptr_->seek(reader_sptr_->last_position());
        
        bool cont = f(from,ptr+1+vlen,dlen);
        ptr = reader_sptr_->move_by(dlen+1+vlen, remaining);
        if( !cont )
        {
          return reader_sptr_->last_position()+act_file_;
        }
      }
    }
    
    return reader_sptr_->last_position()+act_file_;
  }
  
  uint64_t
  simple_subscriber::pull(uint64_t from,
                          simple_subscriber::pull_fun f,
                          uint64_t timeout_ms)
  {
    using std::chrono::steady_clock;
    using std::chrono::milliseconds;
    steady_clock::time_point wait_till = steady_clock::now() +
                                         milliseconds(timeout_ms);
    
    uint64_t latest = sync_.get();
    if( from >= latest )
    {
      latest = sync_.wait_next(from, timeout_ms);
      
      if( from >= latest )
      {
        // timed out
        return from;
      }
    }
    
    return pull_from(from, f);
  }
  
  simple_subscriber::~simple_subscriber()
  {
  }
}}

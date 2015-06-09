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
      char res[17];
      ::memset(res, '0', 16);
      res[16] = 0;
      for( int i=0; i<16; ++i )
      {
        res[i] = (v>>((15-i)*4))&0x0f;
      }
      return std::string(res);
    }
  };
  
  simple_queue::simple_queue(const std::string & path)
  : path_{path}
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
  
  simple_publisher::simple_publisher(const std::string & path)
  : simple_queue{path},
    sync_{path}
  {
    auto name = last_file();
    hexconv hc;
    uint64_t h = hc(name);
  }
  
  simple_publisher::~simple_publisher()
  {
  }
  
  simple_subscriber::simple_subscriber(const std::string & path)
  : simple_queue{path},
    sync_{path}
  {
  }
  
  simple_subscriber::~simple_subscriber()
  {
  }
}}

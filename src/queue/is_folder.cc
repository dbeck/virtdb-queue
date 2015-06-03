#include <queue/is_folder.hh>
#include <sys/stat.h>

namespace virtdb { namespace queue {
  
  bool
  is_folder(const std::string & path)
  {
    if( !path.empty() )
    {
      struct stat res;
      if( lstat(path.c_str(), &res) == 0 )
      {
        // is a directory
        if( res.st_mode & S_IFDIR )
        {
          return true;
        }
      }
    }
    return false;
  }
  
}}

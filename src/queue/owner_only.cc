#include <queue/owner_only.hh>
#include <sys/stat.h>

namespace virtdb { namespace queue {
  
  bool
  owner_only(const std::string & path)
  {
    if( !path.empty() )
    {
      struct stat res;
      if( lstat(path.c_str(), &res) == 0 )
      {
        // neither group or others can access
        if( ((res.st_mode & S_IRWXG) | (res.st_mode & S_IRWXO)) == 0 )
        {
          return true;
        }
      }
    }
    return false;
  }
  
}}

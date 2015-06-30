#pragma once

#include <cstdint>
#include <string>
#include <string.h>

namespace virtdb { namespace queue {
  
  class varint
  {
    uint8_t   buf_[11];
    uint8_t   len_;
    uint64_t  val_;
    
    varint() = delete;
    
  public:
    varint(uint16_t in)
    : len_{0}, val_{0}, buf_{0,0}
    {
      uint8_t * out = buf_;
      val_ = in;
      while( in )
      {
        if( in < 128 ) *out = (in&127);
        else           *out = (in&127) | 128;
        in >>= 7;
        ++len_;
        ++out;
      }
      if( !len_ ) len_=1;
      buf_[10] = 0;
    }
    
    varint(uint64_t in)
    : len_{0}, val_{0}, buf_{0,0}
    {
      uint8_t * out = buf_;
      val_ = in;
      while( in )
      {
        if( in < 128 ) *out = (in&127);
        else           *out = (in&127) | 128;
        in >>= 7;
        ++len_;
        ++out;
      }
      if( !len_ ) len_=1;
      buf_[10] = 0;
    }
    
    varint(const std::string & v)
    : len_{1}, val_{0}, buf_{0,0}
    {
      size_t sz = v.size();
      if( sz > 0 && sz < 11 )
      {
        ::memcpy(buf_,v.c_str(),sz);
        buf_[10] = 0;
        const uint8_t * ptr = buf_;
        
        uint64_t ret   = 0;
        uint64_t shift = 0;
        while( sz > 0 )
        {
          uint64_t t  = *ptr;
          uint64_t tv = (t&127)<<shift;
          ret |= tv;
          ++ptr;
          if( !(t & 128) ) break;
          --sz;
          shift += 7;
        }
        val_ = ret;
        len_ = ptr - buf_;
      }
    }
    
    varint(const uint8_t * ptr, uint8_t sz)
    : len_{1}, val_{0}, buf_{0,0}
    {
      if( sz < 11 && ptr )
      {
        ::memcpy(buf_,ptr,sz);
        buf_[10] = 0;
        ptr = buf_;
        
        uint64_t ret   = 0;
        uint64_t shift = 0;
        while( sz > 0 )
        {
          uint64_t t  = *ptr;
          uint64_t tv = (t&127)<<shift;
          ret |= tv;
          ++ptr;
          if( !(t & 128) ) break;
          --sz;
          shift += 7;
        }
        val_ = ret;
        len_ = ptr - buf_;
      }
    }
    
    inline uint16_t get16() const { return (uint16_t)val_; }
    inline uint64_t get64() const { return (uint64_t)val_; }
    
    inline uint8_t         len() const { return len_; }
    inline const uint8_t * buf() const { return buf_; }
  };
  
}}

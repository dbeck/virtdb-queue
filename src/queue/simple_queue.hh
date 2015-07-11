#pragma once

#include <queue/sync_object.hh>
#include <queue/mmapped_file.hh>
#include <queue/params.hh>
#include <set>
#include <vector>

namespace virtdb { namespace queue {
  
  class simple_queue
  {
    std::string   path_;
    params        parameters_;
    uint64_t      mmap_count_;
    
    // disable copying and default construction
    // until properly implemented
    simple_queue() = delete;
    simple_queue(const simple_queue &) = delete;
    simple_queue& operator=(const simple_queue &) = delete;
    
  protected:
    const std::string & path()   const { return path_; }
    const params & parameters()  const { return parameters_; }
    simple_queue(const std::string & path,
                 const params & p);

    static bool list_files(std::set<std::string> & results,
                           const std::string & path);

    bool list_files(std::set<std::string> & results) const;
    std::string last_file() const;
    void add_mmap_count(uint64_t v);
    
  public:
    virtual ~simple_queue();
    
    // stats
    uint64_t mmap_count() const;
  };
  
  class simple_publisher : public simple_queue
  {
    sync_server           sync_;
    mmapped_writer::sptr  writer_sptr_;
    uint64_t              file_offset_;
    
  public:
    typedef std::pair<const void *, uint64_t>   buffer;
    typedef std::vector<buffer>                 buffer_vector;
    typedef std::shared_ptr<simple_publisher>   sptr;

    
    simple_publisher(const std::string & path,
                     const params & p = params());
    
    virtual ~simple_publisher();
    
    void push(const void * data, uint64_t len);
    void push(const buffer_vector & buffers);
    
    std::string act_file() const;
    uint64_t position() const;

    static void cleanup_all(const std::string & path);
    
    // stats
    uint64_t sync_update_count() const;
  };
  
  class simple_subscriber : public simple_queue
  {
  public:
    typedef std::function<bool(uint64_t id,
                               const uint8_t * ptr,
                               uint64_t len)>   pull_fun;
    typedef std::shared_ptr<simple_subscriber>  sptr;
    
  private:
    sync_client             sync_;
    mmapped_reader::sptr    reader_sptr_;
    std::vector<uint64_t>   file_ids_;
    uint64_t                next_;
    uint64_t                act_file_;
    
    void update_ids();
    
    uint64_t pull_from(uint64_t from,
                       pull_fun f);
    
  public:
    simple_subscriber(const std::string & path,
                      const params & p = params());
    
    virtual ~simple_subscriber();
    
    uint64_t position() const;
    
    // TODO : FIXME : pull max ????
    // there is a chance that server has not yet finished with the write op ...
    uint64_t pull(uint64_t from,
                  pull_fun f,
                  uint64_t timeout_ms);
    
    void seek_to_end();
  };
  
}}

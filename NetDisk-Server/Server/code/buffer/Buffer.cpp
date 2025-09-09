#include "Buffer.h"


Buffer::Buffer(size_t buffer_size) : buffer_(buffer_size), read_pos_(0), write_pos_(0) {

}

size_t Buffer::writeAbleBytes() const {
  return buffer_.size() - write_pos_;
}

size_t Buffer::readAbleBytes() const {
  return write_pos_ - read_pos_;
}

size_t Buffer::prependAbleBytes() const {
  return read_pos_;
}

const char* Buffer::beginRead() const {
  return __begin() + read_pos_;
}

char* Buffer::beginWrite() {
  return __begin() + write_pos_;
}

const char* Buffer::beginWriteConst() const {
  return __begin() + write_pos_;
}

void Buffer::ensureWriteAble(size_t len) {
  if(writeAbleBytes() < len) {    //如果空间不够，增加len字节空间
    __makeSpace(len);
  }
  assert(writeAbleBytes() >= len);//确保可用空间大于等于len字节
}

void Buffer::hasWritten(size_t len) {
  write_pos_ += len;
}

void Buffer::retrieve(size_t len) {
  assert(len <= readAbleBytes());
  read_pos_ += len;
}

void Buffer::retrieveUntil(const char* end) {
  assert(beginRead() <= end);
  retrieve(end - beginRead());
}

void Buffer::retrieveAll() {
  memset(&buffer_[0], 0, buffer_.size());
  read_pos_ = 0;
  write_pos_ = 0;
}

std::string Buffer::retrieveAllToStr() {
  std::string str(beginRead(), readAbleBytes());
  retrieveAll();
  return str;
}

char* Buffer::peek() {
  return __begin() + read_pos_;
}

const char* Buffer::peek() const {
  return __begin() + read_pos_;
}

std::string Buffer::peekToStr(size_t len) {
  if (len >= readAbleBytes()) { // 如果大于可读数据的总长度，返回全部数据
    return peekAllToStr();
  }
  return std::string(beginRead(), beginRead() + len);
}

std::string Buffer::peekAllToStr() {
  return std::string(beginRead(), const_cast<const char*>(beginWrite()));
}

void Buffer::append(const std::string& str) {
  append(str.data(), str.size());
}

void Buffer::append(const char* str, size_t len) {
  assert(str);
  ensureWriteAble(len);   //确保有足够的空间
  std::copy(str, str + len, beginWrite());    //将str到str+len位置的数据从BeginWrite返回的写入位置开始写入缓冲区
  hasWritten(len);
}

void Buffer::append(const void* data, size_t len) {
  assert(data);
  append(static_cast<const char*>(data), len);    //转换为char*指针
}

void Buffer::append(const Buffer& buf) {
  append(buf.beginRead(), buf.readAbleBytes());
}

// 将fd的数据读入缓冲区，若出错则更新save_errno，返回读取的字节数。
ssize_t Buffer::readFd(int fd, int* save_errno) {
  char buff[65535];
  struct iovec iov[2];            // 用于分散读-聚集写
  const size_t write_able = writeAbleBytes();

  // 分散读， 保证数据全部读完
  iov[0].iov_base = __begin() + write_pos_;
  iov[0].iov_len = write_able;
  iov[1].iov_base = buff;
  iov[1].iov_len = sizeof(buff);

  // 将套接字的数据写入iov,先写入第一个，第一个满了，再写入第二个
  const ssize_t recv_bytes = readv(fd, iov, 2);
  if(recv_bytes < 0) {  //如果len==0，说明出现错误，保存errno，以通知其它工作流程
    *save_errno = errno;
  }
  else if(static_cast<size_t>(recv_bytes) <= write_able) {  //如果第一个iov可用存下这些数据，则直接修改缓冲区写索引位置
    write_pos_ += recv_bytes;
  }
  else {  //如果第一个iov空间不够，将写索引改成缓冲区长度，最后的位置，并调用append追加入缓冲区，内部会扩展空间
    write_pos_ = buffer_.size();
    append(buff, recv_bytes - write_able);
  }
  return recv_bytes;
}

// 将缓冲区的数据可供读取的数据写入套接字fd，如果出错更改save_errno，返回写入的字节长度
ssize_t Buffer::writeFd(int fd, int* save_errno) {
  size_t read_able = readAbleBytes();  // 可读取字节数
  ssize_t len = write(fd, beginRead(), read_able);  // 往套接字写入数据
  if(len < 0) { // 如果等于0，则出错
    *save_errno = errno;
  }
  else {  // 否则，读取成功，修改读取位置索引
    read_pos_ += len;
  }
  return len;
}

// 返回缓存区起始地址
char* Buffer::__begin() {
  return &(*buffer_.begin()); //返回迭代器，在解引用成char,再&取地址返回指针
}

// 返回缓存区起始地址 const版本
const char* Buffer::__begin() const {
  return &(*buffer_.begin());
}

// 拓展len字节空间
void Buffer::__makeSpace(size_t len) {
  if(writeAbleBytes() + prependAbleBytes() < len) { // 可供读取字节数+预览留可重用字节数如果小于len,则重新拓展缓冲区
    buffer_.resize(write_pos_ + len + 1);
  }
  else {  // 如果大于len，把前置空间扩展到可写空间
    size_t read_able = readAbleBytes();     // 返回调整可供读取字节数
    std::copy(__begin() + read_pos_, __begin() + write_pos_, __begin());  //将还没读取的数据拷贝到缓冲区前面
    read_pos_ = 0;  // 读取位置标为0
    write_pos_ = read_pos_ + read_able;     // 更改读索引
    assert(read_able == readAbleBytes());
  }
  assert(len <= writeAbleBytes());  // 确保成功
}
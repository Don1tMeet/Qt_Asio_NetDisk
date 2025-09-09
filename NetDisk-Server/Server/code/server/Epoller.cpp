#include "Epoller.h"
#include "protocol.h"
#include "assert.h"

Epoller::Epoller(int max_event) : epollfd_(-1), event_vec_(max_event) {
  assert(max_event > 0);
  epollfd_ = epoll_create(1);
  errCheck(-1 == epollfd_, "create epoll error");
}

Epoller::~Epoller() {
  close(epollfd_);
}

bool Epoller::addFd(int fd, uint32_t events) {
  if (fd < 0) {
    return false;
  }
  epoll_event ev{ {0}, {0} };
  ev.events = events;
  ev.data.fd = fd;
  return 0 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::modFd(int fd, uint32_t events) {
  if (fd < 0) {
    return false;
  }
  epoll_event ev{ {0}, {0} };
  ev.events = events;
  ev.data.fd = fd;
  return 0 == epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::delFd(int fd) {
  if(fd < 0) {
    return false;
  }
  epoll_event ev = { {0}, {0} };
  return 0 == epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::wait(int timeout_ms) {
  return epoll_wait(epollfd_, &event_vec_[0], static_cast<int>(event_vec_.size()), timeout_ms);
}

int Epoller::getEventFd(size_t i) const {
  assert(i < event_vec_.size());
  return event_vec_[i].data.fd;
}

uint32_t Epoller::getEvents(size_t i) const {
  assert(i < event_vec_.size());
  return event_vec_[i].events;
}

size_t Epoller::getEventVecSize() {
  return event_vec_.size();
}
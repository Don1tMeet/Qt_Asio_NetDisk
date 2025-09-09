#include "Epoller.h"

Epoller::Epoller(uint32_t max_event)
  : epoll_fd_(epoll_create(1)), events_(max_event)
{
  assert(epoll_fd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() {
  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
  }
}

bool Epoller::addFd(int fd, uint32_t events) {
  if(fd < 0) {
    return false;
  }

  epoll_event ev = { {0}, {0} };
  ev.data.fd = fd;
  ev.events = events;
  return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::modFd(int fd, uint32_t events) {
  if(fd < 0) {
    return false;
  }

  epoll_event ev = { {0}, {0} };
  ev.data.fd = fd;
  ev.events = events;
  return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::delFd(int fd) {
  if(fd < 0) {
    return false;
  }

  epoll_event ev = { {0}, {0} };
  return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::wait(int timeout_ms) {
  return epoll_wait(epoll_fd_, &events_[0], static_cast<int>(events_.size()), timeout_ms);
}

int Epoller::getEventFd(size_t i) const {
  assert(i < events_.size());
  return events_[i].data.fd;
}

uint32_t Epoller::getEvents(size_t i) const {
  assert(i < events_.size());
  return events_[i].events;
}
// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/EventLoopThread.h"

#include "muduo/net/EventLoop.h"

using namespace muduo;
using namespace muduo::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const string& name)
  : loop_(NULL),
    exiting_(false),
    thread_(std::bind(&EventLoopThread::threadFunc, this), name),
    mutex_(),
    cond_(mutex_),
    callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
  {
    // still a tiny chance to call destructed object, if threadFunc exits just now.
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    loop_->quit();
    thread_.join();
  }
}
//该函数为线程启动的函数，在由EventLoopThreadPool类中调用
EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  thread_.start();   //启动子线程，执行loop.loop()

  EventLoop* loop = NULL;   //如果为空，则表示子线程还未执行
  {
    MutexLockGuard lock(mutex_);   //需要加锁
    while (loop_ == NULL)
    {
      cond_.wait();   //等待子线程先运行，然后返回loop到EventLoopThreadPool类中
    }
    loop = loop_;
  }

  return loop;
}
//线程start后执行的函数
void EventLoopThread::threadFunc()
{
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);
  }

  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;   //让主线程跳出while，然后返回
    cond_.notify();  //唤醒条件变量
  }

  loop.loop();
  //assert(exiting_);
  MutexLockGuard lock(mutex_);  //执行完loop需要将loop_变为NULL
  loop_ = NULL;
}


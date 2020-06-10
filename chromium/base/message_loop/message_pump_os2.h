// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_OS2_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_OS2_H_

#include "base/os2/os2_toolkit.h"

#include <unordered_map>

#include "base/base_export.h"
#include "base/message_loop/message_pump.h"
#include "base/observer_list.h"
#include "base/time/time.h"

namespace base {

// NOTE: This code is a copy-paste from message_pump_win.* with OS/2 specific
// modifications. It may make sense to keep them in sync.

// MessagePumpOS2 serves as the base for specialized versions of the MessagePump
// for Windows. It provides basic functionality like handling of observers and
// controlling the lifetime of the message pump.
class BASE_EXPORT MessagePumpOS2 : public MessagePump {
 public:
  MessagePumpOS2();

  // MessagePump methods:
  void Run(Delegate* delegate) override;
  void Quit() override;

 protected:
  struct RunState {
    Delegate* delegate;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit;

    // Used to count how many Run() invocations are on the stack.
    int run_depth;
  };

  // State used with |work_state_| variable.
  enum WorkState {
    READY = 0,      // Ready to accept new work.
    HAVE_WORK = 1,  // New work has been signalled.
    WORKING = 2     // Handling the work.
  };

  virtual void DoRunLoop() = 0;
  int GetCurrentDelay() const;

  // The time at which delayed work should run.
  TimeTicks delayed_work_time_;

  // A value used to indicate if there is a kMsgDoWork message pending
  // in the Windows Message queue.  There is at most one such message, and it
  // can drive execution of tasks when a native message pump is running.
  volatile unsigned int work_state_ = READY;

  // State for the current invocation of Run.
  RunState* state_ = nullptr;
};

//-----------------------------------------------------------------------------
// MessagePumpForUI extends MessagePumpOS2 with methods that are particular to a
// MessageLoop instantiated with TYPE_UI.
//
// MessagePumpForUI implements a "traditional" Windows message pump. It contains
// a nearly infinite loop that peeks out messages, and then dispatches them.
// Intermixed with those peeks are callouts to DoWork for pending tasks, and
// DoDelayedWork for pending timers. When there are no events to be serviced,
// this pump goes into a wait state. In most cases, this message pump handles
// all processing.
//
// However, when a task, or windows event, invokes on the stack a native dialog
// box or such, that window typically provides a bare bones (native?) message
// pump.  That bare-bones message pump generally supports little more than a
// peek of the Windows message queue, followed by a dispatch of the peeked
// message.  MessageLoop extends that bare-bones message pump to also service
// Tasks, at the cost of some complexity.
//
// The basic structure of the extension (referred to as a sub-pump) is that a
// special message, kMsgHaveWork, is repeatedly injected into the Windows
// Message queue.  Each time the kMsgHaveWork message is peeked, checks are
// made for an extended set of events, including the availability of Tasks to
// run.
//
// After running a task, the special message kMsgHaveWork is again posted to
// the Windows Message queue, ensuring a future time slice for processing a
// future event.  To prevent flooding the Windows Message queue, care is taken
// to be sure that at most one kMsgHaveWork message is EVER pending in the
// Window's Message queue.
//
// There are a few additional complexities in this system where, when there are
// no Tasks to run, this otherwise infinite stream of messages which drives the
// sub-pump is halted.  The pump is automatically re-started when Tasks are
// queued.
//
// A second complexity is that the presence of this stream of posted tasks may
// prevent a bare-bones message pump from ever peeking a WM_PAINT or WM_TIMER.
// Such paint and timer events always give priority to a posted message, such as
// kMsgHaveWork messages.  As a result, care is taken to do some peeking in
// between the posting of each kMsgHaveWork message (i.e., after kMsgHaveWork
// is peeked, and before a replacement kMsgHaveWork is posted).
//
// NOTE: Although it may seem odd that messages are used to start and stop this
// flow (as opposed to signaling objects, etc.), it should be understood that
// the native message pump will *only* respond to messages.  As a result, it is
// an excellent choice.  It is also helpful that the starter messages that are
// placed in the queue when new task arrive also awakens DoRunLoop.
//
class BASE_EXPORT MessagePumpForUI : public MessagePumpOS2 {
 public:
  struct TimerID {
    HWND hwnd; USHORT id;
    bool operator==(const TimerID &other) const {
      return memcmp(this, &other, sizeof(TimerID)) == 0;
    }
  };

  MessagePumpForUI();
  ~MessagePumpForUI() override;

  // MessagePump methods:
  void ScheduleWork() override;
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

  // Make the MessagePumpForUI respond to WM_QUIT messages.
  void EnableWmQuit();

  // An observer interface to give the scheduler an opportunity to log
  // information about MSGs before and after they are dispatched.
  class BASE_EXPORT Observer {
   public:
    virtual void WillDispatchMSG(const QMSG& msg) = 0;
    virtual void DidDispatchMSG(const QMSG& msg) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* obseerver);

 private:
  static ULONG SetTimer(HWND hwnd, MessagePumpForUI *that, int msec);
  static MessagePumpForUI *GetTimer(HWND hwnd, ULONG id);
  static void KillTimer(HWND hwnd, ULONG id);
  static MRESULT APIENTRY WndProcThunk(
      HWND hwnd, ULONG message, MPARAM mp1, MPARAM mp2);
  void DoRunLoop() override;
  void InitMessageWnd();
  void WaitForWork();
  void HandleWorkMessage();
  void HandleTimerMessage();
  void RescheduleTimer();
  bool ProcessNextWindowsMessage();
  bool ProcessMessageHelper(QMSG& msg);
  bool ProcessPumpReplacementMessage();

  // A hidden message-only window.
  HAB hab_;
  HMQ hmq_;
  HWND message_hwnd_;
  ULONG timer_id_;

  // Timer ID -> MessagePumpForUI map
  static Lock timer_ids_lock_;
  typedef std::unordered_map<TimerID, MessagePumpForUI *> TimerIDMap;
  static TimerIDMap timer_ids_;

  // Whether MessagePumpForUI responds to WM_QUIT messages or not.
  // TODO(thestig): Remove when the Cloud Print Service goes away.
  bool enable_wm_quit_ = false;

  ObserverList<Observer>::Unchecked observers_;
};

// NOTE: For MessagePumpForIO we use MessagePumpLibevent on OS/2

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_OS2_H_

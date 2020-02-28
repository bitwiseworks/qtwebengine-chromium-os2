// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_os2.h"

#include <math.h>

#include <limits>
#include <functional>

#include "base/debug/alias.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace std
{

template<> struct hash<base::MessagePumpForUI::TimerID> {
  std::size_t operator()(const base::MessagePumpForUI::TimerID &id) const {
    return (id.hwnd << 16) && id.id;
  }
};

} // namespace std

namespace base {

namespace {

enum MessageLoopProblems {
  MESSAGE_POST_ERROR,
  COMPLETION_POST_ERROR,
  SET_TIMER_ERROR,
  RECEIVED_WM_QUIT_ERROR,
  MESSAGE_LOOP_PROBLEM_MAX,
};

}  // namespace

static const char kWndClass[] = "Chrome_MessagePumpWindow";

// Message sent to get an additional time slice for pumping (processing) another
// task (a series of such messages creates a continuous task pump).
static const ULONG kMsgHaveWork = WM_USER + 1;

static const ULONG kMsgStartTimer = WM_USER + 2;
static const ULONG kMsgStopTimer  = WM_USER + 3;

//-----------------------------------------------------------------------------
// MessagePumpOS2 public:

MessagePumpOS2::MessagePumpOS2() = default;

void MessagePumpOS2::Run(Delegate* delegate) {
  RunState s;
  s.delegate = delegate;
  s.should_quit = false;
  s.run_depth = state_ ? state_->run_depth + 1 : 1;

  RunState* previous_state = state_;
  state_ = &s;

  DoRunLoop();

  state_ = previous_state;
}

void MessagePumpOS2::Quit() {
  DCHECK(state_);
  state_->should_quit = true;
}

//-----------------------------------------------------------------------------
// MessagePumpOS2 protected:

int MessagePumpOS2::GetCurrentDelay() const {
  if (delayed_work_time_.is_null())
    return -1;

  // Be careful here.  TimeDelta has a precision of microseconds, but we want a
  // value in milliseconds.  If there are 5.5ms left, should the delay be 5 or
  // 6?  It should be 6 to avoid executing delayed work too early.
  double timeout = ceil((delayed_work_time_ - TimeTicks::Now()).InMillisecondsF());

  // Range check the |timeout| while converting to an integer.  If the |timeout|
  // is negative, then we need to run delayed work soon.  If the |timeout| is
  // "overflowingly" large, that means a delayed task was posted with a
  // super-long delay.
  return timeout < 0 ? 0 :
      (timeout > std::numeric_limits<int>::max() ?
       std::numeric_limits<int>::max() : static_cast<int>(timeout));
}

//-----------------------------------------------------------------------------
// MessagePumpForUI public:

MessagePumpForUI::MessagePumpForUI() {
  InitMessageWnd();
}

MessagePumpForUI::~MessagePumpForUI() {
  WinDestroyWindow(message_hwnd_);
  WinDestroyMsgQueue(hmq_);
  WinTerminate(hab_);
}

void MessagePumpForUI::ScheduleWork() {
  if (__atomic_xchg(&work_state_, HAVE_WORK) != READY)
    return;  // Someone else continued the pumping.

  // Make sure the MessagePump does some work for us.
  BOOL ret = WinPostMsg(message_hwnd_, kMsgHaveWork, reinterpret_cast<MPARAM>(this), 0);
  if (ret)
    return;  // There was room in the Window Message queue.

  // We have failed to insert a have-work message, so there is a chance that we
  // will starve tasks/timers while sitting in a nested run loop.  Nested
  // loops only look at Windows Message queues, and don't look at *our* task
  // queues, etc., so we might not get a time slice in such. :-(
  // We could abort here, but the fear is that this failure mode is plausibly
  // common (queue is full, of about 2000 messages), so we'll do a near-graceful
  // recovery.  Nested loops are pretty transient (we think), so this will
  // probably be recoverable.

  // Clarify that we didn't really insert.
  __atomic_xchg(&work_state_, READY);
  UMA_HISTOGRAM_ENUMERATION("Chrome.MessageLoopProblem", MESSAGE_POST_ERROR,
                            MESSAGE_LOOP_PROBLEM_MAX);
}

void MessagePumpForUI::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  delayed_work_time_ = delayed_work_time;
  RescheduleTimer();
}

void MessagePumpForUI::EnableWmQuit() {
  enable_wm_quit_ = true;
}

void MessagePumpForUI::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MessagePumpForUI::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

//-----------------------------------------------------------------------------
// MessagePumpForUI private:

// static
Lock MessagePumpForUI::timer_ids_lock_;
// static
MessagePumpForUI::TimerIDMap MessagePumpForUI::timer_ids_;

// static
ULONG MessagePumpForUI::SetTimer(HWND hwnd, MessagePumpForUI *that, int msec) {
  AutoLock auto_lock(timer_ids_lock_);
  TimerID tid = { hwnd, 1 };
  for (; tid.id <= TID_USERMAX; ++tid.id) {
    if (timer_ids_.find(tid) == timer_ids_.end())
      break;
  }
  DCHECK(tid.id <= TID_USERMAX);
  if (tid.id > TID_USERMAX)
    return 0;
  timer_ids_[tid] = that;
  ULONG id = WinStartTimer(0, hwnd, tid.id, msec);
  DCHECK(id == tid.id);
  return id;
}

// static
MessagePumpForUI *MessagePumpForUI::GetTimer(HWND hwnd, ULONG id) {
  AutoLock auto_lock(timer_ids_lock_);
  TimerID tid = { hwnd, (USHORT)id };
  TimerIDMap::iterator it = timer_ids_.find(tid);
  if (it != timer_ids_.end())
    return it->second;
  return nullptr;
}

// static
void MessagePumpForUI::KillTimer(HWND hwnd, ULONG id) {
  AutoLock auto_lock(timer_ids_lock_);
  WinStopTimer(0, hwnd, id);
  TimerID tid = { hwnd, (USHORT)id };
  timer_ids_.erase(tid);
}

// static
MRESULT APIENTRY MessagePumpForUI::WndProcThunk(
    HWND hwnd, ULONG message, MPARAM mp1, MPARAM mp2) {
  switch (message) {
    case kMsgHaveWork:
      reinterpret_cast<MessagePumpForUI*>(mp1)->HandleWorkMessage();
      break;
    case WM_TIMER:
      ULONG id = LONGFROMMP(mp1);
      MessagePumpForUI *that = GetTimer(hwnd, id);
      // Make sure it's not a late msg from a deleted timer
      if (that) {
        DCHECK(that->message_hwnd_ == hwnd);
        DCHECK(that->timer_id_ == id);
        WinStopTimer(0, hwnd, id);
        that->timer_id_ = 0;
        that->HandleTimerMessage();
      }
      break;
  }
  return WinDefWindowProc(hwnd, message, mp1, mp2);
}

void MessagePumpForUI::DoRunLoop() {
  // IF this was just a simple PeekMessage() loop (servicing all possible work
  // queues), then Windows would try to achieve the following order according
  // to MSDN documentation about PeekMessage with no filter):
  //    * Sent messages
  //    * Posted messages
  //    * Sent messages (again)
  //    * WM_PAINT messages
  //    * WM_TIMER messages
  //
  // Summary: none of the above classes is starved, and sent messages has twice
  // the chance of being processed (i.e., reduced service time).

  for (;;) {
    // If we do any work, we may create more messages etc., and more work may
    // possibly be waiting in another task group.  When we (for example)
    // ProcessNextWindowsMessage(), there is a good chance there are still more
    // messages waiting.  On the other hand, when any of these methods return
    // having done no work, then it is pretty unlikely that calling them again
    // quickly will find any work to do.  Finally, if they all say they had no
    // work, then it is a good time to consider sleeping (waiting) for more
    // work.

    state_->delegate->BeforeDoInternalWork();
    bool more_work_is_plausible = ProcessNextWindowsMessage();
    if (state_->should_quit)
      break;

    more_work_is_plausible |= state_->delegate->DoWork();
    if (state_->should_quit)
      break;

    more_work_is_plausible |=
        state_->delegate->DoDelayedWork(&delayed_work_time_);
    // If we did not process any delayed work, then we can assume that our
    // existing WM_TIMER if any will fire when delayed work should run.  We
    // don't want to disturb that timer if it is already in flight.  However,
    // if we did do all remaining delayed work, then lets kill the WM_TIMER.
    if (more_work_is_plausible && delayed_work_time_.is_null())
      if (timer_id_) {
        KillTimer(message_hwnd_, timer_id_);
        timer_id_ = 0;
      }
    if (state_->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    more_work_is_plausible = state_->delegate->DoIdleWork();
    if (state_->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    WaitForWork();  // Wait (sleep) until we have work to do again.
  }
}

void MessagePumpForUI::InitMessageWnd()
{
  // morph to the PM session (needed to use message queues)
  PPIB ppib;
  DosGetInfoBlocks(nullptr, &ppib);
  ppib->pib_ultype = 3;

  hab_ = WinInitialize(0);
  hmq_ = WinCreateMsgQueue(hab_, 0);
  DCHECK(hmq_);

  BOOL rc = WinRegisterClass(0, kWndClass, WndProcThunk, 0, 0);
  DCHECK(rc);

  message_hwnd_ = WinCreateWindow(HWND_OBJECT, kWndClass, 0, 0,
                                  0, 0, 0, 0, NULLHANDLE, HWND_BOTTOM, 0, 0, 0);
  DCHECK(message_hwnd_);
}

void MessagePumpForUI::WaitForWork() {
  // Wait until a message is available.
  WinWaitMsg(0, 0, 0);
}

void MessagePumpForUI::HandleWorkMessage() {
  // If we are being called outside of the context of Run, then don't try to do
  // any work.  This could correspond to a MessageBox call or something of that
  // sort.
  if (!state_) {
    // Since we handled a kMsgHaveWork message, we must still update this flag.
    __atomic_xchg(&work_state_, READY);
    return;
  }

  // Let whatever would have run had we not been putting messages in the queue
  // run now.  This is an attempt to make our dummy message not starve other
  // messages that may be in the Windows message queue.
  ProcessPumpReplacementMessage();

  // Now give the delegate a chance to do some work.  It'll let us know if it
  // needs to do more work.
  if (state_->delegate->DoWork())
    ScheduleWork();
  state_->delegate->DoDelayedWork(&delayed_work_time_);
  RescheduleTimer();
}

void MessagePumpForUI::HandleTimerMessage() {
  // If we are being called outside of the context of Run, then don't do
  // anything.  This could correspond to a MessageBox call or something of
  // that sort.
  if (!state_)
    return;

  state_->delegate->DoDelayedWork(&delayed_work_time_);
  RescheduleTimer();
}

void MessagePumpForUI::RescheduleTimer() {
  if (delayed_work_time_.is_null())
    return;
  //
  // NOTE: OS/2 PM timers are very similar to Windows ones and the comments
  // apply to it most of the time; so we don't bother ourselves changing
  // the names.
  //
  // We would *like* to provide high resolution timers.  Windows timers using
  // SetTimer() have a 10ms granularity.  We have to use WM_TIMER as a wakeup
  // mechanism because the application can enter modal windows loops where it
  // is not running our MessageLoop; the only way to have our timers fire in
  // these cases is to post messages there.
  //
  // To provide sub-10ms timers, we process timers directly from our run loop.
  // For the common case, timers will be processed there as the run loop does
  // its normal work.  However, we *also* set the system timer so that WM_TIMER
  // events fire.  This mops up the case of timers not being able to work in
  // modal message loops.  It is possible for the SetTimer to pop and have no
  // pending timers, because they could have already been processed by the
  // run loop itself.
  //
  // We use a single SetTimer corresponding to the timer that will expire
  // soonest.  As new timers are created and destroyed, we update SetTimer.
  // Getting a spurious SetTimer event firing is benign, as we'll just be
  // processing an empty timer queue.
  //
  int delay_msec = GetCurrentDelay();
  DCHECK_GE(delay_msec, 0);
  if (delay_msec == 0) {
    ScheduleWork();
  } else {
    // Tell the optimizer to retain these values to simplify analyzing hangs.
    base::debug::Alias(&delay_msec);
    // Create a WM_TIMER event that will wake us up to check for any pending
    // timers (in case we are running within a nested, external sub-pump).
    timer_id_ = SetTimer(message_hwnd_, this, delay_msec);
    if (timer_id_)
      return;
    // If we can't set timers, we are in big trouble... but cross our fingers
    // for now.
    // TODO(jar): If we don't see this error, use a CHECK() here instead.
    UMA_HISTOGRAM_ENUMERATION("Chrome.MessageLoopProblem", SET_TIMER_ERROR,
                              MESSAGE_LOOP_PROBLEM_MAX);
  }
}

bool MessagePumpForUI::ProcessNextWindowsMessage() {
  // If there are sent messages in the queue then WinPeekMsg internally
  // dispatches the message and returns false. We return true in this
  // case to ensure that the message loop peeks again instead of calling
  // WinWaitMsg again. Note that there is no proof that it works exactly like
  // that in PM -- we just assume that it's similar to Windows.
  bool sent_messages_in_queue = false;
  ULONG queue_status = WinQueryQueueStatus(HWND_DESKTOP);
  if ((queue_status >> 16) & QS_SENDMSG)
    sent_messages_in_queue = true;

  QMSG msg;
  if (WinPeekMsg(0, &msg, NULLHANDLE, 0, 0, PM_REMOVE))
    return ProcessMessageHelper(msg);

  return sent_messages_in_queue;
}

bool MessagePumpForUI::ProcessMessageHelper(QMSG& msg) {
  TRACE_EVENT1("base,toplevel", "MessagePumpForUI::ProcessMessageHelper",
               "message", msg.msg);
  if (WM_QUIT == msg.msg) {
    // WM_QUIT is the standard way to exit a ::GetMessage() loop. Our
    // MessageLoop has its own quit mechanism, so WM_QUIT should only terminate
    // it if |enable_wm_quit_| is explicitly set (and is generally unexpected
    // otherwise).
    if (enable_wm_quit_) {
      state_->should_quit = true;
      return false;
    }
    UMA_HISTOGRAM_ENUMERATION("Chrome.MessageLoopProblem",
                              RECEIVED_WM_QUIT_ERROR, MESSAGE_LOOP_PROBLEM_MAX);
    return true;
  }

  // While running our main message pump, we discard kMsgHaveWork messages.
  if (msg.msg == kMsgHaveWork && msg.hwnd == message_hwnd_)
    return ProcessPumpReplacementMessage();

  for (Observer& observer : observers_)
    observer.WillDispatchMSG(msg);
  WinDispatchMsg(0, &msg);
  for (Observer& observer : observers_)
    observer.DidDispatchMSG(msg);

  return true;
}

bool MessagePumpForUI::ProcessPumpReplacementMessage() {
  // When we encounter a kMsgHaveWork message, this method is called to peek and
  // process a replacement message. The goal is to make the kMsgHaveWork as non-
  // intrusive as possible, even though a continuous stream of such messages are
  // posted. This method carefully peeks a message while there is no chance for
  // a kMsgHaveWork to be pending, then resets the |have_work_| flag (allowing a
  // replacement kMsgHaveWork to possibly be posted), and finally dispatches
  // that peeked replacement. Note that the re-post of kMsgHaveWork may be
  // asynchronous to this thread!!

  QMSG msg;
  bool have_message = WinPeekMsg(0, &msg, NULLHANDLE, 0, 0, PM_REMOVE) != FALSE;

  // Expect no message or a message different than kMsgHaveWork.
  DCHECK(!have_message || kMsgHaveWork != msg.msg ||
         msg.hwnd != message_hwnd_);

  // Since we discarded a kMsgHaveWork message, we must update the flag.
  int old_work_state_ = __atomic_xchg(&work_state_, READY);
  DCHECK_EQ(HAVE_WORK, old_work_state_);

  // We don't need a special time slice if we didn't have_message to process.
  if (!have_message)
    return false;

  if (WM_QUIT == msg.msg) {
    // If we're in a nested ::GetMessage() loop then we must let that loop see
    // the WM_QUIT in order for it to exit. If we're in DoRunLoop then the re-
    // posted WM_QUIT will be either ignored, or handled, by
    // ProcessMessageHelper() called directly from ProcessNextWindowsMessage().
    WinPostMsg(NULLHANDLE, WM_QUIT, 0, 0);
    // Note: we *must not* ScheduleWork() here as WM_QUIT is a low-priority
    // message on Windows (it is only returned by ::PeekMessage() when idle) :
    // https://blogs.msdn.microsoft.com/oldnewthing/20051104-33/?p=33453. As
    // such posting a kMsgHaveWork message via ScheduleWork() would cause an
    // infinite loop (kMsgHaveWork message handled first means we end up here
    // again and repost WM_QUIT+ScheduleWork() again, etc.). Not leaving a
    // kMsgHaveWork message behind however is also problematic as unwinding
    // multiple layers of nested ::GetMessage() loops can result in starving
    // application tasks. TODO(https://crbug.com/890016) : Fix this. Although
    // not checked, the same might apply to PM as well.

    // The return value is mostly irrelevant but return true like we would after
    // processing a QuitClosure() task.
    return true;
  }

  // Guarantee we'll get another time slice in the case where we go into native
  // windows code. This ScheduleWork() may hurt performance a tiny bit when
  // tasks appear very infrequently, but when the event queue is busy, the
  // kMsgHaveWork events get (percentage wise) rarer and rarer.
  ScheduleWork();
  return ProcessMessageHelper(msg);
}

}  // namespace base

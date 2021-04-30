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

// Returns the number of milliseconds before |next_task_time|, clamped between
// zero and the biggest ULONG value (or (ULONG)-1 if |next_task_time.is_max()|).
// Optionally, a recent value of Now() may be passed in to avoid resampling it.
ULONG GetSleepTimeoutMs(TimeTicks next_task_time,
                        TimeTicks recent_now = TimeTicks()) {
  // Shouldn't need to sleep or install a timer when there's pending immediate
  // work.
  DCHECK(!next_task_time.is_null());

  if (next_task_time.is_max())
    return (ULONG)-1;

  auto now = recent_now.is_null() ? TimeTicks::Now() : recent_now;
  auto timeout_ms = (next_task_time - now).InMillisecondsRoundedUp();

  // A saturated_cast with an unsigned destination automatically clamps negative
  // values at zero.
  static_assert(!std::is_signed<ULONG>::value, "ULONG is unexpectedly signed");
  return saturated_cast<ULONG>(timeout_ms);
}

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
MessagePumpOS2::~MessagePumpOS2() = default;

void MessagePumpOS2::Run(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

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
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  DCHECK(state_);
  state_->should_quit = true;
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
  // This is the only MessagePumpForUI method which can be called outside of
  // |bound_thread_|.
  bool not_scheduled = false;
  if (!work_scheduled_.compare_exchange_strong(not_scheduled, true))
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
  work_scheduled_ = false;
  UMA_HISTOGRAM_ENUMERATION("Chrome.MessageLoopProblem", MESSAGE_POST_ERROR,
                            MESSAGE_LOOP_PROBLEM_MAX);
}

void MessagePumpForUI::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // Since this is always called from |bound_thread_|, there is almost always
  // nothing to do as the loop is already running. When the loop becomes idle,
  // it will typically WaitForWork() in DoRunLoop() with the timeout provided by
  // DoWork(). The only alternative to this is entering a native nested loop
  // (e.g. modal dialog) under a ScopedNestableTaskAllower, in which case
  // HandleWorkMessage() will be invoked when the system picks up kMsgHaveWork
  // and it will ScheduleNativeTimer() if it's out of immediate work. However,
  // in that alternate scenario : it's possible for a Windows native task (e.g.
  // https://docs.microsoft.com/en-us/windows/desktop/winmsg/using-hooks) to
  // wake the native nested loop and PostDelayedTask() to the current thread
  // from it. This is the only case where we must install/adjust the native
  // timer from ScheduleDelayedWork() because if we don't, the native loop will
  // go back to sleep, unaware of the new |delayed_work_time|.
  // TODO(gab): This could potentially be replaced by a ForegroundIdleProc hook
  // if Windows ends up being the only platform requiring ScheduleDelayedWork().
  if (in_native_loop_ && !work_scheduled_) {
    // TODO(gab): Consider passing a NextWorkInfo object to ScheduleDelayedWork
    // to take advantage of |recent_now| here too.
    ScheduleNativeTimer({delayed_work_time, TimeTicks::Now()});
  }
}

void MessagePumpForUI::EnableWmQuit() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  enable_wm_quit_ = true;
}

void MessagePumpForUI::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  observers_.AddObserver(observer);
}

void MessagePumpForUI::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  observers_.RemoveObserver(observer);
}

//-----------------------------------------------------------------------------
// MessagePumpForUI private:

// static
Lock MessagePumpForUI::timer_ids_lock_;
// static
MessagePumpForUI::TimerIDMap MessagePumpForUI::timer_ids_;

// static
ULONG MessagePumpForUI::SetTimer(HWND hwnd, MessagePumpForUI *that, ULONG msec) {
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
BOOL MessagePumpForUI::KillTimer(HWND hwnd, ULONG id) {
  AutoLock auto_lock(timer_ids_lock_);
  BOOL rc = WinStopTimer(0, hwnd, id);
  if (rc) {
    TimerID tid = { hwnd, (USHORT)id };
    timer_ids_.erase(tid);
  }
  return rc;
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
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

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

    in_native_loop_ = false;
    state_->delegate->BeforeDoInternalWork();
    DCHECK(!in_native_loop_);

    bool more_work_is_plausible = ProcessNextWindowsMessage();
    in_native_loop_ = false;
    if (state_->should_quit)
      break;

    Delegate::NextWorkInfo next_work_info = state_->delegate->DoWork();
    in_native_loop_ = false;
    more_work_is_plausible |= next_work_info.is_immediate();
    if (state_->should_quit)
      break;

    if (installed_native_timer_) {
      // As described in ScheduleNativeTimer(), the native timer is only
      // installed and needed while in a nested native loop. If it is installed,
      // it means the above work entered such a loop. Having now resumed, the
      // native timer is no longer needed.
      KillNativeTimer();
    }
    if (more_work_is_plausible)
      continue;

    more_work_is_plausible = state_->delegate->DoIdleWork();
    // DoIdleWork() shouldn't end up in native nested loops and thus shouldn't
    // have any chance of reinstalling a native timer.
    DCHECK(!in_native_loop_);
    DCHECK(!installed_native_timer_);
    if (state_->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    // WaitForWork() does some work itself, so notify the delegate of it.
    state_->delegate->BeforeWait();
    WaitForWork(next_work_info);
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

void MessagePumpForUI::WaitForWork(Delegate::NextWorkInfo next_work_info) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // Wait until a message is available.
  WinWaitMsg(0, 0, 0);
}

void MessagePumpForUI::HandleWorkMessage() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // The kMsgHaveWork message was consumed by a native loop, we must assume
  // we're in one until DoRunLoop() gets control back.
  in_native_loop_ = true;

  // If we are being called outside of the context of Run, then don't try to do
  // any work.  This could correspond to a MessageBox call or something of that
  // sort.
  if (!state_) {
    // Since we handled a kMsgHaveWork message, we must still update this flag.
    work_scheduled_ = false;
    return;
  }

  // Let whatever would have run had we not been putting messages in the queue
  // run now.  This is an attempt to make our dummy message not starve other
  // messages that may be in the Windows message queue.
  ProcessPumpReplacementMessage();

  Delegate::NextWorkInfo next_work_info = state_->delegate->DoWork();
  if (next_work_info.is_immediate()) {
    ScheduleWork();
  } else {
    ScheduleNativeTimer(next_work_info);
  }
}

void MessagePumpForUI::HandleTimerMessage() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // ::KillTimer doesn't remove pending WM_TIMER messages from the queue,
  // explicitly ignore the last WM_TIMER message in that case to avoid handling
  // work from here when DoRunLoop() is active (which could result in scheduling
  // work from two places at once). Note: we're still fine in the event that a
  // second native nested loop is entered before such a dead WM_TIMER message is
  // discarded because ::SetTimer merely resets the timer if invoked twice with
  // the same id.
  if (!installed_native_timer_)
    return;

  // We only need to fire once per specific delay, another timer may be
  // scheduled below but we're done with this one.
  KillNativeTimer();

  // If we are being called outside of the context of Run, then don't do
  // anything.  This could correspond to a MessageBox call or something of
  // that sort.
  if (!state_)
    return;

  Delegate::NextWorkInfo next_work_info = state_->delegate->DoWork();
  if (next_work_info.is_immediate()) {
    ScheduleWork();
  } else {
    ScheduleNativeTimer(next_work_info);
  }
}

void MessagePumpForUI::ScheduleNativeTimer(
    Delegate::NextWorkInfo next_work_info) {
  DCHECK(!next_work_info.is_immediate());
  DCHECK(in_native_loop_);

  // Do not redundantly set the same native timer again if it was already set.
  // This can happen when a nested native loop goes idle with pending delayed
  // tasks, then gets woken up by an immediate task, and goes back to idle with
  // the same pending delay. No need to kill the native timer if there is
  // already one but the |delayed_run_time| has changed as ::SetTimer reuses the
  // same id and will replace and reset the existing timer.
  if (installed_native_timer_ &&
      *installed_native_timer_ == next_work_info.delayed_run_time) {
    return;
  }

  if (next_work_info.delayed_run_time.is_max())
    return;

  // NOTE: OS/2 PM timers are very similar to Windows ones and the comments
  // apply to it most of the time; so we don't bother ourselves changing
  // the names.
  //
  // We do not use native Windows timers in general as they have a poor, 10ms,
  // granularity. Instead we rely on MsgWaitForMultipleObjectsEx's
  // high-resolution timeout to sleep without timers in WaitForWork(). However,
  // when entering a nested native ::GetMessage() loop (e.g. native modal
  // windows) under a ScopedNestableTaskAllower, we have to rely on a native
  // timer when HandleWorkMessage() runs out of immediate work. Since
  // ScopedNestableTaskAllower invokes ScheduleWork() : we are guaranteed that
  // HandleWorkMessage() will be called after entering a nested native loop that
  // should process application tasks. But once HandleWorkMessage() is out of
  // immediate work, ::SetTimer() is used to guarantee we are invoked again
  // should the next delayed task expire before the nested native loop ends. The
  // native timer being unnecessary once we return to our DoRunLoop(), we
  // ::KillTimer when it resumes (nested native loops should be rare so we're
  // not worried about ::SetTimer<=>::KillTimer churn).
  // TODO(gab): The long-standing legacy dependency on the behavior of
  // ScopedNestableTaskAllower is unfortunate, would be nice to make this a
  // MessagePump concept (instead of requiring impls to invoke ScheduleWork()
  // one-way and no-op DoWork() the other way).

  UINT delay_msec = strict_cast<UINT>(GetSleepTimeoutMs(
      next_work_info.delayed_run_time, next_work_info.recent_now));
  if (delay_msec == 0) {
    ScheduleWork();
  } else {
    // Tell the optimizer to retain the delay to simplify analyzing hangs.
    base::debug::Alias(&delay_msec);
    // Create a WM_TIMER event that will wake us up to check for any pending
    // timers (in case we are running within a nested, external sub-pump).
    timer_id_ = SetTimer(message_hwnd_, this, delay_msec);
    if (timer_id_)
      return;
    installed_native_timer_ = next_work_info.delayed_run_time;

    // If we can't set timers, we are in big trouble... but cross our fingers
    // for now.
    // TODO(jar): If we don't see this error, use a CHECK() here instead.
    UMA_HISTOGRAM_ENUMERATION("Chrome.MessageLoopProblem", SET_TIMER_ERROR,
                              MESSAGE_LOOP_PROBLEM_MAX);
  }
}

void MessagePumpForUI::KillNativeTimer() {
  DCHECK(installed_native_timer_);
  DCHECK(timer_id_);
  const bool success =
      KillTimer(message_hwnd_, timer_id_);
  DPCHECK(success);
  timer_id_ = 0;
  installed_native_timer_.reset();
}

bool MessagePumpForUI::ProcessNextWindowsMessage() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

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
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

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
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

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
  DCHECK(work_scheduled_);
  work_scheduled_ = false;

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

// Copyright (C) 2023 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_PERFTIMER_HPP
#define XAYAGAME_PERFTIMER_HPP

#include <chrono>

#include <glog/logging.h>

namespace xaya
{

/**
 * A simple performance timer, which measures time between construction of the
 * instance and when it is Stop()'ed, and can return the duration afterwards
 * as well as print it readabily to a stream.  This is used for log messages
 * that time certain things in the code.
 */
class PerformanceTimer
{

private:

  /** Clock used for the timing.  */
  using Clock = std::chrono::steady_clock;

  /** Duration type used for the logging feedback.  */
  using LogDuration = std::chrono::microseconds;
  /** Unit of the logging duration type.  */
  static constexpr const char* LOG_UNIT = "us";

  /** The starting timepoint.  */
  Clock::time_point start;

  /** The ending timepoint if already stopped.  */
  Clock::time_point end;

  /** True if the timer is already stopped.  */
  bool stopped = false;

public:

  PerformanceTimer ()
  {
    start = Clock::now ();
  }

  void
  Stop ()
  {
    end = Clock::now ();
    CHECK (start <= end) << "Time was not increasing";
    CHECK (!stopped) << "Timer is already stopped";
    stopped = true;
  }

  template <typename Duration>
    Duration
    Get () const
  {
    CHECK (stopped) << "Timer is not yet stopped";
    return std::chrono::duration_cast<Duration> (end - start);
  }

  template <typename T>
    friend T&
    operator<< (T& out, const PerformanceTimer& t)
  {
    out << t.Get<LogDuration> ().count () << " " << LOG_UNIT;
    return out;
  }

};

} // namespace xaya

#endif // XAYAGAME_PERFTIMER_HPP

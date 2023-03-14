package com.google.android.apps.internal.games.memoryadvice;

import java.util.Timer;
import java.util.TimerTask;
import org.json.JSONObject;

/**
 * A MemoryWatcher automatically polls the memory advisor and calls back a client a soon as possible
 * when the state changes.
 */
public class MemoryWatcher {
  private static final int UNRESPONSIVE_THRESHOLD = 100;

  private final long watcherStartTime;
  private final Runnable runner;
  private final Timer timer = new Timer();
  private long expectedTime;
  private long unresponsiveTime;
  private MemoryAdvisor.MemoryState lastReportedState = MemoryAdvisor.MemoryState.UNKNOWN;
  private long totalTimeSpent;

  /**
   * Create a MemoryWatcher object. This calls back the supplied client when the memory state
   * changes.
   * @param memoryAdvisor The memory advisor object to employ.
   * @param maxMillisecondsPerSecond The budget for overhead introduced by the advisor and watcher.
   * @param minimumFrequency The minimum time duration between iterations, in milliseconds.
   * @param client The client to call back when the state changes.
   */
  public MemoryWatcher(final MemoryAdvisor memoryAdvisor, final long maxMillisecondsPerSecond,
      final long minimumFrequency, final Client client) {
    watcherStartTime = System.currentTimeMillis();
    expectedTime = watcherStartTime;
    runner = new Runnable() {
      @Override
      public void run() {
        long start = System.currentTimeMillis();
        JSONObject advice = memoryAdvisor.getAdvice();
        MemoryAdvisor.MemoryState memoryState = MemoryAdvisor.getMemoryState(advice);
        long late = start - expectedTime;
        if (late > UNRESPONSIVE_THRESHOLD) {
          // The timer fired very late. We deduct the 'lost' time from the runtime used for
          // calculation.
          unresponsiveTime += late;
        }
        long end = System.currentTimeMillis();
        if (memoryState != lastReportedState) {
          lastReportedState = memoryState;
          client.newState(memoryState);
        }

        // Time spent this iteration.
        long duration = end - start;
        totalTimeSpent += duration;

        // Calculate the run time required to have enough budget for the time actually spent
        // (in milliseconds).
        long targetTime = totalTimeSpent * 1000 / maxMillisecondsPerSecond;

        // The total time the object has been created (in milliseconds), minus any time when the
        // watcher was unresponsive.
        long timeSinceStart = end - watcherStartTime - unresponsiveTime;

        // Sleep until the moment that the method will be within its budget.
        long sleepFor = targetTime - timeSinceStart;

        if (sleepFor < 1) {
          sleepFor = 1;  // Run immediately in the case of being well under budget.
        } else if (sleepFor > minimumFrequency) {
          sleepFor = minimumFrequency;  // Impose minimum frequency.
        }
        expectedTime = System.currentTimeMillis() + sleepFor;
        timer.schedule(new TimerTask() {
          @Override
          public void run() {
            runner.run();
          }
        }, sleepFor);
      }
    };
    runner.run();
  }

  /**
   * A client for the MemoryWatcher class.
   */
  public abstract static class Client {
    public abstract void newState(MemoryAdvisor.MemoryState state);
  }
}

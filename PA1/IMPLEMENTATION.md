# PA1 C implementation

## Build and run

1. Open `RTOS_M11507W14_HW1/Microsoft/Windows/Kernel/OS2/VS/OS2.sln` in Visual Studio 2022 with the C++ desktop tools and Windows SDK installed. Select **Debug / Win32** (the configuration supplied with this project).
2. Select `#define ALGORITHM RM` or `#define ALGORITHM FIFO` in `Micrium/Software/uCOS-II/Source/ucos_ii.h`, then rebuild. The default is RM. Compiler definitions `ALGORITHM=0` and `ALGORITHM=1` are also supported.
3. Place the desired input in `TaskSet.txt` in the working directory. When running from Visual Studio this is the `VS` directory. When starting the executable from a terminal, change to that directory first.
4. Read `Output.txt` after the program exits. The filenames remain `./TaskSet.txt` and `./Output.txt`, and `SYSTEM_END_TIME` remains 30. Events and task messages at tick 30 are included, as in the supplied examples; tick 31 is never processed. The program also terminates immediately at the first missed deadline.

`OS_TICKS_PER_SEC` remains 1 by default in `Microsoft/Windows/Kernel/OS2/os_cfg.h`. It can be changed there or supplied as a compiler definition. The existing Win32 multimedia timer uses integer milliseconds, so 1000 ticks/s is its nominal resolution limit; host scheduling and timer-event coalescing can reduce actual throughput.

## Part I: TCB linked list

`OS_TCBInit()` prints the newly inserted TCB's priority, actual address, previous link and next link. `OSStart()` walks the entire list and prints its links before dispatch. `%p` prints runtime pointers without truncation. These diagnostics go only to the console and cannot contaminate the checker's `Output.txt`.

## Parts II and III: kernel scheduling

The original startup, dynamic task stacks, task creation, ready bitmaps, TCB list, interrupt handling and context-switch machinery remain in place. The changes are confined to the job state, input/output helpers, periodic task body, and the relevant kernel/Win32 scheduling points. Unrelated source comments and modules are retained. HW1-only trace messages were removed because their format conflicts with the PA1 output.

- `InputFile()` reads the actual TaskID and all four parameters, including tabs, trailing whitespace and blank lines. It checks input bounds, duplicate IDs, missing files and the task limit. RM priorities are distinct ranks of `(period, TaskID)`; FIFO uses distinct ID ranks for its TCB slots.
- Each TCB holds the job number, release time, next release, remaining execution, cumulative preemption time and preemption count. Releases remain anchored to `arrival + job_number * period`, independent of completion time.
- `OSStart()` initializes the periodic TCBs and readies only jobs released at zero. Tasks whose arrival is later stay out of the ready bitmap.
- `OSTimeTick()` first advances the tick and keeps the existing delay processing. The added `OS_PA1_Tick()` charges one unit to the task that ran during the preceding interval. It then checks deadlines, releases new jobs and chooses the next task before `OSIntExit()` performs the existing context switch. This placement makes arrival and execution accounting happen exactly once per tick, including consecutive jobs on the same TCB.
- Under RM, the existing ready bitmap finds the highest priority task. Equal periods use the smaller TaskID. A release can preempt an unfinished lower priority job.
- Under FIFO, `OS_SchedNew()` retains an executing job until completion, then chooses the oldest released pending job, breaking simultaneous arrivals by TaskID. A completed job relinquishes FIFO ownership before its successor is released, so an older waiting job cannot be skipped.
- Completion records use `completion - release` for response time. Preemption time counts only intervals after an actual preemption and before resumption; initial ready-queue waiting is excluded. Preemption instances count task-to-task preemptions, excluding idle dispatch.
- The task body prints `task(ID) is running` once per selected tick. Kernel code prints scheduling events. Idle is labeled with its priority, `task(63)`.

### Deadline policy and event order

The implicit deadline is `release + period`. A job completing exactly at its deadline succeeds: execution is charged before checking for an unfinished job. If an unfinished job reaches its deadline, the program prints `MissDeadline` and ends, matching both supplied failing examples (RM set 2 at tick 15; FIFO set 2 at tick 21). If several jobs miss simultaneously, the smaller TaskID is reported first.

Deadline checking precedes releases and other events, so a miss suppresses completion/preemption at that tick. Otherwise completion and preemption are mutually exclusive: the previous job either finished or was displaced. The event is recorded before the task's running message. Completion can point to a new job of the same task.

### Win32 execution and output

The Win32 tick thread has higher host priority than the task threads. Without synchronization it can advance another tick before the selected task prints its message. An auto-reset event acknowledges the task-level log at startup and after each non-idle tick. The tick thread waits **outside** the CPU critical section; a task can therefore acquire that section and acknowledge without deadlock. This preserves task-function logging while preventing a host scheduling race from dropping output. The port still uses its existing timer and context switches.

`PA1_Log()` appends to a fixed, checked memory buffer while serialized by the existing CPU critical section. The file is opened once and output is flushed at termination, keeping repeated file opens and console writes out of the timed path. Scheduling messages appear on the console after the run; TCB diagnostics appear during initialization.

## Part IV: kernel overhead discussion

The main sources of overhead here are the emulated interrupt/task context switches (`SuspendThread`, `ResumeThread`, signaling and waiting), synchronization, the per-tick TCB scans, and output formatting. On an embedded processor, interrupt entry/exit and saving/restoring task context replace much of the Win32 thread overhead. Frequent scheduling at high tick rates increases the fraction of time spent in these paths.

The implementation reduces output overhead by buffering logs and keeping console and disk writes out of the tick path. RM still resolves ready priorities through the original bitmap. Preemption statistics use bounded per-tick work and require no allocation during execution. Further improvements could use structured binary trace records with formatting after the run, a release/deadline queue to avoid scanning sleeping tasks, a FIFO ready queue, and tickless operation during idle spans. Such changes add complexity and were left out to preserve the supplied architecture. Measure context-switch cost, maximum interrupt duration and effective tick throughput before claiming a higher real-time rate; correct logical traces alone do not establish wall-clock deadlines on Windows.

## Verification

`tests/test_scheduler.py` runs the compiled C executable, compares all three fixtures for the selected algorithm byte-for-byte after normalizing line endings, and checks dynamic TCB list links. It also compares generated task sets with an independent job-level reference scheduler and rejects malformed or missing input.

From the `PA1` directory, with an executable built for the selected algorithm:

```powershell
python tests/test_scheduler.py --exe RTOS_M11507W14_HW1/Microsoft/Windows/Kernel/OS2/VS/Debug/OS2.exe --algorithm RM --random 30
```

Use `--algorithm FIFO` for a FIFO build. Optional `--repeat N` repeats valid cases; `--artifacts DIR` saves the three fixture outputs and console traces for inspection. Tests use separate temporary working directories, preserving the submitted input and output files.

Verified with the actual Visual Studio Win32 executable:

| Algorithm | Configured ticks/s | Schedule and TCB comparisons | Invalid-input checks |
| --- | ---: | ---: | ---: |
| RM | 100 | 42 | 10 |
| FIFO | 100 | 42 | 10 |
| RM | 1000 | 126 (three repetitions) | 10 |
| FIFO | 1000 | 126 (three repetitions) | 10 |

All supplied fixture outputs match, including deadline-miss termination. Additional cases cover unsorted IDs, ties, initial idle time, nested preemption, full task capacity, exact deadlines, a new job on the same TCB, and events at tick 30. The checked-in `VS/Output.txt` is regenerated from its existing `TaskSet.txt` with the default RM configuration.

The local Visual Studio debug-symbol server reported `LNK1101` with the existing `/ZI` configuration. Verification builds appended `/Z7` through the `_CL_` environment variable and `/DEBUG:NONE` through `_LINK_`, avoiding that local symbol-server issue without modifying the project file. Frequency/algorithm tests additionally appended `/DOS_TICKS_PER_SEC=100` (or `1000`) and `/DALGORITHM=0` (or `1`). These environment overrides are confined to the build process; the source defaults remain RM and 1 tick/s.

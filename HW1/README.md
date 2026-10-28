# RTOS HW1 - M11507W14

The original Micrium directory tree, OS2/OS3 projects, and Win32 port are preserved.
HW1 changes are confined to the existing OS2 `main.c`, `app_hooks.c`, and uCOS-II
`ucos_ii.h`, `os_core.c`, `os_time.c`. The scheduler still uses its original
ready bitmap algorithm. Modified blocks are marked `HW1` in source.

## Build and run

Open `micrium_win32_kernel/Micrium_Win32_Kernel/Microsoft/Windows/Kernel/OS2/VS/OS2.sln`.
Select **Debug / Win32**. The existing project specifies the VS 2022 v143 toolset.
If only VS 2026 v145 is installed, use its Developer PowerShell and override the
toolset for this build without changing the project file:

```powershell
Set-Location micrium_win32_kernel/Micrium_Win32_Kernel/Microsoft/Windows/Kernel/OS2/VS
MSBuild.exe OS2.vcxproj /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145
./Debug/OS2.exe
```

Run with the **VS directory as the working directory**. This preserves the required
`./TaskSet.txt` and `./Output.txt` filenames. Do not run from the Debug directory.
The prebuilt Win32 executable is included in the submission ZIP under `VS/Debug`.

The TaskSet rows are `TaskID Arrival ExecutionTime DelayTicks`:

```text
1 0 1 4
2 0 1 7
```

For this HW1, both tasks arrive at zero, priorities are 1 and 2, and execution
time is input metadata. Each task prints then calls `OSTimeDly(4)` or `OSTimeDly(7)`.
Input validation enforces these assignment parameters. There is no artificial
CPU execution-time simulation.

## Trace and result

- `OSStart`: logs initial dispatch with `***********` and switch count 0.
- `OS_Sched` and `OSIntExit`: log immediately before their real context switches,
  after the original `OSCtxSwCtr` increment. Actual switches are displayed with
  zero-based indices (`OSCtxSwCtr - 1`), so the sequence is 0,0,1,2,3,... including
  the initial dispatch. The kernel counter itself is unchanged.
- `OSTimeDly`: snapshots the affected ready-table byte and group before/after blocking.
- `OSTimeTick`: snapshots the same values before/after making a delayed task ready.
- Job numbers begin at 0 and advance on wakeup, before the next dispatch is logged.
- Idle is `task(63)`; all bitmap values have `0x` prefixes.
- Logging is serialized by the existing kernel critical section and each output
  line is flushed. The existing simulation limit remains 30 ticks (exit on tick 31).

The checked-in `Output.txt` is from the actual compiled OS2 executable. Task 1
runs at ticks 0,4,8,12,16,20,24,28; task 2 at 0,7,14,21,28. At tick 28 both
become ready, task 1 runs first, then task 2. There are 23 actual context switches;
the final displayed index is 22, matching the assignment's zero-based format.

## Submission

`RTOS_M11507W14_HW1.zip` contains the report PDF and project folder
`RTOS_M11507W14_HW1`, with the original `Micrium` and `Microsoft` trees intact.
The report contains the filled system flow, function explanations, marked source
captures, actual output excerpts, and validation results. Visual Studio caches
and intermediate object/PDB files are excluded.

Validation: Debug/Win32 built with the original v143 toolset; native run exited successfully; output
checked for periods, job indices, ready bitmap transitions, switch count and
priority at tick 28; missing and malformed input must exit with a nonzero code.

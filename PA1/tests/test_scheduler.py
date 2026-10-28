"""Check the real Win32 uC/OS-II executable against fixtures and a job-level oracle."""
import argparse
import difflib
from pathlib import Path
import random
import re
import subprocess
import tempfile


def expected_output(tasks, algorithm):
    jobs = []
    current = None
    lines = []

    def label(job):
        return "task(63)" if job is None else f"task({job['id']:2d})({job['number']:2d})"

    for tick in range(31):
        completed = current if current and current["left"] == 0 else None
        missed = sorted((j for j in jobs if j["left"] and j["deadline"] <= tick),
                        key=lambda j: j["id"])
        if missed:
            lines.append(f"{tick}\tMissDeadline\t{label(missed[0])}\t-------------------")
            break
        for task_id, arrival, execution, period in tasks:
            if tick >= arrival and (tick - arrival) % period == 0:
                jobs.append(dict(id=task_id, release=tick, deadline=tick + period,
                                 number=(tick - arrival) // period, left=execution,
                                 period=period, preemptions=0, paused=0, suspended=False))
        ready = [j for j in jobs if j["left"]]
        if algorithm == "FIFO" and current and current["left"]:
            following = current
        else:
            key = (lambda j: (j["period"], j["id"])) if algorithm == "RM" else (
                lambda j: (j["release"], j["id"]))
            following = min(ready, key=key) if ready else None
        if tick and completed is None and following is not current:
            lines.append(f"{tick}\tPreemption\t{label(current)}\t{label(following)}")
            if current:
                current["preemptions"] += 1
                current["suspended"] = True
        if completed:
            lines.append(f"{tick}\tCompletion\t{label(completed)}\t{label(following)}\t"
                         f"{tick - completed['release']}\t{completed['paused']}\t"
                         f"{completed['preemptions']}")
        current = following
        if current:
            current["suspended"] = False
            lines.append(f"{tick}\ttask({current['id']:2d}) is running")
            current["left"] -= 1
        for job in ready:
            if job is not current and job["suspended"]:
                job["paused"] += 1
    return lines


def check_tcb_list(console, count):
    rows = re.findall(r"(?m)^(\d+)\t([0-9a-fA-F]+)\t([0-9a-fA-F]+)\t([0-9a-fA-F]+)\r?$", console)
    assert len(rows) == count + 1, f"TCB list has {len(rows)} entries, expected {count + 1}"
    nodes = [(int(p), int(prev, 16), int(addr, 16), int(nxt, 16)) for p, prev, addr, nxt in rows]
    assert len({node[2] for node in nodes}) == len(nodes), "Repeated TCB address"
    for i, (_, prev, addr, nxt) in enumerate(nodes):
        assert addr != 0
        assert prev == (nodes[i - 1][2] if i else 0), "Broken previous TCB link"
        assert nxt == (nodes[i + 1][2] if i + 1 < len(nodes) else 0), "Broken next TCB link"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--algorithm", choices=["RM", "FIFO"], required=True)
    parser.add_argument("--random", type=int, default=20)
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--artifacts", type=Path)
    parser.add_argument("--fixtures-only", action="store_true")
    args = parser.parse_args()
    exe = args.exe.resolve()
    examples = Path(__file__).resolve().parents[1] / "PA1_TaskSetExample"
    cases = []
    for number in range(1, 4):
        name = f"{args.algorithm}_TaskSet_{number}"
        text = (examples / f"{name}.txt").read_text()
        tasks = [tuple(map(int, line.split())) for line in text.splitlines() if line.strip()]
        fixture = (examples / f"Output_{name}.txt").read_text().splitlines()
        assert expected_output(tasks, args.algorithm) == fixture, f"Oracle disagrees with {name}"
        cases.append((name, text, tasks, fixture))
    edge_cases = [
        [(42, 0, 3, 3)],                         # complete exactly at deadline; same TCB/new job
        [(9, 0, 2, 6), (3, 0, 1, 6)],            # equal period, unsorted/nonconsecutive IDs
        [(7, 3, 1, 9), (2, 30, 1, 8)],           # initial idle, arrival at final tick
        [(1, 31, 1, 4)],                         # no releases during the simulation
        [(3, 0, 4, 2)],                          # execution greater than period
        [(9, 0, 3, 3), (2, 1, 1, 20)],           # FIFO must select an older waiting job
        [(8, 0, 3, 9), (7, 1, 2, 4), (4, 2, 1, 2)],  # nested preemption
        [(i, 0, 1, 100) for i in range(20, 0, -1)],    # maximum number of tasks
        [(2, 0, 2, 4), (1, 0, 2, 4)],            # simultaneous completion/release
    ]
    rng = random.Random(20261105)
    for _ in range(args.random):
        task_ids = rng.sample(range(1, 200), rng.randint(1, 6))
        edge_cases.append([(i, rng.randint(0, 10), rng.randint(1, 5), rng.randint(2, 18))
                           for i in task_ids])
    for i, tasks in enumerate([] if args.fixtures_only else edge_cases):
        # Exercise tabs, CRLF, blank lines and trailing whitespace in the C input parser.
        text = "\r\n" + "\r\n".join("\t".join(map(str, t)) + "  " for t in tasks) + "\r\n\r\n"
        cases.append((f"generated_{i:03d}", text, tasks, expected_output(tasks, args.algorithm)))
    passed = 0
    with tempfile.TemporaryDirectory(prefix="pa1-") as temp:
        work = Path(temp)
        for _ in range(args.repeat):
            for name, text, tasks, expected in cases:
                (work / "TaskSet.txt").write_bytes(text.encode("ascii"))
                result = subprocess.run([str(exe)], cwd=work, capture_output=True, timeout=45)
                assert result.returncode == 0, result.stderr.decode(errors="replace")
                actual = (work / "Output.txt").read_text().splitlines()
                assert actual == expected, name + "\n" + "\n".join(difflib.unified_diff(expected, actual))
                check_tcb_list(result.stdout.decode(errors="replace"), len(tasks))
                if args.artifacts and not name.startswith("generated"):
                    args.artifacts.mkdir(parents=True, exist_ok=True)
                    (args.artifacts / f"Output_{name}.txt").write_text("\n".join(actual) + "\n")
                    (args.artifacts / f"Console_{name}.txt").write_bytes(result.stdout)
                passed += 1
        for invalid in ["", "1 0 1", "1 0 0 4", "1 -1 1 4", "1 0 1 0", "1 0 1 3 extra",
                        "1 0 1 4\n1 0 1 5", "999999999999 0 1 3",
                        "\n".join(f"{i} 0 1 100" for i in range(21))]:
            (work / "TaskSet.txt").write_text(invalid)
            result = subprocess.run([str(exe)], cwd=work, capture_output=True, timeout=5)
            assert result.returncode != 0, f"Invalid input was accepted: {invalid}"
        (work / "TaskSet.txt").unlink()
        result = subprocess.run([str(exe)], cwd=work, capture_output=True, timeout=5)
        assert result.returncode != 0, "Missing input was accepted"
    print(f"{args.algorithm}: {passed} schedule/TCB comparisons and 10 invalid-input checks passed")


if __name__ == "__main__":
    main()

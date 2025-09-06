#!/usr/bin/env python3
import os, xml.etree.ElementTree as ET

tests = fail = err = skip = 0
LF = LH = 0

def acc(ts):
    global tests, fail, err, skip
    tests += int(ts.attrib.get("tests", 0) or 0)
    fail  += int(ts.attrib.get("failures", 0) or 0)
    err   += int(ts.attrib.get("errors", 0) or 0)
    skip  += int(ts.attrib.get("skipped", 0) or 0)

for root, _, files in os.walk("artifacts"):
    for f in files:
        p = os.path.join(root, f)

        # JUnit
        if f.endswith(".xml"):
            try:
                r = ET.parse(p).getroot()
                if r.tag == "testsuite":
                    acc(r)
                elif r.tag == "testsuites":
                    for ts in r.findall("testsuite"):
                        acc(ts)
            except Exception as e:
                print(f"::warning file={p}::Failed to parse JUnit: {e}")

        # Coverage (lcov)
        if f in ("lcov.info",) or f.endswith((".lcov",".info")):
            try:
                with open(p) as fh:
                    for line in fh:
                        if line.startswith("LF:"):
                            LF += int(line[3:])
                        elif line.startswith("LH:"):
                            LH += int(line[3:])
            except Exception as e:
                print(f"::warning file={p}::Failed to read lcov: {e}")

cov = (100.0*LH/LF) if LF else 0.0
print(f"JUnit: {tests} tests, {fail} failures, {err} errors, {skip} skipped")
print(f"Coverage: {cov:.1f}% (LH={LH}, LF={LF})")

COV_MIN = float(os.environ.get("COV_MIN", "18"))
if cov < COV_MIN or fail > 0 or err > 0:
    print(f"::error:: Gate failed (coverage {cov:.1f}% < {COV_MIN}% or test failures present)")
    # Uncomment to make the job fail on gate:
    # raise SystemExit(1)

with open(os.environ.get("GITHUB_STEP_SUMMARY","/dev/null"), "a") as s:
    s.write(f"### JUnit\n{tests} tests, {fail} failures, {err} errors, {skip} skipped\n\n")
    s.write(f"### Coverage\n{cov:.1f}% (LH={LH}, LF={LF})\n")

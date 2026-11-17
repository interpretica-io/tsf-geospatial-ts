#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# Regenerate conf/trc.xml from the package.xml tree: every test, every
# iteration (the cartesian product of its <arg> values) expected PASSED,
# except the iterations listed in EXPECTED below. Run after adding a
# test or an iteration; review the diff.
import itertools
import os
import sys
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

TOP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SUITE = os.path.join(TOP, "tsf-geospatial-ts")

# (group, test, {arg: value}) -> (result, [verdicts]). Every known-bad
# control of this suite is written to pass when its defect is caught, so
# nothing is expected to fail; a module defect found later goes here,
# with its verdict, while it is open.
EXPECTED = {}


def iterations(run):
    args = [(a.get("name"), [v.text for v in a.findall("value")])
            for a in run.findall("arg")]
    names = [a[0] for a in args]
    for combo in itertools.product(*[a[1] for a in args]):
        yield tuple(zip(names, combo))


def objective(node):
    o = node.find("objective")
    return (o.text or "").strip().split("\n")[0] if o is not None else ""


def test_xml(group, name, obj, iters, ind):
    out = ['%s<test name="%s" type="script">' % (ind, name),
           "%s  <objective>%s</objective>" % (ind, escape(obj)),
           "%s  <notes />" % ind]
    for it in iters:
        res, verdicts = EXPECTED.get((group, name, it), ("PASSED", []))
        out.append('%s  <iter result="%s">' % (ind, res))
        for k, v in it:
            out.append('%s    <arg name="%s">%s</arg>' % (ind, k, escape(v)))
        if verdicts:
            out.append("%s    <notes>Expected failure: a known defect, "
                       "one verdict each.</notes>" % ind)
        else:
            out.append("%s    <notes />" % ind)
        if verdicts:
            out.append('%s    <results>' % ind)
            out.append('%s      <result value="%s">' % (ind, res))
            for v in verdicts:
                out.append("%s        <verdict>%s</verdict>" % (ind,
                                                              escape(v)))
            out.append("%s      </result>" % ind)
            out.append("%s    </results>" % ind)
        out.append("%s  </iter>" % ind)
    out.append("%s</test>" % ind)
    return out


def main():
    top = ET.parse(os.path.join(SUITE, "package.xml")).getroot()
    lines = ['<?xml version="1.0" encoding="UTF-8" ?>',
             '<trc_db unknown_exp_status="passed_ok">',
             '  <test name="tsf-geospatial-ts" type="package">',
             "    <objective>Geospatial Test Suite</objective>",
             "    <notes />", '    <iter result="PASSED">', "      <notes />"]
    lines += test_xml("", "prologue", "Test suite prologue", [()], "      ")
    for pkg in top.iter("package"):
        if pkg.get("name") is None:
            continue
        g = pkg.get("name")
        gp = ET.parse(os.path.join(SUITE, g, "package.xml")).getroot()
        lines += ['      <test name="%s" type="package">' % g,
                  "        <objective>%s</objective>" %
                  escape((gp.find("description").text or "").strip()),
                  "        <notes />", '        <iter result="PASSED">',
                  "          <notes />"]
        lines += test_xml(g, "prologue", "Group prologue", [()],
                          "          ")
        seen = {}
        for run in gp.iter("run"):
            sc = run.find("script")
            if sc is None:
                continue
            name = sc.get("name")
            seen.setdefault(name, [objective(sc), []])
            seen[name][1] += list(iterations(run))
        for name, (obj, iters) in seen.items():
            lines += test_xml(g, name, obj, iters, "          ")
        lines += test_xml(g, "epilogue", "Group epilogue", [()],
                          "          ")
        lines += ["        </iter>", "      </test>"]
    lines += test_xml("", "epilogue", "Test suite epilogue", [()], "      ")
    lines += ["    </iter>", "  </test>", "</trc_db>"]
    with open(os.path.join(TOP, "conf", "trc.xml"), "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    sys.exit(main())
